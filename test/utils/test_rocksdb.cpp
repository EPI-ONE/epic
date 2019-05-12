#include <cstdio>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

#include "rocksdb.h"
#include "test-methods/block-factory.h"

// Temporary db folder prefix
static std::string PREFIX = "test_rocks/";

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

    const void WriteBatch(const std::string& column, const std::map<std::string, std::string>& batch) const {
        RocksDBStore::WriteBatch(column, batch);
    }

    void Delete(const std::string& column, const std::string& key) const {
        RocksDBStore::Delete(column, key);
    }
};

class TestRocksDB : public testing::Test {
public:
    static TestImplRocksDBStore* db;
    static std::vector<RecordPtr> records;
    static std::vector<uint256> keys;
    static size_t size;

protected:
    // Shared resource across all test cases

    // One set up before all test cases
    static void SetUpTestCase() {
        std::string filename;
        // Get the current time and make into a temp file name
        std::ostringstream os;
        os << time(nullptr);
        filename = PREFIX + os.str();
        db       = new TestImplRocksDBStore(filename);

        // Initialize batch blocks and keys
        // blocks.reserve(size);
        // keys.reserve(size);
        for (int i = 0; i < size; ++i) {
            RecordPtr rec = FakeRecordPtr(rand() % 100 + 1, rand() % 100 + 1);
            records[i]  = rec;
            keys[i]    = rec->cBlock->GetHash();
        }
    }

    void SetUp() {}
    void TearDown() {}

    // One tear down after all test cases
    static void TearDownTestCase() {
        // Remove the temporary db folder
        std::string cmd = "exec rm -r " + PREFIX;
        system(cmd.c_str());
        delete db;
        records.clear();
        keys.clear();
    }

    // Creates a random string using numbers and alphabet with given length
    const std::string RandomString(const int len) {
        static const char alph[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";
        char s[len + 1];
        for (int i = 0; i < len; ++i) {
            s[i] = alph[rand() % (sizeof(alph) - 1)];
        }
        s[len] = 0;
        std::string result(s);
        return result;
    }
};

TestImplRocksDBStore* TestRocksDB::db     = nullptr;
size_t TestRocksDB::size                  = 1000;
std::vector<RecordPtr> TestRocksDB::records = std::vector<RecordPtr>(size);
std::vector<uint256> TestRocksDB::keys    = std::vector<uint256>(size);

TEST_F(TestRocksDB, single_insertion_and_deletion) {
    const std::string column = "default";
    std::string key          = "a random key";
    std::string value        = "a random value";
    // Insert
    EXPECT_TRUE(db->Write(column, key, value));
    EXPECT_EQ(value, db->Get(column, key));
    // Delete
    db->Delete(column, key);
    EXPECT_EQ("", db->Get(column, key));
}

TEST_F(TestRocksDB, batch_insertion) {
    const std::string column                 = "default";
    std::map<std::string, std::string> batch = {};
    for (int i = 0; i < 1000; ++i) {
        auto key   = RandomString(32);
        auto value = RandomString(500);
        batch[key] = value;
    }
    db->WriteBatch(column, batch);
    for (auto const& [key, value] : batch) {
        EXPECT_EQ(value, db->Get(column, key));
    }
}

TEST_F(TestRocksDB, write_single_block) {
    RecordPtr rec = FakeRecordPtr(1, 1);
    std::cout << "=======================" << rec->GetOptimalStorageSize();
    EXPECT_TRUE(db->WriteRecord(rec));

    std::unique_ptr<NodeRecord> value = db->GetRecord(rec->cBlock->GetHash());
    EXPECT_EQ(*rec, *value);
}

TEST_F(TestRocksDB, write_batch_blocks) {
    EXPECT_TRUE(db->WriteRecords(records));

    for (int i = 0; i < size; ++i) {
        auto pblock = db->GetRecord(keys[i]);
        EXPECT_EQ(*records[i], *pblock);
    }
}

TEST_F(TestRocksDB, write_blocks_one_by_one) {
    for (int i = 0; i < size; ++i) {
        EXPECT_TRUE(db->WriteRecord(records[i]));
    }

    for (int i = 0; i < size; ++i) {
        auto pblock = db->GetRecord(keys[i]);
        EXPECT_EQ(*records[i], *pblock);
    }
}
