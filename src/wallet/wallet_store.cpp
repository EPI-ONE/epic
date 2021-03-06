// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet_store.h"
#include "big_uint.h"
#include "crypter.h"
#include "file_utils.h"
#include "key.h"
#include "spdlog/spdlog.h"
#include "stream.h"
#include "transaction.h"

#include <fstream>

using namespace rocksdb;
using utxo_map = ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>>;

enum { UNSPENT = 0, PENDING = 1, SPENT = 2 };
const std::string kKeyBook    = kDefaultColumnFamilyName;
const std::string kTx         = "pending_tx";
const std::string kUnspentTXO = "utxo";
const std::string kPendingTXO = "pending_utxo";
const std::string kSpentTXO   = "spent_txo";
const std::string kInfo       = "info";
const std::string kMasterInfo = "master_info";
const std::string kFirstReg   = "first_reg";
const std::string kLastRedem  = "last_redem_addr";
const std::string kMinerInfo  = "miner_info";

const std::array<std::string, 3> utxoStr{kUnspentTXO, kPendingTXO, kSpentTXO};

static const std::vector<std::string> COLUMN_NAMES = {
    kKeyBook, // (key) address
              // (value) {AES256CBC-encrypted private key (32B) + public key (65B)}
              // update on each changes

    kTx, // (key) transaction hash
         // (value) transaction instance; will become a ConstTxPtr when read out
         // pendingTx
         // force update periodically

    // (key) utxo key: outpoint hash ^ outpoint index
    // (value) {address (20B) + outpoint index (4B) + utxo coin value (8B)}
    kUnspentTXO, kPendingTXO, // force update periodically
    kSpentTXO,                // update on each changes

    kInfo // (key) name of the information
          // (value) serialized info
          // store encrypted information of master key
};

inline bool put(DB* db, ColumnFamilyHandle* handle, const VStream& key, const VStream& value) {
    return db->Put(WriteOptions(), handle, Slice{key.data(), key.size()}, Slice{value.data(), value.size()}).ok();
}

WalletStore::WalletStore(std::string dbPath) : RocksDB(std::move(dbPath), COLUMN_NAMES) {}

bool WalletStore::StoreTx(const Transaction& tx) const {
    VStream key{tx.GetHash()};
    VStream value{tx};
    return put(db_, handleMap_.at(kTx), key, value);
}

ConcurrentHashMap<uint256, ConstTxPtr> WalletStore::GetAllTx() const {
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(kTx));
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
            spdlog::info("Fail when reading tx from wallet store with error {}", e.what());
        }
        result.emplace(txhash, ptx);
    }
    assert(iter->status().ok());
    delete iter;

    return result;
}

bool WalletStore::StoreKeys(const CKeyID& addr, const CiphertextKey& encrypted, const CPubKey& pubkey) const {
    VStream key, value;
    key << EncodeAddress(addr);
    value << encrypted << pubkey;

    return put(db_, handleMap_.at(kKeyBook), key, value);
}

bool WalletStore::IsExistKey(const CKeyID& addr) const {
    VStream key;
    key << EncodeAddress(addr);
    return !RocksDB::Get(kKeyBook, Slice{key.data(), key.size()}).empty();
}

std::optional<std::tuple<CiphertextKey, CPubKey>> WalletStore::GetKey(const CKeyID& addr) const {
    VStream key;
    key << EncodeAddress(addr);
    PinnableSlice valueSlice;
    if (db_->Get(ReadOptions(), handleMap_.at(kKeyBook), Slice{key.data(), key.size()}, &valueSlice).ok()) {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            CiphertextKey privkey;
            CPubKey pubkey;
            value >> privkey >> pubkey;
            if (pubkey.IsFullyValid()) { // TODO: consider that do we need this check?
                return std::tuple{privkey, pubkey};
            }
        } catch (const std::exception& e) {
            spdlog::info("Fail when reading private keys from wallet store with error {}", e.what());
        }
    }

    return {};
}

ConcurrentHashMap<CKeyID, std::tuple<CiphertextKey, CPubKey>> WalletStore::GetAllKey() const {
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(kKeyBook));
    ConcurrentHashMap<CKeyID, std::tuple<CiphertextKey, CPubKey>> result;
    CKeyID addr;
    CiphertextKey privkey;
    CPubKey pubkey;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        try {
            VStream key{iter->key().data(), iter->key().data() + iter->key().size()};
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            std::string keystr;
            key >> keystr;
            value >> privkey >> pubkey;
            if (auto oAddr = DecodeAddress(keystr)) {
                addr = *oAddr;
                result.emplace(addr, std::forward_as_tuple(privkey, pubkey));
            }
        } catch (std::exception& e) {
            spdlog::info("Fail when reading private keys from wallet store with error {}", e.what());
        }
    }
    assert(iter->status().ok());
    delete iter;

    return result;
}

bool WalletStore::StoreUTXO(const uint256& utxokey,
                            const CKeyID& addr,
                            uint32_t txIndex,
                            uint32_t outputIndex,
                            uint64_t coin,
                            uint8_t category) const {
    VStream key{utxokey};
    VStream value;
    value << EncodeAddress(addr) << txIndex << outputIndex << coin;

    return put(db_, handleMap_.at(utxoStr.at(category)), key, value);
}

utxo_map WalletStore::GetAllUTXO(uint8_t category) const {
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(utxoStr.at(category)));
    utxo_map result;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        uint256 utxokey;
        CKeyID addr;
        uint32_t txIndex;
        uint32_t outputIndex;
        uint64_t coinvalue;
        try {
            VStream key{iter->key().data(), iter->key().data() + iter->key().size()};
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            std::string addrstr;
            key >> utxokey;
            value >> addrstr >> txIndex >> outputIndex >> coinvalue;
            if (auto oAddr = DecodeAddress(addrstr)) {
                addr = *oAddr;
            }
        } catch (std::exception& e) {
            spdlog::info("Fail when reading UTXO from wallet store with error {}", e.what());
        }
        result.emplace(utxokey, std::make_tuple(addr, txIndex, outputIndex, coinvalue));
    }
    assert(iter->status().ok());
    delete iter;

    return result;
}

bool WalletStore::StoreUnspent(
    const uint256& utxokey, const CKeyID& addr, uint32_t txIndex, uint32_t outputIndex, uint64_t coin) const {
    return StoreUTXO(utxokey, addr, txIndex, outputIndex, coin, UNSPENT);
}
bool WalletStore::StorePending(
    const uint256& utxokey, const CKeyID& addr, uint32_t txIndex, uint32_t outputIndex, uint64_t coin) const {
    return StoreUTXO(utxokey, addr, txIndex, outputIndex, coin, PENDING);
}
bool WalletStore::StoreSpent(
    const uint256& utxokey, const CKeyID& addr, uint32_t txIndex, uint32_t outputIndex, uint64_t coin) const {
    return StoreUTXO(utxokey, addr, txIndex, outputIndex, coin, SPENT);
}

ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> WalletStore::GetAllUnspent() const {
    return GetAllUTXO(UNSPENT);
}
ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> WalletStore::GetAllPending() const {
    return GetAllUTXO(PENDING);
}
ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> WalletStore::GetAllSpent() const {
    return GetAllUTXO(SPENT);
}

bool WalletStore::StoreMasterInfo(const MasterInfo& info) const {
    VStream key, value;
    key << kMasterInfo;
    value << info;
    return put(db_, handleMap_.at(kInfo), key, value);
}

std::optional<MasterInfo> WalletStore::GetMasterInfo() const {
    VStream key;
    key << kMasterInfo;
    PinnableSlice valueSlice;
    if (db_->Get(ReadOptions(), handleMap_.at(kInfo), Slice{key.data(), key.size()}, &valueSlice).ok()) {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            MasterInfo info{};
            value >> info;
            return info;
        } catch (const std::exception& e) {
            spdlog::info("Fail when reading master information from wallet store with error {}", e.what());
        }
    }
    return {};
}

bool WalletStore::StoreLastRedempInfo(const uint256& lastRedemHash, const CKeyID& lastRedemAddress) const {
    VStream key, value;
    key << kLastRedem;
    value << lastRedemHash << lastRedemAddress;
    return put(db_, handleMap_.at(kInfo), key, value);
}

bool WalletStore::StoreMinerInfo(const std::pair<uint256, Coin>& minerInfo) const {
    VStream key, value;
    key << kMinerInfo;
    value << minerInfo;
    return put(db_, handleMap_.at(kInfo), key, value);
}

std::optional<std::pair<uint256, CKeyID>> WalletStore::GetLastRedem() const {
    VStream key;
    key << kLastRedem;
    PinnableSlice valueSlice;
    if (db_->Get(ReadOptions(), handleMap_.at(kInfo), Slice{key.data(), key.size()}, &valueSlice).ok()) {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            uint256 lastRedemHash{};
            CKeyID lastRedemAddress{};
            value >> lastRedemHash >> lastRedemAddress;
            return {{lastRedemHash, lastRedemAddress}};
        } catch (const std::exception& e) {
            spdlog::info("Fail when reading last redemption hash from wallet store with error {}", e.what());
        }
    }
    return {};
}

std::optional<std::pair<uint256, Coin>> WalletStore::GetMinerInfo() const {
    VStream key{kMinerInfo};
    PinnableSlice valueSlice;
    if (db_->Get(ReadOptions(), handleMap_.at(kInfo), Slice{key.data(), key.size()}, &valueSlice).ok()) {
        try {
            VStream value(valueSlice.data(), valueSlice.data() + valueSlice.size());
            std::pair<uint256, Coin> minerInfo{};
            value >> minerInfo;
            return minerInfo;
        } catch (const std::exception& e) {
            spdlog::info("Fail when reading the miner info from wallet store with error {}", e.what());
        }
    }
    return {{uint256{}, 0}};
}

bool WalletStore::StoreFirstRegInfo() const {
    VStream key;
    return put(db_, handleMap_.at(kInfo), key, VStream{true});
}

bool WalletStore::GetFirstRegInfo() const {
    VStream key;
    return !RocksDB::Get(kInfo, Slice{key.data(), key.size()}).empty();
}

// get encoded pubkeys
int WalletStore::KeysToFile(std::string filePath) const {
    // get all data
    Iterator* iter = db_->NewIterator(ReadOptions(), handleMap_.at(kKeyBook));
    std::vector<std::string> keyList;
    std::vector<unsigned char> ciphertext;
    for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
        try {
            VStream value{iter->value().data(), iter->value().data() + iter->value().size()};
            std::string valuestr;
            value >> ciphertext >> valuestr;
            keyList.emplace_back(valuestr);
            ciphertext.clear();
        } catch (std::exception& e) {
            spdlog::info("Fail when writing private keys to file {} with error {}", filePath, e.what());
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
    auto vecStr = std::array{kTx, kUnspentTXO, kPendingTXO};
    for (auto& key : vecStr) {
        DeleteColumn(key);
        CreateColumn(key);
    }
}
