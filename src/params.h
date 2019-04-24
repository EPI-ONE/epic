#ifndef __SRC_PARAMS_H__
#define __SRC_PARAMS_H__

#include "arith_uint256.h"
#include "block.h"
#include "coin.h"
#include "uint256.h"
#include <iostream>
#include <sstream>

// day(s) per diffculty cycle on average
static constexpr uint32_t TARGET_TIMESPAN = 24 * 60 * 60;
// second(s) per milestone block
static constexpr uint32_t TIME_INTERVAL = 10;
// number of milestones per difficulty adjustment
static constexpr uint32_t INTERVAL = TARGET_TIMESPAN / TIME_INTERVAL;
// transaction(s) per second
static constexpr uint32_t TPS = 1000;
// threshold of rejecting an old block
static constexpr uint32_t PUNTUALITY_THRESHOLD = 2 * 60 * 60;
// transaction sortition: number of blocks to traverse back
static constexpr uint32_t SORTITION_THRESHOLD = 10 * 1000;
// transaction sortition coefficient
static constexpr double SORTITION_COEFFICIENT = 0.01;
// maximum time in a block header allowed to be in advanced to the current time
static constexpr uint32_t ALLOWED_TIME_DRIFT = 2 * 60 * 60;
// default genesis block version number
static constexpr uint32_t GENESIS_BLOCK_VERSION = 1;
// max block size
static constexpr uint32_t MAX_BLOCK_SIZE = 20 * 1000;
// max number of signature verification OPs in a block TODO: determine the size
static constexpr int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE / 50;
// max amount of money in one output
static constexpr Coin MAX_MONEY = 9999999999L;

class Params {
public:
    // consensus parameter setting
    uint32_t targetTimespan;
    uint32_t timeInterval;
    uint32_t interval;
    uint32_t targetTPS;
    uint32_t punctualityThred;
    arith_uint256 maxTarget;
    Coin maxMoney;
    Coin reward;
    const Block& GetGenesisBlock() const {
        return genesisBlock;
    }
    const uint256& GetGenesisBlockHash() const {
        return genesisBlockHash;
    }

protected:
    Params(){};
    // genesis
    uint256 genesisBlockHash;
    Block genesisBlock;
    virtual void CreateGenesis() = 0;
};

class TestNetParams : public Params {
public:
    static const Params& GetParams() {
        static const TestNetParams instance;
        return instance;
    }

protected:
    TestNetParams() {
        targetTimespan          = TARGET_TIMESPAN;
        timeInterval            = TIME_INTERVAL;
        interval                = INTERVAL;
        targetTPS               = 100;
        punctualityThred        = PUNTUALITY_THRESHOLD;
        arith_uint256 maxTarget = arith_uint256().SetCompact(0x2100ffffL);
        maxMoney                = MAX_MONEY;
        reward                  = 1;
        CreateGenesis();
        genesisBlockHash = genesisBlock.GetHash();
    }
    void CreateGenesis();
};

#endif // __SRC_PARAMS_H__
