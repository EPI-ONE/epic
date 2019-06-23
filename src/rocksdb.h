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
using std::tuple;

static const std::vector<string> COLUMN_NAMES = {
    rocksdb::kDefaultColumnFamilyName, // (key) block hash
                                       // (value) {height, blk offset, ms offset}
                                       // Note: offsets are relative to the offsets of
                                       // the milestone contained in the same level set

    "ms", // (key) level set height
          // (value) {blk FilePos, rec FilePos}

    "utxo", // (key) outpoint hash ^ outpoint index
            // (value) utxo

    "info" // Stores necessary info to recover the system,
           // e.g., lastest ms head in db
};

class RocksDBStore {
public:
    RocksDBStore()                    = delete;
    RocksDBStore(const RocksDBStore&) = delete;
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
     * Writes the file offsets of the milestone hash
     * key = ms height, value = {ms hash, ms blk FilePos, ms rec FilePos}
     */
    bool WriteMsPos(const uint64_t&, const uint256&, const FilePos&, const FilePos&) const;

    /**
     * Writes the file offsets of the hash with
     * key = hash, value = {height, blk offset, rec offset}
     */
    bool WriteRecPos(const uint256&, const uint64_t&, const uint32_t&, const uint32_t&) const;
    bool WriteRecPoses(const std::vector<uint256>&,
                       const std::vector<uint64_t>&,
                       const std::vector<uint32_t>&,
                       const std::vector<uint32_t>&) const;

    bool DeleteRecPos(const uint256&) const;
    bool DeleteMsPos(const uint256&) const;

private:
    std::unordered_map<string, rocksdb::ColumnFamilyHandle*> handleMap;
    rocksdb::DB* db;
    string DBPATH;

    void InitHandleMap(std::vector<rocksdb::ColumnFamilyHandle*> handles);
    uint256 GetMsHashAt(const uint64_t& height) const;
    optional<tuple<uint64_t, uint32_t, uint32_t>> GetRecordOffsets(const uint256&) const;

    string Get(const string& column, const rocksdb::Slice& key) const;
    string Get(const string& column, const string& key) const;
    bool Delete(const string& column, const string& key) const;

    template <typename K, typename H, typename P1, typename P2>
    bool WritePosImpl(const string& column, const K&, const H&, const P1&, const P2&) const;
};

#endif /* ifndef __SRC_ROCKSDB_H__ */
