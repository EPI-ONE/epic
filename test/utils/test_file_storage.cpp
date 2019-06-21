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

TEST_F(TestFileStorage, learning) {
    const std::string filepath = "testdata/test.txt";
    std::ofstream ofbuf{filepath, std::ostream::out | std::ofstream::trunc | std::ofstream::binary};
    EXPECT_TRUE(CheckFileExist(filepath));
    EXPECT_EQ(ofbuf.tellp(), 0);
    auto blk = fac.CreateBlock();
    blk.Solve();
    ::Serialize(ofbuf, blk);

    EXPECT_EQ(ofbuf.tellp(), blk.GetOptimalEncodingSize());
    ofbuf.close();

    std::ofstream ofbuf1{filepath, std::ostream::out | std::ofstream::binary};
    ofbuf1.seekp(0, std::ios::beg);
    //ofbuf1.seekp(blk.GetOptimalEncodingSize());
    auto blk1 = fac.CreateBlock(3,5);
    blk1.Solve();
    ::Serialize(ofbuf1, blk1);
    //EXPECT_EQ(ofbuf1.tellp(), blk.GetOptimalEncodingSize() + blk1.GetOptimalEncodingSize());
    EXPECT_EQ(ofbuf1.tellp(), blk1.GetOptimalEncodingSize());
    ofbuf1.close();

    std::ifstream inbuf{"test.txt", std::ifstream::in | std::ifstream::binary};
    inbuf.seekg(0, std::ios::beg);
    Block des_blk{};
    EXPECT_EQ(inbuf.tellg(), 0);
    ::Deserialize(inbuf, des_blk);
    EXPECT_EQ(inbuf.tellg(), blk1.GetOptimalEncodingSize());
    EXPECT_EQ(blk1, des_blk);
}

TEST_F(TestFileStorage, basic_read_write) {
    FilePos fpos{0, 0, 0};
    FileWriter writer{file::FileType::BLK, fpos};
    //std::cout << writer.GetFileName() << "\n";
    /*try {
        FileWriter writer{file::FileType::BLK, fpos};
    } catch (std::string& str) {
        std::cout << str << std::endl;
    }*/

    /*EXPECT_EQ(writer.GetOffset(), 0);

    auto blk = fac.CreateBlock();
    blk.Solve();
    writer << blk;

    EXPECT_EQ(writer.GetOffset(), blk.GetOptimalEncodingSize());

    FileReader reader{file::FileType::BLK, fpos};
    Block blk1{};

    reader >> blk1;

    EXPECT_EQ(reader.GetOffset(), blk.GetOptimalEncodingSize());
    EXPECT_EQ(blk, blk1);*/
}
