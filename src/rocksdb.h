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

static const std::vector<std::string> COLUMN_NAMES = {
    rocksdb::kDefaultColumnFamilyName, // the default column family that must have
    "msList", "UTXO", "selfChain", "regKeys", "info"};

class RocksDBStore {
public:
    RocksDBStore() = delete;

    RocksDBStore(std::string dbPath);

    bool Exists(const uint256& blockHash) const;

    std::unique_ptr<Block> GetBlock(const uint256& blockHash) const;

    bool WriteBlock(const BlockPtr& block) const;

    bool WriteBlocks(const std::vector<BlockPtr> blocks) const;

    ~RocksDBStore();

protected:
    mutable std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> handleMap;
    rocksdb::DB* db;
    std::string DBPATH;

    void InitHandleMap(std::vector<rocksdb::ColumnFamilyHandle*> handles);

    const std::string Get(const std::string& column, const rocksdb::Slice& key) const;

    const std::string Get(const std::string& column, const std::string& key) const;

    bool Write(const std::string& column, const rocksdb::Slice& key, const rocksdb::Slice& value) const;

    bool Write(const std::string& column, const std::string& key, const std::string& value) const;

    const void WriteBatch(const std::string& column, const std::map<std::string, std::string>& batch) const;

    void Delete(const std::string& column, const std::string& key) const;
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
