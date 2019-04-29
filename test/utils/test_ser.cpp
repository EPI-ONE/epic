#include <gtest/gtest.h>
#include <optional>

#include "block.h"
#include "key.h"
#include "pubkey.h"
#include "stream.h"
#include "transaction.h"

class TestSer : public testing::Test {
protected:
    Script randomBytes;
    uint256 rand1;
    uint256 rand2;
    uint256 zeros;

    void SetUp() {
        rand1.randomize();
        rand2.randomize();
    }
};

TEST_F(TestSer, SerializeOptional) {
    std::optional<uint32_t> o1 = 4, o2;
    VStream vstream;
    vstream << o1 << o2;
    std::optional<uint32_t> o3, o4 = 5;
    vstream >> o3 >> o4;

    ASSERT_EQ(o1, o3);
    ASSERT_EQ(o2, o4);
}

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
    Transaction tx      = Transaction();

    tx.AddInput(TxInput(outpoint, Script()));
    tx.AddOutput(TxOutput(100, Script()));

    VStream sinput;
    sinput << tx;
    std::string s = sinput.str();

    Transaction txFromDeserialization;
    sinput >> txFromDeserialization;

    VStream soutput;
    soutput << txFromDeserialization;

    EXPECT_EQ(s, soutput.str());

    Transaction txx(tx);
    std::optional<Transaction> ot(std::forward<Transaction>(txx)), ots;
    VStream vs;
    vs << ot;
    vs >> ots;

    ASSERT_TRUE(ot.has_value());
    ASSERT_TRUE(ots.has_value());
    EXPECT_EQ(ot, ots);
}

TEST_F(TestSer, SerializeEqDeserializeBlock) {
    Block block = Block(BlockHeader(1, rand1, zeros, rand2, time(nullptr), 1, 1));

    // Add tx to block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();

    tx.AddInput(TxInput(outpoint, Script()));
    tx.AddOutput(TxOutput(100, Script()));
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

TEST_F(TestSer, SerializeEqDeserializeBlockToDB) {
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
    Milestone ms(time(nullptr), 100000, 100, arith_uint256(0X3E8).GetCompact(), arith_uint256(0).GetCompact(),
        arith_uint256(0X3E8));
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
