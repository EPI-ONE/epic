#ifndef __TEST_TEST_FACTORY_H__
#define __TEST_TEST_FACTORY_H__

#include "block.h"
#include "consensus.h"
#include "key.h"
#include "pubkey.h"
#include "tasm.h"

#include <random>

class NumberGenerator {
public:
    NumberGenerator(): generator(time(nullptr)), distribution(0, 65536) {}
    NumberGenerator(uint32_t seed, uint32_t rangeStart, uint32_t rangeEnd) : generator(seed), distribution(rangeStart, rangeEnd) {}

    uint32_t GetRand() {
        return distribution(generator);
    }

private:
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> distribution;
};

class TimeGenerator {
public:
    TimeGenerator() : simulatedTime(GENESIS.GetTime()), distribution(1, 10) {}
    TimeGenerator(uint32_t timeStart, uint32_t rangeStart, uint32_t rangeEnd, uint32_t seed)
        : simulatedTime(timeStart), generator(seed), distribution(rangeStart, rangeEnd) {}

    uint32_t NextTime() {
        simulatedTime += distribution(generator);
        return simulatedTime;
    }

private:
    uint32_t simulatedTime; // arbitrarily take a starting point
    std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> distribution;
};

class TestFactory {
public:
    TestFactory() : timeGenerator(), numGenerator() {}

    std::string GetRandomString(size_t len);
    uint256 CreateRandomHash();

    std::pair<CKey, CPubKey> CreateKeyPair(bool compressed = true);
    std::pair<uint256, std::vector<unsigned char>> CreateSig(const CKey&);
    Transaction CreateReg();
    Transaction CreateTx(int numTxInput, int numTxOutput);

    Block CreateBlock(int numTxInput = 0, int numTxOutput = 0, bool finalize = false);
    ConstBlockPtr CreateBlockPtr(int numTxInput = 0, int numTxOutput = 0, bool finalize = false);
    NodeRecord CreateNodeRecord(ConstBlockPtr b);
    RecordPtr CreateRecordPtr(int numTxInput = 0, int numTxOutput = 0, bool finalize = false);
    RecordPtr CreateConsecutiveRecordPtr();
    ChainStatePtr CreateChainStatePtr(ChainStatePtr previous, RecordPtr& pRec);
    ChainStatePtr CreateChainStatePtr(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes);

    uint32_t GetRand() {
        return numGenerator.GetRand();
    }

    uint32_t NextTime() {
        return timeGenerator.NextTime();
    }

private:
    using Listing = Tasm::Listing;

    TimeGenerator timeGenerator;
    NumberGenerator numGenerator;

    class TestBlock : public Block {
    public:
        TestBlock(Block&& b, TestFactory* fac) : Block(b) {
            RandomizeHash(fac);
        }

        void RandomizeHash(TestFactory* fac) {
            hash_ = fac->CreateRandomHash();
        }
    };
};

#endif // __TEST_TEST_FACTORY_H__
