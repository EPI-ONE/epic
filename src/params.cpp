#include "params.h"

const Block& Params::GetGenesisBlock() const {
    return genesisBlock;
}

const uint256& Params::GetGenesisBlockHash() const {
    return genesisBlockHash;
}

const Params& TestNetParams::GetParams() {
    static const TestNetParams instance;
    return instance;
}

TestNetParams::TestNetParams() {
    targetTimespan          = TARGET_TIMESPAN;
    timeInterval            = TIME_INTERVAL;
    interval                = INTERVAL;
    targetTPS               = 100;
    punctualityThred        = PUNTUALITY_THRESHOLD;
    arith_uint256 maxTarget = arith_uint256().SetCompact(0x2100ffffL);
    maxMoney                = MAX_MONEY;
    reward                  = 1;

    CreateGenesis();
    genesisBlockHash = genesisBlock.GetHash();
}

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
    tx.AddInput(TxInput(Script(vs))).FinalizeHash();

    // TODO: add output
    genesisBlock.AddTransaction(tx);
    genesisBlock.SetMinerChainHeight(0);
    genesisBlock.ResetReward();
}
