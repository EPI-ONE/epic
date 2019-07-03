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
    auto blk = fac.CreateBlock();
    blk.Solve();
    NodeRecord rec{blk};
    FilePos fpos{0, 0, 0};

    FileWriter writer{file::FileType::BLK, fpos};
    EXPECT_EQ(writer.GetOffset(), 0);
    writer << blk;
    EXPECT_EQ(writer.GetOffset(), blk.GetOptimalEncodingSize());
    writer << rec;
    EXPECT_EQ(writer.GetOffset(), blk.GetOptimalEncodingSize() + rec.GetOptimalStorageSize());
    writer.Close();

    FileReader reader{file::FileType::BLK, fpos};
    Block blk1{};
    EXPECT_EQ(reader.GetOffset(), 0);
    reader >> blk1;
    EXPECT_EQ(reader.GetOffset(), blk.GetOptimalEncodingSize());
    EXPECT_EQ(blk, blk1);

    NodeRecord rec1{};
    reader >> rec1;
    EXPECT_EQ(reader.GetOffset(), blk.GetOptimalEncodingSize() + rec.GetOptimalStorageSize());
    EXPECT_EQ(rec, rec1);
}

TEST_F(TestFileStorage, cat_store_and_get_records_and_get_lvs) {
    std::ostringstream os;
    os << time(nullptr);
    CAT = std::make_unique<Caterpillar>(prefix + os.str());
    CAT->SetFileCapacities(8000, 2);

    std::vector<RecordPtr> blocks;

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

        ASSERT_TRUE(CAT->StoreRecords(lvs));
    }

    // Inspect inserted records
    for (size_t i = 0; i < blocks.size(); ++i) {
        auto blk = CAT->GetRecord(blocks[i]->cblock->GetHash());
        ASSERT_NE(blk->cblock, nullptr);
        ASSERT_EQ(*blocks[i], *blk);
    }

    // Recover level sets in batch
    auto pvs = CAT->GetRawLevelSetBetween(0, nLvs - 1);

    for (size_t i = 0; i < blocks.size(); ++i) {
        Block recovered(*pvs);
        ASSERT_EQ(*blocks[i]->cblock, recovered);
    }

    CAT.reset();
}
