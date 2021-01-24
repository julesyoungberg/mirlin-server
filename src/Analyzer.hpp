#ifndef _ANALYZER
#define _ANALYZER

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>
#include <essentia/streaming/algorithms/ringbufferinput.h>
#include <essentia/streaming/algorithms/vectorinput.h>
#include <essentia/streaming/algorithms/vectoroutput.h>
#include <essentia/streaming/streamingalgorithm.h>

#include <essentia/scheduler/network.h>
#include <essentia/streaming/algorithms/poolstorage.h>

#include "WebsocketServer.hpp"

using namespace essentia;
using namespace streaming;

#define NOVELTY_MULT 1000000

typedef std::map<std::string, bool> FeatureSubscription;
typedef std::map<std::string, std::vector<Real>> Features;

class Analyzer {
public:
    Analyzer();
    ~Analyzer();

    bool is_busy();

    void start_session(ClientConnection conn, unsigned int sample_rate, unsigned int hop_size,
                       unsigned int memory, std::vector<std::string> features);

    void end_session();

    void process_frame(std::vector<float> frame);

    void buffer_frame(std::vector<float> frame);

    template <typename FeaturesCallback> void handle_features(FeaturesCallback handler) {
        feature_handler_ = handler;
    }

    Features get_features();

private:
    void configure_subscription(std::vector<std::string> features);
    void timer();
    void clear();
    void end();
    void aggregate();
    Features extract_features(const Pool& p);
    void analyze();

    bool busy_ = false;
    bool analyzing_ = false;
    unsigned int sample_rate_;
    unsigned int hop_size_;
    unsigned int memory_;
    unsigned int window_size_;
    unsigned int frame_count_;

    FeatureSubscription subscription_;

    float combine_ms_;

    std::vector<std::vector<Real>> frames_;
    std::vector<Real> window_;
    std::vector<std::string> features_;

    /// ESSENTIA
    /// algos
    streaming::Algorithm* frame_cutter_;
    streaming::Algorithm* windowing_;
    streaming::Algorithm* rms_;
    streaming::Algorithm* energy_;
    streaming::Algorithm* centroid_;
    streaming::Algorithm* loudness_;
    streaming::Algorithm* spectrum_;
    streaming::Algorithm* flatness_;
    streaming::Algorithm* yin_;
    streaming::Algorithm* mfcc_;
    streaming::Algorithm* hpcp_;
    streaming::Algorithm* spectral_peaks_;
    streaming::Algorithm* dissonance_;
    streaming::Algorithm* key_;
    streaming::Algorithm* tristimulus_;
    streaming::Algorithm* spectral_contrast_;
    streaming::Algorithm* spectral_complexity_;
    streaming::Algorithm* chroma_;
    streaming::Algorithm* triangle_bands_;
    streaming::Algorithm* super_flux_novelty_;
    streaming::Algorithm* super_flux_peaks_;
    //// IO
    VectorInput<Real>* gen_;

    essentia::standard::Algorithm* aggregator_;

    scheduler::Network* network_ = NULL;

    Pool aggr_pool_;
    Pool sfx_pool_;
    Pool onset_pool_;

    std::thread timer_thread_;
    std::thread analyzer_thread_;
    std::chrono::time_point<std::chrono::system_clock> last_frame_;
    std::mutex mutex_;

    ClientConnection conn_;
    std::function<void(ClientConnection, Features)> feature_handler_;
};

#endif
