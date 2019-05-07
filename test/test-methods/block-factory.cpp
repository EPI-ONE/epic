#include "block-factory.h"

typedef Tasm::Listing Listing;

Block FakeBlock(int numTxInput, int numTxOutput, bool solve) {
    uint256 r1;
    r1.randomize();
    uint256 r2;
    r2.randomize();
    uint256 r3;
    r3.randomize();

    Block b = Block(1, r1, r2, r3, time(nullptr), 0x1f00ffffL, 0);

    if (numTxInput || numTxOutput) {
        Transaction tx;
        int maxPos = rand() % 128;
        for (int i = 0; i < numTxInput; ++i) {
            uint256 inputH;
            inputH.randomize();
            tx.AddInput(TxInput(inputH, i % maxPos, Listing(std::vector<unsigned char>(i))));
        }

        for (int i = 0; i < numTxOutput; ++i) {
            tx.AddOutput(TxOutput(i, Listing(std::vector<unsigned char>(i))));
        }

        b.AddTransaction(tx);
    }

    if (solve) {
        b.Solve();
    }

    return b;
}

ConstBlockPtr FakeBlockPtr(int numTxInput, int numTxOutput, bool solve) {
    ConstBlockPtr b = std::make_shared<BlockNet>(FakeBlock(numTxInput, numTxOutput, solve));
    return b;
}
