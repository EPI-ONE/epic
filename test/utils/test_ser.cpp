#include <gtest/gtest.h>
#include "transaction.h"
#include "block.h"
#include <sstream>

class TestSer: public testing::Test {

    void SetUp() {
    }
    void TearDown() {
    }
};

TEST_F(TestSer, SerializeEqDeserializeTxOutPoint) {
    uint256 zeros = uint256S(new char[256]());
    TxOutPoint outpoint = TxOutPoint(zeros, 1);
    std::stringstream sinput;
    outpoint.Serialize(sinput);

    TxOutPoint outpointFromUnserialization;
    outpointFromUnserialization.Unserialize(sinput);

    std::stringstream soutput;
    outpointFromUnserialization.Serialize(soutput);

    EXPECT_EQ(sinput.str(), soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTxInput) {
    uint256 zeros = uint256S(new char[256]());
    TxOutPoint outpoint = TxOutPoint(zeros, 1);
    TxInput input = TxInput(outpoint, Script());
    std::stringstream sinput;
    input.Serialize(sinput);

    TxInput inputFromUnserialization;
    inputFromUnserialization.Unserialize(sinput);

    std::stringstream soutput;
    inputFromUnserialization.Serialize(soutput);

    EXPECT_EQ(sinput.str(), soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTxOutput) {
    TxOutput output = TxOutput(100, Script());
    std::stringstream sinput;
    output.Serialize(sinput);

    TxOutput outputFromUnserialization;
    outputFromUnserialization.Unserialize(sinput);

    std::stringstream soutput;
    outputFromUnserialization.Serialize(soutput);

    EXPECT_EQ(sinput.str(), soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeTransaction) {
    uint256 zeros = uint256S(new char[256]());
    TxOutPoint outpoint = TxOutPoint(zeros, 1);
    TxInput input = TxInput(outpoint, Script());
    TxOutput output = TxOutput(100, Script());
    Transaction tx = Transaction(1);
    tx.AddInput(input);
    tx.AddOutput(output);
    std::stringstream sinput;
    tx.Serialize(sinput);

    Transaction txFromUnserialization;
    txFromUnserialization.Unserialize(sinput);

    std::stringstream soutput;
    txFromUnserialization.Serialize(soutput);

    EXPECT_EQ(sinput.str(), soutput.str());
}

TEST_F(TestSer, SerializeEqDeserializeBlock) {
    uint256 zeros = uint256S(new char[256]());
    Block block = Block(1, zeros, zeros, zeros, zeros, 1, 1, 1);
    std::stringstream sinput;
    block.Serialize(sinput);

    Block blockFromUnserialization;
    blockFromUnserialization.Unserialize(sinput);

    std::stringstream soutput;
    blockFromUnserialization.Serialize(soutput);

    EXPECT_EQ(sinput.str(), soutput.str());
}
