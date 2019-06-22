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
    std::string name{};
    FilePos fpos{0, 0, 0};

    {
        FileWriter writer{file::FileType::BLK, fpos};
        name = writer.GetFileName();
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
