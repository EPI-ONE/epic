// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_ROCKSDB_H
#define EPIC_ROCKSDB_H

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

class RocksDB {
public:
    RocksDB()               = delete;
    RocksDB(const RocksDB&) = delete;

    virtual ~RocksDB();

protected:
    std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> handleMap_;
    rocksdb::DB* db_;
    std::string dbpath_;
    bool DeleteColumn(const std::string& column);
    bool CreateColumn(const std::string& column);
    explicit RocksDB(std::string dbPath, std::vector<std::string> columnNames);
    void InitHandleMap(std::vector<rocksdb::ColumnFamilyHandle*> handles, std::vector<std::string> columnNames);
    std::string Get(const std::string& column, const rocksdb::Slice& key) const;
    std::string Get(const std::string& column, const std::string& key) const;
    bool Delete(const std::string& column, std::string&& key) const;
    void PrintColumns() const;
};

#endif // EPIC_ROCKSDB_H
