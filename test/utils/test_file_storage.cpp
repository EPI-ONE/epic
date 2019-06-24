#include <gtest/gtest.h>

#include "caterpillar.h"
#include "file_utils.h"
#include "test_env.h"

#include <fstream>
#include <iostream>
#include <string>

class TestFileStorage : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
    static std::string prefix;

    static void SetUpTestCase() {
        file::SetDataDirPrefix(prefix);
    }

    static void TearDownTestCase() {
        std::string cmd = "rm -r " + prefix;
        system(cmd.c_str());
    }
};

std::string TestFileStorage::prefix = "test_file_store/";

TEST_F(TestFileStorage, basic_read_write) {
    auto blk = fac.CreateBlock();
    blk.Solve();
    NodeRecord rec{blk};
    FilePos fpos{0, 0, 0};

    {
        FileWriter writer{file::FileType::BLK, fpos};
        EXPECT_EQ(writer.GetOffset(), 0);
        writer << blk;
        EXPECT_EQ(writer.GetOffset(), blk.GetOptimalEncodingSize());
        writer << rec;
        EXPECT_EQ(writer.GetOffset(), blk.GetOptimalEncodingSize() + rec.GetOptimalStorageSize());
    }

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

// TEST_F(TestFileStorage, multiple_read_write) {
// auto blk1 = fac.CreateBlock(true);
// auto blk2 = fac.CreateBlock(2, 3, true);
// blk1.Solve();
// blk2.Solve();

// FilePos pos1{0,1,0};
// FilePos pos2{0,1,0};
// pos2.nOffset = blk1.GetOptimalEncodingSize();

//// multiple writing
//{
// FileWriter writer1(file::FileType::BLK, pos1);
// FileWriter writer2(file::FileType::BLK, pos2);

// EXPECT_EQ(writer1.GetFileName(), writer2.GetFileName());

// writer2 << blk2;
// writer1 << blk1;

// EXPECT_EQ(writer1.GetOffset(), blk1.GetOptimalEncodingSize());
// EXPECT_EQ(writer2.GetOffset(), blk1.GetOptimalEncodingSize() + blk2.GetOptimalEncodingSize());
//}

//// multiple reading
// FileReader reader1{file::FileType::BLK, pos1};
// FileReader reader2{file::FileType::BLK, pos2};

// Block deser_blk1, deser_blk2;
// reader2 >> deser_blk2;
// reader1 >> deser_blk1;

// std::cout << std::to_string(blk1) << std::endl;
// std::cout << std::to_string(deser_blk1) << std::endl;
// EXPECT_EQ(blk1, deser_blk1);

// std::cout << std::to_string(blk2) << std::endl;
// std::cout << std::to_string(deser_blk2) << std::endl;
// EXPECT_EQ(blk2, deser_blk2);
//}

TEST_F(TestFileStorage, cat_store_and_get_records_and_get_lvs) {
    std::ostringstream os;
    os << time(nullptr);
    CAT = std::make_unique<Caterpillar>(prefix + os.str());
    CAT->SetFileCapacities(8000, 2);

    std::vector<RecordPtr> blocks;

    int nLvs = 20;

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
        ASSERT_EQ(*blocks[i], *blk);
    }

    // Recover level sets in batch
    VStream vs = CAT->GetRawLevelSetBetween(0, nLvs - 1);

    for (size_t i = 0; i < blocks.size(); ++i) {
        Block recovered(vs);
        ASSERT_EQ(*blocks[i]->cblock, recovered);
    }

    CAT.reset();
}
