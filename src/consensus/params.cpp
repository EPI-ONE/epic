// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "params.h"
#include "block.h"
#include "mean.cuh"
#include "vertex.h"

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
// max amount of money allowed in one output
static constexpr uint64_t MAX_MONEY = 9999999999L;
// version of genesis block
static constexpr uint32_t GENESIS_BLOCK_VERSION = 1;
// an easy enough difficulty target
static constexpr uint32_t EASIEST_COMP_DIFF_TARGET = 0x2100ffffL;
// transaction sortition: coefficient for computing allowed distance
static constexpr size_t SORTITION_COEFFICIENT = 100;
// transaction sortition: number of block to go back
static constexpr size_t SORTITION_THRESHOLD = 10 * 1000;
// coefficient of taking additional reward for milestone
static constexpr uint32_t REWARD_COEFFICIENT = 50;
static constexpr size_t CACHE_STATES         = 100;
// capacity of transactions in a block
static constexpr size_t BLK_CAPACITY = 128;

Params::Params() {
    solverParams = std::make_unique<SolverParams>();
#ifndef __CUDA_ENABLED__
    solverParams->nthreads = nSipThreads;
    solverParams->ntrims   = EDGEBITS >= 30 ? 96 : 68;
#else
    FillDefaultGPUParams(*solverParams);
#endif
}

const Block& Params::GetGenesis() const {
    return *genesis_;
}

const Vertex& Params::GetGenesisVertex() const {
    return *genesisVertex_;
}

void Params::CreateGenesis(const std::string& genesisHexStr) {
    const std::vector<unsigned char> parsed = ParseHex(genesisHexStr);
    VStream vs(parsed);
    Block genesisBlock{vs};
    genesisBlock.FinalizeHash();
    genesisBlock.CalculateOptimalEncodingSize();

    genesis_                    = std::make_unique<Block>(genesisBlock);
    genesisVertex_              = std::make_shared<Vertex>(genesisBlock);
    genesisVertex_->validity[0] = Vertex::VALID;

    arith_uint256 msTarget    = initialMsTarget * 2 / arith_uint256{targetTimespan};
    arith_uint256 blockTarget = msTarget * arith_uint256{targetTPS} * arith_uint256{timeInterval};
    uint64_t hashRate         = (arith_uint256{maxTarget} / (msTarget + 1)).GetLow64() / timeInterval;
    auto chainwork            = maxTarget / (arith_uint256().SetCompact(genesisBlock.GetDifficultyTarget()) + 1);

    static auto genesisState = std::make_shared<Milestone>(
        0, chainwork, msTarget, blockTarget, hashRate, genesisBlock.GetTime(), std::vector<VertexWPtr>{genesisVertex_});

    genesisVertex_->LinkMilestone(genesisState);
}

unsigned char Params::GetKeyPrefix(KeyPrefixType type) const {
    return keyPrefixes[type];
}

MainNetParams::MainNetParams() {
    version              = GENESIS_BLOCK_VERSION;
    targetTimespan       = TARGET_TIMESPAN;
    timeInterval         = TIME_INTERVAL;
    interval             = INTERVAL;
    targetTPS            = TPS;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    msRewardCoefficient  = REWARD_COEFFICIENT;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(SORTITION_COEFFICIENT);
    sortitionThreshold   = SORTITION_THRESHOLD;
    cacheStatesSize      = CACHE_STATES;
    deleteForkThreshold  = 5;
    blockCapacity        = BLK_CAPACITY;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "0100e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e464"
        "9b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b8553e7b565dffff001d1ad3a83a"
        "0101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffffffffffff00484704ffff001d0104454974"
        "206973206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20776f"
        "726b696e6721014200142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);
}

TestNetParams::TestNetParams() {
    version              = 10;
    targetTimespan       = 100;
    timeInterval         = TIME_INTERVAL;
    interval             = targetTimespan / (double) timeInterval;
    targetTPS            = 100;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 1;
    msRewardCoefficient  = REWARD_COEFFICIENT;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(SORTITION_COEFFICIENT);
    sortitionThreshold   = SORTITION_THRESHOLD;
    cacheStatesSize      = CACHE_STATES;
    deleteForkThreshold  = 5;
    blockCapacity        = BLK_CAPACITY;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "0a00e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e464"
        "9b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855388ff95cffff001e0f253f02"
        "0101e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffffffffffff00484704ffff001d0104454974"
        "206973206e6f772074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20776f"
        "726b696e6721014200142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);
}

UnitTestParams::UnitTestParams() {
    version              = 100;
    targetTimespan       = 99;
    timeInterval         = 3; // cannot be less than 3
    interval             = targetTimespan / (double) timeInterval;
    targetTPS            = 100;
    punctualityThred     = PUNTUALITY_THRESHOLD;
    maxTarget            = arith_uint256().SetCompact(EASIEST_COMP_DIFF_TARGET);
    maxMoney             = MAX_MONEY;
    reward               = 10;
    msRewardCoefficient  = 1;
    initialMsTarget      = arith_uint256(INITIAL_MS_TARGET);
    sortitionCoefficient = arith_uint256(1);
    sortitionThreshold   = 2;
    cacheStatesSize      = 25;
    deleteForkThreshold  = 10;
    blockCapacity        = 10;

    keyPrefixes = {
        0,  // keyPrefixes[PUBKEY_ADDRESS]
        128 // keyPrefixes[SECRET_KEY]
    };

    const std::string genesisHexStr{
        "6400e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e464"
        "9b934ca495991b7852b855e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b8555b9fa07329a2149b758dbec2"
        "530cd81cbe05b33cdb32b6b03470fb6601ef3255388ff95cffff002027040000470c4209aee0d80e089deb111bb6a1130101e3b0c44298"
        "fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855ffffffffffffffff00484704ffff001d0104454974206973206e6f77"
        "2074656e20706173742074656e20696e20746865206576656e696e6720616e6420776520617265207374696c6c20776f726b696e672101"
        "4200142ac277ce311a053c91e47fd2c4759b263e1b31b4"};

    CreateGenesis(genesisHexStr);

    genesisVertex_->snapshot->hashRate    = 1;
    genesisVertex_->snapshot->blockTarget = maxTarget;
    genesisVertex_->snapshot->milestoneTarget.SetCompact(0x20c0ffffL);
}

static std::unique_ptr<const Params> pparams;

const Params& GetParams() {
    assert(pparams);
    return *pparams;
}

void SelectParams(ParamsType type) {
    if (type == ParamsType::MAINNET) {
        pparams = std::make_unique<MainNetParams>();
    } else if (type == ParamsType::TESTNET) {
        pparams = std::make_unique<TestNetParams>();
    } else if (type == ParamsType::UNITTEST) {
        pparams = std::make_unique<UnitTestParams>();
    } else {
        throw std::invalid_argument("Invalid Param Type!");
    }

    GENESIS        = pparams->GetGenesis();
    GENESIS_VERTEX = pparams->GetGenesisVertex();
}
