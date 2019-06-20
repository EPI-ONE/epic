#ifndef __SRC_ROCKSDB_H__
#define __SRC_ROCKSDB_H__

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "block.h"
#include "consensus.h"
#include "file_utils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

typedef std::unique_ptr<NodeRecord> StoredRecord;
typedef std::unique_ptr<Block> BlockCache;

static const std::vector<std::string> COLUMN_NAMES = {
    rocksdb::kDefaultColumnFamilyName, // the default column family that must have
    "ms", "utxo", "selfChain", "regKeys", "info"};

class RocksDBStore {
public:
    RocksDBStore() = delete;

    RocksDBStore(std::string dbPath);

    ~RocksDBStore();

    bool Exists(const uint256&) const;

    bool IsMilestone(const uint256&) const;

    StoredRecord GetMsRecordAt(const uint64_t&) const;

    StoredRecord GetRecord(const uint256&) const;

    std::vector<StoredRecord> GetLevelSetAt(const uint64_t&) const;

    std::vector<StoredRecord> GetLevelSet(const uint256&) const;

    size_t GetHeight(const uint256&) const;

    bool WriteRecord(const RecordPtr&) const;

    bool WriteRecords(const std::vector<RecordPtr>&) const;

private:
    std::unordered_map<std::string, rocksdb::ColumnFamilyHandle*> handleMap;
    rocksdb::DB* db;
    std::string DBPATH;
    void InitHandleMap(std::vector<rocksdb::ColumnFamilyHandle*> handles);
    std::vector<uint256> GetLevelSetHashes(const uint64_t& height) const;

protected:
    std::string Get(const std::string& column, const rocksdb::Slice& key) const;

    std::string Get(const std::string& column, const std::string& key) const;

    template <typename K, typename B>
    void GetRecordImpl(const K& key, rocksdb::ColumnFamilyHandle*, std::unique_ptr<B>& result) const;

    bool Write(const std::string& column, const rocksdb::Slice& key, const rocksdb::Slice& value) const;

    bool Write(const std::string& column, const std::string& key, const std::string& value) const;

    bool WriteBatch(const std::string& column, const std::map<std::string, std::string>& batch) const;

    bool Delete(const std::string& column, const std::string& key) const;
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
