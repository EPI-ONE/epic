#ifndef __SRC_PARAMS_H__
#define __SRC_PARAMS_H__

#include <iostream>
#include <sstream>

/* the inclusion below do not
 * result into circular deps */
#include "arith_uint256.h"
#include "coin.h"

// 1 day per diffculty cycle on average
static constexpr uint32_t TARGET_TIMESPAN = 24 * 60 * 60;
// 10 seconds per milestone block
static constexpr uint32_t TIME_INTERVAL = 10;
static constexpr uint32_t INTERVAL      = TARGET_TIMESPAN / TIME_INTERVAL;
// transaction per second
static constexpr uint32_t TPS = 1000;
// threshold for rejecting an old block
static constexpr uint32_t PUNTUALITY_THRESHOLD = 2 * 60 * 60;
// transaction sortition: number of block to go back
static constexpr uint32_t SORTITION_THRESHOLD = 10 * 1000;
// transaction sortition: coefficient for computing allowed distance
static constexpr double SORTITION_COEFFICIENT = 0.01;
// maximum time in a block header allowed to be in advanced to the current time
static constexpr uint32_t ALLOWED_TIME_DRIFT = 2 * 60 * 60;
// max amount of money allowed in one output
static constexpr Coin MAX_MONEY = 9999999999L;
// version of genesis block
static constexpr uint32_t GENESIS_BLOCK_VERSION = 1;
// maximum allowed block size in optimal encoding format
static constexpr uint32_t MAX_BLOCK_SIZE = 20 * 1000;
// max number of signature verification OPs in a block TODO: determine the size
static constexpr int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE / 50;
// an easy enough difficulty target
static const arith_uint256 EASY_DIFFICULTY_TARGET =
    arith_uint256("20000000000000000000000000000000000000000000000000000000000000000000000000");

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

protected:
    Params() = default;
};

class TestNetParams : public Params {
public:
    static const Params& GetParams();

protected:
    TestNetParams();
};

// instance of the parameters for usage throughout the project
static const Params& params = TestNetParams::GetParams();

#endif // __SRC_PARAMS_H__
