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

using std::optional;
using std::pair;
using std::string;

static const std::vector<string> COLUMN_NAMES = {
    rocksdb::kDefaultColumnFamilyName, // the default column family that must have
    "ms",
    "utxo",
    "selfChain",
    "regKeys",
    "info"};

class RocksDBStore {
public:
    RocksDBStore() = delete;
    RocksDBStore(string dbPath);

    ~RocksDBStore();

    bool Exists(const uint256&) const;
    size_t GetHeight(const uint256&) const;
    bool IsMilestone(const uint256&) const;

    /**
     * Gets the milesonte file posisionts at height
     * Returns {ms hash, blk FilePos, rec FilePos}
     */
    optional<pair<FilePos, FilePos>> GetMsPos(const uint64_t& height) const;
    optional<FilePos> GetMsBlockPos(const uint64_t& height) const;

    /**
     * Gets the milesonte file posisionts at height of blk
     * Returns {blk FilePos, rec FilePos}
     */
    optional<pair<FilePos, FilePos>> GetMsPos(const uint256& blk) const;

    /**
     * Gets the file posisionts of the hash.
     * Returns {blk FilePos, rec FilePos}
     */
    optional<pair<FilePos, FilePos>> GetRecordPos(const uint256&) const;

    /**
     * Writes the file offsets of the hash with
     * key = hash, value = {height, blk offset, rec offset}
     */
    bool WriteRecPos(const uint256&, const uint64_t&, const uint32_t&, const uint32_t&) const;
    bool WriteRecPoses(std::vector<uint256>&&,
                       std::vector<uint64_t>&&,
                       std::vector<uint32_t>&&,
                       std::vector<uint32_t>&&) const;

    /**
     * Writes the file offsets of the milestone hash
     * key = ms height, value = {ms hash, ms blk FilePos, ms rec FilePos}
     */
    bool WriteMsPos(const uint64_t&, const uint256&, const FilePos&, const FilePos&) const;

    bool DeleteRecPos(const uint256&) const;

    bool DeleteMsPos(const uint256&) const;

private:
    std::unordered_map<string, rocksdb::ColumnFamilyHandle*> handleMap;
    rocksdb::DB* db;
    string DBPATH;

    void InitHandleMap(std::vector<rocksdb::ColumnFamilyHandle*> handles);
    uint256 GetMsHashAt(const uint64_t& height) const;

    string Get(const string& column, const rocksdb::Slice& key) const;
    string Get(const string& column, const string& key) const;
    bool Delete(const string& column, const string& key) const;

    template <typename K, typename H, typename P1, typename P2>
    bool WritePosImpl(const string& column, const K&, const H&, const P1&, const P2&) const;
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
