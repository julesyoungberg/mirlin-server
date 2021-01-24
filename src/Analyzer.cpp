// adapted from:
// https://github.com/GiantSteps/MC-Sonaar/blob/431048b80b86c29d9caac28ee23061cdf1013b13/essentiaRT~/EssentiaSFX.cpp
#include "Analyzer.hpp"

Analyzer::Analyzer() { essentia::init(); }

Analyzer::~Analyzer() {}

void Analyzer::configure_subscription(std::vector<std::string> features) {
    // initialize default feature subscription with all falses
    subscription_ = FeatureSubscription();
    subscription_["rms"] = false;
    subscription_["energy"] = false;
    subscription_["centroid"] = false;
    subscription_["loudness"] = false;
    subscription_["noisiness"] = false;
    subscription_["pitch"] = false;
    subscription_["mfcc"] = false;
    subscription_["dissonance"] = false;
    subscription_["key"] = false;
    subscription_["tristimulus"] = false;
    subscription_["spectral_contrast"] = false;
    subscription_["spectral_complexity"] = false;
    subscription_["chroma"] = false; // TODO: fix - needs input frame size of 32768
    subscription_["onset"] = false;
    // insert true values for features provided
    for (auto const& feature : features) {
        subscription_[feature] = true;
    }
}

bool Analyzer::is_busy() {
    std::lock_guard<std::mutex> guard(mutex_);
    return busy_;
}

void Analyzer::start_session(ClientConnection conn, unsigned int sample_rate, unsigned int hop_size,
                             unsigned int memory, std::vector<std::string> features) {
    conn_ = conn;
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

    spectrum_ = factory.create("Spectrum");
    spectral_peaks_ = factory.create("SpectralPeaks", "sampleRate", sample_rate_);

    if (subscription_["rms"]) {
        rms_ = factory.create("RMS");
    }

    if (subscription_["energy"]) {
        energy_ = factory.create("Energy");
    }

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

    if (subscription_["dissonance"]) {
        dissonance_ = factory.create("Dissonance");
    }

    if (subscription_["key"]) {
        hpcp_ = factory.create("HPCP", "size", 48);
        key_ = factory.create("Key");
    }

    if (subscription_["tristimulus"]) {
        tristimulus_ = factory.create("Tristimulus");
    }

    if (subscription_["spectral_contrast"]) {
        spectral_contrast_ = factory.create("SpectralContrast");
    }

    if (subscription_["spectral_complexity"]) {
        spectral_complexity_ = factory.create("SpectralComplexity");
    }

    if (subscription_["chroma"]) {
        chroma_ = factory.create("Chromagram");
    }

    if (subscription_["onset"]) {
        triangle_bands_ = factory.create("TriangularBands", "log", false);
        super_flux_novelty_ = factory.create("SuperFluxNovelty", "binWidth", 5, "frameWidth", 1);
        super_flux_peaks_ = factory.create("SuperFluxPeaks", "ratioThreshold", 4, "threshold",
                                           .7 / NOVELTY_MULT, "pre_max", 80, "pre_avg", 120,
                                           "frameRate", sample_rate_ * 1.0 / hop_size_, "combine",
                                           combine_ms_);
        super_flux_peaks_->input(0).setAcquireSize(1);
        super_flux_peaks_->input(0).setReleaseSize(1);
    }

    // Aggregation
    const char* stats[] = {"mean", "var"};
    aggregator_ = standard_factory.create("PoolAggregator", "defaultStats",
                                          arrayToVector<std::string>(stats));

    // connect the algorithms
    gen_->output("data") >> frame_cutter_->input("signal");
    frame_cutter_->output("frame") >> windowing_->input("frame");
    windowing_->output("frame") >> spectrum_->input("frame");
    spectrum_->output("spectrum") >> spectral_peaks_->input("spectrum");

    if (subscription_["rms"]) {
        windowing_->output("frame") >> rms_->input("array");
        rms_->output("rms") >> PC(sfx_pool_, "rms");
    }

    if (subscription_["energy"]) {
        windowing_->output("frame") >> energy_->input("array");
        energy_->output("energy") >> PC(sfx_pool_, "energy");
    }

    if (subscription_["centroid"]) {
        spectrum_->output("spectrum") >> centroid_->input("array");
        centroid_->output("centroid") >> PC(sfx_pool_, "centroid");
    }

    if (subscription_["loudness"]) {
        frame_cutter_->output("frame") >> loudness_->input("array");
        loudness_->output("power") >> PC(sfx_pool_, "loudness");
    }

    if (subscription_["noisiness"]) {
        spectrum_->output("spectrum") >> flatness_->input("array");
        flatness_->output("flatness") >> PC(sfx_pool_, "noisiness");
    }

    if (subscription_["pitch"]) {
        spectrum_->output("spectrum") >> yin_->input("spectrum");
        yin_->output("pitch") >> PC(sfx_pool_, "f0");
        yin_->output("pitchConfidence") >> PC(sfx_pool_, "f0_fonfidence");
    }

    if (subscription_["mfcc"]) {
        spectrum_->output("spectrum") >> mfcc_->input("spectrum");
        mfcc_->output("bands") >> NOWHERE;
        mfcc_->output("mfcc") >> PC(sfx_pool_, "mfcc");
    }

    if (subscription_["dissonance"]) {
        spectral_peaks_->output("frequencies") >> dissonance_->input("frequencies");
        spectral_peaks_->output("magnitudes") >> dissonance_->input("magnitudes");
        dissonance_->output("dissonance") >> PC(sfx_pool_, "dissonance");
    }

    if (subscription_["key"]) {
        spectral_peaks_->output("frequencies") >> hpcp_->input("frequencies");
        spectral_peaks_->output("magnitudes") >> hpcp_->input("magnitudes");
        hpcp_->output("hpcp") >> key_->input("pcp");
        key_->output("key") >> PC(sfx_pool_, "key");
        key_->output("scale") >> PC(sfx_pool_, "scale");
        key_->output("strength") >> PC(sfx_pool_, "key_strength");
    }

    if (subscription_["tristimulus"]) {
        spectral_peaks_->output("frequencies") >> tristimulus_->input("frequencies");
        spectral_peaks_->output("magnitudes") >> tristimulus_->input("magnitudes");
        tristimulus_->output("tristimulus") >> PC(sfx_pool_, "tristimulus");
    }

    if (subscription_["spectral_contrast"]) {
        spectrum_->output("spectrum") >> spectral_contrast_->input("spectrum");
        spectral_contrast_->output("spectralContrast") >> PC(sfx_pool_, "spectral_contrast");
        spectral_contrast_->output("spectralValley") >> PC(sfx_pool_, "spectral_valley");
    }

    if (subscription_["spectral_complexity"]) {
        spectrum_->output("spectrum") >> spectral_complexity_->input("spectrum");
        spectral_complexity_->output("spectralComplexity") >> PC(sfx_pool_, "spectral_complexity");
    }

    if (subscription_["chroma"]) {
        windowing_->output("frame") >> chroma_->input("frame");
        chroma_->output("chromagram") >> PC(sfx_pool_, "chroma");
    }

    if (subscription_["onset"]) {
        spectrum_->output("spectrum") >> triangle_bands_->input("spectrum");
        triangle_bands_->output("bands") >> super_flux_novelty_->input("bands");
        super_flux_novelty_->output("differences") >> super_flux_peaks_->input("novelty");
        super_flux_peaks_->output("peaks") >> PC(onset_pool_, "onset");
    }

    aggregator_->input("input").set(sfx_pool_);
    aggregator_->output("output").set(aggr_pool_);
    network_ = new scheduler::Network(gen_);

    last_frame_ = std::chrono::system_clock::now();
    timer_thread_ = std::thread(&Analyzer::timer, this);
    analyzer_thread_ = std::thread(&Analyzer::analyze, this);
}

void Analyzer::clear() {
    network_->reset();
    frame_cutter_->reset();
    std::fill(window_.begin(), window_.end(), 0);
    gen_->setVector(&window_);
    aggr_pool_.clear();
    sfx_pool_.clear();
}

void Analyzer::end() {
    analyzer_thread_.join();
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

            if (!analyzing_ && elapsed_seconds.count() > 5) {
                break;
            }
        }
    }

    end();
}

void Analyzer::buffer_frame(std::vector<Real> frame) {
    std::lock_guard<std::mutex> guard(mutex_);
    last_frame_ = std::chrono::system_clock::now();
    frames_.push_back(frame);
    if (frames_.size() > memory_) {
        frames_.erase(frames_.begin());
    }
}

void Analyzer::aggregate() {
    aggregator_->compute();

    // rescaling values afterward
    aggr_pool_.set("centroid.mean", aggr_pool_.value<Real>("centroid.mean") * sample_rate_ / 2);
    aggr_pool_.set("centroid.var",
                  aggr_pool_.value<Real>("centroid.var") * sample_rate_ * sample_rate_ / 4);

    // normalize mfcc
    if (aggr_pool_.contains<std::vector<Real>>("mfcc.mean")) {
        std::vector<Real> v = aggr_pool_.value<std::vector<Real>>("mfcc.mean");
        float factor = (window_size_);
        aggr_pool_.remove("mfcc.mean");
        for (auto& e : v) {
            aggr_pool_.add("mfcc.mean", e * 1.0 / factor);
        }

        v = aggr_pool_.value<std::vector<Real>>("mfcc.var");
        factor *= factor;
        aggr_pool_.remove("mfcc.var");
        for (auto& e : v) {
            aggr_pool_.add("mfcc.var", e * 1.0 / factor);
        }
    } else if (sfx_pool_.contains<std::vector<std::vector<Real>>>("mfcc")) {
        // only one frame was aquired  ( no aggregation but we still want output!)
        std::vector<Real> v = sfx_pool_.value<std::vector<std::vector<Real>>>("mfcc")[0];
        float factor = (window_size_);
        aggr_pool_.removeNamespace("mfcc");
        aggr_pool_.remove("mfcc");
        for (auto& e : v) {
            aggr_pool_.add("mfcc.mean", e * 1.0 / factor);
            aggr_pool_.add("mfcc.var", 0);
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
    if (sfx_pool_.getRealPool().size() < 1) {
        return std::map<std::string, std::vector<Real>>();
    }

    aggregate();

    auto features = extract_features(aggr_pool_);

    if (subscription_["onset"]) {
        auto onset = extract_features(onset_pool_);
        features["onset"] = onset["onset"];
    }

    clear();

    return features;
}

void Analyzer::analyze() {
    while (true) {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            std::fill(window_.begin(), window_.end(), 0);
            for (int f = 0; f < frames_.size(); f++) {
                for (int i = 0; i < hop_size_; i++) {
                    window_[f * hop_size_ + i] = frames_[f][i];
                }
            }
            gen_->setVector(&window_);
            analyzing_ = true;
        }

        gen_->process();
        network_->run();
        auto features = get_features();
        feature_handler_(conn_, features);

        {
            std::lock_guard<std::mutex> guard(mutex_);
            analyzing_ = false;
        }
    }
}
