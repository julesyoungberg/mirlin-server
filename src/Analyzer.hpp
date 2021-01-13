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
    ~Analyzer();

    void start_session(unsigned int sample_rate, unsigned int hop_size, unsigned int memory, std::vector<std::string> features);

    void end_session();

    float process_frame(std::vector<float> raw_frame);

    bool busy = false;

private:
    unsigned int sample_rate_;
    unsigned int hop_size_;
    unsigned int memory_;
    unsigned int window_size_;
    unsigned int frame_count_;

    float combine_ms_;

    std::vector<std::vector<Real>> frames_;
    std::vector<Real> window_;
    std::vector<std::string> features_;
    std::vector<std::vector<Real>> peaks_;

    /// ESSENTIA
    /// algos
    Algorithm* spectrum_;
    Algorithm* triangle_bands_;
    Algorithm* super_flux_novelty_;
    Algorithm* super_flux_peaks_;
    Algorithm* frame_cutter_;
    Algorithm* centroid_;
    Algorithm* mfcc_;
    Algorithm* power_spectrum_;
    Algorithm* windowing_;
    //// IO
    VectorOutput<std::vector<Real>>* essout_;
    VectorInput<Real>* gen_;

    Pool pool_;

    scheduler::Network* network_ = NULL;
};

#endif
