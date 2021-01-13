// adapted from: https://github.com/GiantSteps/MC-Sonaar/blob/master/essentiaRT~/EssentiaOnset.cpp
#include "Analyzer.hpp"

Analyzer::Analyzer() {
    essentia::init();
}

Analyzer::~Analyzer() {
    end_session();
}

void Analyzer::start_session(unsigned int sample_rate, unsigned int hop_size, unsigned int memory, std::vector<std::string> features) {
    std::clog << "Analyzer session initiated with sample rate: " << sample_rate
              << std::endl;
    sample_rate_ = sample_rate;
    features_ = features;
    busy = true;
    hop_size_ = hop_size;
    memory_ = memory;
    window_size_ = hop_size * memory;
    frame_count_ = 0;

    combine_ms_ = 50;
    peaks_.resize(2);
    window_.resize(window_size_);

    // create algorithms
    AlgorithmFactory& factory = AlgorithmFactory::instance();

    frame_cutter_ = factory.create("FrameCutter", "frameSize", window_size_, "hopSize", hop_size_,
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

    mfcc_ = factory.create("MFCC", "inputSize", window_size_ / 2 + 1);

    gen_ = new VectorInput<Real>();
    gen_->setVector(&window_);

    essout_ = new VectorOutput<std::vector<Real>>();
    essout_->setVector(&peaks_);

    // windowing
    gen_->output("data") >> frame_cutter_->input("signal");
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

    network_ = new scheduler::Network(gen_);
}

void Analyzer::end_session() {
    std::clog << "Analyzer session ended" << std::endl;
    delete spectrum_;
    delete triangle_bands_;
    delete super_flux_novelty_;
    delete super_flux_peaks_;
    delete frame_cutter_;
    delete centroid_;
    delete mfcc_;
    delete power_spectrum_;
    delete windowing_;
    delete network_;
    busy = false;
}

float Analyzer::process_frame(std::vector<float> raw_frame) {
    std::clog << "Received frame of size " << raw_frame.size() << std::endl;
    peaks_.clear();
    essout_->setVector(&peaks_);

    // convert floats to reals
    std::vector<Real> frame(raw_frame.size());
    for (int i = 0; i < raw_frame.size(); i++) {
        frame[i] = Real(raw_frame[i]);
    }

    // add frame to memory and remove the oldest if needed
    frames_.push_back(frame);
    if (frames_.size() > memory_) {
        frames_.erase(frames_.begin());
    }

    // create window from frame memory
    std::fill(window_.begin(), window_.end(), 0);
    for (int f = 0; f < frames_.size(); f++) {
        for (int i = 0; i < hop_size_; i++) {
            window_[f * hop_size_ + i] = frames_[f][i];
        }
    }

    gen_->setVector(&window_);

    // std::clog << "processing window" << std::endl;
    // gen_->process();

    std::clog << "running network" << std::endl;
    network_->run();

    // std::clog << "final produce" << std::endl;
    // dynamic_cast<AccumulatorAlgorithm*>(super_flux_peaks_)->finalProduce();
    // std::clog << "output buffer size: " << peaks_.size() << std::endl;

    std::clog << "done" << std::endl;
    float val = -1.0f;
    frame_count_ += frame.size();

    // if (frame_count_ * 1000.0 > combine_ms_ * sample_rate_) {
    //     if (peaks_.size() > 0 && peaks_[0].size() > 0) {
    //         val = peaks_[0][0];

    //         if (val > 0) {
    //             val *= NOVELTY_MULT;
    //             frame_count_ = 0;
    //         }
    //     }
    // }

    // pool_.set("i.centroid", pool_.value<Real>("i.centroid")* sample_rate_ / 2);
    // std::vector<Real> v = pool_.value<std::vector<Real>>("i.mfcc");
    // for (auto& e : v) {
    //     e /= (window_size_ / 2 + 1);
    // }
    // pool_.set("i.mfcc", v);
    // peaks_.clear();
    // essout_->setVector(&peaks_);

    return val;
}
