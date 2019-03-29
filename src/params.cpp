#ifndef __SRC_PARAMS_CPP__
#define __SRC_PARAMS_CPP__ 

#include "params.h"

class TestNetParams : public Params {
    public:
    const static Params& GetParams() {
        //static const auto instance = std::make_unique<Params>();
        static const TestNetParams instance;
        return instance;
    }
    protected: 
        TestNetParams() {
            targetTimespan = TARGET_TIMESPAN;
            timeInterval = TIME_INTERVAL;
            interval = INTERVAL;
            targetTPS = 100;
            puntualityThre = PUNTUALITY_THRESHOLD;
        }
};

#endif // __SRC_PARAMS_CPP__
