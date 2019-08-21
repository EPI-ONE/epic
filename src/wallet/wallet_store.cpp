#include "wallet_store.h"
#include "big_uint.h"
#include "file_utils.h"
#include "key.h"
#include "stream.h"
#include "transaction.h"

//#include <cstdio>
#include <fstream>

using namespace rocksdb;
using utxo_map = ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint64_t>>;

enum { UNSPENT = 0, PENDING = 1, SPENT = 2 };
const std::string kKeyBook    = kDefaultColumnFamilyName;
const std::string kTx         = "pending_tx";
const std::string kUnspentTXO = "utxo";
const std::string kPendingTXO = "pending_utxo";
const std::string kSpentTXO   = "spent_txo";

const std::array<std::string, 3> utxoStr{kUnspentTXO, kPendingTXO, kSpentTXO};

static const std::vector<std::string> COLUMN_NAMES = {
    kKeyBook, // (key) address
              // (value) encoded private key
              // update on each changes

    kTx, // (key) transaction hash
         // (value) transaction instance; will become a ConstTxPtr when read out
         // pendingTx
         // force update periodically

    // (key) utxo key: outpoint hash ^ outpoint index
    // (value) {address (20B) + outpoint index (4B) + utxo coin value (8B)}
    kUnspentTXO, kPendingTXO, // force update periodically
    kSpentTXO                 // update on each changes
};

inline bool put(DB* db, ColumnFamilyHandle* handle, const VStream& key, const VStream& value) {
    return db->Put(WriteOptions(), handle, Slice{key.data(), key.size()}, Slice{value.data(), value.size()}).ok();
}

WalletStore::WalletStore(std::string dbPath) : DBWrapper(std::move(dbPath), COLUMN_NAMES) {}

bool WalletStore::StoreTx(const Transaction& tx) {
    VStream key{tx.GetHash()};
    VStream value{tx};
    return put(db_, handleMap_.at(kTx), key, value);
}

ConcurrentHashMap<uint256, ConstTxPtr> WalletStore::GetAllTx() {
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(kTx));
    // TODO: may get an estimated num of keys
    ConcurrentHashMap<uint256, ConstTxPtr> result;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        uint256 txhash;
        ConstTxPtr ptx;
        try {
            VStream key{iter->key().data(), iter->key().data() + iter->key().size()};
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            key >> txhash;
            ptx = std::make_shared<const Transaction>(value);
        } catch (std::exception& e) {
            std::cout << "err tx\n";
        }
        result.emplace(txhash, ptx);
    }
    assert(iter->status().ok());
    delete iter;

    return result;
}

bool WalletStore::StoreKeys(const CKeyID& addr, const CKey& privkey) {
    VStream key, value;
    key << EncodeAddress(addr);
    value << EncodeSecret(privkey);

    return put(db_, handleMap_.at(kKeyBook), key, value);
}

bool WalletStore::IsExistKey(const CKeyID& addr) {
    VStream key;
    key << EncodeAddress(addr);
    return !DBWrapper::Get(kKeyBook, Slice{key.data(), key.size()}).empty();
}

std::optional<CKey> WalletStore::GetKey(const CKeyID& addr) {
    CKey privkey;
    VStream key;
    key << addr;
    PinnableSlice valueSlice;
    if (db_->Get(ReadOptions(), handleMap_.at(kKeyBook), Slice{key.data(), key.size()}, &valueSlice).ok()) {
        try {
            std::string encodedkey;
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            value >> encodedkey;
            if (auto oKey = DecodeSecret(encodedkey)) {
                privkey = *oKey;
                return privkey;
            }
        } catch (const std::exception& e) {
            std::cout << "err key\n";
        }
    }

    return {};
}

ConcurrentHashMap<CKeyID, CKey> WalletStore::GetAllKey() {
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(kKeyBook));
    ConcurrentHashMap<CKeyID, CKey> result;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        CKeyID addr;
        CKey privkey;
        try {
            VStream key{iter->key().data(), iter->key().data() + iter->key().size()};
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            std::string keystr, valuestr;
            key >> keystr;
            value >> valuestr;
            if (auto oAddr = DecodeAddress(keystr)) {
                addr = *oAddr;
            }
            if (auto okey = DecodeSecret(valuestr)) {
                privkey = *okey;
            }
            result.emplace(addr, privkey);
        } catch (std::exception& e) {
            std::cout << "err all key\n";
        }
    }
    assert(iter->status().ok());
    delete iter;

    return result;
}

bool WalletStore::StoreUTXO(
    const uint256& utxokey, const CKeyID& addr, uint32_t outputIndex, uint64_t coin, uint8_t category) {
    VStream key{utxokey};
    VStream value;
    value << EncodeAddress(addr) << outputIndex << coin;

    return put(db_, handleMap_.at(utxoStr.at(category)), key, value);
}

utxo_map WalletStore::GetAllUTXO(uint8_t category) {
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(utxoStr.at(category)));
    utxo_map result;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        uint256 utxokey;
        CKeyID addr;
        uint32_t outputIndex;
        uint64_t coinvalue;
        try {
            VStream key{iter->key().data(), iter->key().data() + iter->key().size()};
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            std::string addrstr;
            key >> utxokey;
            value >> addrstr >> outputIndex >> coinvalue;
            if (auto oAddr = DecodeAddress(addrstr)) {
                addr = *oAddr;
            }
        } catch (std::exception& e) {
            std::cout << e.what();
            std::cout << "err utxo\n";
        }
        result.emplace(utxokey, std::make_tuple(addr, outputIndex, coinvalue));
    }
    assert(iter->status().ok());
    delete iter;

    return result;
}

bool WalletStore::StoreUnspent(const uint256& utxokey, const CKeyID& addr, uint32_t outputIndex, uint64_t coin) {
    return StoreUTXO(utxokey, addr, outputIndex, coin, UNSPENT);
}
bool WalletStore::StorePending(const uint256& utxokey, const CKeyID& addr, uint32_t outputIndex, uint64_t coin) {
    return StoreUTXO(utxokey, addr, outputIndex, coin, PENDING);
}
bool WalletStore::StoreSpent(const uint256& utxokey, const CKeyID& addr, uint32_t outputIndex, uint64_t coin) {
    return StoreUTXO(utxokey, addr, outputIndex, coin, SPENT);
}

ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint64_t>> WalletStore::GetAllUnspent() {
    return GetAllUTXO(UNSPENT);
}
ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint64_t>> WalletStore::GetAllPending() {
    return GetAllUTXO(PENDING);
}
ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint64_t>> WalletStore::GetAllSpent() {
    return GetAllUTXO(SPENT);
}

int WalletStore::KeysToFile(std::string filePath) {
    // get all data
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(kKeyBook));
    std::vector<std::string> keyList;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        try {
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            std::string valuestr;
            value >> valuestr;
            keyList.emplace_back(valuestr);
        } catch (std::exception& e) {
            std::cout << "err file\n";
        }
    }
    assert(iter->status().ok());
    delete iter;

    std::string tmpPath = filePath + ".tmp";
    std::ofstream output{tmpPath};
    for (const auto& keystr : keyList) {
        output << keystr << std::endl;
    }
    output.close();

    return std::rename(tmpPath.c_str(), filePath.c_str());
}

void WalletStore::ClearOldData() {
    auto vecStr  = std::array{kTx, kUnspentTXO, kPendingTXO};
    auto vecDrop = std::vector{handleMap_.at(kTx), handleMap_.at(kUnspentTXO), handleMap_.at(kPendingTXO)};

    // delete old columns
    db_->DropColumnFamilies(vecDrop);
    for (auto& handle : vecDrop) {
        db_->DestroyColumnFamilyHandle(handle);
    }

    // Create column families
    std::vector<ColumnFamilyDescriptor> descriptors;
    for (const std::string& columnName : vecStr) {
        ColumnFamilyOptions cOptions;
        descriptors.emplace_back(ColumnFamilyDescriptor(columnName, cOptions));
    }
    std::vector<ColumnFamilyHandle*> vec;
    db_->CreateColumnFamilies(descriptors, &vec);

    auto name_it   = vecStr.cbegin();
    auto handle_it = vec.cbegin();
    while (name_it != vecStr.cend() && handle_it != vec.cend()) {
        handleMap_.at(*name_it) = *handle_it;
        name_it++;
        handle_it++;
    }
}
