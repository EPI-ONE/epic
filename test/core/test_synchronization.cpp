#include <gtest/gtest.h>
#include <iostream>

#include "caterpillar.h"
#include "consensus.h"
#include "dag_manager.h"
#include "test_env.h"
#include "test_network.h"

using LevelSet  = std::vector<NodeRecord>;
using TestChain = std::vector<LevelSet>;

class TestSync : public testing::Test {
public:
    static void SetUpTestCase() {
        config = std::make_unique<Config>();
        config->SetDBPath("testSync/");
        spdlog::set_level(spdlog::level::debug);

        DAG = std::make_unique<DAGManager>();
        file::SetDataDirPrefix(config->GetDBPath());
        CAT = std::make_unique<Caterpillar>(config->GetDBPath());

        // Initialize CAT with genesis block
        std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
        CAT->StoreRecords(genesisLvs);
        CAT->EnableOBC();

        peerManager = std::make_unique<TestPM>();
    }

    static void TearDownTestCase() {
        std::string cmd = "rm -r " + config->GetDBPath();
        system(cmd.c_str());

        config.reset();
        DAG.reset();
        CAT.reset();
    }

    TestFactory fac;

    void PrintChain(const TestChain& chain) {
        for (size_t i = 0; i < chain.size(); i++) {
            std::cout << "Height " << i << std::endl;
            for (auto& block : chain[i]) {
                std::cout << "hash = " << block.cblock->GetHash().to_substr();
                std::cout << ", prev = " << block.cblock->GetPrevHash().to_substr();
                std::cout << ", milestone = " << block.cblock->GetMilestoneHash().to_substr();
                std::cout << ", tip = " << block.cblock->GetTipHash().to_substr();
                std::cout << std::endl;
            }
            std::cout << std::endl;
        }
    }
};

TEST_F(TestSync, test_basic_network) {
    TestPM testPm;
    testPm.AddNewTestPeer(1);
    auto p = testPm.GetPeer((const void*) 1);
    ASSERT_TRUE(p);

    Ping ping(1);
    p->SendMessage(NetMessage((const void*) 1, PING, VStream(ping)));

    auto testPeer = (TestPeer*) p.get();
    ASSERT_TRUE(!testPeer->sentMsgBox.Empty());

    NetMessage msg;
    testPeer->sentMsgBox.Take(msg);

    ASSERT_TRUE(testPeer->sentMsgBox.Empty());
}

TEST_F(TestSync, test_version_ack) {
    TestPM* testPeerManager = (TestPM*) peerManager.get();

    const void* peer_handle = (const void*) 1;

    testPeerManager->AddNewTestPeer(1);
    auto p = testPeerManager->GetPeer(peer_handle);
    ASSERT_TRUE(p);

    auto testPeer            = (TestPeer*) p.get();
    testPeer->versionMessage = new VersionMessage(0, 0, 0, TestPeer::GetFakeAddr(), 10);


    /**Start the synchronization as the block requester*/

    // receive version ack
    NetMessage version_ack(peer_handle, VERSION_ACK, VStream(VersionACK()));
    testPeer->ProcessMessage(version_ack);

    // send GetInv
    NetMessage message_getInv;
    testPeer->sentMsgBox.Take(message_getInv);

    NetMessage message_getInv_Cmp = message_getInv;
    GetInv getInv(message_getInv.payload);

    // check GetInv message
    ASSERT_EQ(getInv.locator.size(), 1);
    ASSERT_EQ(getInv.locator[0], GENESIS_RECORD.cblock->GetHash());

    // check getInv task size before receiving Inv
    ASSERT_EQ(testPeer->GetInvTaskSize(), 1);

    // create a new chain
    long testChainHeight = 5;
    auto chain           = fac.CreateChain(GENESIS_RECORD, testChainHeight);

    PrintChain(chain);

    // receive Inv
    Inv inv(getInv.nonce);
    for (auto& levelSet : chain) {
        inv.AddItem(levelSet[levelSet.size() - 1].cblock->GetHash());
    }
    NetMessage inventory(peer_handle, INV, VStream(inv));
    testPeer->ProcessMessage(inventory);

    // check GetInv task size after receiving Inv
    ASSERT_EQ(testPeer->GetInvTaskSize(), 0);

    // send GetData
    NetMessage message_getData;
    testPeer->sentMsgBox.Take(message_getData);

    NetMessage message_getData_Cmp = message_getData;
    GetData getData(message_getData.payload);

    // check GetData message
    ASSERT_EQ(getData.hashes.size(), testChainHeight - 1);
    for (int i = 1; i < testChainHeight; i++) {
        ASSERT_EQ(getData.hashes[i - 1], chain[i][chain[i].size() - 1].cblock->GetHash());
    }

    // checkout GetData task size before receiving Bundle
    ASSERT_EQ(testPeer->GetDataTaskSize(), testChainHeight - 1);

    // receive bundle
    for (size_t i = 0; i < getData.hashes.size(); i++) {
        Bundle bundle(getData.bundleNonce[i]);
        for (auto& block : chain[i + 1]) {
            bundle.AddBlock(block.cblock);
        }
        NetMessage bundle_message(peer_handle, BUNDLE, VStream(bundle));
        testPeer->ProcessMessage(bundle_message);
    }

    CAT->Wait();
    DAG->Wait();

    // the last GetInv to ensure that local node has downloaded enough blocks
    ASSERT_EQ(testPeer->GetInvTaskSize(), 1);
    ASSERT_EQ(testPeer->GetDataTaskSize(), 0);

    // after downloading all task block, local node will send a GetInv to trigger the next synchronization
    NetMessage message_ms_ack;
    testPeer->sentMsgBox.Take(message_ms_ack);
    GetInv ms_ack(message_ms_ack.payload);

    // tell node that he has downloaded enough blocks
    Inv sync_complete_ack(ms_ack.nonce);
    NetMessage message_sync_complete_ack(peer_handle, INV, VStream(sync_complete_ack));
    testPeer->ProcessMessage(message_sync_complete_ack);

    CAT->Wait();
    DAG->Wait();

    ASSERT_EQ(testPeer->GetInvTaskSize(), 0);
    ASSERT_EQ(testPeer->GetDataTaskSize(), 1);

    NetMessage message_pendingSet;
    testPeer->sentMsgBox.Take(message_pendingSet);
    GetData pendingSet_request(message_pendingSet.payload);
    ASSERT_EQ(pendingSet_request.type, GetDataTask::PENDING_SET);

    Bundle pending_set(pendingSet_request.bundleNonce[0]);
    NetMessage message_pendingSet_bundle(peer_handle, BUNDLE, VStream(pending_set));
    testPeer->ProcessMessage(message_pendingSet_bundle);

    CAT->Wait();
    DAG->Wait();

    ASSERT_EQ(testPeer->GetInvTaskSize(), 0);
    ASSERT_EQ(testPeer->GetDataTaskSize(), 0);
    /**Finish the synchronization as the block requester*/


    /**Start  the synchronization as the block provider*/

    // receive GetInv
    testPeer->ProcessMessage(message_getInv_Cmp);

    // send Inv
    NetMessage message_inv;
    testPeer->sentMsgBox.Take(message_inv);
    Inv inv1(message_inv.payload);

    // check Inv nonce
    ASSERT_EQ(inv1.nonce, getInv.nonce);
    // check Inv hash size
    ASSERT_EQ(inv1.hashes.size(), testChainHeight);

    // receive GetData
    testPeer->ProcessMessage(message_getData_Cmp);

    // send Bundle
    ASSERT_EQ(testPeer->sentMsgBox.Size(), testChainHeight - 1);
    for (int i = 1; i < testChainHeight; i++) {
        NetMessage msg;
        testPeer->sentMsgBox.Take(msg);
        Bundle bundle(msg.payload);
        ASSERT_EQ(bundle.blocks.size(), chain[i].size());
        ASSERT_EQ(bundle.blocks[bundle.blocks.size() - 1]->GetHash(), chain[i][chain[i].size() - 1].cblock->GetHash());
    }

    /**Finish the synchronization as the block provider*/
}
