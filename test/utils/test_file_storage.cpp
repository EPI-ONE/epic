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
    FilePos fpos{100000, 0, 0};

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

TEST_F(TestFileStorage, multiple_read_write) {
    auto blk1 = fac.CreateBlock(true);
    auto blk2 = fac.CreateBlock(2, 3, true);
    blk1.Solve();
    blk2.Solve();

    FilePos pos1{100000, 1, 0};
    FilePos pos2{100000, 1, 0};
    pos2.nOffset = blk1.GetOptimalEncodingSize();

    // multiple writing
    {
        FileWriter writer1(file::FileType::BLK, pos1);
        FileWriter writer2(file::FileType::BLK, pos2);

        EXPECT_EQ(writer1.GetFileName(), writer2.GetFileName());

        writer2 << blk2;
        writer1 << blk1;

        EXPECT_EQ(writer1.GetOffset(), blk1.GetOptimalEncodingSize());
        EXPECT_EQ(writer2.GetOffset(), blk1.GetOptimalEncodingSize() + blk2.GetOptimalEncodingSize());
    }

    // multiple reading
    FileReader reader1{file::FileType::BLK, pos1};
    FileReader reader2{file::FileType::BLK, pos2};

    Block deser_blk1, deser_blk2;
    reader2 >> deser_blk2;
    reader1 >> deser_blk1;

    EXPECT_EQ(blk1, deser_blk1);
    EXPECT_EQ(blk2, deser_blk2);
}

TEST_F(TestFileStorage, caterpillar_api) {
    std::ostringstream os;
    os << time(nullptr);
    CAT = std::make_unique<Caterpillar>(prefix + os.str());
    CAT->SetFileCapacities(3000, 2);

    // Consturct two consecutive milestones
    auto ms1 = fac.CreateRecordPtr(1, 1, true);
    fac.CreateChainStatePtr(GENESIS_RECORD.snapshot, ms1);
    ms1->isMilestone = true;

    auto ms2 = fac.CreateRecordPtr(1, 1, true);
    fac.CreateChainStatePtr(ms1->snapshot, ms2);
    ms2->isMilestone = true;

    int size = 10;

    // Construct the level set of ms1
    std::vector<RecordPtr> lvs  = {ms1};
    std::vector<RecordPtr> lvs2 = {ms2};
    for (int i = 1; i < size; ++i) {
        lvs.emplace_back(fac.CreateRecordPtr(fac.GetRand() % 10, fac.GetRand() % 10, true));
        lvs.back()->isMilestone = false;
    }

    ASSERT_TRUE(CAT->StoreRecords(lvs));
    ASSERT_TRUE(CAT->StoreRecords(lvs2));

    for (int i = 0; i < size; ++i) {
        auto blk = CAT->GetRecord(lvs[i]->cblock->GetHash());
        ASSERT_EQ(*blk, *lvs[i]);
    }

    CAT.reset();
}
