#ifndef _ANALYZER
#define _ANALYZER

#include <vector>
#include <iostream>
#include <essentia/algorithmfactory.h>
#include <essentia/essentiamath.h>
#include <essentia/pool.h>

using namespace essentia::streaming;

// TODO: use essentia to extract features from audio
// https://essentia.upf.edu/howto_standard_extractor.html
// TODO: investigate potential of streaming mode
// https://essentia.upf.edu/reference/streaming_VectorInput.html
// check out vector input 
// https://essentia.upf.edu/streaming_architecture.html#special-connectors
class Analyzer {
public:
    Analyzer() {}

    void start_session(unsigned int sample_rate, std::vector<std::string> features) {
        std::clog << "Analyzer session initiated with sample rate: " << std::to_string(sample_rate) << std::endl;
        this->sample_rate_ = sample_rate;
        this->features_ = features;
        this->busy = true;
    }

    void end_session() {
        std::clog << "Analyzer session ended" << std::endl;
        this->busy = false;
    }

    void process_frame(std::vector<float> frame) {
        std::clog << "Analyzing new frame of size " << frame.size() << std::endl;
    }

    bool busy = false;

private:
    unsigned int sample_rate_;
    std::vector<std::string> features_;
};

#endif
