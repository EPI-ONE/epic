// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_WALLET_STORE_H
#define EPIC_WALLET_STORE_H

#include "coin.h"
#include "concurrent_container.h"
#include "rocksdb.h"

#include <optional>

class CKey;
class CKeyID;
class MasterInfo;
class CPubKey;
class Transaction;
class uint256;

using CiphertextKey = std::vector<unsigned char>;

class WalletStore : public RocksDB {
public:
    explicit WalletStore(std::string dbPath);

    bool StoreTx(const Transaction&) const;
    bool StoreKeys(const CKeyID& addr, const CiphertextKey& encrypted, const CPubKey& pubkey) const;
    bool StoreUnspent(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t) const;
    bool StorePending(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t) const;
    bool StoreSpent(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t) const;
    bool StoreMasterInfo(const MasterInfo&) const;
    bool StoreFirstRegInfo() const;
    bool StoreLastRedempInfo(const uint256&, const CKeyID&) const;
    bool StoreMinerInfo(const std::pair<uint256, Coin>&) const;

    bool IsExistKey(const CKeyID&) const;
    std::optional<std::tuple<CiphertextKey, CPubKey>> GetKey(const CKeyID&) const;

    ConcurrentHashMap<uint256, std::shared_ptr<const Transaction>> GetAllTx() const;
    ConcurrentHashMap<CKeyID, std::tuple<CiphertextKey, CPubKey>> GetAllKey() const;
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllUnspent() const;
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllPending() const;
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllSpent() const;

    std::optional<MasterInfo> GetMasterInfo() const;
    bool GetFirstRegInfo() const;
    std::optional<std::pair<uint256, CKeyID>> GetLastRedem() const;
    std::optional<std::pair<uint256, Coin>> GetMinerInfo() const;

    // returns 0 upon success or non-zero value on error.
    int KeysToFile(std::string filePath) const;
    void ClearOldData();

private:
    bool StoreUTXO(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t, uint8_t) const;
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllUTXO(uint8_t) const;
};

#endif // EPIC_WALLET_STORE_H
