#include "params.h"
#include "utilstrencodings.h"

void TestNetParams::CreateGenesis() {
    genesisBlock   = Block(GENESIS_BLOCK_VERSION);
    Transaction tx = Transaction();
    // Construct a script containing the difficulty bits and the following
    // message:
    std::string hexStr("04ffff001d0104454974206973206e6f772074656e2070617374207"
                       "4656e20696e20746865206576656e696e6720616e64207765206172"
                       "65207374696c6c20776f726b696e6721");
    // Convert the string to bytes
    auto vs = VStream(ParseHex(hexStr));
    tx.AddInput(TxInput(Script(vs)));

    // TODO: add output

    genesisBlock.AddTransaction(tx);
    genesisBlock.SetMinerChainHeight(0);
    genesisBlock.ResetReward();
}
