#ifndef _ANALYZER
#define _ANALYZER

#include <vector>
#include <iostream>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>

using namespace essentia::streaming;

// https://github.com/GiantSteps/MC-Sonaar/blob/master/essentiaRT~/EssentiaOnset.cpp#L70
class Analyzer {
public:
    Analyzer() {}

    void start_session(unsigned int sample_rate, std::vector<std::string> features);

    void end_session();

    void process_frame(std::vector<float> frame);

    bool busy = false;

private:
    unsigned int sample_rate_;
    std::vector<std::string> features_;
};

#endif
