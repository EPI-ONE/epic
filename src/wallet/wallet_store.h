// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_WALLET_STORE_H
#define EPIC_WALLET_STORE_H

#include "concurrent_container.h"
#include "db_wrapper.h"

#include <optional>

class CKey;
class CKeyID;
class CPubKey;
class Transaction;
class uint256;

using CiphertextKey = std::vector<unsigned char>;

class WalletStore : public DBWrapper {
public:
    explicit WalletStore(std::string dbPath);

    bool StoreTx(const Transaction&);
    bool StoreKeys(const CKeyID& addr, const CiphertextKey& encrypted, const CPubKey& pubkey);
    bool StoreUnspent(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t);
    bool StorePending(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t);
    bool StoreSpent(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t);

    bool IsExistKey(const CKeyID&);
    std::optional<std::tuple<CiphertextKey, CPubKey>> GetKey(const CKeyID&);

    ConcurrentHashMap<uint256, std::shared_ptr<const Transaction>> GetAllTx();
    ConcurrentHashMap<CKeyID, std::tuple<CiphertextKey, CPubKey>> GetAllKey();
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllUnspent();
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllPending();
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllSpent();

    // returns 0 upon success or non-zero value on error.
    int KeysToFile(std::string filePath);
    void ClearOldData();

private:
    bool StoreUTXO(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t, uint8_t);
    ConcurrentHashMap<uint256, std::tuple<CKeyID, uint32_t, uint32_t, uint64_t>> GetAllUTXO(uint8_t);
};

#endif // EPIC_WALLET_STORE_H
