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
    // Open DB and store handles into a map
    std::vector<ColumnFamilyHandle*> handles;
    if (!DB::Open(dbOptions, DBPATH, descriptors, &handles, &db).ok()) {
        throw "DB initialization failed";
    }
    InitHandleMap(handles);
    handles.clear();
    descriptors.clear();
}

bool RocksDBStore::Exists(const uint256& blockHash) const {
    VStream key;
    key.reserve(32);
    key << blockHash;
    Slice keySlice(key.data(), key.size());
    return Get(kDefaultColumnFamilyName, keySlice) != "";
}

std::unique_ptr<NodeRecord> RocksDBStore::GetRecord(const uint256& blockHash) const {
    VStream key;
    key.reserve(32);
    key << blockHash;
    Slice keySlice(key.data(), key.size());

    // PinnableSlice is used here to reduce memcpy overhead.
    // See https://rocksdb.org/blog/2017/08/24/pinnableslice.html
    PinnableSlice valueSlice;

    Status s = db->Get(ReadOptions(), handleMap[kDefaultColumnFamilyName], keySlice, &valueSlice);
    if (!s.ok()) {
        return nullptr;
    }
    std::unique_ptr<NodeRecord> b;
    try {
        VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
        b = std::make_unique<NodeRecord>(value);
    } catch (const std::exception&) {
        return nullptr;
    }
    return b;
}

const std::string RocksDBStore::Get(const std::string& column, const Slice& key) const {
    std::string value;
    Status s = db->Get(ReadOptions(), handleMap[column], key, &value);
    if (s.ok()) {
        return value;
    }
    return "";
}

const std::string RocksDBStore::Get(const std::string& column, const std::string& key) const {
    return Get(column, Slice(key));
}

bool RocksDBStore::WriteRecord(const RecordPtr& record) const {
    VStream key;
    key.reserve(32);
    key << record->cBlock->GetHash();
    Slice keySlice(key.data(), key.size());

    VStream value;
    value.reserve(record->GetOptimalStorageSize());
    value << *(record->cBlock);
    Slice valueSlice(value.data(), value.size());

    return Write(kDefaultColumnFamilyName, keySlice, valueSlice);
}

bool RocksDBStore::WriteRecords(const std::vector<RecordPtr>& records) const {
    class WriteBatch wb;
    VStream key;
    key.reserve(32);
    VStream value;
    for (const auto& record : records) {
        // Prepare key
        key << record->cBlock->GetHash();
        Slice keySlice(key.data(), key.size());

        // Prepare value
        value.reserve(record->GetOptimalStorageSize());
        value << *(record->cBlock);
        Slice valueSlice(value.data(), value.size());

        wb.Put(handleMap[kDefaultColumnFamilyName], keySlice, valueSlice);

        key.clear();
        value.clear();
    }
    return db->Write(WriteOptions(), &wb).ok();
}

bool RocksDBStore::Write(const std::string& column, const Slice& key, const Slice& value) const {
    return db->Put(WriteOptions(), handleMap[column], key, value).ok();
}

bool RocksDBStore::Write(const std::string& column, const std::string& key, const std::string& value) const {
    return Write(column, Slice(key), Slice(value));
}

const void RocksDBStore::WriteBatch(const std::string& column, const std::map<std::string, std::string>& batch) const {
    class WriteBatch wb;
    for (auto const& [key, value] : batch) {
        wb.Put(handleMap[column], key, value);
    }
    bool ok = db->Write(WriteOptions(), &wb).ok();
    assert(ok);
}

void RocksDBStore::Delete(const std::string& column, const std::string& key) const {
    bool ok = db->Delete(WriteOptions(), handleMap[column], key).ok();
    assert(ok);
}

RocksDBStore::~RocksDBStore() {
    for (auto entry = handleMap.begin(); entry != handleMap.end(); ++entry) {
        delete entry->second;
    }
    handleMap.clear();
    delete db;
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
