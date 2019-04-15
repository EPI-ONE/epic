#include <gtest/gtest.h>
#include "block.h"
#include "stream.h"
#include "transaction.h"

class TestSer : public testing::Test {
   protected:
    uint256 rand1;
    void SetUp() {
        rand1 = uint256S(
            "9efd5d25c8cc0e2eda7dfc94c258122685ad24e6b559ed95fe3d54d363e79798");
    }
    void TearDown() {}
};

TEST_F(TestSer, SerializeEqDeserializeTxOutPoint) {
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    VStream sinput;
    sinput << outpoint;
    std::string s = sinput.str();

    TxOutPoint outpointFromUnserialization;
    sinput >> outpointFromUnserialization;

    VStream soutput;
    soutput << outpointFromUnserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTxInput) {
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    TxInput input = TxInput(outpoint, Script());
    VStream sinput;
    sinput << input;
    std::string s = sinput.str();

    TxInput inputFromUnserialization;
    sinput >> inputFromUnserialization;

    VStream soutput;
    soutput << inputFromUnserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTxOutput) {
    TxOutput output = TxOutput(100, Script());
    VStream sinput;
    sinput << output;
    std::string s = sinput.str();

    TxOutput outputFromUnserialization;
    sinput >> outputFromUnserialization;

    VStream soutput;
    soutput << outputFromUnserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTransaction) {
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    TxInput input = TxInput(outpoint, Script());
    TxOutput output = TxOutput(100, Script());
    Transaction tx = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);

    VStream sinput;
    sinput << tx;
    std::string s = sinput.str();

    Transaction txFromUnserialization;
    sinput >> txFromUnserialization;

    VStream soutput;
    soutput << txFromUnserialization;

    EXPECT_EQ(s, soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeBlock) {
    auto emptyChar = new char[256]();
    uint256 zeros = uint256S(emptyChar);
    delete[] emptyChar;

    uint256 rand2 = uint256S(
        "e6558bb8ac0fb9823e96b529b8eca3531b991ab7451045ffaa4944a1eb0f0088");
    uint256 rand3 = uint256S(
        "84a01628cd9f0f715a9a611d99ea1f20bd7d823d04f41194aa49e03957d7e22e");

    Block block = Block(1, rand1, zeros, rand2, rand3, 1, 1, 1);

    // Add tx to block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    TxInput input = TxInput(outpoint, Script());
    TxOutput output = TxOutput(100, Script());
    Transaction tx = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);
    block.AddTransaction(tx);

    VStream sinput;
    sinput << block;
    std::string s = sinput.str();

    Block blockFromUnserialization;
    sinput >> blockFromUnserialization;

    VStream soutput;
    soutput << blockFromUnserialization;

    EXPECT_EQ(s, soutput.str());
}
