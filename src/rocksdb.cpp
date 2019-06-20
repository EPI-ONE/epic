#include "rocksdb.h"

using namespace rocksdb;

RocksDBStore::RocksDBStore(std::string dbPath) {
    this->DBPATH = dbPath;
    // Make directory DBPATH if missing
    if (!CheckDirExist(DBPATH)) {
        Mkdir_recursive(DBPATH);
    }

    // Create column families
    std::vector<ColumnFamilyDescriptor> descriptors;
    for (const std::string& columnName : COLUMN_NAMES) {
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

bool RocksDBStore::IsMilestone(const uint256& blkHash) const {
    auto hashes = GetLevelSetHashes(GetHeight(blkHash));
    if (hashes.empty()) {
        return false;
    }
    return blkHash == hashes.back();
}

StoredRecord RocksDBStore::GetMsRecordAt(const uint64_t& height) const {
    MAKE_KEY_SLICE(height, 8);
    PinnableSlice valueSlice;

    Status s = db->Get(ReadOptions(), handleMap.at("ms"), keySlice, &valueSlice);

    if (!s.ok()) {
        return nullptr;
    } else {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            std::vector<uint256> hashes;
            value >> hashes;
            return GetRecord(hashes.back());
        } catch (const std::exception&) {
            return nullptr;
        }
    }
}

StoredRecord RocksDBStore::GetRecord(const uint256& blockHash) const {
    std::unique_ptr<NodeRecord> pnodeRecord;
    GetRecordImpl(blockHash, db->DefaultColumnFamily(), pnodeRecord);
    return pnodeRecord;
}

std::vector<StoredRecord> RocksDBStore::GetLevelSetAt(const uint64_t& height) const {
    auto lvsHashes = GetLevelSetHashes(height);

    if (lvsHashes.empty()) {
        return std::vector<StoredRecord>();
    }

    std::vector<StoredRecord> result;
    for (auto& blkHash : lvsHashes) {
        result.push_back(GetRecord(blkHash));
    }

    return result;
}

std::vector<StoredRecord> RocksDBStore::GetLevelSet(const uint256& blkHash) const {
    return GetLevelSetAt(GetHeight(blkHash));
}

size_t RocksDBStore::GetHeight(const uint256& blkHash) const {
    MAKE_KEY_SLICE(blkHash, Hash::SIZE);

    // PinnableSlice is used here to reduce memcpy overhead.
    // See https://rocksdb.org/blog/2017/08/24/pinnableslice.html
    PinnableSlice valueSlice;

    Status s = db->Get(ReadOptions(), db->DefaultColumnFamily(), keySlice, &valueSlice);

    uint64_t height;
    if (!s.ok()) {
        height = 0;
    } else {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            value >> height;
        } catch (const std::exception&) {
            height = 0;
        }
    }

    return height;
}

std::string RocksDBStore::Get(const std::string& column, const Slice& key) const {
    std::string value;
    Status s = db->Get(ReadOptions(), handleMap.at(column), key, &value);
    if (s.ok()) {
        return value;
    }

    return "";
}

std::string RocksDBStore::Get(const std::string& column, const std::string& key) const {
    return Get(column, Slice(key));
}

bool RocksDBStore::WriteRecord(const RecordPtr& record) const {
    VStream key;
    key.reserve(Hash::SIZE);
    key << record->cblock->GetHash();
    Slice keySlice(key.data(), key.size());

    VStream value;
    value.reserve(record->GetOptimalStorageSize());
    value << *record;
    Slice valueSlice(value.data(), value.size());

    return Write(kDefaultColumnFamilyName, keySlice, valueSlice);
}

bool RocksDBStore::WriteRecords(const std::vector<RecordPtr>& records) const {
    class WriteBatch wb;
    VStream key;
    key.reserve(Hash::SIZE);
    VStream value;

    for (const auto& record : records) {
        // Prepare key
        key << record->cblock->GetHash();
        Slice keySlice(key.data(), key.size());

        // Prepare value
        value.reserve(record->GetOptimalStorageSize());
        value << *record;
        Slice valueSlice(value.data(), value.size());

        wb.Put(db->DefaultColumnFamily(), keySlice, valueSlice);

        key.clear();
        value.clear();
    }

    return db->Write(WriteOptions(), &wb).ok();
}

bool RocksDBStore::Write(const std::string& column, const Slice& key, const Slice& value) const {
    return db->Put(WriteOptions(), handleMap.at(column), key, value).ok();
}

bool RocksDBStore::Write(const std::string& column, const std::string& key, const std::string& value) const {
    return Write(column, Slice(key), Slice(value));
}

bool RocksDBStore::WriteBatch(const std::string& column, const std::map<std::string, std::string>& batch) const {
    // Prepare WriteBatch
    class WriteBatch wb;
    for (auto const& [key, value] : batch) {
        wb.Put(handleMap.at(column), key, value);
    }

    return db->Write(WriteOptions(), &wb).ok();
}

bool RocksDBStore::Delete(const std::string& column, const std::string& key) const {
    return db->Delete(WriteOptions(), handleMap.at(column), key).ok();
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

std::vector<uint256> RocksDBStore::GetLevelSetHashes(const uint64_t& height) const {
    MAKE_KEY_SLICE(height, 8);
    PinnableSlice valueSlice;

    Status s = db->Get(ReadOptions(), handleMap.at("ms"), keySlice, &valueSlice);

    std::vector<uint256> hashes;
    if (!s.ok()) {
        return hashes;
    } else {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            value >> hashes;
            return hashes;
        } catch (const std::exception&) {
            return std::vector<uint256>();
        }
    }
}

template <typename K, typename B>
void RocksDBStore::GetRecordImpl(const K& key, rocksdb::ColumnFamilyHandle* handle, std::unique_ptr<B>& result) const {
    MAKE_KEY_SLICE(key, sizeof(K));

    // PinnableSlice is used here to reduce memcpy overhead.
    // See https://rocksdb.org/blog/2017/08/24/pinnableslice.html
    PinnableSlice valueSlice;

    Status s = db->Get(ReadOptions(), handle, keySlice, &valueSlice);

    std::unique_ptr<B> presult;
    if (!s.ok()) {
        presult = nullptr;
    } else {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            presult = std::make_unique<B>(value);
        } catch (const std::exception&) {
            presult = nullptr;
        }
    }

    result.swap(presult);
}

template void RocksDBStore::GetRecordImpl(const uint256&,
    rocksdb::ColumnFamilyHandle*,
    std::unique_ptr<NodeRecord>&) const;
template void RocksDBStore::GetRecordImpl(const uint64_t&,
    rocksdb::ColumnFamilyHandle*,
    std::unique_ptr<NodeRecord>&) const;
