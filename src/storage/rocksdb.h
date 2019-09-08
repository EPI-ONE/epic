// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_ROCKSDB_H
#define EPIC_ROCKSDB_H

#include "db_wrapper.h"
#include "file_utils.h"
#include "vertex.h"

#include <string>
#include <vector>

class RocksDBStore : public DBWrapper {
public:
    explicit RocksDBStore(std::string dbPath);

    bool Exists(const uint256&) const;
    size_t GetHeight(const uint256&) const;
    bool IsMilestone(const uint256&) const;

    /**
     * Gets the milesonte file posisionts at height
     * Returns {ms hash, blk FilePos, vtx FilePos}
     */
    std::optional<std::pair<FilePos, FilePos>> GetMsPos(const uint64_t& height) const;
    std::optional<FilePos> GetMsBlockPos(const uint64_t& height) const;

    /**
     * Gets the milesonte file posisionts at height of blk
     * Returns {blk FilePos, vtx FilePos}
     */
    std::optional<std::pair<FilePos, FilePos>> GetMsPos(const uint256& blk) const;

    /**
     * Gets the file posisionts of the hash.
     * Returns {blk FilePos, vtx FilePos}
     */
    std::optional<std::pair<FilePos, FilePos>> GetVertexPos(const uint256&) const;

    /**
     * Writes the file offsets of the milestone hash
     * key = ms height, value = {ms hash, ms blk FilePos, ms vtx FilePos}
     */
    bool WriteMsPos(const uint64_t&, const uint256&, const FilePos&, const FilePos&) const;

    /**
     * Writes the file offsets of the hash with
     * key = hash, value = {height, blk offset, vtx offset}
     */
    bool WriteVtxPos(const uint256&, const uint64_t&, const uint32_t&, const uint32_t&) const;
    bool WriteVtxPoses(const std::vector<uint256>&,
                       const std::vector<uint64_t>&,
                       const std::vector<uint32_t>&,
                       const std::vector<uint32_t>&) const;

    bool DeleteVtxPos(const uint256&) const;
    bool DeleteMsPos(const uint256&) const;

    bool ExistsUTXO(const uint256&) const;
    std::unique_ptr<UTXO> GetUTXO(const uint256&) const;
    bool WriteUTXO(const uint256&, const UTXOPtr&) const;
    bool RemoveUTXO(const uint256&) const;

    uint256 GetLastReg(const uint256&) const;
    bool UpdateReg(const RegChange&) const;
    bool RollBackReg(const RegChange&) const;

    template <typename V>
    bool WriteInfo(const std::string& key, const V& value) const;
    template <typename V>
    V GetInfo(const std::string&) const;

private:
    uint256 GetMsHashAt(const uint64_t& height) const;
    std::optional<std::tuple<uint64_t, uint32_t, uint32_t>> GetVertexOffsets(const uint256&) const;

    bool WriteRegSet(const std::unordered_set<std::pair<uint256, uint256>>&) const;
    bool DeleteRegSet(const std::unordered_set<std::pair<uint256, uint256>>&) const;

    template <typename K, typename H, typename P1, typename P2>
    bool WritePosImpl(const std::string& column, const K&, const H&, const P1&, const P2&) const;
};

#endif // EPIC_ROCKSDB_H
