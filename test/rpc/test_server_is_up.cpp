#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

#include "rpc_client.h"
#include "rpc_server.h"

class TestRPCServer : public testing::Test {};

TEST_F(TestRPCServer, DummyServerResponses) {
    std::string adr  = "0.0.0.0:3777";
    auto netAddress  = NetAddress::GetByIP(adr);
    RPCServer server = RPCServer(*netAddress);
    server.Start();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));


    auto genesis_resp = client.GetBlock(GENESIS.GetHash());
    EXPECT_EQ(genesis_resp.block_hash().hash(), std::to_string(GENESIS.GetHash()));

    EXPECT_TRUE(client.GetLevelSet());
    EXPECT_TRUE(client.GetLevelSetSize());
    EXPECT_TRUE(client.GetNewMilestoneSince());
    EXPECT_TRUE(client.GetLatestMilestone());

    server.Shutdown();

    genesis_resp = client.GetBlock(GENESIS.GetHash());
    EXPECT_EQ(genesis_resp.block_hash().hash(), "");
    EXPECT_FALSE(client.GetLevelSet());
    EXPECT_FALSE(client.GetLevelSetSize());
    EXPECT_FALSE(client.GetNewMilestoneSince());
    EXPECT_FALSE(client.GetLatestMilestone());
}
