// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __SRC_DB_WRAPPER__
#define __SRC_DB_WRAPPER__

#include <rocksdb/db.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>

class DBWrapper {
public:
    DBWrapper()                 = delete;
    DBWrapper(const DBWrapper&) = delete;

    virtual ~DBWrapper();

protected:
    std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> handleMap_;
    rocksdb::DB* db_;
    std::string dbpath_;

    explicit DBWrapper(std::string dbPath, std::vector<std::string> columnNames);

    void InitHandleMap(std::vector<rocksdb::ColumnFamilyHandle*> handles, std::vector<std::string> columnNames);

    std::string Get(const std::string& column, const rocksdb::Slice& key) const;
    std::string Get(const std::string& column, const std::string& key) const;
    bool Delete(const std::string& column, std::string&& key) const;

    void PrintColumns() const;
};

#endif // __SRC_DB_WRAPPER__
