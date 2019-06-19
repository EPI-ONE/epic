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
    auto hashMsg = Hash<1>(msg.cbegin(), msg.cend());
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
    return tx;
}

Block TestFactory::CreateBlock(int numTxInput, int numTxOutput, bool finalize) {

    Block b = Block(GetParams().version, CreateRandomHash(), CreateRandomHash(), CreateRandomHash(), time(nullptr), 0x1f00ffffL, 0);

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

    b.SetTime(timeGenerator.NextTime());
    b.CalculateOptimalEncodingSize();

    if (finalize) {
        b.FinalizeHash();
        return b;
    } else {
        TestBlock tb(std::move(b), this);
        return static_cast<Block&>(tb);
    }
}

ConstBlockPtr TestFactory::CreateBlockPtr(int numTxInput, int numTxOutput , bool finalize) {
    return std::make_shared<const Block>(CreateBlock(numTxInput, numTxOutput, finalize));
}

NodeRecord TestFactory::CreateNodeRecord(ConstBlockPtr b) {
    NodeRecord rec(b);
    // Set extra info
    rec.minerChainHeight = GetRand();
    rec.cumulativeReward = Coin(GetRand());

    if (GetRand() % 2) {
        // Link a ms instance
        auto cs = std::make_shared<ChainState>(GetRand(), arith_uint256(GetRand()), NextTime(),
            arith_uint256(GetRand()), arith_uint256(GetRand()), GetRand(), std::vector<uint256>{});
        rec.LinkChainState(cs);

        if (GetRand() % 2) {
            // Make it a fake milestone
            rec.InvalidateMilestone();
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

RecordPtr TestFactory::CreateConsecutiveRecordPtr() {
    Block b{};
    b.SetDifficultyTarget(GetParams().maxTarget.GetCompact());
    b.SetTime(timeGenerator.NextTime());
    b.Solve();

    auto pb = std::make_shared<const Block>(b);
    return std::make_shared<NodeRecord>(pb);
}

ChainStatePtr TestFactory::CreateChainStatePtr(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes) {
    return make_shared_ChainState(previous, record, std::move(hashes));
}

ChainStatePtr TestFactory::CreateChainStatePtr(ChainStatePtr previous, RecordPtr& pRec) {
    return make_shared_ChainState(previous, *pRec, std::vector<uint256>{pRec->cblock->GetHash()});
}
