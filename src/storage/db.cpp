// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "db.h"

using std::optional;
using std::pair;
using std::string;
using std::tuple;
using namespace rocksdb;

#define MAKE_KEY_SLICE(obj)                             \
    VStream keyStream{obj};                             \
    Slice keySlice(keyStream.data(), keyStream.size()); \
    keyStream.clear();

#define GET_VALUE(column_handle, null_return)                                 \
    PinnableSlice valueSlice;                                                 \
    Status s = db_->Get(ReadOptions(), column_handle, keySlice, &valueSlice); \
    if (!s.ok()) {                                                            \
        return null_return;                                                   \
    }

static const std::vector<std::string> COLUMN_NAMES = {
    "default", // (key) block hash
               // (value) {height, blk offset, ms offset}
               // Note: offsets are relative to the offsets of
               // the milestone contained in the same level set

    "ms", // (key) level set height
          // (value) {blk FilePos, vtx FilePos}

    "utxo", // (key) outpoint hash ^ outpoint index
            // (value) utxo

    "reg", // (key) hash of peer chain head
           // (value) hash of the last registration block on this peer chain

    "info" // Stores necessary info to recover the system,
           // e.g., lastest ms head in db
};

DBStore::DBStore(string dbPath) : RocksDB(std::move(dbPath), COLUMN_NAMES) {}

bool DBStore::Exists(const uint256& blockHash) const {
    MAKE_KEY_SLICE((uint64_t) GetHeight(blockHash));
    return !RocksDB::Get("ms", keySlice).empty();
}

size_t DBStore::GetHeight(const uint256& blkHash) const {
    MAKE_KEY_SLICE(blkHash);

    uint64_t height = UINT_FAST64_MAX;
    GET_VALUE(db_->DefaultColumnFamily(), height);

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        value >> VARINT(height);
    } catch (const std::exception&) {
        // do nothing
    }

    return height;
}

bool DBStore::IsMilestone(const uint256& blkHash) const {
    auto value = GetVertexOffsets(blkHash);
    if (!value) {
        return false;
    }

    return std::get<1>(*value) == 0 && std::get<2>(*value) == 0;
}

optional<pair<FilePos, FilePos>> DBStore::GetMsPos(const uint64_t& height) const {
    MAKE_KEY_SLICE(height);
    GET_VALUE(handleMap_.at("ms"), {});

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        value.ignore(Hash::SIZE);
        FilePos blkPos(value);
        FilePos vtxPos(value);
        return {{blkPos, vtxPos}};
    } catch (const std::exception&) {
        return {};
    }
}

optional<pair<FilePos, FilePos>> DBStore::GetMsPos(const uint256& blkHash) const {
    return GetMsPos(GetHeight(blkHash));
}

optional<FilePos> DBStore::GetMsBlockPos(const uint64_t& height) const {
    auto value = GetMsPos(height);
    if (value) {
        return value->first;
    }
    return {};
}

optional<pair<FilePos, FilePos>> DBStore::GetVertexPos(const uint256& blkHash) const {
    auto offsets = GetVertexOffsets(blkHash);
    if (!offsets) {
        return {};
    }

    auto [height, blkOffset, vtxOffset] = *offsets;
    auto [blkPos, vtxPos]               = *GetMsPos(height);

    blkPos.nOffset += blkOffset;
    vtxPos.nOffset += vtxOffset;

    return {{blkPos, vtxPos}};
}


bool DBStore::WriteVtxPos(const uint256& key,
                          const uint64_t& height,
                          const uint32_t& blkOffset,
                          const uint32_t& vtxOffset) const {
    return WritePosImpl(kDefaultColumnFamilyName, key, VARINT(height), blkOffset, vtxOffset);
}


bool DBStore::WriteVtxPoses(const std::vector<uint256>& keys,
                            const std::vector<uint64_t>& heights,
                            const std::vector<uint32_t>& blkOffsets,
                            const std::vector<uint32_t>& vtxOffsets) const {
    class WriteBatch wb;
    VStream keyStream;
    keyStream.reserve(Hash::SIZE);
    VStream valueStream;
    valueStream.reserve(16);

    assert(keys.size() == heights.size() && keys.size() == blkOffsets.size() && keys.size() == vtxOffsets.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        // Prepare key
        keyStream << keys[i];
        Slice keySlice(keyStream.data(), keyStream.size());

        // Prepare value
        valueStream << VARINT(heights[i]) << blkOffsets[i] << vtxOffsets[i];
        Slice valueSlice(valueStream.data(), valueStream.size());

        wb.Put(db_->DefaultColumnFamily(), keySlice, valueSlice);

        keyStream.clear();
        valueStream.clear();
    }

    return db_->Write(WriteOptions(), &wb).ok();
}

bool DBStore::WriteMsPos(const uint64_t& key,
                         const uint256& msHash,
                         const FilePos& blkPos,
                         const FilePos& vtxPos) const {
    return WritePosImpl("ms", key, msHash, blkPos, vtxPos);
}


bool DBStore::ExistsUTXO(const uint256& key) const {
    MAKE_KEY_SLICE(key);
    return !RocksDB::Get("utxo", keySlice).empty();
}

std::unique_ptr<UTXO> DBStore::GetUTXO(const uint256& key) const {
    MAKE_KEY_SLICE(key);
    GET_VALUE(handleMap_.at("utxo"), nullptr);

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        return std::make_unique<UTXO>(value);
    } catch (const std::exception&) {
        return nullptr;
    }
}

bool DBStore::WriteUTXO(const uint256& key, const UTXOPtr& utxo) const {
    MAKE_KEY_SLICE(key);

    VStream value(utxo);
    Slice valueSlice(value.data(), value.size());

    return db_->Put(WriteOptions(), handleMap_.at("utxo"), keySlice, valueSlice).ok();
}

bool DBStore::RemoveUTXO(const uint256& key) const {
    VStream keyStream(key);
    Slice keySlice(keyStream.data(), keyStream.size());

    return db_->Delete(WriteOptions(), handleMap_.at("utxo"), keySlice).ok();
}

bool DBStore::DeleteVtxPos(const uint256& h) const {
    return db_->Delete(WriteOptions(), db_->DefaultColumnFamily(), VStream(h).str()).ok();
}

bool DBStore::DeleteMsPos(const uint256& h) const {
    bool status = db_->Delete(WriteOptions(), handleMap_.at("ms"), std::to_string(GetHeight(h))).ok();
    if (status && IsMilestone(h)) {
        return DeleteVtxPos(h);
    }
    return status;
}

uint256 DBStore::GetLastReg(const uint256& key) const {
    MAKE_KEY_SLICE(key);
    GET_VALUE(handleMap_.at("reg"), uint256{});

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();
        uint256 result;
        value >> result;
        return result;
    } catch (const std::exception&) {
        return uint256{};
    }
}

bool DBStore::UpdateReg(const RegChange& change) const {
    if (!DeleteRegSet(change.GetRemoved())) {
        return false;
    }

    return WriteRegSet(change.GetCreated());
}

bool DBStore::RollBackReg(const RegChange& change) const {
    if (!DeleteRegSet(change.GetCreated())) {
        return false;
    }

    return WriteRegSet(change.GetRemoved());
}

template <typename V>
bool DBStore::WriteInfo(const std::string& k, const V& v) const {
    VStream value(v);
    Slice valueSlice(value.data(), value.size());
    return db_->Put(WriteOptions(), handleMap_.at("info"), k, valueSlice).ok();
}
template bool DBStore::WriteInfo(const std::string&, const uint256&) const;
template bool DBStore::WriteInfo(const std::string&, const uint64_t&) const;
template bool DBStore::WriteInfo(const std::string&, const uint32_t&) const;
template bool DBStore::WriteInfo(const std::string&, const uint16_t&) const;

template <typename V>
V DBStore::GetInfo(const std::string& k) const {
    Slice keySlice(k);
    GET_VALUE(handleMap_.at("info"), V{});
    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();
        V result;
        value >> result;
        return result;
    } catch (const std::exception&) {
        return V{};
    }
}
template uint256 DBStore::GetInfo(const std::string&) const;
template uint64_t DBStore::GetInfo(const std::string&) const;
template uint32_t DBStore::GetInfo(const std::string&) const;
template uint16_t DBStore::GetInfo(const std::string&) const;

uint256 DBStore::GetMsHashAt(const uint64_t& height) const {
    MAKE_KEY_SLICE(height);
    GET_VALUE(handleMap_.at("ms"), uint256());

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        uint256 msHash;
        value >> msHash;
        return msHash;
    } catch (const std::exception&) {
        return uint256();
    }
}

optional<tuple<uint64_t, uint32_t, uint32_t>> DBStore::GetVertexOffsets(const uint256& blkHash) const {
    MAKE_KEY_SLICE(blkHash);
    GET_VALUE(db_->DefaultColumnFamily(), {});

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        uint64_t height;
        uint32_t blkOffset, vtxOffset;
        value >> VARINT(height) >> blkOffset >> vtxOffset;
        return std::make_tuple(height, blkOffset, vtxOffset);

    } catch (const std::exception&) {
        return {};
    }
}

bool DBStore::WriteRegSet(const std::unordered_set<std::pair<uint256, uint256>>& s) const {
    class WriteBatch wb;
    for (const auto& e : s) {
        Slice keySlice((char*) e.first.begin(), Hash::SIZE);
        Slice valueSlice((char*) e.second.begin(), Hash::SIZE);
        wb.Put(handleMap_.at("reg"), keySlice, valueSlice);
    }

    return db_->Write(WriteOptions(), &wb).ok();
}

bool DBStore::DeleteRegSet(const std::unordered_set<std::pair<uint256, uint256>>& s) const {
    for (const auto& e : s) {
        if (!RocksDB::Delete("reg", VStream(e.first).str())) {
            return false;
        }
    }
    return true;
}

template <typename K, typename H, typename P1, typename P2>
bool DBStore::WritePosImpl(const string& column, const K& key, const H& h, const P1& b, const P2& r) const {
    MAKE_KEY_SLICE(key);

    VStream value;
    value.reserve(sizeof(H) + sizeof(P1) + sizeof(P2));
    value << h << b << r;
    Slice valueSlice(value.data(), value.size());

    return db_->Put(WriteOptions(), handleMap_.at(column), keySlice, valueSlice).ok();
}

template bool DBStore::WritePosImpl(
    const string& column, const uint256&, const uint64_t&, const uint32_t&, const uint32_t&) const;

template bool DBStore::WritePosImpl(
    const string& column, const uint64_t&, const uint256&, const FilePos&, const FilePos&) const;
