// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "block_store.h"
#include "file_utils.h"
#include "test_env.h"

#include <string>

class TestFileStorage : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
    Miner m{1};
    std::string prefix = "test_file_store/";

    void SetUp() override {
        file::SetDataDirPrefix(prefix);
    }

    void TearDown() override {
        EpicTestEnvironment::TearDownDAG(prefix);
    }
};

TEST_F(TestFileStorage, basic_read_write) {
    // data preparation
    auto blk = fac.CreateBlock();
    m.Solve(blk);
    Vertex vtx{blk};
    uint32_t blksize = blk.GetOptimalEncodingSize(), vtxsize = vtx.GetOptimalStorageSize();
    FilePos fpos{0, 0, 0};
    FilePos fpos1{0, 0, blksize};

    // writing
    FileWriter writer{file::FileType::BLK, fpos};
    ASSERT_EQ(writer.GetOffset(), 0);
    writer << blk;
    ASSERT_EQ(writer.GetOffset(), blksize);
    writer << vtx;
    ASSERT_EQ(writer.GetOffset(), blksize + vtxsize);
    writer.Close();

    // reading
    FileReader reader{file::FileType::BLK, fpos};
    Block blk1{};
    ASSERT_EQ(reader.GetOffset(), 0);
    reader >> blk1;
    ASSERT_EQ(reader.GetOffset(), blksize);
    ASSERT_EQ(blk, blk1);

    Vertex vtx1{};
    reader >> vtx1;
    ASSERT_EQ(reader.GetOffset(), blksize + vtxsize);
    ASSERT_EQ(vtx, vtx1);
    reader.Close();

    // modifying
    FileModifier modifier{file::FileType::BLK, fpos1};
    vtx.isRedeemed = Vertex::IS_REDEEMED;
    modifier << vtx;
    modifier.Close();

    // checking modifying result
    Vertex vtx2{};
    FileReader reader2{file::FileType::BLK, fpos1};
    reader2 >> vtx2;
    ASSERT_EQ(reader2.GetOffset(), blksize + vtxsize);
    ASSERT_EQ(vtx, vtx2);
    reader2.Close();
}

TEST_F(TestFileStorage, cat_store_and_get_vertices_and_get_lvs) {
    EpicTestEnvironment::SetUpDAG(prefix);
    STORE->SetFileCapacities(8000, 2);

    std::vector<VertexPtr> blocks;
    std::vector<std::vector<VertexPtr>> levelsets;

    constexpr int nLvs = 20;

    // Consturct level sets
    for (int i = 1; i <= nLvs; ++i) {
        int size = fac.GetRand() % 10;

        std::vector<VertexPtr> lvs;
        lvs.reserve(size);

        // Construct milestone
        auto ms      = fac.CreateVertexPtr(1, 1, true);
        auto prev_ms = levelsets.empty() ? *GENESIS_VERTEX : *levelsets.back()[0];
        fac.CreateMilestonePtr(prev_ms.snapshot, ms);
        ms->isMilestone = true;
        ms->height      = i;

        lvs.push_back(ms);
        blocks.push_back(ms);

        // Construct blocks in the level set
        for (int j = 0; j < size; ++j) {
            auto b         = fac.CreateVertexPtr(fac.GetRand() % 10, fac.GetRand() % 10, true);
            b->isMilestone = false;
            b->height      = ms->height;
            lvs.push_back(b);
            blocks.emplace_back(b);
        }

        ASSERT_TRUE(STORE->StoreLevelSet(lvs));
        levelsets.emplace_back(std::move(lvs));
    }

    // Inspect inserted vertices
    for (auto& block : blocks) {
        const auto& h = block->cblock->GetHash();

        // without cblock
        auto vtx = STORE->GetVertex(h, false);
        ASSERT_TRUE(vtx);
        ASSERT_FALSE(vtx->cblock);
        ASSERT_EQ(*block, *vtx);

        // with cblock
        auto vtx_blk = STORE->GetVertex(h);
        ASSERT_TRUE(vtx_blk->cblock);
        ASSERT_EQ(*block, *vtx_blk);
    }

    // Recover level sets in blocks in batch
    auto vs_blks = STORE->GetRawLevelSetBetween(1, nLvs);
    ASSERT_FALSE(vs_blks.empty());

    for (auto& block : blocks) {
        Block recovered_blk(vs_blks);
        ASSERT_EQ(*block->cblock, recovered_blk);
    }

    // Recover level sets in vtcs in batch
    auto vs_vtcs = STORE->GetRawLevelSetBetween(1, nLvs, file::FileType::VTX);
    ASSERT_FALSE(vs_vtcs.empty());

    for (auto& block : blocks) {
        Vertex recovered_rec(vs_vtcs);
        ASSERT_EQ(*block, recovered_rec);
    }

    // Recover single level set
    const auto& lvs = levelsets.back();
    auto height     = lvs.front()->height;

    auto recovered_blks      = STORE->GetLevelSetBlksAt(height);
    auto recovered_vtcs_blks = STORE->GetLevelSetVtcsAt(height);
    auto recovered_vtcs      = STORE->GetLevelSetVtcsAt(height, false);

    ASSERT_EQ(recovered_blks.size(), lvs.size());
    ASSERT_EQ(recovered_vtcs_blks.size(), lvs.size());
    ASSERT_EQ(recovered_vtcs.size(), lvs.size());

    ASSERT_TRUE(recovered_vtcs_blks[0]->snapshot);
    ASSERT_FALSE(recovered_vtcs_blks[0]->snapshot->GetLevelSet().empty());
    ASSERT_TRUE(recovered_vtcs_blks[0]->snapshot->GetLevelSet()[0].lock());

    for (size_t i = 0; i < lvs.size(); ++i) {
        ASSERT_TRUE(recovered_vtcs_blks[i]->cblock);
        ASSERT_EQ(*lvs[i]->cblock, *recovered_blks[i]);
        ASSERT_EQ(*lvs[i], *recovered_vtcs_blks[i]);
        ASSERT_EQ(*lvs[i], *recovered_vtcs[i]);
    }

    // update vertices
    Vertex copyVtx;
    {
        auto vtx        = STORE->GetVertex(blocks[0]->cblock->GetHash());
        vtx->isRedeemed = Vertex::IS_REDEEMED;
        copyVtx         = *vtx;
    }
    auto newout = STORE->GetVertex(blocks[0]->cblock->GetHash());
    ASSERT_EQ(copyVtx, *newout);
}

TEST_F(TestFileStorage, test_modifier) {
    EpicTestEnvironment::SetUpDAG(prefix);

    // Construct a fake ms vertex
    auto vertex                = fac.CreateVertexPtr(1, 1, true);
    vertex->snapshot           = std::make_shared<Milestone>();
    vertex->snapshot->height   = 1;
    vertex->isRedeemed         = Vertex::NOT_YET_REDEEMED;
    std::vector<VertexPtr> lvs = {vertex};

    // Store it to file
    auto vtx_hash = vertex->cblock->GetHash();
    STORE->StoreLevelSet(lvs);

    // Retrive it from file and make sure nothing is modified
    auto vtx_from_file = STORE->GetVertex(vtx_hash);
    ASSERT_EQ(*vtx_from_file, *vertex);

    // Modify the redemption status and destruct it
    vtx_from_file->isRedeemed = Vertex::IS_REDEEMED;
    vtx_from_file.reset();

    // Retrive it again from file and make sure the redemption status
    // in the file is also modified
    auto vtx_modified = STORE->GetVertex(vtx_hash);
    ASSERT_EQ(vtx_modified->isRedeemed, Vertex::IS_REDEEMED);

    // Make sure that everything else stays the same
    vtx_modified->isRedeemed = Vertex::NOT_YET_REDEEMED;
    ASSERT_EQ(*vtx_modified, *vertex);
}
