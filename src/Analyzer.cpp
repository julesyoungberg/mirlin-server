// adapted from: https://github.com/GiantSteps/MC-Sonaar/blob/master/essentiaRT~/EssentiaOnset.cpp
#include "Analyzer.hpp"

Analyzer::Analyzer() {}

void Analyzer::start_session(unsigned int sample_rate, std::vector<std::string> features) {
    std::clog << "Analyzer session initiated with sample rate: " << std::to_string(sample_rate)
              << std::endl;
    sample_rate_ = sample_rate;
    features_ = features;
    busy = true;
    frame_size_ = 1024;
    hop_size_ = 512;

    combine_ms_ = 50;
    strength_.resize(2);

    // create algorithms
    AlgorithmFactory& factory = AlgorithmFactory::instance();

    frame_cutter_ = factory.create("FrameCutter", "frameSize", frame_size_, "hopSize", hop_size_,
                                   "startFromZero", true, "validFrameThreshold", .1,
                                   "lastFrameToEndOfFile", true, "silentFrames", "keep");

    windowing_ = factory.create("Windowing", "type", "hanning", "zeroPhase", true);

    spectrum_ = factory.create("Spectrum");

    power_spectrum_ = factory.create("PowerSpectrum");

    triangle_bands_ = factory.create("TriangularBands", "log", false);

    super_flux_novelty_ = factory.create("SuperFluxNovelty", "binWidth", 5, "frameWidth", 1);

    super_flux_peaks_ =
        factory.create("SuperFluxPeaks", "ratioThreshold", 4, "threshold", .7 / NOVELTY_MULT,
                       "pre_max", 80, "pre_avg", 120, "frameRate", sample_rate_ * 1.0 / hop_size_,
                       "combine", combine_ms_, "activation_slope", true);

    super_flux_peaks_->input(0).setAcquireSize(1);
    super_flux_peaks_->input(0).setReleaseSize(1);

    centroid_ = factory.create("Centroid");

    mfcc_ = factory.create("MFCC", "inputSize", frame_size_ / 2 + 1);

    // sliding buffer for a real time audio stream input
    ring_buffer_ = factory.create("RingBufferInput", "bufferSize", frame_size_);
    ring_buffer_->output(0).setAcquireSize(frame_size_);
    ring_buffer_->output(0).setReleaseSize(frame_size_);
    ring_buffer_->configure();

    essout_ = new VectorOutput<std::vector<Real>>();
    essout_->setVector(&strength_);
    std::clog << "hello world" << std::endl;

    // windowing
    ring_buffer_->output("signal") >> frame_cutter_->input("signal");
    frame_cutter_->output("frame") >> windowing_->input("frame");
    windowing_->output("frame") >> spectrum_->input("frame");

    // Onset detection
    spectrum_->output("spectrum") >> triangle_bands_->input("spectrum");
    triangle_bands_->output("bands") >> super_flux_novelty_->input("bands");
    super_flux_novelty_->output("Difference") >> super_flux_peaks_->input("novelty");
    super_flux_peaks_->output("strengths") >> essout_->input("data");
    super_flux_peaks_->output("peaks") >> DEVNULL;

    // MFCC
    spectrum_->output("spectrum") >> mfcc_->input("spectrum");
    mfcc_->output("bands") >> DEVNULL;

    // centroid
    spectrum_->output("spectrum") >> centroid_->input("array");

    // 2 Pool
    connectSingleValue(centroid_->output("centroid"), pool_, "i.centroid");
    connectSingleValue(mfcc_->output("mfcc"), pool_, "i.mfcc");

    network_ = new scheduler::Network(ring_buffer_);
    network_->run();
}

void Analyzer::end_session() {
    std::clog << "Analyzer session ended" << std::endl;
    busy = false;
}

void Analyzer::process_frame(std::vector<float> frame) {
    std::clog << "Analyzing new frame of size " << frame.size() << std::endl;
}
