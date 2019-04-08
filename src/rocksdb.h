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
        this->DBPATH = dbPath;
        // Make directory DBPATH if missing
        if (!CheckDirExist(DBPATH)) {
            Mkdir_recursive(DBPATH);
        }

        // Create column families
        vector<ColumnFamilyDescriptor> descriptors;
        for (string columnName : COLUMN_NAMES) {
            ColumnFamilyOptions cOptions;
            if (columnName == kDefaultColumnFamilyName) {
                cOptions.OptimizeForPointLookup(500L);
            }
            descriptors.push_back(ColumnFamilyDescriptor(columnName, cOptions));
        }
        // Set options
        DBOptions dbOptions;
        dbOptions.db_log_dir = DBPATH + "/log";
        dbOptions.create_if_missing = true;
        dbOptions.create_missing_column_families = true;
        dbOptions.IncreaseParallelism(2);
        // Open DB and store handles into a map
        vector<ColumnFamilyHandle*> handles;
        if (!DB::Open(dbOptions, DBPATH, descriptors, &handles, &db).ok()) {
            throw "DB initialization failed";
        }
        initHandleMap(handles);
        handles.clear();
        descriptors.clear();
    }

    const string Get(const string& column, const string& key) {
        string value;
        Status s = db->Get(ReadOptions(), handleMap[column], key, &value);
        if (s.ok()) {
            return value;
        }
        return "";
    }

    const void Write(const string& column, const string& key, const string& value) {
        bool ok = db->Put(WriteOptions(), handleMap[column], key, value).ok();
        assert(ok);
    }

    const void WriteBatch(const string& column, const map<string, string>& batch) {
        class WriteBatch wb;
        for (auto const& [key, value] : batch) {
            wb.Put(handleMap[column], key, value);
        }
        bool ok = db->Write(WriteOptions(), &wb).ok();
        assert(ok);
    }

    const void Delete(const string& column, const string& key) {
        bool ok = db->Delete(WriteOptions(), handleMap[column], key).ok();
        assert(ok);
    }

    ~RocksDBStore() {
        for (auto entry = handleMap.begin(); entry != handleMap.end(); ++entry) {
            delete entry->second;
        }
        handleMap.clear();
        delete db;
    }

   private:
    unordered_map<string, ColumnFamilyHandle*> handleMap;
    DB* db;
    string DBPATH;

    void initHandleMap(vector<ColumnFamilyHandle*> handles) {
        handleMap.reserve(COLUMN_NAMES.size());
        auto keyIter = COLUMN_NAMES.begin();
        auto valIter = handles.begin();
        while (keyIter != COLUMN_NAMES.end() && valIter != handles.end()) {
            handleMap[*keyIter] = *valIter;
            ++keyIter;
            ++valIter;
        }
        assert(!handleMap.empty());
    }
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
