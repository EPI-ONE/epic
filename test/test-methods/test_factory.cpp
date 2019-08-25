#include "test_factory.h"

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
        tx.AddInput(TxInput(CreateRandomHash(), i % maxPos, Listing(std::vector<unsigned char>(i))));
    }

    for (int i = 0; i < numTxOutput; ++i) {
        tx.AddOutput(TxOutput(i, Listing(std::vector<unsigned char>(i))));
    }
    tx.FinalizeHash();
    return tx;
}

Block TestFactory::CreateBlock(int numTxInput, int numTxOutput, bool finalize) {
    Block b = Block(GetParams().version, CreateRandomHash(), CreateRandomHash(), CreateRandomHash(),
                    timeGenerator.NextTime(), GetParams().maxTarget.GetCompact(), 0);

    if (numTxInput || numTxOutput) {
        Transaction tx;
        int maxPos = (numGenerator.GetRand() % 128) + 1;
        for (int i = 0; i < numTxInput; ++i) {
            tx.AddInput(TxInput(CreateRandomHash(), i % maxPos, Listing(std::vector<unsigned char>(i))));
        }

        for (int i = 0; i < numTxOutput; ++i) {
            tx.AddOutput(TxOutput(i, Listing(std::vector<unsigned char>(i))));
        }

        b.AddTransaction(tx);
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

ConstBlockPtr TestFactory::CreateBlockPtr(int numTxInput, int numTxOutput, bool finalize) {
    return std::make_shared<const Block>(CreateBlock(numTxInput, numTxOutput, finalize));
}

NodeRecord TestFactory::CreateNodeRecord(ConstBlockPtr b) {
    NodeRecord rec(b);
    // Set extra info
    rec.minerChainHeight = GetRand();
    rec.cumulativeReward = Coin(GetRand());

    if (GetRand() % 2) {
        // Link a ms instance
        auto cs =
            std::make_shared<ChainState>(GetRand(), arith_uint256(GetRand()), NextTime(), arith_uint256(GetRand()),
                                         arith_uint256(GetRand()), GetRand(), std::vector<RecordWPtr>{});
        rec.LinkChainState(cs);

        if (GetRand() % 2) {
            // Make it a fake milestone
            rec.isMilestone = false;
        }
    }

    if (GetRand() % 2) {
        rec.validity = NodeRecord::VALID;
    } else {
        rec.validity = NodeRecord::INVALID;
    }

    return rec;
}

RecordPtr TestFactory::CreateRecordPtr(int numTxInput, int numTxOutput, bool finalize) {
    return std::make_shared<NodeRecord>(CreateNodeRecord(CreateBlockPtr(numTxInput, numTxOutput, finalize)));
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

std::tuple<TestChain, std::vector<NodeRecord>> TestFactory::CreateChain(const NodeRecord& startMs,
                                                                        size_t height,
                                                                        bool tx) {
    NodeRecord lastMs       = startMs;
    ConstBlockPtr prevBlock = startMs.cblock;

    TestChain testChain{{}};
    std::vector<NodeRecord> vMs;
    vMs.reserve(height);

    size_t count = 1;
    TimeGenerator timeg{startMs.cblock->GetTime(), 1, GetRand() % 10 + 2, GetRand()};
    while (count < height) {
        Block b{GetParams().version};
        if (tx) {
            b.AddTransaction(CreateTx(GetRand() % 10 + 1, GetRand() % 10 + 1));
        }
        b.SetMilestoneHash(lastMs.cblock->GetHash());
        b.SetPrevHash(prevBlock->GetHash());
        if (testChain.size() == 1) {
            b.SetTipHash(GENESIS.GetHash());
        } else {
            b.SetTipHash(testChain[GetRand() % (testChain.size() - 1)][0]->GetHash());
        }
        b.SetTime(timeg.NextTime());
        b.SetDifficultyTarget(lastMs.snapshot->blockTarget.GetCompact());

        // Special transaction on the first registration block
        if (b.GetPrevHash() == GENESIS.GetHash()) {
            b.AddTransaction(Transaction{CreateKeyPair().second.GetID()});
        }
        b.CalculateOptimalEncodingSize();
        b.Solve();

        ConstBlockPtr blkptr   = std::make_shared<const Block>(std::move(b));
        bool make_new_levelset = false;

        if (CheckMsPOW(blkptr, lastMs.snapshot)) {
            NodeRecord node{blkptr};
            ChainStatePtr cs = CreateNextChainState(lastMs.snapshot, node, std::vector<RecordWPtr>{});
            vMs.emplace_back(std::move(node));
            lastMs = vMs.back();
            count++;
            if (count < height) {
                make_new_levelset = true;
            }
        }

        prevBlock = blkptr;
        testChain.back().emplace_back(std::move(blkptr));
        if (make_new_levelset) {
            testChain.emplace_back();
        }
    }
    return {testChain, vMs};
}
