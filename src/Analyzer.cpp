#include "Analyzer.hpp"

Analyzer::Analyzer() {}

void Analyzer::start_session(unsigned int sample_rate, std::vector<std::string> features) {
    std::clog << "Analyzer session initiated with sample rate: " << std::to_string(sample_rate) << std::endl;
    this->sample_rate_ = sample_rate;
    this->features_ = features;
    this->busy = true;
}

void Analyzer::end_session() {
    std::clog << "Analyzer session ended" << std::endl;
    this->busy = false;
}

void Analyzer::process_frame(std::vector<float> frame) {
    std::clog << "Analyzing new frame of size " << frame.size() << std::endl;
}
