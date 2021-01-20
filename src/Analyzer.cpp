// adapted from:
// https://github.com/GiantSteps/MC-Sonaar/blob/431048b80b86c29d9caac28ee23061cdf1013b13/essentiaRT~/EssentiaSFX.cpp
#include "Analyzer.hpp"

Analyzer::Analyzer() { essentia::init(); }

Analyzer::~Analyzer() {}

// TODO add
// Tempo maybe CNN, onset detection, Dissonance, 
void Analyzer::configure_subscription(std::vector<std::string> features) {
    // initialize default feature subscription with all falses
    subscription_ = FeatureSubscription();
    subscription_["centroid"] = false;
    subscription_["loudness"] = false;
    subscription_["noisiness"] = false;
    subscription_["pitch"] = false;
    subscription_["mfcc"] = false;
    subscription_["hpcp"] = false;
    // insert true values for features provided
    for (auto const& feature : features) {
        subscription_[feature] = true;
    }
}

bool Analyzer::is_busy() {
    std::lock_guard<std::mutex> guard(mutex_);
    return busy_;
}

void Analyzer::start_session(unsigned int sample_rate, unsigned int hop_size, unsigned int memory, std::vector<std::string> features) {
    configure_subscription(features);
    std::clog << "Analyzer session initiated with sample rate: " << std::to_string(sample_rate)
              << std::endl;
    sample_rate_ = sample_rate;
    features_ = features;
    busy_ = true;
    hop_size_ = hop_size;
    memory_ = memory;
    window_size_ = hop_size * memory;
    frame_count_ = 0;

    combine_ms_ = 50;
    window_.resize(window_size_);

    // input
    gen_ = new VectorInput<Real>();
    gen_->setVector(&window_);

    // setup
    AlgorithmFactory& factory = AlgorithmFactory::instance();
    standard::AlgorithmFactory& standard_factory = standard::AlgorithmFactory::instance();

    // create algorithms
    frame_cutter_ = factory.create("FrameCutter", "frameSize", window_size_, "hopSize", hop_size_,
                                   "startFromZero", true, "validFrameThresholdRatio", .1,
                                   "lastFrameToEndOfFile", true, "silentFrames", "keep");
    windowing_ = factory.create("Windowing", "type", "square", "zeroPhase", true);

    // Core
    spectrum_ = factory.create("Spectrum");

    // features
    if (subscription_["centroid"]) {
        centroid_ = factory.create("Centroid");
    }

    if (subscription_["loudness"]) {
        loudness_ = factory.create("InstantPower");
    }
    
    if (subscription_["noisiness"]) {
        flatness_ = factory.create("Flatness");
    }

    if (subscription_["pitch"]) {
        yin_ = factory.create("PitchYinFFT");
    }

    if (subscription_["mfcc"]) {
        mfcc_ = factory.create("MFCC", "inputSize", window_size_ / 2 + 1);
    }

    if (subscription_["hpcp"]) {
        spectral_peaks_ = factory.create("SpectralPeaks", "sampleRate", sample_rate_);
        hpcp_ = factory.create("HPCP", "size", 48);
    }

    // Aggregation
    const char* stats[] = {"mean", "var"};
    aggregator_ =
        standard_factory.create("PoolAggregator", "defaultStats", arrayToVector<std::string>(stats));

    // connect the algorithms
    gen_->output("data") >> frame_cutter_->input("signal");
    frame_cutter_->output("frame") >> windowing_->input("frame");
    windowing_->output("frame") >> spectrum_->input("frame");

    if (subscription_["centroid"]) {
        spectrum_->output("spectrum") >> centroid_->input("array");
        centroid_->output("centroid") >> PC(sfx_pool, "centroid");
    }

    if (subscription_["loudness"]) {
        frame_cutter_->output("frame") >> loudness_->input("array");
        loudness_->output("power") >> PC(sfx_pool, "loudness");
    }

    if (subscription_["noisiness"]) {
        spectrum_->output("spectrum") >> flatness_->input("array");
        flatness_->output("flatness") >> PC(sfx_pool, "noisiness");
    }

    if (subscription_["pitch"]) {
        spectrum_->output("spectrum") >> yin_->input("spectrum");
        yin_->output("pitch") >> PC(sfx_pool, "f0");
        yin_->output("pitchConfidence") >> PC(sfx_pool, "f0_fonfidence");
    }

    if (subscription_["mfcc"]) {
        spectrum_->output("spectrum") >> mfcc_->input("spectrum");
        mfcc_->output("bands") >> NOWHERE;
        mfcc_->output("mfcc") >> PC(sfx_pool, "mfcc");
    }

    if (subscription_["hpcp"]) {
        spectrum_->output("spectrum") >> spectral_peaks_->input("spectrum");
        spectral_peaks_->output("frequencies") >> hpcp_->input("frequencies");
        spectral_peaks_->output("magnitudes") >> hpcp_->input("magnitudes");
        hpcp_->output("hpcp") >> PC(sfx_pool, "hpcp");
    }

    aggregator_->input("input").set(sfx_pool);
    aggregator_->output("output").set(aggr_pool);
    network_ = new scheduler::Network(gen_);

    last_frame_ = std::chrono::system_clock::now();
    timer_thread_ = std::thread(&Analyzer::timer, this);
}

void Analyzer::clear() {
    network_->reset();
    frame_cutter_->reset();
    std::fill(window_.begin(), window_.end(), 0);
    gen_->setVector(&window_);
    aggr_pool.clear();
    sfx_pool.clear();
}

void Analyzer::end() {
    std::lock_guard<std::mutex> guard(mutex_);
    clear();
    std::clog << "Analyzer session ended" << std::endl;
    busy_ = false;
}

void Analyzer::end_session() {
    timer_thread_.join();
    end();
}

void Analyzer::timer() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        {
            std::lock_guard<std::mutex> guard(mutex_);
            auto now = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed_seconds = now - last_frame_;

            if (elapsed_seconds.count() > 5) {
                break;
            }
        }
    }

    end();
}

void Analyzer::process_frame(std::vector<Real> frame) {
    std::lock_guard<std::mutex> guard(mutex_);
    last_frame_ = std::chrono::system_clock::now();

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
    gen_->process();
    network_->run();
}

void Analyzer::aggregate() {
    aggregator_->compute();

    // rescaling values afterward
    aggr_pool.set("centroid.mean", aggr_pool.value<Real>("centroid.mean") * sample_rate_ / 2);
    aggr_pool.set("centroid.var",
                  aggr_pool.value<Real>("centroid.var") * sample_rate_ * sample_rate_ / 4);

    // normalize mfcc
    if (aggr_pool.contains<std::vector<Real>>("mfcc.mean")) {
        std::vector<Real> v = aggr_pool.value<std::vector<Real>>("mfcc.mean");
        float factor = (window_size_);
        aggr_pool.remove("mfcc.mean");
        for (auto& e : v) {
            aggr_pool.add("mfcc.mean", e * 1.0 / factor);
        }

        v = aggr_pool.value<std::vector<Real>>("mfcc.var");
        factor *= factor;
        aggr_pool.remove("mfcc.var");
        for (auto& e : v) {
            aggr_pool.add("mfcc.var", e * 1.0 / factor);
        }
    } else if (sfx_pool.contains<std::vector<std::vector<Real>>>("mfcc")) {
        // only one frame was aquired  ( no aggregation but we still want output!)
        std::vector<Real> v = sfx_pool.value<std::vector<std::vector<Real>>>("mfcc")[0];
        float factor = (window_size_);
        aggr_pool.removeNamespace("mfcc");
        aggr_pool.remove("mfcc");
        for (auto& e : v) {
            aggr_pool.add("mfcc.mean", e * 1.0 / factor);
            aggr_pool.add("mfcc.var", 0);
        }

        v = sfx_pool.value<std::vector<std::vector<Real>>>("hpcp")[0];

        aggr_pool.removeNamespace("hpcp");
        aggr_pool.remove("hpcp");
        for (auto& e : v) {
            aggr_pool.add("hpcp.mean", e);
            aggr_pool.add("hpcp.var", 0);
        }
    }
}

Features Analyzer::extract_features(const Pool& p) {
    Features vectors_out;

    Features vectors_in = p.getRealPool();
    for (auto const& iter : vectors_in) {
        std::string k = iter.first;
        std::vector<Real> v = (iter.second);
        vectors_out[k] = v;
    }

    Features reals_in = p.getSingleVectorRealPool();
    for (auto const& iter : reals_in) {
        std::string k = iter.first;
        std::vector<Real> v = iter.second;
        vectors_out[k] = v;
    }

    auto reals2_in = p.getSingleRealPool();
    for (auto const& iter : reals2_in) {
        std::string k = iter.first;
        std::vector<Real> v = std::vector<Real>(1, iter.second);
        vectors_out[k] = v;
    }

    return vectors_out;
}

Features Analyzer::get_features() {
    std::lock_guard<std::mutex> guard(mutex_);

    if (sfx_pool.getRealPool().size() < 1) {
        return std::map<std::string, std::vector<Real>>();
    }

    aggregate();

    auto features = extract_features(aggr_pool);

    clear();

    return features;
}
