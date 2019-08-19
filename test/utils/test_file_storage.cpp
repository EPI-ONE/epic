#include <gtest/gtest.h>

#include "caterpillar.h"
#include "file_utils.h"
#include "test_env.h"

#include <fstream>
#include <iostream>
#include <string>

class TestFileStorage : public testing::Test {
public:
    TestFactory fac    = EpicTestEnvironment::GetFactory();
    std::string prefix = "test_file_store/";

    void SetUp() override {
        file::SetDataDirPrefix(prefix);
    }

    void TearDown() override {
        std::string cmd = "rm -r " + prefix;
        system(cmd.c_str());
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
    EXPECT_EQ(writer.GetOffset(), 0);
    writer << blk;
    EXPECT_EQ(writer.GetOffset(), blksize);
    writer << rec;
    EXPECT_EQ(writer.GetOffset(), blksize + recsize);
    writer.Close();

    // reading
    FileReader reader{file::FileType::BLK, fpos};
    Block blk1{};
    EXPECT_EQ(reader.GetOffset(), 0);
    reader >> blk1;
    EXPECT_EQ(reader.GetOffset(), blksize);
    EXPECT_EQ(blk, blk1);

    NodeRecord rec1{};
    reader >> rec1;
    EXPECT_EQ(reader.GetOffset(), blksize + recsize);
    EXPECT_EQ(rec, rec1);
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
    EXPECT_EQ(reader2.GetOffset(), blksize + recsize);
    EXPECT_EQ(rec, rec2);
    reader2.Close();
}

TEST_F(TestFileStorage, cat_store_and_get_records_and_get_lvs) {
    std::ostringstream os;
    os << time(nullptr);
    CAT = std::make_unique<Caterpillar>(prefix + os.str());
    CAT->SetFileCapacities(8000, 2);

    std::vector<RecordPtr> blocks;
    std::vector<std::vector<RecordPtr>> levelsets;

    constexpr int nLvs = 20;

    // Consturct level sets
    for (int i = 0; i < nLvs; ++i) {
        int size = fac.GetRand() % 10;

        std::vector<RecordPtr> lvs;
        lvs.reserve(size);

        // Construct milestone
        auto ms = fac.CreateRecordPtr(1, 1, true);
        fac.CreateChainStatePtr(GENESIS_RECORD.snapshot, ms);
        ms->isMilestone      = true;
        ms->snapshot->height = i;
        ms->height           = i;

        lvs.push_back(ms);
        blocks.push_back(ms);

        // Construct blocks in the level set
        for (int i = 1; i < size; ++i) {
            auto b         = fac.CreateRecordPtr(fac.GetRand() % 10, fac.GetRand() % 10, true);
            b->isMilestone = false;
            b->height      = ms->height;
            lvs.push_back(b);
            blocks.emplace_back(b);
        }

        ASSERT_TRUE(CAT->StoreLevelSet(lvs));
        levelsets.emplace_back(std::move(lvs));
    }

    // Inspect inserted records
    for (size_t i = 0; i < blocks.size(); ++i) {
        const auto& h = blocks[i]->cblock->GetHash();

        // without cblock
        auto rec = CAT->GetRecord(h, false);
        ASSERT_TRUE(rec);
        ASSERT_FALSE(rec->cblock);
        ASSERT_EQ(*blocks[i], *rec);

        // with cblock
        auto rec_blk = CAT->GetRecord(h);
        ASSERT_TRUE(rec_blk->cblock);
        ASSERT_EQ(*blocks[i], *rec_blk);
    }

    // Recover level sets in blocks in batch
    auto vs_blks = CAT->GetRawLevelSetBetween(0, nLvs - 1);
    ASSERT_FALSE(vs_blks.empty());

    for (size_t i = 0; i < blocks.size(); ++i) {
        Block recovered_blk(vs_blks);
        ASSERT_EQ(*blocks[i]->cblock, recovered_blk);
    }

    // Recover level sets in recs in batch
    auto vs_recs = CAT->GetRawLevelSetBetween(0, nLvs - 1, file::FileType::REC);
    ASSERT_FALSE(vs_recs.empty());

    for (size_t i = 0; i < blocks.size(); ++i) {
        NodeRecord recovered_rec(vs_recs);
        ASSERT_EQ(*blocks[i], recovered_rec);
    }

    // Recover single level set
    const auto& lvs = levelsets.back();
    auto height     = lvs.front()->height;

    auto recovered_blks      = CAT->GetLevelSetBlksAt(height);
    auto recovered_recs_blks = CAT->GetLevelSetRecsAt(height);
    auto recovered_recs      = CAT->GetLevelSetRecsAt(height, false);

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
        auto rec        = CAT->GetRecord(blocks[0]->cblock->GetHash());
        rec->isRedeemed = NodeRecord::IS_REDEEMED;
        copyRec         = *rec;
    }
    auto newout = CAT->GetRecord(blocks[0]->cblock->GetHash());
    EXPECT_EQ(copyRec, *newout);
    CAT.reset();
}
