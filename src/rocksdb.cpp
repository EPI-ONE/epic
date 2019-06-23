#include "rocksdb.h"

using namespace rocksdb;

RocksDBStore::RocksDBStore(string dbPath) {
    this->DBPATH = dbPath;
    // Make directory DBPATH if missing
    if (!CheckDirExist(DBPATH)) {
        Mkdir_recursive(DBPATH);
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
    dbOptions.db_log_dir                     = DBPATH + "/log";
    dbOptions.create_if_missing              = true;
    dbOptions.create_missing_column_families = true;
    dbOptions.IncreaseParallelism(2);

    std::vector<ColumnFamilyHandle*> handles;

    // Open DB
    if (!DB::Open(dbOptions, DBPATH, descriptors, &handles, &db).ok()) {
        throw "DB initialization failed";
    }

    // Store handles into a map
    InitHandleMap(handles);
    handles.clear();
    descriptors.clear();
}

RocksDBStore::~RocksDBStore() {
    // delete column falimy handles
    for (auto entry = handleMap.begin(); entry != handleMap.end(); ++entry) {
        delete entry->second;
    }
    handleMap.clear();

    delete db;
}

#define MAKE_KEY_SLICE(arg, n)     \
    VStream keyStream;             \
    keyStream.reserve((size_t) n); \
    keyStream << arg;              \
    Slice keySlice(keyStream.data(), keyStream.size());

bool RocksDBStore::Exists(const uint256& blockHash) const {
    MAKE_KEY_SLICE(blockHash, Hash::SIZE);
    return !Get(kDefaultColumnFamilyName, keySlice).empty();
}

size_t RocksDBStore::GetHeight(const uint256& blkHash) const {
    MAKE_KEY_SLICE(blkHash, Hash::SIZE);
    PinnableSlice valueSlice;
    Status s = db->Get(ReadOptions(), db->DefaultColumnFamily(), keySlice, &valueSlice);

    uint64_t height = -1;
    if (s.ok()) {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            value >> VARINT(height);
        } catch (const std::exception&) {
            // do nothing
        }
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
    MAKE_KEY_SLICE(height, 8);
    PinnableSlice valueSlice;
    Status s = db->Get(ReadOptions(), handleMap.at("ms"), keySlice, &valueSlice);

    if (!s.ok()) {
        return {};
    }

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
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
    return GetMsPos(height)->first;
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

        wb.Put(db->DefaultColumnFamily(), keySlice, valueSlice);

        keyStream.clear();
        valueStream.clear();
    }

    return db->Write(WriteOptions(), &wb).ok();
}

bool RocksDBStore::WriteMsPos(const uint64_t& key,
                              const uint256& msHash,
                              const FilePos& blkPos,
                              const FilePos& recPos) const {
    return WritePosImpl("ms", key, msHash, blkPos, recPos);
}

string RocksDBStore::Get(const string& column, const Slice& key) const {
    string value;
    Status s = db->Get(ReadOptions(), handleMap.at(column), key, &value);
    if (s.ok()) {
        return value;
    }

    return "";
}

string RocksDBStore::Get(const string& column, const string& key) const {
    return Get(column, Slice(key));
}

bool RocksDBStore::DeleteRecPos(const uint256& h) const {
    return db->Delete(WriteOptions(), db->DefaultColumnFamily(), VStream(h).str()).ok();
}

bool RocksDBStore::DeleteMsPos(const uint256& h) const {
    bool status = db->Delete(WriteOptions(), handleMap.at("ms"), std::to_string(GetHeight(h))).ok();
    if (status && IsMilestone(h)) {
        return DeleteRecPos(h);
    }
    return status;
}

void RocksDBStore::InitHandleMap(std::vector<ColumnFamilyHandle*> handles) {
    handleMap.reserve(COLUMN_NAMES.size());
    auto keyIter = COLUMN_NAMES.begin();
    auto valIter = handles.begin();
    while (keyIter != COLUMN_NAMES.end() && valIter != handles.end()) {
        handleMap[*keyIter] = *valIter;
        ++keyIter;
        ++valIter;
    }

    assert(!handleMap.empty());
}

uint256 RocksDBStore::GetMsHashAt(const uint64_t& height) const {
    MAKE_KEY_SLICE(height, 8);
    PinnableSlice valueSlice;

    Status s = db->Get(ReadOptions(), handleMap.at("ms"), keySlice, &valueSlice);

    if (!s.ok()) {
        return uint256();
    } else {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            uint256 msHash(value);
            return msHash;
        } catch (const std::exception&) {
            return uint256();
        }
    }
}

optional<tuple<uint64_t, uint32_t, uint32_t>> RocksDBStore::GetRecordOffsets(const uint256& blkHash) const {
    MAKE_KEY_SLICE(blkHash, Hash::SIZE);
    PinnableSlice valueSlice;
    Status s = db->Get(ReadOptions(), db->DefaultColumnFamily(), keySlice, &valueSlice);

    if (!s.ok()) {
        return {};
    }

    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
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
    MAKE_KEY_SLICE(key, sizeof(K));

    VStream value;
    value.reserve(sizeof(H) + sizeof(P1) + sizeof(P2));
    value << h << b << r;
    Slice valueSlice(value.data(), value.size());

    return db->Put(WriteOptions(), handleMap.at(column), keySlice, valueSlice).ok();
}

template bool RocksDBStore::WritePosImpl(
    const string& column, const uint256&, const uint64_t&, const uint32_t&, const uint32_t&) const;

template bool RocksDBStore::WritePosImpl(
    const string& column, const uint64_t&, const uint256&, const FilePos&, const FilePos&) const;
