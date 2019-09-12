// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_PARAMS_H
#define EPIC_PARAMS_H

#include "arith_uint256.h"
#include "coin.h"

#include <array>
#include <iostream>
#include <sstream>
#include <string>

class Block;
class Vertex;

enum ParamsType : uint8_t {
    MAINNET = 0,
    SPADE,
    DIAMOND,
    UNITTEST,
};

static const std::string ParamsTypeStr[] = {
    "MAINNET",
    "SPADE",
    "DIAMOND",
    "UNITTEST",
};

class Params {
public:
    virtual ~Params() {}

    enum KeyPrefixType : uint8_t {
        PUBKEY_ADDRESS = 0,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        MAX_KEY_PREFIX_TYPES
    };

    // consensus parameter setting
    uint32_t magic;
    uint16_t version;
    uint32_t targetTimespan;
    uint32_t timeInterval;
    uint32_t interval;
    uint32_t targetTPS;
    uint32_t punctualityThred;
    arith_uint256 maxTarget;
    uint32_t deleteForkThreshold;

    Coin maxMoney;
    uint64_t baseReward;
    uint32_t rewardAdjustInterval;
    uint32_t msRewardCoefficient;

    float sortitionCoefficient;
    size_t sortitionThreshold;

    // proof-of-work parameter
    int cycleLen;

    size_t blockCapacity;

    unsigned char GetKeyPrefix(KeyPrefixType type) const;
    std::shared_ptr<Vertex> CreateGenesis() const;

    virtual Coin GetReward(size_t height) const;

protected:
    Params() = default;

    std::array<unsigned char, MAX_KEY_PREFIX_TYPES> keyPrefixes;
    std::string genesisHexStr;

    virtual void SetGenesisParams(const std::shared_ptr<Vertex>&) const {}
};

class MainNetParams : public Params {
public:
    MainNetParams();
};

class TestNetSpadeParams : public Params {
public:
    TestNetSpadeParams();
};

class TestNetDiamondParams : public Params {
public:
    TestNetDiamondParams();
};

class UnitTestParams : public Params {
public:
    UnitTestParams();

protected:
    void SetGenesisParams(const std::shared_ptr<Vertex>&) const override;
};

// instance of the parameters for usage throughout the project
const Params& GetParams();
void SelectParams(ParamsType type, bool withGenesis = true);

#endif // EPIC_PARAMS_H
