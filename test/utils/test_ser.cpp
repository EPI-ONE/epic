#include <gtest/gtest.h>
#include <optional>

#include "consensus.h"
#include "key.h"
#include "test_env.h"

typedef Tasm::Listing Listing;

class TestSer : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();

protected:
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

    ASSERT_EQ(s, soutput.str());
    ASSERT_EQ(outpoint, outpointFromDeserialization);
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

    ASSERT_EQ(s, soutput.str());

    VStream soutput1;
    soutput1 << v2;
    ASSERT_EQ(soutput, soutput1);
    ASSERT_EQ(v1, v2);
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

    ASSERT_EQ(s, soutput.str());
    ASSERT_EQ(l1, l2);
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

    ASSERT_EQ(s, soutput.str());
    ASSERT_EQ(input, inputFromDeserialization);
}

TEST_F(TestSer, SerializeEqDeserializeTxOutput) {
    // coin test first
    Coin coin{100};
    VStream vscoin{coin};
    Coin coin1;
    std::string strcoin = vscoin.str();
    vscoin >> coin1;
    ASSERT_EQ(coin, coin1);
    VStream vscoin1{coin1};
    ASSERT_EQ(strcoin, vscoin1.str());

    // then test TxOutput
    TxOutput output = TxOutput(100, randomBytes);
    VStream sinput;
    sinput << output;
    std::string s = sinput.str();

    TxOutput outputFromDeserialization;
    sinput >> outputFromDeserialization;

    VStream soutput;
    soutput << outputFromDeserialization;

    ASSERT_EQ(s, soutput.str());
    ASSERT_EQ(output, outputFromDeserialization);
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

    ASSERT_EQ(s, soutput.str());

    Transaction txx(tx);
    std::optional<Transaction> ot(std::forward<Transaction>(txx)), ots;
    VStream vs;
    vs << ot;
    vs >> ots;

    ASSERT_TRUE(ot.has_value());
    ASSERT_TRUE(ots.has_value());
    ASSERT_EQ(ot, ots);
}

TEST_F(TestSer, SerializeEqDeserializeBlock) {
    Block block = Block(1, rand1, zeros, rand2, time(nullptr), 1, 1);

    // Add tx to block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();

    tx.AddInput(TxInput(outpoint, Listing(randomBytes)));
    tx.AddOutput(TxOutput(100, Listing(randomBytes)));
    block.AddTransaction(tx);
    block.FinalizeHash();

    VStream sinput;
    sinput << block;
    std::string s = sinput.str();

    Block block1;
    sinput >> block1;

    VStream soutput;
    soutput << block1;

    ASSERT_EQ(s, soutput.str());
    ASSERT_EQ(block, block1);
    ASSERT_EQ(VStream(block1).size(), block1.GetOptimalEncodingSize());

    // Check parent pointers
    const Transaction* ptrTx = &(*block1.GetTransaction());
    ASSERT_EQ(&block1, ptrTx->GetParentBlock());
    for (const TxInput& input : ptrTx->GetInputs()) {
        ASSERT_EQ(ptrTx, input.GetParentTx());
    }
    for (const TxOutput& output : ptrTx->GetOutputs()) {
        ASSERT_EQ(ptrTx, output.GetParentTx());
    }

    Block block2(soutput);

    // check parent pointers
    ptrTx = &*block2.GetTransaction();
    ASSERT_EQ(&block2, ptrTx->GetParentBlock());
    for (const TxInput& input : ptrTx->GetInputs()) {
        ASSERT_EQ(ptrTx, input.GetParentTx());
    }
    for (const TxOutput& output : ptrTx->GetOutputs()) {
        ASSERT_EQ(ptrTx, output.GetParentTx());
    }

    // Check encoding size
    ASSERT_EQ(VStream(block2).size(), block2.GetOptimalEncodingSize());
}

TEST_F(TestSer, SerializeEqDeserializeNodeRecord) {
    Block blk = Block(1, rand1, zeros, rand2, time(nullptr), 1, 1);

    // Add a tx into the block
    TxOutPoint outpoint = TxOutPoint(rand1, 1);
    Transaction tx      = Transaction();
    tx.AddInput(TxInput(outpoint, randomBytes));
    tx.AddOutput(TxOutput(100, randomBytes));
    blk.AddTransaction(tx);
    blk.FinalizeHash();

    // Construct NodeRecord
    NodeRecord block(std::move(blk));
    block.minerChainHeight = 100;
    block.cumulativeReward = 10;

    // Link the chain state
    auto pstate = std::make_shared<ChainState>(100, arith_uint256(0), fac.NextTime(), arith_uint256(0X3E8),
                                               arith_uint256(0X3E8), 100000, std::vector<uint256>{});
    block.LinkChainState(pstate);

    // Make it a fake milestone
    block.isMilestone = false;

    VStream sinput;
    block.Serialize(sinput);
    std::string s = sinput.str();

    NodeRecord block1;
    block1.Deserialize(sinput);
    block1.cblock = block.cblock;

    VStream soutput;
    block1.Serialize(soutput);

    ASSERT_EQ(soutput.size(), block1.GetOptimalStorageSize());
    ASSERT_EQ(s, soutput.str());
    ASSERT_EQ(block, block1);

}
