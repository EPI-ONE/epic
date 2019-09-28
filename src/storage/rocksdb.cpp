// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rocksdb.h"
#include "file_utils.h"

using namespace rocksdb;

RocksDB::RocksDB(std::string dbPath, std::vector<std::string> columnNames) {
    this->dbpath_ = dbPath;
    // Make directory DBPATH if missing
    if (!CheckDirExist(dbpath_)) {
        MkdirRecursive(dbpath_);
    }

    // Create column families
    std::vector<ColumnFamilyDescriptor> descriptors;
    for (const std::string& columnName : columnNames) {
        ColumnFamilyOptions cOptions;
        if (columnName == kDefaultColumnFamilyName) {
            cOptions.OptimizeForPointLookup(500L);
        }

        descriptors.push_back(ColumnFamilyDescriptor(columnName, cOptions));
    }

    // Set options
    DBOptions dbOptions;
    dbOptions.db_log_dir                     = dbpath_ + "/log";
    dbOptions.create_if_missing              = true;
    dbOptions.create_missing_column_families = true;
    dbOptions.IncreaseParallelism(2);

    std::vector<ColumnFamilyHandle*> handles;

    // Open DB
    if (!DB::Open(dbOptions, dbpath_, descriptors, &handles, &db_).ok()) {
        throw std::string("DB initialization failed");
    }

    // Store handles into a map
    InitHandleMap(handles, columnNames);
    handles.clear();
    descriptors.clear();
}

void RocksDB::InitHandleMap(std::vector<ColumnFamilyHandle*> handles, std::vector<std::string> columnNames) {
    handleMap_.reserve(columnNames.size());
    auto keyIter = columnNames.begin();
    auto valIter = handles.begin();
    while (keyIter != columnNames.end() && valIter != handles.end()) {
        handleMap_[*keyIter] = *valIter;
        ++keyIter;
        ++valIter;
    }

    assert(!handleMap_.empty());
}

RocksDB::~RocksDB() {
    // delete column falimy handles
    for (auto entry = handleMap_.begin(); entry != handleMap_.end(); ++entry) {
        delete entry->second;
    }
    handleMap_.clear();

    delete db_;
}

std::string RocksDB::Get(const std::string& column, const Slice& keySlice) const {
    PinnableSlice valueSlice;
    Status s = db_->Get(ReadOptions(), handleMap_.at(column), keySlice, &valueSlice);
    if (!s.ok()) {
        return "";
    }
    return valueSlice.ToString();
}

std::string RocksDB::Get(const std::string& column, const std::string& key) const {
    return Get(column, Slice(key));
}

bool RocksDB::Delete(const std::string& column, std::string&& key) const {
    return db_->Delete(WriteOptions(), handleMap_.at(column), key).ok();
}


void RocksDB::PrintColumns() const {
    std::vector<std::string> families{};
    DB::ListColumnFamilies(db_->GetOptions(), dbpath_, &families);
    for (auto& ss : families) {
        std::cout << ss << std::endl;
    }
    families.clear();
}
