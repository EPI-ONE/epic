#include "block-factory.h"

typedef Tasm::Listing Listing;

BlockNet FakeBlock(int numTxInput, int numTxOutput, bool solve) {
    uint256 r1;
    r1.randomize();
    uint256 r2;
    r2.randomize();
    uint256 r3;
    r3.randomize();

    BlockNet b(1, r1, r2, r3, time(nullptr), 0x1f00ffffL, 0);

    if (numTxInput || numTxOutput) {
        b.AddTransaction(FakeTx(numTxInput, numTxOutput));
    }

    b.FinalizeHash();
    b.CalculateOptimalEncodingSize();

    if (solve) {
        b.Solve();
    }

    return b;
}

ConstBlockPtr FakeBlockPtr(int numTxInput, int numTxOutput, bool solve) {
    return std::make_shared<BlockNet>(FakeBlock(numTxInput, numTxOutput, solve));
}

Transaction FakeTx(int numTxInput, int numTxOutput) {
    Transaction tx;

    int maxPos = rand() % 128 + 1;
    for (int i = 0; i < numTxInput; ++i) {
        uint256 inputH;
        inputH.randomize();
        tx.AddInput(TxInput(inputH, i % maxPos, Listing(std::vector<unsigned char>(i))));
    }

    for (int i = 0; i < numTxOutput; ++i) {
        tx.AddOutput(TxOutput(i, Listing(std::vector<unsigned char>(i))));
    }

    return tx;
}

NodeRecord FakeNodeRecord(const BlockNet& b) {
    NodeRecord rec(b);
    // Set extra info
    rec.minerChainHeight = rand();
    rec.cumulativeReward = Coin(rand());

    if (rand() % 2) {
        // Link a ms instance
        ChainState cs(time(nullptr), rand(), rand(), arith_uint256(rand()).GetCompact(),
            arith_uint256(rand()).GetCompact(), arith_uint256(rand()));
        rec.LinkChainState(cs);

        if (rand() % 2) {
            // Make it a fake milestone
            rec.InvalidateMilestone();
        }
    }

    if (rand() % 2) {
        rec.validity = NodeRecord::VALID;
    } else {
        rec.validity = NodeRecord::INVALID;
    }

    return rec;
}

NodeRecord FakeNodeRecord(int numTxInput, int numTxOutput, bool solve) {
    return FakeNodeRecord(FakeBlock(numTxInput, numTxOutput, solve));
}

RecordPtr FakeRecordPtr(int numTxInput, int numTxOutput, bool solve) {
    return std::make_shared<NodeRecord>(FakeNodeRecord(numTxInput, numTxOutput, solve));
}
