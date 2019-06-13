#include "rocksdb.h"

using std::optional;
using std::pair;
using std::string;
using std::tuple;
using namespace rocksdb;

#define MAKE_KEY_SLICE(obj) \
    VStream keyStream{obj}; \
    Slice keySlice(keyStream.data(), keyStream.size());

#define GET_VALUE(column_handle, null_return)                                 \
    PinnableSlice valueSlice;                                                 \
    Status s = db_->Get(ReadOptions(), column_handle, keySlice, &valueSlice); \
    if (!s.ok()) {                                                            \
        return null_return;                                                   \
    }

RocksDBStore::RocksDBStore(string dbPath) {
    this->dbpath_ = dbPath;
    // Make directory DBPATH if missing
    if (!CheckDirExist(dbpath_)) {
        Mkdir_recursive(dbpath_);
    }

    // Create column families
    std::vector<ColumnFamilyDescriptor> descriptors;
    for (const string& columnName : COLUMN_NAMES) {
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
    InitHandleMap(handles);
    handles.clear();
    descriptors.clear();
}

RocksDBStore::~RocksDBStore() {
    // delete column falimy handles
    for (auto entry = handleMap_.begin(); entry != handleMap_.end(); ++entry) {
        delete entry->second;
    }
    handleMap_.clear();

    delete db_;
}

bool RocksDBStore::Exists(const uint256& blockHash) const {
    MAKE_KEY_SLICE((uint64_t) GetHeight(blockHash));
    return !Get("ms", keySlice).empty();
}

size_t RocksDBStore::GetHeight(const uint256& blkHash) const {
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

bool RocksDBStore::IsMilestone(const uint256& blkHash) const {
    auto value = GetRecordOffsets(blkHash);
    if (!value) {
        return false;
    }

    return std::get<1>(*value) == 0 && std::get<2>(*value) == 0;
}

optional<pair<FilePos, FilePos>> RocksDBStore::GetMsPos(const uint64_t& height) const {
    MAKE_KEY_SLICE(height);
    GET_VALUE(handleMap_.at("ms"), {});

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        value.ignore(Hash::SIZE);
        FilePos blkPos(value);
        FilePos recPos(value);
        return std::make_pair(blkPos, recPos);
    } catch (const std::exception&) {
        return {};
    }
}

optional<pair<FilePos, FilePos>> RocksDBStore::GetMsPos(const uint256& blkHash) const {
    return GetMsPos(GetHeight(blkHash));
}

optional<FilePos> RocksDBStore::GetMsBlockPos(const uint64_t& height) const {
    auto value = GetMsPos(height);
    if (value) {
        return value->first;
    }
    return {};
}

optional<pair<FilePos, FilePos>> RocksDBStore::GetRecordPos(const uint256& blkHash) const {
    auto offsets = GetRecordOffsets(blkHash);
    if (!offsets) {
        return {};
    }

    auto [height, blkOffset, recOffset] = *offsets;
    auto [blkPos, recPos]               = *GetMsPos(height);

    blkPos.nOffset += blkOffset;
    recPos.nOffset += recOffset;

    return std::make_pair(blkPos, recPos);
}

bool RocksDBStore::WriteRecPos(const uint256& key,
                               const uint64_t& height,
                               const uint32_t& blkOffset,
                               const uint32_t& recOffset) const {
    return WritePosImpl(kDefaultColumnFamilyName, key, VARINT(height), blkOffset, recOffset);
}

bool RocksDBStore::WriteRecPoses(const std::vector<uint256>& keys,
                                 const std::vector<uint64_t>& heights,
                                 const std::vector<uint32_t>& blkOffsets,
                                 const std::vector<uint32_t>& recOffsets) const {
    class WriteBatch wb;
    VStream keyStream;
    keyStream.reserve(Hash::SIZE);
    VStream valueStream;
    valueStream.reserve(16);

    assert(keys.size() == heights.size() && keys.size() == blkOffsets.size() && keys.size() == recOffsets.size());

    for (size_t i = 0; i < keys.size(); ++i) {
        // Prepare key
        keyStream << keys[i];
        Slice keySlice(keyStream.data(), keyStream.size());

        // Prepare value
        valueStream << VARINT(heights[i]) << blkOffsets[i] << recOffsets[i];
        Slice valueSlice(valueStream.data(), valueStream.size());

        wb.Put(db_->DefaultColumnFamily(), keySlice, valueSlice);

        keyStream.clear();
        valueStream.clear();
    }

    return db_->Write(WriteOptions(), &wb).ok();
}

bool RocksDBStore::WriteMsPos(const uint64_t& key,
                              const uint256& msHash,
                              const FilePos& blkPos,
                              const FilePos& recPos) const {
    return WritePosImpl("ms", key, msHash, blkPos, recPos);
}

std::unique_ptr<UTXO> RocksDBStore::GetUTXO(const uint256& key) const {
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

bool RocksDBStore::WriteUTXO(const uint256& key, const UTXOPtr& utxo) const {
    MAKE_KEY_SLICE(key);

    VStream value(utxo);
    Slice valueSlice(value.data(), value.size());

    return db_->Put(WriteOptions(), handleMap_.at("utxo"), keySlice, valueSlice).ok();
}

bool RocksDBStore::RemoveUTXO(const uint256& key) const {
    VStream keyStream(key);
    Slice keySlice(keyStream.data(), keyStream.size());

    return db_->Delete(WriteOptions(), handleMap_.at("utxo"), keySlice).ok();
}

bool RocksDBStore::DeleteRecPos(const uint256& h) const {
    return db_->Delete(WriteOptions(), db_->DefaultColumnFamily(), VStream(h).str()).ok();
}

bool RocksDBStore::DeleteMsPos(const uint256& h) const {
    bool status = db_->Delete(WriteOptions(), handleMap_.at("ms"), std::to_string(GetHeight(h))).ok();
    if (status && IsMilestone(h)) {
        return DeleteRecPos(h);
    }
    return status;
}

string RocksDBStore::Get(const string& column, const Slice& keySlice) const {
    GET_VALUE(handleMap_.at(column), "");
    return valueSlice.ToString();
}

string RocksDBStore::Get(const string& column, const string& key) const {
    return Get(column, Slice(key));
}

void RocksDBStore::InitHandleMap(std::vector<ColumnFamilyHandle*> handles) {
    handleMap_.reserve(COLUMN_NAMES.size());
    auto keyIter = COLUMN_NAMES.begin();
    auto valIter = handles.begin();
    while (keyIter != COLUMN_NAMES.end() && valIter != handles.end()) {
        handleMap_[*keyIter] = *valIter;
        ++keyIter;
        ++valIter;
    }

    assert(!handleMap_.empty());
}

uint256 RocksDBStore::GetMsHashAt(const uint64_t& height) const {
    MAKE_KEY_SLICE(height);
    GET_VALUE(handleMap_.at("ms"), uint256());

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        uint256 msHash(value);
        return msHash;
    } catch (const std::exception&) {
        return uint256();
    }
}

optional<tuple<uint64_t, uint32_t, uint32_t>> RocksDBStore::GetRecordOffsets(const uint256& blkHash) const {
    MAKE_KEY_SLICE(blkHash);
    GET_VALUE(db_->DefaultColumnFamily(), {});

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        valueSlice.Reset();

        uint64_t height;
        uint32_t blkOffset, recOffset;
        value >> VARINT(height) >> blkOffset >> recOffset;
        return std::make_tuple(height, blkOffset, recOffset);

    } catch (const std::exception&) {
        return {};
    }
}

template <typename K, typename H, typename P1, typename P2>
bool RocksDBStore::WritePosImpl(const string& column, const K& key, const H& h, const P1& b, const P2& r) const {
    MAKE_KEY_SLICE(key);

    VStream value;
    value.reserve(sizeof(H) + sizeof(P1) + sizeof(P2));
    value << h << b << r;
    Slice valueSlice(value.data(), value.size());

    return db_->Put(WriteOptions(), handleMap_.at(column), keySlice, valueSlice).ok();
}

template bool RocksDBStore::WritePosImpl(
    const string& column, const uint256&, const uint64_t&, const uint32_t&, const uint32_t&) const;

template bool RocksDBStore::WritePosImpl(
    const string& column, const uint64_t&, const uint256&, const FilePos&, const FilePos&) const;
