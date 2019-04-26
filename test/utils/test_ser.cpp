#include <gtest/gtest.h>

#include "block.h"
#include "key.h"
#include "pubkey.h"
#include "stream.h"
#include "transaction.h"

class TestSer : public testing::Test {
protected:
    uint256 rand1;
    uint256 rand2;
    uint256 zeros;
    Script randomBytes;
    void SetUp() {
        rand1 = uint256S("9efd5d25c8cc0e2eda7dfc94c258122685ad24e6b559ed95fe3d54d363e79798");
    }

    void TearDown() {}
};

TEST_F(TestSer, SerializeEqDeserializePublicKey) {
    ECC_Start();
    CKey seckey = CKey();
    seckey.MakeNewKey(true);
    CPubKey pubkey = seckey.GetPubKey();

    VStream vstream;
    vstream << pubkey;
    CPubKey outPubkey;
    vstream >> outPubkey;

    ASSERT_EQ(pubkey, outPubkey);
    ECC_Stop();
}

TEST_F(TestSer, SerializeEqDeserializeTxOutPoint) {
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    VStream sinput;
    sinput << outpoint;
    std::string s = sinput.str();

    TxOutPoint outpointFromDeserialization;
    sinput >> outpointFromDeserialization;

    VStream soutput;
    soutput << outpointFromDeserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTxInput) {
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    TxInput input       = TxInput(outpoint, randomBytes);
    VStream sinput;
    sinput << input;
    std::string s = sinput.str();

    TxInput inputFromDeserialization;
    sinput >> inputFromDeserialization;

    VStream soutput;
    soutput << inputFromDeserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTxOutput) {
    TxOutput output = TxOutput(100, randomBytes);
    VStream sinput;
    sinput << output;
    std::string s = sinput.str();

    TxOutput outputFromDeserialization;
    sinput >> outputFromDeserialization;

    VStream soutput;
    soutput << outputFromDeserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTransaction) {
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    TxInput input       = TxInput(outpoint, Script());
    TxOutput output     = TxOutput(100, Script());
    Transaction tx      = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);

    VStream sinput;
    sinput << tx;
    std::string s = sinput.str();

    Transaction txFromDeserialization;
    sinput >> txFromDeserialization;

    VStream soutput;
    soutput << txFromDeserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeBlock) {
    auto emptyChar = new char[256]();
    uint256 zeros  = uint256S(emptyChar);
    delete[] emptyChar;

    uint256 rand2 = uint256S("e6558bb8ac0fb9823e96b529b8eca3531b991ab7451045ffaa4944a1eb0f0088");
    uint256 rand3 = uint256S("84a01628cd9f0f715a9a611d99ea1f20bd7d823d04f41194aa49e03957d7e22e");

    Block block = Block(1, rand1, zeros, rand2, rand3, 1, 1, 1);

    // Add tx to block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    TxInput input       = TxInput(outpoint, Script());
    TxOutput output     = TxOutput(100, Script());
    Transaction tx      = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);
    block.AddTransaction(tx);

    VStream sinput;
    sinput << block;
    std::string s = sinput.str();

    Block blockFromDeserialization;
    sinput >> blockFromDeserialization;

    VStream soutput;
    soutput << blockFromDeserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeBlockTODB) {
    Block block = Block(BlockHeader(1, rand1, zeros, rand2, time(nullptr), 1, 1));

    // Add a tx into the block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();
    tx.AddInput(TxInput(outpoint, randomBytes));
    tx.AddOutput(TxOutput(100, randomBytes));
    block.AddTransaction(tx);
    tx.Validate();

    // Set extra info
    block.SetMinerChainHeight(100);
    block.ResetReward();

    // Link a ms instance
    Milestone ms;
    ms.height           = 100;
    ms.chainwork        = arith_uint256(0X3E8);
    ms.lastUpdateTime_  = time(nullptr);
    ms.milestoneTarget_ = arith_uint256(0X3E8).GetCompact();
    ms.blockTarget_     = arith_uint256(0).GetCompact();
    ms.hashRate_        = 100000;
    block.SetMilestoneInstance(ms);

    // Make it a fake milestone
    block.InvalidateMilestone();

    VStream sinput;
    block.SerializeToDB(sinput);
    std::string s = sinput.str();

    Block blockFromUnserialization;
    blockFromUnserialization.DeserializeFromDB(sinput);

    VStream soutput;
    blockFromUnserialization.SerializeToDB(soutput);

    EXPECT_EQ(s, soutput.str());
}
