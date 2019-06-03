#include <gtest/gtest.h>
#include <optional>

#include "test_factory.h"
#include "consensus.h"
#include "key.h"

typedef Tasm::Listing Listing;

class TestSer : public testing::Test {
protected:
    TestFactory fac;
    Listing randomBytes;
    uint256 rand1;
    uint256 rand2;
    uint256 zeros;

    void SetUp() {
        rand1 = fac.CreateRandomHash();
        rand2 = fac.CreateRandomHash();

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
    auto pubkey = fac.CreateKeyPair().second;

    // serialize on pubkey
    VStream vstream;
    vstream << pubkey;
    CPubKey outPubkey;
    vstream >> outPubkey;

    ASSERT_EQ(pubkey, outPubkey);
    
    // serialize on address
    std::string strAddr = EncodeAddress(pubkey.GetID());
    vstream << strAddr;
    std::string deserAddr;
    vstream >> deserAddr;
    ASSERT_EQ(strAddr, deserAddr);
    auto decodeDeserAddr = DecodeAddress(deserAddr);
    ASSERT_EQ(pubkey.GetID(), *decodeDeserAddr);

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

    BlockNet block1;
    sinput >> block1;

    VStream soutput;
    soutput << block1;

    EXPECT_EQ(s, soutput.str());

    // Check parent pointers
    const Transaction* ptrTx = &(*block1.GetTransaction());
    EXPECT_EQ(&block1, ptrTx->GetParentBlock());
    for (const TxInput& input : ptrTx->GetInputs()) {
        EXPECT_EQ(ptrTx, input.GetParentTx());
    }
    for (const TxOutput& output : ptrTx->GetOutputs()) {
        EXPECT_EQ(ptrTx, output.GetParentTx());
    }

    BlockNet block2(soutput);

    // check parent pointers
    ptrTx = &*block2.GetTransaction();
    EXPECT_EQ(&block2, ptrTx->GetParentBlock());
    for (const TxInput& input : ptrTx->GetInputs()) {
        EXPECT_EQ(ptrTx, input.GetParentTx());
    }
    for (const TxOutput& output : ptrTx->GetOutputs()) {
        EXPECT_EQ(ptrTx, output.GetParentTx());
    }

    // Check encoding size
    EXPECT_EQ(VStream(block2).size(), block2.GetOptimalEncodingSize());
}

TEST_F(TestSer, SerializeEqDeserializeNodeRecord) {
    BlockNet blk = BlockNet(1, rand1, zeros, rand2, time(nullptr), 1, 1);

    // Add a tx into the block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();
    tx.AddInput(TxInput(outpoint, randomBytes));
    tx.AddOutput(TxOutput(100, randomBytes));
    blk.AddTransaction(tx);

    // Construct NodeRecord
    NodeRecord block(blk);
    block.minerChainHeight = 100;
    block.cumulativeReward = 10;

    // Link the chain state
    auto pstate = std::make_shared<ChainState>(time(nullptr), 100000, 100, arith_uint256(0X3E8).GetCompact(),
        arith_uint256(0).GetCompact(), arith_uint256(0X3E8));
    block.LinkChainState(pstate);

    // Make it a fake milestone
    block.InvalidateMilestone();

    VStream sinput;
    block.Serialize(sinput);
    std::string s = sinput.str();

    NodeRecord block1;
    block1.Deserialize(sinput);

    VStream soutput;
    block1.Serialize(soutput);

    EXPECT_EQ(s, soutput.str());
}
