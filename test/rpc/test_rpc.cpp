// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "key.h"
#include "rpc_client.h"
#include "rpc_server.h"
#include "rpc_tools.h"
#include "test_env.h"

#include <chrono>

class TestRPCServer : public testing::Test {
public:
    TestFactory fac;
    std::string prefix = "test_rpc/";
    static std::string adr;
    const uint256 double0hash = Hash::GetDoubleZeroHash();

    static void SetUpTestCase() {
        adr = "0.0.0.0:3778";
    }

    void SetUp() {
        EpicTestEnvironment::SetUpDAG(prefix);
        auto netAddress = NetAddress::GetByIP(adr);
        RPC             = std::make_unique<RPCServer>(*netAddress, std::vector{RPCServiceType::BLOCK_EXPLORER_SERVER});
        RPC->Start();

        while (!RPC->IsRunning()) {
            std::this_thread::yield();
        }
    }

    void TearDown() {
        EpicTestEnvironment::TearDownDAG(prefix);
        RPC->Shutdown();
    }
};

std::string TestRPCServer::adr = "";

TEST_F(TestRPCServer, GetBlock) {
    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));

    auto req_hash = std::to_string(GENESIS->GetHash());
    auto res      = client.GetBlock(req_hash);
    ASSERT_TRUE(res.has_value());

    auto blk = ToBlock(*res);
    ASSERT_EQ(blk, *GENESIS);

    auto res_fake = client.GetBlock(std::to_string(double0hash));
    ASSERT_TRUE(res_fake.has_value());

    auto blk_fake    = ToBlock(*res_fake);
    auto blk_default = ToBlock(rpc::Block().default_instance());
    ASSERT_EQ(blk_fake, blk_default);
}

TEST_F(TestRPCServer, GetLevelSetAndItsSize) {
    int size = 1;
    std::vector<VertexPtr> lvs;
    lvs.reserve(size);
    auto ms = fac.CreateVertexPtr(1, 1, true);
    fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, ms);
    ms->isMilestone      = true;
    ms->snapshot->height = 1;
    ms->height           = 1;
    lvs.push_back(ms);
    for (int i = 1; i < size; ++i) {
        auto b         = fac.CreateVertexPtr(fac.GetRand() % 10, fac.GetRand() % 10, true);
        b->isMilestone = false;
        b->height      = ms->height;
        lvs.push_back(b);
    }
    ASSERT_TRUE(STORE->StoreLevelSet(lvs));

    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));

    for (int i = 0; i < size; ++i) {
        auto req_hash = std::to_string(lvs[i]->cblock->GetHash());
        auto res_size = client.GetLevelSetSize(req_hash);
        ASSERT_TRUE(res_size.has_value());
        EXPECT_EQ(res_size.value(), size);
    }
    auto req_hash = std::to_string(lvs[0]->cblock->GetHash());
    auto res_set  = client.GetLevelSet(req_hash);
    ASSERT_TRUE(res_set.has_value());

    for (size_t i = 0; i < lvs.size(); ++i) {
        auto res_blk = ToBlock(res_set.value()[i]);
        EXPECT_EQ(res_blk, *lvs[i]->cblock);
    }
}

TEST_F(TestRPCServer, GetLatestMilestone) {
    int size       = 5;
    auto chain     = fac.CreateChain(GENESIS_VERTEX, size);
    auto latest_ms = chain.back().back();
    for (auto lvs : chain) {
        for (auto elem : lvs) {
            DAG->AddNewBlock(elem->cblock, nullptr);
        }
    }

    usleep(50000);
    DAG->Stop();

    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));

    auto res = client.GetLatestMilestone();
    ASSERT_TRUE(res.has_value());

    auto res_blk = ToBlock(*res);
    EXPECT_EQ(res_blk, *latest_ms->cblock);
}

TEST_F(TestRPCServer, GetNewMilestoneSince) {
    int size = 5;
    std::vector<VertexPtr> mss;
    std::vector<VertexPtr> mss_to_check;
    auto first_ms = fac.CreateVertexPtr(1, 1, true);
    fac.CreateMilestonePtr(GENESIS_VERTEX->snapshot, first_ms);
    first_ms->isMilestone      = true;
    first_ms->snapshot->height = 1;
    first_ms->height           = 1;
    mss.push_back(first_ms);
    mss_to_check.push_back(first_ms);
    ASSERT_TRUE(STORE->StoreLevelSet(mss));
    mss.clear();
    auto prev = first_ms->snapshot;
    for (int i = 2; i < size; ++i) {
        auto ms = fac.CreateVertexPtr(i, i, true);
        fac.CreateMilestonePtr(prev, ms);
        ms->isMilestone            = true;
        first_ms->snapshot->height = i;
        ms->height                 = i;
        mss.push_back(ms);
        mss_to_check.push_back(ms);
        ASSERT_TRUE(STORE->StoreLevelSet(mss));
        mss.clear();
        prev = ms->snapshot;
    }

    usleep(50000);
    DAG->Stop();

    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));

    size_t request_milestone_number = 3;
    auto req_hash                   = std::to_string(mss_to_check[0]->cblock->GetHash());
    auto res                        = client.GetNewMilestoneSince(req_hash, request_milestone_number);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value().size(), request_milestone_number);
    for (int i = 0; i < request_milestone_number; ++i) {
        auto res_blk = ToBlock(res.value()[i]);
        EXPECT_EQ(res_blk, *mss_to_check[i + 1]->cblock);
    }
}
