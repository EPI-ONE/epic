#ifndef __SRC_PARAMS_H__
#define __SRC_PARAMS_H__

#include "block.h"
#include "uint256.h"
#include "coin.h"

// 1 day per diffculty cycle on average
static constexpr uint32_t TARGET_TIMESPAN = 24 * 60 * 60;

// 10 seconds per milestone block
static constexpr uint32_t TIME_INTERVAL = 10;

static constexpr uint32_t INTERVAL = TARGET_TIMESPAN / TIME_INTERVAL;

// transaction per second
static constexpr uint32_t TPS = 1000;

// threshold for rejecting an old block 
static constexpr uint32_t PUNTUALITY_THRESHOLD = 2 * 60 * 60;

// transaction sortition: number of block to go back
static constexpr uint32_t SORTITION_THRESHOLD = 10 * 1000;

// transaction sortition: coefficient for computing allowed distance
static constexpr double SORTITION_COEFFICIENT = 0.01;

// maximun number of coins to be generated 
static constexpr uint64_t MAX_COINS = 9999999999L;

class Params {
public:
    // consensus parameter setting
    //const uint256 maxTarget;
    uint32_t targetTimespan;
    uint32_t timeInterval;
    uint32_t interval;
    uint32_t targetTPS;
    uint32_t puntualityThre;
    //const Coin reward;

    //const Block& GetGenesisBlock() const { return genesisBlock; }

    const static Params& GetParams() {
        //static const auto instance = std::make_unique<Params>();
        static const Params instance;
        return instance;
    }

protected:
    Params() {};
    
    // genesis 
    //uint256 genesisBlockHash;
    //Block genesisBlock;

    //virtual void CreateGenesis()=0;

};


#endif // __SRC_PARAMS_H__
