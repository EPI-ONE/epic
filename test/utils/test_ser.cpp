#include <gtest/gtest.h>
#include <optional>

#include "consensus.h"
#include "key.h"

typedef Tasm::Listing Listing;

class TestSer : public testing::Test {
protected:
    Listing randomBytes;
    uint256 rand1;
    uint256 rand2;
    uint256 zeros;

    void SetUp() {
        rand1.randomize();
        rand2.randomize();

        VStream s(rand1);
        randomBytes = Listing(s);
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

TEST_F(TestSer, SerializeEqDeserializeVStream) {
    std::vector<char> deterministicVector = {'x', 'y', 'z'};
    VStream v1(deterministicVector);
    VStream sinput;
    sinput << v1;
    std::string s = sinput.str();

    VStream v2;
    sinput >> v2;

    VStream soutput;
    soutput << v2;

    EXPECT_EQ(s, soutput.str());
}


TEST_F(TestSer, SerializeEqDeserializeListing) {
    std::vector<char> deterministicVector = {'x', 'y', 'z'};
    VStream v(deterministicVector);
    std::vector<uint8_t> p = {1, 1};
    Listing l1(p, v);
    VStream sinput;
    sinput << l1;
    std::string s = sinput.str();

    Listing l2;
    sinput >> l2;

    VStream soutput;
    soutput << l2;

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

    tx.AddInput(TxInput(outpoint, Listing(randomBytes)));
    tx.AddOutput(TxOutput(100, Listing(randomBytes)));

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
    BlockNet block = Block(1, rand1, zeros, rand2, time(nullptr), 1, 1);

    // Add tx to block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();

    tx.AddInput(TxInput(outpoint, Listing(randomBytes)));
    tx.AddOutput(TxOutput(100, Listing(randomBytes)));
    block.AddTransaction(tx);

    VStream sinput;
    sinput << block;
    std::string s = sinput.str();

    BlockNet blockFromDeserialization;
    sinput >> blockFromDeserialization;

    VStream soutput;
    soutput << blockFromDeserialization;

    EXPECT_EQ(s, soutput.str());

    // Check parent pointers
    Transaction* ptrTx = &*blockFromDeserialization.GetTransaction();
    EXPECT_EQ(&blockFromDeserialization, ptrTx->GetParentBlock());
    for (const TxInput& input : ptrTx->GetInputs()) {
        EXPECT_EQ(ptrTx, input.GetParentTx());
    }

    for (const TxOutput& output : ptrTx->GetOutputs()) {
        EXPECT_EQ(ptrTx, output.GetParentTx());
    }
}

TEST_F(TestSer, SerializeEqDeserializeBlockStatus) {
    BlockNet blk = BlockNet(1, rand1, zeros, rand2, time(nullptr), 1, 1);

    // Add a tx into the block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();
    tx.AddInput(TxInput(outpoint, randomBytes));
    tx.AddOutput(TxOutput(100, randomBytes));
    blk.AddTransaction(tx);

    // Construct BlockStatus
    BlockStatus block(blk);
    block.minerChainHeight = 100;
    block.cumulativeReward = 0;

    // Link the chain state
    ChainState state(time(nullptr), 100000, 100, arith_uint256(0X3E8).GetCompact(), arith_uint256(0).GetCompact(),
        arith_uint256(0X3E8));
    block.LinkChainState(state);

    // Make it a fake milestone
    block.InvalidateMilestone();

    VStream sinput;
    block.Serialize(sinput);
    std::string s = sinput.str();

    BlockStatus blockFromUnserialization;
    blockFromUnserialization.Deserialize(sinput);

    VStream soutput;
    blockFromUnserialization.Serialize(soutput);

    EXPECT_EQ(s, soutput.str());
}
