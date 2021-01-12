// adapted from: https://github.com/GiantSteps/MC-Sonaar/blob/431048b80b86c29d9caac28ee23061cdf1013b13/essentiaRT~/EssentiaOnset.h
#ifndef _ANALYZER
#define _ANALYZER

#include <iostream>
#include <vector>
#include <iostream>
#include <thread>
// #include <unistd.h>
// #include <chrono>

#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>
#include <essentia/scheduler/network.h>
#include <essentia/streaming/accumulatoralgorithm.h>
#include <essentia/streaming/algorithms/poolstorage.h>
#include <essentia/streaming/algorithms/ringbufferinput.h>
#include <essentia/streaming/algorithms/ringbufferoutput.h>
#include <essentia/streaming/algorithms/vectorinput.h>
#include <essentia/streaming/algorithms/vectoroutput.h>
#include <essentia/streaming/streamingalgorithm.h>

using namespace essentia;
using namespace streaming;

#define NOVELTY_MULT 1000000

class Analyzer {
public:
    Analyzer();

    void start_session(unsigned int sample_rate, std::vector<std::string> features);

    void end_session();

    float process_frame(std::vector<float> raw_frame);

    bool busy = false;

private:
    unsigned int sample_rate_;
    unsigned int frame_size_;
    unsigned int hop_size_;
    unsigned int frame_count_;

    float combine_ms_;

    std::vector<std::string> features_;
    std::vector<std::vector<Real>> peaks_;

    /// ESSENTIA
    /// algos
    streaming::Algorithm* spectrum_;
    streaming::Algorithm* triangle_bands_;
    streaming::Algorithm* super_flux_novelty_;
    streaming::Algorithm* frame_cutter_;
    streaming::Algorithm* centroid_;
    streaming::Algorithm* mfcc_;
    streaming::Algorithm* power_spectrum_;
    streaming::Algorithm* windowing_;
    streaming::Algorithm* super_flux_peaks_;
    //// IO
    streaming::VectorOutput<std::vector<Real>>* essout_;
    RingBufferInput* ring_buffer_;

    Pool pool_;

    scheduler::Network* network_ = NULL;

    std::thread worker_;
};

#endif
