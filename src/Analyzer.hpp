#ifndef _ANALYZER
#define _ANALYZER

#include <vector>
#include <iostream>

// TODO: use essentia to extract features from audio
// https://essentia.upf.edu/howto_standard_extractor.html
class Analyzer {
public:
    Analyzer() {}

    void start_session(std::vector<std::string> features) {
        std::clog << "Analyzer session initiated" << std::endl;
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
    std::vector<std::string> features_;
};

#endif
