#ifndef __SRC_WALLET_WALLET_STORE__
#define __SRC_WALLET_WALLET_STORE__

#include "concurrent_container.h"
#include "db_wrapper.h"

#include <optional>

class CKey;
class CKeyID;
class Transaction;
class uint256;

class WalletStore : public DBWrapper {
public:
    explicit WalletStore(std::string dbPath);

    bool StoreTx(const Transaction&);
    bool StoreKeys(const CKeyID&, const CKey&); // TODO: add encryption
    bool StoreUnspent(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t);
    bool StorePending(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t);
    bool StoreSpent(const uint256&, const CKeyID&, uint32_t, uint32_t, uint64_t);

    bool IsExistKey(const CKeyID&);
    std::optional<CKey> GetKey(const CKeyID&);

    ConcurrentHashMap<uint256, std::shared_ptr<const Transaction>> GetAllTx();
    ConcurrentHashMap<CKeyID, CKey> GetAllKey();
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

#endif // __SRC_WALLET_WALLET_STORE__
