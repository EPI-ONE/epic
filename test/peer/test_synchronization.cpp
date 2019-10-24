// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "block_store.h"
#include "peer_manager.h"
#include "test_env.h"
#include "vertex.h"

#include <algorithm>
#include <vector>

class TestSync : public testing::Test {
public:
    ConnectionManager server;
    ConnectionManager client;
    AddressManager* addressManager;

    PeerPtr peer_server;
    shared_connection_t client_connection;

    void ServerCallback(shared_connection_t connection) {
        std::optional<NetAddress> address = NetAddress::GetByIP(connection->GetRemote());
        peer_server                       = std::make_shared<Peer>(*address, connection, false, addressManager, 100);
        peer_server->SetWeakPeer(peer_server);
    }

    void ClientCallback(shared_connection_t connection) {
        client_connection = connection;
    }

    static void SetUpTestCase() {
        CONFIG = std::make_unique<Config>();
        CONFIG->SetDBPath("testSync/");
        SetLogLevel(SPDLOG_LEVEL_DEBUG);

        EpicTestEnvironment::SetUpDAG(CONFIG->GetDBPath());

        STORE->EnableOBC();
        PEERMAN = std::make_unique<PeerManager>();
    }

    static void TearDownTestCase() {
        EpicTestEnvironment::TearDownDAG(CONFIG->GetDBPath());
        CONFIG.reset();
        ResetLogLevel();
    }

    void SetUp() {
        addressManager = new AddressManager();
        server.Start();
        client.Start();
        server.RegisterNewConnectionCallback(std::bind(&TestSync::ServerCallback, this, std::placeholders::_1));
        client.RegisterNewConnectionCallback(std::bind(&TestSync::ClientCallback, this, std::placeholders::_1));
    }

    void TearDown() {
        peer_server->Disconnect();
        server.Stop();
        client.Stop();
        delete addressManager;
    }

    TestFactory fac;
};

TEST_F(TestSync, test_basic_sync_workflow) {
    // create a new chain
    constexpr long testChainHeight = 5;
    TestRawChain chain;
    std::tie(chain, std::ignore) = fac.CreateRawChain(GENESIS_VERTEX, testChainHeight);

    ASSERT_TRUE(server.Bind(0x7f000001));
    ASSERT_TRUE(server.Listen(12121));
    ASSERT_TRUE(client.Connect(0x7f000001, 12121));
    usleep(50000);

    client_connection->SendMessage(
        std::make_unique<VersionMessage>(peer_server->address, peer_server->address, testChainHeight, 0, 100));
    usleep(50000);
    connection_message_t message;

    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::VERSION_MSG);
    peer_server->ProcessMessage(message.second);

    ASSERT_TRUE(client.ReceiveMessage(message));
    ASSERT_TRUE(client.ReceiveMessage(message));
    client_connection->SendMessage(std::make_unique<NetMessage>(NetMessage::VERSION_ACK));

    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::VERSION_ACK);
    peer_server->ProcessMessage(message.second);

    /**Start the synchronization as the block requester*/
    peer_server->StartSync();
    usleep(50000);
    ASSERT_TRUE(client.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::GET_INV);
    GetInv* getInv = dynamic_cast<GetInv*>(message.second.get());

    // check GetInv message
    ASSERT_EQ(getInv->locator.size(), 1);
    ASSERT_EQ(getInv->locator[0], GENESIS->GetHash());

    auto getInv_Cmp = std::unique_ptr<GetInv>(dynamic_cast<GetInv*>(message.second.release()));

    // check getInv task size before receiving Inv
    ASSERT_EQ(peer_server->GetInvTaskSize(), 1);

    // receive Inv
    std::vector<uint256> hashes;
    for (auto& levelSet : chain) {
        hashes.push_back(levelSet[levelSet.size() - 1]->GetHash());
    }
    client_connection->SendMessage(std::make_unique<Inv>(hashes, getInv->nonce));
    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::INV);
    peer_server->ProcessMessage(message.second);

    // check GetInv task size after receiving Inv
    usleep(50000);
    ASSERT_EQ(peer_server->GetInvTaskSize(), 0);

    // send GetData
    ASSERT_TRUE(client.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::GET_DATA);
    GetData* getData = dynamic_cast<GetData*>(message.second.get());

    // check GetData message
    ASSERT_EQ(getData->hashes.size(), testChainHeight);
    for (int i = 0; i < testChainHeight - 1; i++) {
        ASSERT_EQ(getData->hashes[i], chain[i][chain[i].size() - 1]->GetHash());
    }

    auto getData_Cmp = std::unique_ptr<GetData>(dynamic_cast<GetData*>(message.second.release()));

    // receive bundle in a random order
    std::vector<int> bundle_order(getData->hashes.size());
    for (int i = 0; i < getData->hashes.size(); i++) {
        bundle_order[i] = i;
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(bundle_order.begin(), bundle_order.end(), g);

    for (auto& i : bundle_order) {
        auto bundle = std::make_unique<Bundle>(getData->bundleNonce[i]);
        for (auto& block : chain[i]) {
            bundle->AddBlock(block);
        }
        std::swap(bundle->blocks.front(), bundle->blocks.back());
        client_connection->SendMessage(std::move(bundle));
    }

    for (int i = 0; i < bundle_order.size(); i++) {
        ASSERT_TRUE(server.ReceiveMessage(message));
        ASSERT_EQ(message.second->GetType(), NetMessage::BUNDLE);
        peer_server->ProcessMessage(message.second);
    }

    usleep(50000);
    STORE->Wait();
    DAG->Wait();

    peer_server->StartSync();
    usleep(50000);

    // the last GetInv to ensure that local node has downloaded enough blocks
    ASSERT_EQ(peer_server->GetInvTaskSize(), 1);
    ASSERT_EQ(peer_server->GetDataTaskSize(), 0);

    // after downloading all task block, local node will send a GetInv to trigger the next synchronization
    ASSERT_TRUE(client.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::GET_INV);
    GetInv* ms_ack = dynamic_cast<GetInv*>(message.second.get());

    // tell node that it has downloaded enough blocks
    client_connection->SendMessage(std::make_unique<Inv>(ms_ack->nonce));
    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::INV);
    peer_server->ProcessMessage(message.second);


    usleep(50000);
    DAG->Wait();

    ASSERT_EQ(peer_server->GetInvTaskSize(), 0);
    ASSERT_EQ(peer_server->GetDataTaskSize(), 1);

    ASSERT_TRUE(client.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::GET_DATA);
    GetData* pendingSet_request = dynamic_cast<GetData*>(message.second.get());
    ASSERT_EQ(pendingSet_request->type, GetDataTask::PENDING_SET);

    client_connection->SendMessage(std::make_unique<Bundle>(pendingSet_request->bundleNonce[0]));
    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::BUNDLE);
    peer_server->ProcessMessage(message.second);

    usleep(50000);
    DAG->Wait();

    ASSERT_EQ(peer_server->GetInvTaskSize(), 0);
    ASSERT_EQ(peer_server->GetDataTaskSize(), 0);
    /**Finish the synchronization as the block requester*/

    /**Start  the synchronization as the block provider*/
    // receive GetInv
    uint32_t nonce = getInv_Cmp->nonce;
    client_connection->SendMessage(std::move(getInv_Cmp));
    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::GET_INV);
    peer_server->ProcessMessage(message.second);

    // send Inv
    ASSERT_TRUE(client.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::INV);
    Inv* inv1 = dynamic_cast<Inv*>(message.second.get());

    // check Inv nonce
    ASSERT_EQ(inv1->nonce, nonce);
    // check Inv hash size
    ASSERT_EQ(inv1->hashes.size(), testChainHeight);

    // receive GetData
    client_connection->SendMessage(std::move(getData_Cmp));
    ASSERT_TRUE(server.ReceiveMessage(message));
    ASSERT_EQ(message.second->GetType(), NetMessage::GET_DATA);
    peer_server->ProcessMessage(message.second);

    usleep(50000);
    DAG->Wait();

    // send Bundle
    std::set<uint256> ms_hash;
    for (auto& levelSet : chain) {
        ms_hash.insert(levelSet[0]->GetHash());
    }
    for (int i = 0; i < testChainHeight; i++) {
        ASSERT_TRUE(client.ReceiveMessage(message));
        ASSERT_EQ(message.second->GetType(), NetMessage::BUNDLE);
        Bundle* bundle = dynamic_cast<Bundle*>(message.second.get());
        int j          = bundle->nonce - 1;
        ASSERT_EQ(bundle->blocks.size(), chain[j].size());
        ASSERT_EQ(bundle->blocks[bundle->blocks.size() - 1]->GetHash(), chain[j][0]->GetHash());
        ms_hash.erase(bundle->blocks[bundle->blocks.size() - 1]->GetHash());
    }

    ASSERT_TRUE(ms_hash.empty());

    /**Finish the synchronization as the block provider*/
}
