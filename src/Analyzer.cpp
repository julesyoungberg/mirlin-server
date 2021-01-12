// adapted from:
// https://github.com/GiantSteps/MC-Sonaar/blob/431048b80b86c29d9caac28ee23061cdf1013b13/essentiaRT~/EssentiaSFX.cpp
#include "Analyzer.hpp"

Analyzer::Analyzer() { essentia::init(); }

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

    // IO
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

    // setup
    AlgorithmFactory& factory = AlgorithmFactory::instance();
    standard::AlgorithmFactory& standard_factory = standard::AlgorithmFactory::instance();

    // create algorithms
    frame_cutter_ = factory.create("FrameCutter", "frameSize", frame_size_, "hopSize", hop_size_,
                                   "startFromZero", true, "validFrameThresholdRatio", .1,
                                   "lastFrameToEndOfFile", true, "silentFrames", "keep");
    windowing_ = factory.create("Windowing", "type", "square", "zeroPhase", true);
    envelope_ = factory.create("Envelope");

    // Core
    centroid_ = factory.create("Centroid");
    loudness_ = factory.create("InstantPower");
    spectrum_ = factory.create("Spectrum");
    flatness_ = factory.create("Flatness");
    yin_ = factory.create("PitchYinFFT");
    tc_total_ = factory.create("TCToTotal");
    mfcc_ = factory.create("MFCC", "inputSize", frame_size_ / 2 + 1);
    hpcp_ = factory.create("HPCP", "size", 48);
    spectral_peaks_ = factory.create("SpectralPeaks", "sampleRate", sample_rate_);

    // Aggregation
    const char* stats[] = {"mean", "var"};
    aggregator_ =
        standard_factory.create("PoolAggregator", "defaultStats", arrayToVector<std::string>(stats));

    // connect the algorithms
    ring_buffer_->output("signal") >> frame_cutter_->input("signal");
    ring_buffer_->output("signal") >> envelope_->input("signal");

    frame_cutter_->output("frame") >> windowing_->input("frame");
    frame_cutter_->output("frame") >> loudness_->input("array");

    windowing_->output("frame") >> spectrum_->input("frame");

    spectrum_->output("spectrum") >> flatness_->input("array");
    spectrum_->output("spectrum") >> yin_->input("spectrum");
    spectrum_->output("spectrum") >> mfcc_->input("spectrum");
    spectrum_->output("spectrum") >> spectral_peaks_->input("spectrum");
    spectrum_->output("spectrum") >> centroid_->input("array");

    mfcc_->output("bands") >> NOWHERE;

    spectral_peaks_->output("frequencies") >> hpcp_->input("frequencies");
    spectral_peaks_->output("magnitudes") >> hpcp_->input("magnitudes");

    envelope_->output("signal") >> tc_total_->input("envelope");

    // output
    flatness_->output("flatness") >> PC(sfx_pool, "noisiness");
    loudness_->output("power") >> PC(sfx_pool, "loudness");
    yin_->output("pitch") >> PC(sfx_pool, "f0");
    yin_->output("pitchConfidence") >> PC(sfx_pool, "f0_fonfidence");
    mfcc_->output("mfcc") >> PC(sfx_pool, "mfcc");
    centroid_->output("centroid") >> PC(sfx_pool, "centroid");
    hpcp_->output("hpcp") >> PC(sfx_pool, "hpcp");

    tc_total_->output("TCToTotal") >> PC(aggr_pool, "temp_centroid");
    aggregator_->input("input").set(sfx_pool);
    aggregator_->output("output").set(aggr_pool);

    network_ = new scheduler::Network(ring_buffer_);
    network_->runPrepare();
    // worker_ = std::thread([this]() { this->network_->run(); });
}

void Analyzer::clear() {
    network_->reset();
    frame_cutter_->reset();
    ring_buffer_->reset();
    aggr_pool.clear();
    sfx_pool.clear();
}

void Analyzer::end_session() {
    // worker_.join();
    clear();
    std::clog << "Analyzer session ended" << std::endl;
    busy = false;
}

void Analyzer::process_frame(std::vector<Real> frame) {
    ring_buffer_->add(&frame[0], frame.size());
    std::clog << "processing ring buffer" << std::endl;
    ring_buffer_->process();
    std::clog << "running network" << std::endl;
    network_->runStep();
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
        float factor = (frame_size_);
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
        float factor = (frame_size_);
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
    if (sfx_pool.getRealPool().size() < 1) {
        return std::map<std::string, std::vector<Real>>();
    }

    aggregate();

    auto features = extract_features(aggr_pool);

    clear();

    return features;
}
