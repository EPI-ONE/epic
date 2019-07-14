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
    uint32_t deleteForkThreshold;

    Coin maxMoney;
    Coin reward;
    uint32_t msRewardCoefficient;

    arith_uint256 sortitionCoefficient;
    size_t sortitionThreshold;

    uint64_t initialDifficulty;
    arith_uint256 initialMsTarget;

    size_t cacheStatesSize;
    size_t cacheStatesToDelete;

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
