// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rocksdb.h"
#include "file_utils.h"
#include "spdlog/spdlog.h"

using namespace rocksdb;

RocksDB::RocksDB(std::string dbPath, std::vector<std::string> columnNames) {
    dbpath_ = dbPath;
    // Make directory DBPATH if missing
    if (!CheckDirExist(dbpath_)) {
        spdlog::trace("Creating a new database...");
        MkdirRecursive(dbpath_);
    } else {
        spdlog::trace("Loading an old database from {}...", dbpath_);
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
    spdlog::trace("Rocksdb is successfully initialized");
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
    spdlog::trace("Destructing rocksdb");
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

bool RocksDB::DeleteColumn(const std::string& column) {
    auto it = handleMap_.find(column);
    if (it == handleMap_.end()) {
        spdlog::error("Invalid column name {}", column);
        return false;
    }
    db_->DropColumnFamily(it->second);
    auto status = db_->DestroyColumnFamilyHandle(it->second);
    handleMap_.erase(it);
    if (status.ok()) {
        spdlog::info("Deleted the column {} from DB", column);
        return true;
    } else {
        spdlog::error("Failed to delete the column {}", column);
        return false;
    }
}

bool RocksDB::CreateColumn(const std::string& column) {
    if (handleMap_.find(column) != handleMap_.end()) {
        spdlog::error("Column {} exists in the DB now", column);
        return false;
    }
    ColumnFamilyHandle* handle;
    ColumnFamilyOptions options;
    auto status = db_->CreateColumnFamily(options, column, &handle);
    if (status.ok()) {
        spdlog::info("Created column {}", column);
        handleMap_.insert_or_assign(column, handle);
        return true;
    } else {
        spdlog::error("Failed to create column {}", column);
        return false;
    }
}

void RocksDB::PrintColumns() const {
    std::vector<std::string> families{};
    DB::ListColumnFamilies(db_->GetOptions(), dbpath_, &families);
    for (auto& ss : families) {
        std::cout << ss << std::endl;
    }
    families.clear();
}
