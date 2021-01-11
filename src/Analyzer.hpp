#ifndef _ANALYZER
#define _ANALYZER

#include <vector>
#include <iostream>

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

    bool busy = false;

private:
    std::vector<std::string> features_;
};

#endif
