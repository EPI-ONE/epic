#include <chrono>
#include <gtest/gtest.h>
#include <iostream>

#include "rpc_client.h"
#include "rpc_server.h"

#include "key.h"
#include "test_env.h"

class TestRPCServer : public testing::Test {
public:
    TestFactory fac;
    std::string prefix = "test_rpc/";
    static std::string adr;

    static void SetUpTestCase() {
        adr = "0.0.0.0:3778";
    }

    void SetUp() {
        EpicTestEnvironment::SetUpDAG(prefix);
        auto netAddress = NetAddress::GetByIP(adr);
        RPC             = std::make_unique<RPCServer>(*netAddress);
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

    auto g        = std::make_shared<NodeRecord>(GENESIS_RECORD);
    auto req_hash = std::to_string(g->cblock->GetHash());
    auto res      = client.GetBlock(req_hash);
    ASSERT_TRUE(res.has_value());
    auto expected = HashToRPCHash(g->cblock->GetHash());
    EXPECT_EQ(res.value().block_hash().hash(), expected->hash());
    delete expected;
}

TEST_F(TestRPCServer, GetLevelSetAndItsSize) {
    int size = 1;
    std::vector<RecordPtr> lvs;
    lvs.reserve(size);
    auto ms = fac.CreateRecordPtr(1, 1, true);
    fac.CreateChainStatePtr(GENESIS_RECORD.snapshot, ms);
    ms->isMilestone      = true;
    ms->snapshot->height = 1;
    ms->height           = 1;
    lvs.push_back(ms);
    for (int i = 1; i < size; ++i) {
        auto b         = fac.CreateRecordPtr(fac.GetRand() % 10, fac.GetRand() % 10, true);
        b->isMilestone = false;
        b->height      = ms->height;
        lvs.push_back(b);
    }
    ASSERT_TRUE(CAT->StoreLevelSet(lvs));

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
    auto expected = HashToRPCHash(lvs[0]->cblock->GetHash());
    EXPECT_EQ(res_set.value()[0].block_hash().hash(), expected->hash());
    delete expected;
}

TEST_F(TestRPCServer, GetLatestMilestone) {
    int size       = 5;
    auto chain     = std::get<0>(fac.CreateChain(GENESIS_RECORD, size));
    auto latest_ms = chain.back().back();
    for (auto lvs : chain) {
        for (auto elem : lvs) {
            DAG->AddNewBlock(elem, nullptr);
        }
    }

    usleep(50000);
    DAG->Stop();

    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));

    auto res = client.GetLatestMilestone();
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res.value().block_hash().hash(), std::to_string(latest_ms->GetHash()));
}

TEST_F(TestRPCServer, GetNewMilestoneSince) {
    int size = 5;
    std::vector<RecordPtr> mss;
    std::vector<RecordPtr> mss_to_check;
    auto first_ms = fac.CreateRecordPtr(1, 1, true);
    fac.CreateChainStatePtr(GENESIS_RECORD.snapshot, first_ms);
    first_ms->isMilestone      = true;
    first_ms->snapshot->height = 1;
    first_ms->height           = 1;
    mss.push_back(first_ms);
    mss_to_check.push_back(first_ms);
    ASSERT_TRUE(CAT->StoreLevelSet(mss));
    mss.clear();
    auto prev = first_ms->snapshot;
    for (int i = 2; i < size; ++i) {
        auto ms = fac.CreateRecordPtr(i, i, true);
        fac.CreateChainStatePtr(prev, ms);
        ms->isMilestone            = true;
        first_ms->snapshot->height = i;
        ms->height                 = i;
        mss.push_back(ms);
        mss_to_check.push_back(ms);
        ASSERT_TRUE(CAT->StoreLevelSet(mss));
        mss.clear();
        prev = ms->snapshot;
    }

    usleep(50000);
    CAT->Stop();
    DAG->Stop();

    RPCClient client(grpc::CreateChannel(adr, grpc::InsecureChannelCredentials()));

    size_t request_milestone_number = 3;
    auto req_hash                   = std::to_string(mss_to_check[0]->cblock->GetHash());
    auto res                        = client.GetNewMilestoneSince(req_hash, request_milestone_number);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res.value().size(), request_milestone_number);
    for (int i = 0; i < request_milestone_number; ++i) {
        EXPECT_EQ(res.value()[i].block_hash().hash(), std::to_string(mss_to_check[i + 1]->cblock->GetHash()));
    }
}
