#include "test_factory.h"
#include <cstring>
#include <memory>

std::string TestFactory::GetRandomString(size_t len) {
    static const char alph[] = "0123456789"
                               "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz";
    char s[len + 1];
    for (size_t i = 0; i < len; ++i) {
        s[i] = alph[GetRand() % (sizeof(alph) - 1)];
    }
    s[len] = 0;
    return std::string(s);
}

uint256 TestFactory::CreateRandomHash() {
    /* draw 64 bit */
    unsigned long long x;
    while (!_rdrand64_step(&x))
        ;

    std::vector<unsigned char> vch(Hash::SIZE);
    for (std::size_t i = 0; i < 8; i++) {
        vch[i] = ((uint8_t*) &x)[i];
    }

    return uint256(vch);
}

std::pair<CKey, CPubKey> TestFactory::CreateKeyPair(bool compressed) {
    CKey seckey = CKey();
    seckey.MakeNewKey(compressed);
    CPubKey pubkey = seckey.GetPubKey();
    return std::make_pair(std::move(seckey), std::move(pubkey));
}

std::pair<uint256, std::vector<unsigned char>> TestFactory::CreateSig(const CKey& privateKey) {
    auto msg     = GetRandomString(10);
    auto hashMsg = HashSHA2<1>(msg.data(), msg.size());
    std::vector<unsigned char> sig;
    privateKey.Sign(hashMsg, sig);
    return std::make_pair(hashMsg, sig);
}

Transaction TestFactory::CreateTx(int numTxInput, int numTxOutput) {
    Transaction tx;
    uint32_t maxPos = GetRand() % 128 + 1;
    for (int i = 0; i < numTxInput; ++i) {
        tx.AddInput(TxInput(CreateRandomHash(), i % maxPos, i % maxPos, Listing(std::vector<unsigned char>(i))));
    }

    for (int i = 0; i < numTxOutput; ++i) {
        tx.AddOutput(TxOutput(i, Listing(std::vector<unsigned char>(i))));
    }
    tx.FinalizeHash();
    return tx;
}

Block TestFactory::CreateBlock(int numTxInput, int numTxOutput, bool finalize, int maxTxns) {
    Block b = Block(GetParams().version, CreateRandomHash(), CreateRandomHash(), CreateRandomHash(), uint256(),
                    timeGenerator.NextTime(), GENESIS_RECORD.snapshot->blockTarget.GetCompact(), 0);

    if (numTxInput && numTxOutput) {
        for (int i = 0; i < maxTxns; ++i) {
            b.AddTransaction(CreateTx(numTxInput, numTxOutput));
        }
    }

    b.CalculateOptimalEncodingSize();

    if (finalize) {
        b.FinalizeHash();
        return b;
    } else {
        TestBlock tb(std::move(b), this);
        return static_cast<Block&>(tb);
    }
}

ConstBlockPtr TestFactory::CreateBlockPtr(int numTxInput, int numTxOutput, bool finalize, int maxTxns) {
    return std::make_shared<const Block>(CreateBlock(numTxInput, numTxOutput, finalize, maxTxns));
}

NodeRecord TestFactory::CreateNodeRecord(ConstBlockPtr b) {
    NodeRecord rec(b);
    // Set extra info
    rec.minerChainHeight = GetRand();
    rec.cumulativeReward = Coin(GetRand());

    if (GetRand() % 2) {
        rec.validity.push_back(NodeRecord::VALID);
    } else {
        rec.validity.push_back(NodeRecord::INVALID);
    }

    return rec;
}

RecordPtr TestFactory::CreateRecordPtr(int numTxInput, int numTxOutput, bool finalize, int maxTxns) {
    return std::make_shared<NodeRecord>(CreateNodeRecord(CreateBlockPtr(numTxInput, numTxOutput, finalize, maxTxns)));
}

RecordPtr TestFactory::CreateConsecutiveRecordPtr(uint32_t timeToset) {
    Block b = CreateBlock(0, 0, false);
    b.SetTime(timeToset);
    do {
        b.SetNonce(b.GetNonce() + 1);
        b.Solve();
    } while (UintToArith256(b.GetHash()) > GENESIS_RECORD.snapshot->milestoneTarget);

    return std::make_shared<NodeRecord>(std::move(b));
}

ChainStatePtr TestFactory::CreateChainStatePtr(ChainStatePtr previous,
                                               NodeRecord& record,
                                               std::vector<RecordWPtr>&& lvs) {
    return CreateNextChainState(previous, record, std::move(lvs));
}

ChainStatePtr TestFactory::CreateChainStatePtr(ChainStatePtr previous, RecordPtr& pRec) {
    return CreateNextChainState(previous, *pRec, std::vector<RecordWPtr>{pRec});
}

TestChain TestFactory::CreateChain(const RecordPtr& startMs, size_t height, bool tx) {
    assert(height);
    RecordPtr lastMs    = startMs;
    RecordPtr prevBlock = startMs;

    TestChain testChain{{}};

    size_t count = 0;
    TimeGenerator timeg{startMs->cblock->GetTime(), 1, GetRand() % 10 + 2, GetRand()};
    while (count < height) {
        Block b{GetParams().version};
        b.SetMilestoneHash(lastMs->cblock->GetHash());
        b.SetPrevHash(prevBlock->cblock->GetHash());
        if (testChain.size() == 1) {
            b.SetTipHash(GENESIS.GetHash());
        } else {
            b.SetTipHash(testChain[GetRand() % (testChain.size() - 1)][0]->cblock->GetHash());
        }
        b.SetTime(timeg.NextTime());
        b.SetDifficultyTarget(lastMs->snapshot->blockTarget.GetCompact());

        // Special transaction on the first registration block
        if (b.GetPrevHash() == GENESIS.GetHash()) {
            Transaction tx = Transaction{CreateKeyPair().second.GetID()};
            b.AddTransaction(tx);
        } else if (tx) {
            for (int i = 0; i < GetRand() % 10; ++i) {
                b.AddTransaction(CreateTx(GetRand() % 10 + 1, GetRand() % 10 + 1));
            }
        }
        b.CalculateOptimalEncodingSize();
        b.Solve();

        ConstBlockPtr blkptr = std::make_shared<const Block>(std::move(b));
        RecordPtr node       = std::make_shared<NodeRecord>(blkptr);

        // Set proper info in node
        node->height           = lastMs->height + 1;
        node->minerChainHeight = prevBlock->minerChainHeight + 1;
        node->validity.resize(blkptr->GetTransactionSize());
        memset(node->validity.data(), NodeRecord::Validity::VALID, blkptr->GetTransactionSize());

        prevBlock = node;
        testChain.back().emplace_back(node);

        if (CheckMsPOW(blkptr, lastMs->snapshot)) {
            // Prepare the lvs of the milestone
            std::vector<RecordWPtr> lvs;
            const auto& lvs_recs = testChain.back();
            lvs.reserve(lvs_recs.size());
            std::transform(lvs_recs.begin(), lvs_recs.end(), std::back_inserter(lvs),
                           [](RecordPtr p) -> RecordWPtr { return RecordWPtr(p); });

            CreateNextChainState(lastMs->snapshot, *node, std::move(lvs));
            lastMs = std::move(node);

            if (count++ < height - 1) {
                testChain.emplace_back();
            }
        }
    }
    // PrintChain(testChain);
    return testChain;
}

TestChain TestFactory::CreateChain(const NodeRecord& startMs, size_t height, bool tx) {
    return CreateChain(std::make_shared<NodeRecord>(startMs), height, tx);
}

std::tuple<TestRawChain, std::vector<RecordPtr>> TestFactory::CreateRawChain(const RecordPtr& startMs,
                                                                             size_t height,
                                                                             bool tx) {
    auto chain = CreateChain(startMs, height, tx);

    TestRawChain rawChain;
    rawChain.reserve(chain.size());
    std::vector<RecordPtr> milestons;
    milestons.reserve(chain.size());

    std::transform(chain.begin(), chain.end(), std::back_inserter(rawChain), [&milestons](LevelSetRecs lvs) {
        // extract milestone
        milestons.emplace_back(lvs.back());

        // convert each block from RecordPtr to ConstBlockPtr
        LevelSetBlks rawLvs;
        rawLvs.reserve(lvs.size());
        std::transform(lvs.begin(), lvs.end(), std::back_inserter(rawLvs), [](RecordPtr rec) { return rec->cblock; });
        return rawLvs;
    });

    return {rawChain, milestons};
}

std::tuple<TestRawChain, std::vector<RecordPtr>> TestFactory::CreateRawChain(const NodeRecord& startMs,
                                                                             size_t height,
                                                                             bool tx) {
    return CreateRawChain(std::make_shared<NodeRecord>(startMs), height, tx);
}
