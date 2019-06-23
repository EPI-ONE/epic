#include <cstdio>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "rocksdb.h"
#include "test_factory.h"

class TestImplRocksDBStore : public RocksDBStore {
public:
    TestImplRocksDBStore(std::string dbPath) : RocksDBStore(dbPath) {}

    const rocksdb::Slice Get(const std::string& column, const rocksdb::Slice& key) const {
        return RocksDBStore::Get(column, key);
    }

    bool Write(const std::string& column, const rocksdb::Slice& key, const rocksdb::Slice& value) const {
        return RocksDBStore::Write(column, key, value);
    }

    const std::string Get(const std::string& column, const std::string& key) const {
        return RocksDBStore::Get(column, key);
    }

    bool Write(const std::string& column, const std::string& key, const std::string& value) const {
        return RocksDBStore::Write(column, key, value);
    }

    void WriteBatch(const std::string& column, const std::map<std::string, std::string>& batch) const {
        RocksDBStore::WriteBatch(column, batch);
    }

    void Delete(const std::string& column, const std::string& key) const {
        RocksDBStore::Delete(column, key);
    }
};

class TestRocksDB : public testing::Test {
public:
    static std::string prefix;
    static TestImplRocksDBStore* db;
    //static std::vector<RecordPtr> records;
    static std::vector<uint256> keys;
    static size_t size;
    static TestFactory* fac;

protected:
    // Shared resource across all test cases

    // One set up before all test cases
    static void SetUpTestCase() {
        // Get the current time and make into a temp file name
        std::ostringstream os;
        os << time(nullptr);
        db  = new TestImplRocksDBStore(prefix + os.str());
        fac = new TestFactory();
        // Initialize batch blocks and keys
        for (size_t i = 0; i < size; ++i) {
            auto rec   = fac->CreateRecordPtr(fac->GetRand() % 100 + 1, fac->GetRand() % 100 + 1, true);
            //records[i] = rec;
            keys[i]    = rec->cblock->GetHash();
        }
    }

    void SetUp() {}
    void TearDown() {}

    // One tear down after all test cases
    static void TearDownTestCase() {
        // Remove the temporary db folder
        std::string cmd = "exec rm -r " + prefix;
        system(cmd.c_str());
        delete db;
        delete fac;
        //records.clear();
        keys.clear();
    }
};

std::string TestRocksDB::prefix       = "test_rocks/"; // temporary db folder prefix
TestImplRocksDBStore* TestRocksDB::db = nullptr;
TestFactory* TestRocksDB::fac         = nullptr;
size_t TestRocksDB::size              = 100;
//auto TestRocksDB::records             = std::vector<RecordPtr>(size);
auto TestRocksDB::keys                = std::vector<uint256>(size);

TEST_F(TestRocksDB, single_insertion_and_deletion) {
    const auto column = "default";
    std::string key   = "a random key";
    std::string value = "a random value";
    // Insert
    ASSERT_TRUE(db->Write(column, key, value));
    ASSERT_EQ(value, db->Get(column, key));
    // Delete
    db->Delete(column, key);
    ASSERT_EQ("", db->Get(column, key));
}

TEST_F(TestRocksDB, batch_insertion) {
    const auto column = "default";
    std::map<std::string, std::string> batch;
    for (int i = 0; i < 1000; ++i) {
        auto key   = fac->GetRandomString(32);
        auto value = fac->GetRandomString(500);
        batch[key] = value;
    }
    db->WriteBatch(column, batch);
    for (auto const& [key, value] : batch) {
        ASSERT_EQ(value, db->Get(column, key));
    }
}

/*TEST_F(TestRocksDB, write_single_block) {
    RecordPtr rec = fac->CreateRecordPtr(1, 1, true);
    ASSERT_TRUE(db->WriteRecord(rec));

    std::unique_ptr<NodeRecord> value = db->GetRecord(rec->cblock->GetHash());
    ASSERT_EQ(*rec, *value);
}

TEST_F(TestRocksDB, write_batch_blocks) {
    ASSERT_TRUE(db->WriteRecords(records));

    for (size_t i = 0; i < size; ++i) {
        auto pblock = db->GetRecord(keys[i]);
        ASSERT_EQ(*records[i], *pblock);
    }
}

TEST_F(TestRocksDB, write_blocks_one_by_one) {
    for (size_t i = 0; i < size; ++i) {
        ASSERT_TRUE(db->WriteRecord(records[i]));
    }

    for (size_t i = 0; i < size; ++i) {
        auto pblock = db->GetRecord(keys[i]);
        ASSERT_EQ(*records[i], *pblock);
    }
}*/
