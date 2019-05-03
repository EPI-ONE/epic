#ifndef __SRC_PARAMS_H__
#define __SRC_PARAMS_H__

#include <array>
#include <iostream>
#include <sstream>

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
// max amount of of money allowd in one output
static constexpr uint64_t MAX_MONEY = 9999999999L;
// version of genesis block
static constexpr uint32_t GENESIS_BLOCK_VERSION = 1;
// maximum allowed block size in optimal encoding format
static constexpr uint32_t MAX_BLOCK_SIZE = 20 * 1000;
// an easy enough difficulty target
static constexpr uint32_t EASIEST_COMP_DIFF_TARGET = 0x2100ffffL;

class Params {
public:
    enum KeyPrefixType : uint8_t {
        PUBKEY_ADDRESS = 0,
        SECRET_KEY,

        MAX_KEY_PREFIX_TYPES
    };

    // consensus parameter setting
    uint32_t targetTimespan;
    uint32_t timeInterval;
    uint32_t interval;
    uint32_t targetTPS;
    uint32_t punctualityThred;
    arith_uint256 maxTarget;
    Coin maxMoney;
    Coin reward;

    std::array<unsigned char, MAX_KEY_PREFIX_TYPES> keyPrefixes;

    const unsigned char GetKeyPrefix(KeyPrefixType type) const;

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
