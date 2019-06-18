#include <gtest/gtest.h>

#include "file_utils.h"
#include "test_env.h"

#include <fstream>
#include <iostream>
#include <string>


class TestFileStorage : public testing::Test {
public:
    TestFactory fac = EpicTestEnvironment::GetFactory();
};

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

TEST_F(TestFileStorage, multiple_read_write) {
    auto blk1 = fac.CreateBlock(true);
    auto blk2 = fac.CreateBlock(2, 3, true);
    blk1.Solve();
    blk2.Solve();

    FilePos pos1{0,1,0};
    FilePos pos2{0,1,0};
    pos2.nOffset = blk1.GetOptimalEncodingSize();

    //multiple writing
    {
        FileWriter writer1(file::FileType::BLK, pos1);
        FileWriter writer2(file::FileType::BLK, pos2);

        EXPECT_EQ(writer1.GetFileName(), writer2.GetFileName());

        writer2 << blk2;
        writer1 << blk1; 

        EXPECT_EQ(writer1.GetOffset(), blk1.GetOptimalEncodingSize());
        EXPECT_EQ(writer2.GetOffset(), blk1.GetOptimalEncodingSize() + blk2.GetOptimalEncodingSize());
    }

    //multiple reading
    FileReader reader1{file::FileType::BLK, pos1};
    FileReader reader2{file::FileType::BLK, pos2};

    Block deser_blk1, deser_blk2;
    reader2 >> deser_blk2;
    reader1 >> deser_blk1;

    EXPECT_EQ(blk1, deser_blk1);
    EXPECT_EQ(blk2, deser_blk2);
}
