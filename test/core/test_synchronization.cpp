#include <algorithm>
#include <gtest/gtest.h>
#include <iostream>
#include <vector>

#include "caterpillar.h"
#include "consensus.h"
#include "dag_manager.h"
#include "test_env.h"
#include "test_network.h"

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

TEST_F(TestSync, test_basic_sync_workflow) {
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
    constexpr long testChainHeight = 5;
    TestChain chain;
    std::tie(chain, std::ignore) = fac.CreateChain(GENESIS_RECORD, testChainHeight);

    // receive Inv
    Inv inv(getInv.nonce);
    for (auto& levelSet : chain) {
        inv.AddItem(levelSet[levelSet.size() - 1]->GetHash());
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
    for (int i = 0; i < testChainHeight - 1; i++) {
        ASSERT_EQ(getData.hashes[i], chain[i][chain[i].size() - 1]->GetHash());
    }

    // checkout GetData task size before receiving Bundle
    ASSERT_EQ(testPeer->GetDataTaskSize(), testChainHeight - 1);


    // receive bundle in a random order
    std::vector<int> bundle_order(getData.hashes.size());
    for (int i = 0; i < getData.hashes.size(); i++) {
        bundle_order[i] = i;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(bundle_order.begin(), bundle_order.end(), g);

    for (auto& i : bundle_order) {
        Bundle bundle(getData.bundleNonce[i]);
        for (auto& block : chain[i ]) {
            bundle.AddBlock(block);
        }
        NetMessage bundle_message(peer_handle, BUNDLE, VStream(bundle));
        testPeer->ProcessMessage(bundle_message);
    }


    usleep(50000);
    CAT->Wait();
    DAG->Wait();

    // the last GetInv to ensure that local node has downloaded enough blocks
    ASSERT_EQ(testPeer->GetInvTaskSize(), 1);
    ASSERT_EQ(testPeer->GetDataTaskSize(), 0);

    // after downloading all task block, local node will send a GetInv to trigger the next synchronization
    NetMessage message_ms_ack;
    testPeer->sentMsgBox.Take(message_ms_ack);
    GetInv ms_ack(message_ms_ack.payload);

    // tell node that it has downloaded enough blocks
    Inv sync_complete_ack(ms_ack.nonce);
    NetMessage message_sync_complete_ack(peer_handle, INV, VStream(sync_complete_ack));
    testPeer->ProcessMessage(message_sync_complete_ack);

    usleep(50000);
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

    usleep(50000);
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

    usleep(50000);
    DAG->Wait();

    // send Bundle
    ASSERT_EQ(testPeer->sentMsgBox.Size(), testChainHeight - 1);
    for (int i = 0; i < testChainHeight - 1; i++) {
        NetMessage msg;
        testPeer->sentMsgBox.Take(msg);
        Bundle bundle(msg.payload);
        ASSERT_EQ(bundle.blocks.size(), chain[i].size());
        ASSERT_EQ(bundle.blocks[bundle.blocks.size() - 1]->GetHash(), chain[i][chain[i].size() - 1]->GetHash());
    }

    /**Finish the synchronization as the block provider*/
}
