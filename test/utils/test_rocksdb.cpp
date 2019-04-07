#include <gtest/gtest.h>
#include <cstdio>
#include <iostream>
#include <string>
#include "rocksdb.h"

// Temporary db folder prefix
static std::string PREFIX = "test_rocks/";

class TestRocksDB : public testing::Test {
   protected:
    // Shared resource across all test cases
    static RocksDBStore *db;

    // One set up before all test cases
    static void SetUpTestSuite() {
        std::string filename;
        // Get the current time and make into a temp file name
        std::ostringstream os;
        os << time(nullptr);
        filename = PREFIX + os.str();
        db = new RocksDBStore(filename);
    }
    void SetUp() {}
    void TearDown() {}

    // One tear down after all test cases
    static void TearDownTestSuite() {
        // Remove the temporary db folder
        std::string cmd = "exec rm -r " + PREFIX;
        system(cmd.c_str());
        delete db;
        db = NULL;
    }

    // Creates a random string using numbers and alphabet with given length
    const std::string RandomString(const int len) {
        static const char alph[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";
        char s[len];
        for (int i = 0; i < len; ++i) {
            s[i] = alph[rand() % (sizeof(alph) - 1)];
        }
        s[len] = 0;
        std::string result(s);
        return result;
    }
};

RocksDBStore *TestRocksDB::db = NULL;

TEST_F(TestRocksDB, single_insertion_and_deletion) {
    const std::string column = "default";
    std::string key = "a random key";
    std::string value = "a random value";
    // Insert
    db->Write(column, key, value);
    EXPECT_EQ(value, db->Get(column, key));
    // Delete
    db->Delete(column, key);
    EXPECT_EQ("", db->Get(column, key));
}

TEST_F(TestRocksDB, batch_insertion) {
    const std::string column = "default";
    std::map<std::string, std::string> batch = {};
    for (int i = 0; i < 1000; ++i) {
        auto key = RandomString(32);
        auto value = RandomString(21000);
        batch[key] = value;
    }
    db->WriteBatch(column, batch);
    for (auto const &[key, value] : batch) {
        EXPECT_EQ(value, db->Get(column, key));
    }
}
