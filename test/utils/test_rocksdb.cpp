#include <cstdio>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "rocksdb.h"
#include "test_env.h"

class TestRocksDB : public testing::Test {
public:
    // Shared resource across all test cases
    static std::string prefix;
    static RocksDBStore* db;
    TestFactory fac = EpicTestEnvironment::GetFactory();

protected:
    // One set up before all test cases
    static void SetUpTestCase() {
        // Get the current time and make into a temp file name
        std::ostringstream os;
        os << time(nullptr);
        db = new RocksDBStore(prefix + os.str());
    }

    // One tear down after all test cases
    static void TearDownTestCase() {
        // Remove the temporary db folder
        std::string cmd = "exec rm -r " + prefix;
        system(cmd.c_str());
        delete db;
    }
};

std::string TestRocksDB::prefix = "test_rocks/"; // temporary db folder prefix
RocksDBStore* TestRocksDB::db   = nullptr;

TEST_F(TestRocksDB, single_insertion_and_deletion) {
    // Consturct a milestone file position
    auto msHash     = fac.CreateRandomHash();
    uint32_t height = fac.GetRand();
    FilePos msBlkPos{fac.GetRand() % 10, fac.GetRand() % 100, fac.GetRand()};
    FilePos msRecPos{fac.GetRand() % 10, fac.GetRand() % 100, fac.GetRand()};

    // Construct a normal block file position contained
    // in the same level set as the above milestone
    auto blkHash       = fac.CreateRandomHash();
    uint32_t blkOffset = fac.GetRand();
    uint32_t recOffset = fac.GetRand();
    FilePos blkPos{msBlkPos.nEpoch, msBlkPos.nName, msBlkPos.nOffset + blkOffset};
    FilePos recPos{msRecPos.nEpoch, msRecPos.nName, msRecPos.nOffset + recOffset};

    // Write
    ASSERT_TRUE(db->WriteMsPos(height, msHash, msBlkPos, msRecPos));     // write milestone to "ms" column
    ASSERT_TRUE(db->WriteRecPos(msHash, height, 0, 0));                  // write milestone to default column
    ASSERT_TRUE(db->WriteRecPos(blkHash, height, blkOffset, recOffset)); // write normal block to default column

    // Read
    ASSERT_TRUE(db->IsMilestone(msHash));
    auto msPos1   = *db->GetMsPos(height);      // get milestone positions by height
    auto msPos2   = *db->GetMsPos(blkHash);     // get milestone positions by the normal block hash
    auto msPos3   = *db->GetMsPos(msHash);      // get milestone positions by it's own hash
    auto blkPoses = *db->GetRecordPos(blkHash); // get normal block positions by it's own hash

    ASSERT_EQ(msPos1, msPos2);
    ASSERT_EQ(msPos1, msPos3);

    ASSERT_EQ(msPos1.first, msBlkPos);
    ASSERT_EQ(msPos1.second, msRecPos);

    ASSERT_EQ(blkPoses.first, blkPos);
    ASSERT_EQ(blkPoses.second, recPos);

    // Delete
    db->DeleteRecPos(blkHash);
    ASSERT_FALSE(db->Exists(blkHash));
    ASSERT_EQ(-1, db->GetHeight(blkHash));

    db->DeleteMsPos(msHash);
    ASSERT_FALSE(db->Exists(msHash));
    ASSERT_FALSE(db->IsMilestone(msHash));
}

TEST_F(TestRocksDB, batch_insertion) {
    // Consturct a milestone file position
    auto msHash     = fac.CreateRandomHash();
    uint32_t height = fac.GetRand();
    FilePos msBlkPos{fac.GetRand() % 10, fac.GetRand() % 100, fac.GetRand()};
    FilePos msRecPos{fac.GetRand() % 10, fac.GetRand() % 100, fac.GetRand()};

    ASSERT_TRUE(db->WriteMsPos(height, msHash, msBlkPos, msRecPos));

    // Construct normal block positions in the same level set
    std::vector<uint256> hashes      = {msHash};
    std::vector<uint64_t> heights    = {height};
    std::vector<uint32_t> blkOffsets = {0};
    std::vector<uint32_t> recOffsets = {0};

    std::vector<FilePos> blkPoses = {msBlkPos};
    std::vector<FilePos> recPoses = {msRecPos};

    int size = 100;

    for (int i = 1; i < size; ++i) {
        hashes.push_back(fac.CreateRandomHash());
        heights.push_back(height);
        blkOffsets.push_back(fac.GetRand() % 500 + blkOffsets.back());
        recOffsets.push_back(fac.GetRand() % 50 + recOffsets.back());
        blkPoses.emplace_back(msBlkPos.nEpoch, msBlkPos.nName, msBlkPos.nOffset + blkOffsets.back());
        recPoses.emplace_back(msRecPos.nEpoch, msRecPos.nName, msRecPos.nOffset + recOffsets.back());
    }

    ASSERT_TRUE(db->WriteRecPoses(hashes, heights, blkOffsets, recOffsets));

    for (int i = 1; i < size; ++i) {
        auto pos = *db->GetRecordPos(hashes[i]);
        ASSERT_EQ(blkPoses[i], pos.first);
        ASSERT_EQ(recPoses[i], pos.second);
    }
}

TEST_F(TestRocksDB, utxo) {
    auto index   = fac.GetRand() % 100;
    auto block   = fac.CreateBlock(0, 100);
    UTXOPtr utxo = std::make_shared<UTXO>(block.GetTransaction()->GetOutputs()[index], index);
    auto key     = utxo->GetKey();
    ASSERT_TRUE(db->WriteUTXO(key, utxo));

    auto utxo_fromdb = db->GetUTXO(key);
    ASSERT_EQ(*utxo, *utxo_fromdb);

    ASSERT_TRUE(db->RemoveUTXO(key));
    ASSERT_EQ(nullptr, db->GetUTXO(key));
}

TEST_F(TestRocksDB, reg) {
    constexpr int size = 10;

    RegChange addition;
    for (int i = 0; i < size; ++i) {
        auto h1 = fac.CreateRandomHash();
        auto h2 = fac.CreateRandomHash();
        addition.Create(h1, h2);
    }

    RegChange subtraction;
    for (const auto& e : addition.GetCreated()) {
        subtraction.Remove(e);
    }

    ASSERT_EQ(addition.GetCreated(), subtraction.GetRemoved());
    ASSERT_TRUE(db->UpdateReg(addition));

    for (const auto& e : addition.GetCreated()) {
        ASSERT_EQ(e.second, db->GetLastReg(e.first));
    }

    ASSERT_TRUE(db->UpdateReg(subtraction));

    for (const auto& e : subtraction.GetRemoved()) {
        ASSERT_TRUE(db->GetLastReg(e.first).IsNull());
    }

    ASSERT_TRUE(db->RollBackReg(subtraction));

    for (const auto& e : subtraction.GetRemoved()) {
        ASSERT_EQ(e.second, db->GetLastReg(e.first));
    }

    for (const auto& e : subtraction.GetRemoved()) {
        addition.Remove(e);
    }

    ASSERT_TRUE(addition.GetCreated().empty());
}

TEST_F(TestRocksDB, headheight) {
    for (int i = 0; i < 100; i++) {
        db->WriteHeadHeight(i);
        uint64_t read_height = db->GetHeadHeight();
        ASSERT_EQ(i, read_height);
    }
}