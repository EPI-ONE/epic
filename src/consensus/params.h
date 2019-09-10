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
    TESTNET,
    UNITTEST,
};

static const std::string ParamsTypeStr[] = {
    "MAINNET",
    "TESTNET",
    "UNITTEST",
};

class Params {
public:
    enum KeyPrefixType : uint8_t {
        PUBKEY_ADDRESS = 0,
        SECRET_KEY,

        MAX_KEY_PREFIX_TYPES
    };

    // consensus parameter setting
    uint16_t version;
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

// proof-of-work parameters
#define EDGEBITS 29
    int cycleLen;

    size_t cacheStatesSize;

    size_t blockCapacity;

    unsigned char GetKeyPrefix(KeyPrefixType type) const;
    const Block& GetGenesis() const;
    const Vertex& GetGenesisVertex() const;

protected:
    Params() = default;

    std::array<unsigned char, MAX_KEY_PREFIX_TYPES> keyPrefixes;

    std::unique_ptr<Block> genesis_;
    std::shared_ptr<Vertex> genesisVertex_;
    void CreateGenesis(const std::string& genesisHexStr);
};

class MainNetParams : public Params {
public:
    MainNetParams(bool withGenesis = true);
};

class TestNetParams : public Params {
public:
    TestNetParams(bool withGenesis = true);
};

class UnitTestParams : public Params {
public:
    UnitTestParams(bool withGenesis = true);
};

// instance of the parameters for usage throughout the project
const Params& GetParams();
void SelectParams(ParamsType type, bool withGenesis = true);

#endif // EPIC_PARAMS_H
