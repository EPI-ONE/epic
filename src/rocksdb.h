#ifndef __SRC_ROCKSDB_H__
#define __SRC_ROCKSDB_H__

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "block.h"
#include "file_utils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

using namespace rocksdb;
using namespace std;

static const vector<string> COLUMN_NAMES = {
    kDefaultColumnFamilyName, "msList", "UTXO", "selfChain", "regKeys", "info"};

class RocksDBStore {
public:
    RocksDBStore(string dbPath) {
        this->DBPATH_ = dbPath;
        // Make directory DBPATH_ if missing
        if (!CheckDirExist(DBPATH_)) {
            Mkdir_recursive(DBPATH_);
        }

        // Create column families
        vector<ColumnFamilyDescriptor> descriptors;
        for (const string& columnName : COLUMN_NAMES) {
            ColumnFamilyOptions cOptions;
            if (columnName == kDefaultColumnFamilyName) {
                cOptions.OptimizeForPointLookup(500L);
            }
            descriptors.push_back(ColumnFamilyDescriptor(columnName, cOptions));
        }

        // Set options
        DBOptions dbOptions;
        dbOptions.db_log_dir = DBPATH_ + "/log";
        dbOptions.create_if_missing = true;
        dbOptions.create_missing_column_families = true;
        dbOptions.IncreaseParallelism(2);
        // Open DB and store handles into a map
        vector<ColumnFamilyHandle*> handles;

        if (!DB::Open(dbOptions, DBPATH_, descriptors, &handles, &db_).ok()) {
            throw "DB initialization failed";
        }

        initHandleMap(handles);
        handles.clear();
        descriptors.clear();
    }

    const string Get(const string& column, const string& key) {
        string value;
        Status s = db_->Get(ReadOptions(), handleMap_[column], key, &value);

        if (s.ok()) {
            return value;
        }

        return "";
    }

    const void Write(const string& column, const string& key, const string& value) {
        bool ok = db_->Put(WriteOptions(), handleMap_[column], key, value).ok();
        assert(ok);
    }

    const void WriteBatch(const string& column, const map<string, string>& batch) {
        class WriteBatch wb;
        for (auto const& [key, value] : batch) {
            wb.Put(handleMap_[column], key, value);
        }

        bool ok = db_->Write(WriteOptions(), &wb).ok();
        assert(ok);
    }

    const void Delete(const string& column, const string& key) {
        bool ok = db_->Delete(WriteOptions(), handleMap_[column], key).ok();
        assert(ok);
    }

    ~RocksDBStore() {
        for (auto entry = handleMap_.begin(); entry != handleMap_.end(); ++entry) {
            delete entry->second;
        }

        handleMap_.clear();
        delete db_;
    }

private:
    unordered_map<string, ColumnFamilyHandle*> handleMap_;
    DB* db_;
    string DBPATH_;

    void initHandleMap(vector<ColumnFamilyHandle*> handles) {
        handleMap_.reserve(COLUMN_NAMES.size());
        auto keyIter = COLUMN_NAMES.begin();
        auto valIter = handles.begin();

        while (keyIter != COLUMN_NAMES.end() && valIter != handles.end()) {
            handleMap_[*keyIter] = *valIter;
            ++keyIter;
            ++valIter;
        }

        assert(!handleMap_.empty());
    }
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
