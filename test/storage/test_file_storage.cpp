// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "file_utils.h"
#include "storage.h"
#include "test_env.h"

#include <string>

class TestFileStorage : public testing::Test {
public:
    TestFactory fac    = EpicTestEnvironment::GetFactory();
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
    blk.Solve();
    NodeRecord rec{blk};
    uint32_t blksize = blk.GetOptimalEncodingSize(), recsize = rec.GetOptimalStorageSize();
    FilePos fpos{0, 0, 0};
    FilePos fpos1{0, 0, blksize};

    // writing
    FileWriter writer{file::FileType::BLK, fpos};
    ASSERT_EQ(writer.GetOffset(), 0);
    writer << blk;
    ASSERT_EQ(writer.GetOffset(), blksize);
    writer << rec;
    ASSERT_EQ(writer.GetOffset(), blksize + recsize);
    writer.Close();

    // reading
    FileReader reader{file::FileType::BLK, fpos};
    Block blk1{};
    ASSERT_EQ(reader.GetOffset(), 0);
    reader >> blk1;
    ASSERT_EQ(reader.GetOffset(), blksize);
    ASSERT_EQ(blk, blk1);

    NodeRecord rec1{};
    reader >> rec1;
    ASSERT_EQ(reader.GetOffset(), blksize + recsize);
    ASSERT_EQ(rec, rec1);
    reader.Close();

    // modifying
    FileModifier modifier{file::FileType::BLK, fpos1};
    rec.isRedeemed = NodeRecord::IS_REDEEMED;
    modifier << rec;
    modifier.Close();

    // checking modifying result
    NodeRecord rec2{};
    FileReader reader2{file::FileType::BLK, fpos1};
    reader2 >> rec2;
    ASSERT_EQ(reader2.GetOffset(), blksize + recsize);
    ASSERT_EQ(rec, rec2);
    reader2.Close();
}

TEST_F(TestFileStorage, cat_store_and_get_records_and_get_lvs) {
    EpicTestEnvironment::SetUpDAG(prefix);
    STORE->SetFileCapacities(8000, 2);

    std::vector<RecordPtr> blocks;
    std::vector<std::vector<RecordPtr>> levelsets;

    constexpr int nLvs = 20;

    // Consturct level sets
    for (int i = 1; i <= nLvs; ++i) {
        int size = fac.GetRand() % 10;

        std::vector<RecordPtr> lvs;
        lvs.reserve(size);

        // Construct milestone
        auto ms      = fac.CreateRecordPtr(1, 1, true);
        auto prev_ms = levelsets.empty() ? GENESIS_RECORD : *levelsets.back()[0];
        fac.CreateChainStatePtr(prev_ms.snapshot, ms);
        ms->isMilestone = true;
        ms->height      = i;

        lvs.push_back(ms);
        blocks.push_back(ms);

        // Construct blocks in the level set
        for (int j = 0; j < size; ++j) {
            auto b         = fac.CreateRecordPtr(fac.GetRand() % 10, fac.GetRand() % 10, true);
            b->isMilestone = false;
            b->height      = ms->height;
            lvs.push_back(b);
            blocks.emplace_back(b);
        }

        ASSERT_TRUE(STORE->StoreLevelSet(lvs));
        levelsets.emplace_back(std::move(lvs));
    }

    // Inspect inserted records
    for (auto& block : blocks) {
        const auto& h = block->cblock->GetHash();

        // without cblock
        auto rec = STORE->GetRecord(h, false);
        ASSERT_TRUE(rec);
        ASSERT_FALSE(rec->cblock);
        ASSERT_EQ(*block, *rec);

        // with cblock
        auto rec_blk = STORE->GetRecord(h);
        ASSERT_TRUE(rec_blk->cblock);
        ASSERT_EQ(*block, *rec_blk);
    }

    // Recover level sets in blocks in batch
    auto vs_blks = STORE->GetRawLevelSetBetween(1, nLvs);
    ASSERT_FALSE(vs_blks.empty());

    for (auto& block : blocks) {
        Block recovered_blk(vs_blks);
        ASSERT_EQ(*block->cblock, recovered_blk);
    }

    // Recover level sets in recs in batch
    auto vs_recs = STORE->GetRawLevelSetBetween(1, nLvs, file::FileType::REC);
    ASSERT_FALSE(vs_recs.empty());

    for (auto& block : blocks) {
        NodeRecord recovered_rec(vs_recs);
        ASSERT_EQ(*block, recovered_rec);
    }

    // Recover single level set
    const auto& lvs = levelsets.back();
    auto height     = lvs.front()->height;

    auto recovered_blks      = STORE->GetLevelSetBlksAt(height);
    auto recovered_recs_blks = STORE->GetLevelSetRecsAt(height);
    auto recovered_recs      = STORE->GetLevelSetRecsAt(height, false);

    ASSERT_EQ(recovered_blks.size(), lvs.size());
    ASSERT_EQ(recovered_recs_blks.size(), lvs.size());
    ASSERT_EQ(recovered_recs.size(), lvs.size());

    ASSERT_TRUE(recovered_recs_blks[0]->snapshot);
    ASSERT_FALSE(recovered_recs_blks[0]->snapshot->GetLevelSet().empty());
    ASSERT_TRUE(recovered_recs_blks[0]->snapshot->GetLevelSet()[0].lock());

    for (size_t i = 0; i < lvs.size(); ++i) {
        ASSERT_TRUE(recovered_recs_blks[i]->cblock);
        ASSERT_EQ(*lvs[i]->cblock, *recovered_blks[i]);
        ASSERT_EQ(*lvs[i], *recovered_recs_blks[i]);
        ASSERT_EQ(*lvs[i], *recovered_recs[i]);
    }

    // update records
    NodeRecord copyRec;
    {
        auto rec        = STORE->GetRecord(blocks[0]->cblock->GetHash());
        rec->isRedeemed = NodeRecord::IS_REDEEMED;
        copyRec         = *rec;
    }
    auto newout = STORE->GetRecord(blocks[0]->cblock->GetHash());
    ASSERT_EQ(copyRec, *newout);
}
