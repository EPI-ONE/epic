#ifndef __SRC_PARAMS_H__
#define __SRC_PARAMS_H__

#include <array>
#include <iostream>
#include <sstream>
#include <string>

#include "arith_uint256.h"
#include "coin.h"

class Block;
class NodeRecord;
// 1 day per diffculty cycle on average
static constexpr uint32_t TARGET_TIMESPAN = 24 * 60 * 60;
// 10 seconds per milestone block
static constexpr uint32_t TIME_INTERVAL = 10;
// number of milestones between two difficulty adjustment
static constexpr uint32_t INTERVAL = TARGET_TIMESPAN / static_cast<double>(TIME_INTERVAL);
// transaction per second
static constexpr uint32_t TPS = 1000;
// threshold for rejecting an old block
static constexpr uint32_t PUNTUALITY_THRESHOLD = 2 * 60 * 60;
// maximum time in a block header allowed to be in advanced to the current time (sec)
static constexpr uint32_t ALLOWED_TIME_DRIFT = 1;
// max amount of money allowed in one output
static constexpr uint64_t MAX_MONEY = 9999999999L;
// version of genesis block
static constexpr uint32_t GENESIS_BLOCK_VERSION = 1;
// maximum allowed block size in optimal encoding format
static constexpr uint32_t MAX_BLOCK_SIZE = 20 * 1000;
// an easy enough difficulty target
static constexpr uint32_t EASIEST_COMP_DIFF_TARGET = 0x2100ffffL;
// transaction sortition: coefficient for computing allowed distance
static constexpr size_t SORTITION_COEFFICIENT = 100;
// transaction sortition: number of block to go back
static constexpr size_t SORTITION_THRESHOLD = 10 * 1000;
// coefficient of taking additional reward for milestone
static constexpr uint32_t REWARD_COEFFICIENT = 50;

enum ParamsType : uint8_t {
    MAINNET = 1,
    TESTNET,
    UNITTEST,
};

class Params {
public:
    static inline const std::string INITIAL_MS_TARGET = "346dc5d63886594af4f0d844d013a92a305532617c1bda5119ce075f6fd21";

    enum KeyPrefixType : uint8_t {
        PUBKEY_ADDRESS = 0,
        SECRET_KEY,

        MAX_KEY_PREFIX_TYPES
    };

    // consensus parameter setting
    uint32_t version;
    uint32_t targetTimespan;
    uint32_t timeInterval;
    uint32_t interval;
    uint32_t targetTPS;
    uint32_t punctualityThred;
    arith_uint256 maxTarget;

    Coin maxMoney;
    Coin reward;
    uint32_t msRewardCoefficient;

    arith_uint256 sortitionCoefficient;
    size_t sortitionThreshold;

    uint64_t initialDifficulty;
    arith_uint256 initialMsTarget;

    unsigned char GetKeyPrefix(KeyPrefixType type) const;
    const Block& GetGenesis() const;
    const NodeRecord& GetGenesisRecord() const;

protected:
    Params() = default;

    std::array<unsigned char, MAX_KEY_PREFIX_TYPES> keyPrefixes;

    std::unique_ptr<Block> genesis_;
    std::unique_ptr<NodeRecord> genesisRecord_;
    void CreateGenesis(const std::string& genesisHexStr);
};

class MainNetParams : public Params {
public:
    MainNetParams();
};

class TestNetParams : public Params {
public:
    TestNetParams();
};

class UnitTestParams : public Params {
public:
    UnitTestParams();
};

// instance of the parameters for usage throughout the project
const Params& GetParams();
void SelectParams(ParamsType type);

#endif // __SRC_PARAMS_H__
