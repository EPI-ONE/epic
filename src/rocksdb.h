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

static const std::vector<std::string> COLUMN_NAMES = {
    kDefaultColumnFamilyName, // the default column family that must have
    "msList", "UTXO", "selfChain", "regKeys", "info"};

class RocksDBStore {
public:
    RocksDBStore() = default;

    RocksDBStore(std::string dbPath);

    bool Exists(const uint256& blockHash) const;

    bool GetBlock(const uint256& blockHash, BlockPtr& block) const;

    bool WriteBlock(const BlockPtr& block) const;

    bool WriteBlocks(const std::vector<BlockPtr> blocks) const;

    ~RocksDBStore();

private:
    mutable std::unordered_map<std::string, ColumnFamilyHandle*> handleMap;
    DB* db;
    std::string DBPATH;

    void initHandleMap(std::vector<ColumnFamilyHandle*> handles);

    const Slice Get(const std::string& column, const Slice& key) const;

    const std::string Get(const std::string& column, const std::string& key) const;

    bool Write(const std::string& column, const Slice& key, const Slice& value) const;

    bool Write(const std::string& column, const std::string& key, const std::string& value) const;

    void Delete(const std::string& column, const std::string& key) const;
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
