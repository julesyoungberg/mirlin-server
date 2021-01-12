// adapted from: https://github.com/GiantSteps/MC-Sonaar/blob/master/essentiaRT~/EssentiaOnset.cpp
#include "Analyzer.hpp"

Analyzer::Analyzer() {
    essentia::init();
}

void Analyzer::start_session(unsigned int sample_rate, std::vector<std::string> features) {
    std::clog << "Analyzer session initiated with sample rate: " << std::to_string(sample_rate)
              << std::endl;
    sample_rate_ = sample_rate;
    features_ = features;
    busy = true;
    frame_size_ = 1024;
    hop_size_ = 512;
    frame_count_ = 0;

    combine_ms_ = 50;
    peaks_.resize(2);

    // create algorithms
    AlgorithmFactory& factory = AlgorithmFactory::instance();

    frame_cutter_ = factory.create("FrameCutter", "frameSize", frame_size_, "hopSize", hop_size_,
                                   "startFromZero", true, "validFrameThresholdRatio", .1,
                                   "lastFrameToEndOfFile", true, "silentFrames", "keep");

    windowing_ = factory.create("Windowing", "type", "square", "zeroPhase", true);

    spectrum_ = factory.create("Spectrum");

    power_spectrum_ = factory.create("PowerSpectrum");

    triangle_bands_ = factory.create("TriangularBands", "log", false);

    super_flux_novelty_ = factory.create("SuperFluxNovelty", "binWidth", 5, "frameWidth", 1);

    super_flux_peaks_ =
        factory.create("SuperFluxPeaks", "ratioThreshold", 4, "threshold", .7 / NOVELTY_MULT,
                       "pre_max", 80, "pre_avg", 120, "frameRate", sample_rate_ * 1.0 / hop_size_,
                       "combine", combine_ms_);

    super_flux_peaks_->input(0).setAcquireSize(1);
    super_flux_peaks_->input(0).setReleaseSize(1);

    centroid_ = factory.create("Centroid");

    mfcc_ = factory.create("MFCC", "inputSize", frame_size_ / 2 + 1);

    // sliding buffer for a real time audio stream input
    ring_buffer_ = new RingBufferInput();
    ring_buffer_->declareParameters();
    ParameterMap pars;
    pars.add("bufferSize", frame_size_);
    ring_buffer_->setParameters(pars);
    ring_buffer_->output(0).setAcquireSize(frame_size_);
    ring_buffer_->output(0).setReleaseSize(frame_size_);
    ring_buffer_->configure();

    essout_ = new VectorOutput<std::vector<Real>>();
    essout_->setVector(&peaks_);

    // windowing
    ring_buffer_->output("signal") >> frame_cutter_->input("signal");
    frame_cutter_->output("frame") >> windowing_->input("frame");
    windowing_->output("frame") >> spectrum_->input("frame");

    // Onset detection
    spectrum_->output("spectrum") >> triangle_bands_->input("spectrum");
    triangle_bands_->output("bands") >> super_flux_novelty_->input("bands");
    super_flux_novelty_->output("differences") >> super_flux_peaks_->input("novelty");
    super_flux_peaks_->output("peaks") >> essout_->input("data");

    // MFCC
    spectrum_->output("spectrum") >> mfcc_->input("spectrum");
    mfcc_->output("bands") >> DEVNULL;

    // centroid
    spectrum_->output("spectrum") >> centroid_->input("array");

    // 2 Pool
    connectSingleValue(centroid_->output("centroid"), pool_, "i.centroid");
    connectSingleValue(mfcc_->output("mfcc"), pool_, "i.mfcc");

    network_ = new scheduler::Network(ring_buffer_);
    worker_ = std::thread([this]() { this->network_->run(); });
}

void Analyzer::end_session() {
    worker_.join();
    std::clog << "Analyzer session ended" << std::endl;
    busy = false;
}

float Analyzer::process_frame(std::vector<float> raw_frame) {
    peaks_.clear();
    essout_->setVector(&peaks_);

    // convert floats to reals
    std::vector<Real> frame(raw_frame.size());
    for (int i = 0; i < raw_frame.size(); i++) {
        frame[i] = Real(raw_frame[i]);
    }

    ring_buffer_->add(&frame[0], frame.size());
    ring_buffer_->process();

    dynamic_cast<AccumulatorAlgorithm*>(super_flux_peaks_)->finalProduce();

    float val = -1.0f;
    frame_count_ += frame.size();

    if (frame_count_ * 1000.0 > combine_ms_ * sample_rate_) {
        if (peaks_.size() > 0 && peaks_[0].size() > 0) {
            val = peaks_[0][0];

            if (val > 0) {
                val *= NOVELTY_MULT;
                frame_count_ = 0;
            }
        }
    }

    return val;
}
