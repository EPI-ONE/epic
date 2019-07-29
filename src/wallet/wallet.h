#ifndef EPIC_WALLET_H
#define EPIC_WALLET_H

#include "concurrent_container.h"
#include "consensus.h"
#include "dag_service.h"
#include "key.h"
#include "threadpool.h"

#include <utility>

constexpr uint64_t MIN_FEE = 1;

class Wallet : public OnLvsConfirmedInterface {
public:
    using UTXOKey     = uint256;
    using TxHash      = uint256;
    using OutputIndex = uint32_t;
    using utxo_info   = std::pair<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>>;
    enum { CKEY_ID = 0, OUTPUT_INDEX, COIN };

    Wallet() : totalBalance{0} {
        threadPool.SetThreadSize(2);
    }

    virtual ~Wallet() = default;

    void Start();

    void Stop();

    void OnLvsConfirmed(std::vector<RecordPtr> records,
                        std::unordered_set<UTXOPtr> UTXOs,
                        std::unordered_set<uint256> STXOs) override;

    void ProcessUTXO(const UTXOPtr& utxo);

    void ProcessSTXO(const UTXOKey& stxo);

    void ProcessRecord(const RecordPtr& record);

    const Coin GetBalance() const {
        return Coin(totalBalance.load());
    }

    CKeyID GetRandomAddress();

    void ImportKey(CKey& key, CPubKey& pubKey);

    CKeyID CreateNewKey(bool compressed);

    ConstTxPtr CreateRedemption(CKeyID targetAddr, CKeyID nextAddr, std::string msg);

    ConstTxPtr CreateFirstRegistration(CKeyID address);

    ConstTxPtr CreateTx(std::vector<std::pair<Coin, CKeyID>> outputs, const Coin& fee = 0);

    Coin GetCurrentMinerReward() const;

    bool SendTxToMemPool(ConstTxPtr txPtr);

    const auto& GetUnspent() const {
        return unspent;
    }

    const auto& GetSpent() const {
        return spent;
    }

    const auto& GetPending() const {
        return pending;
    }

    const auto& GetPendingTx() const {
        return pendingTx;
    }

private:
    ConcurrentHashMap<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>> unspent, pending, spent;

    ConcurrentHashMap<TxHash, ConstTxPtr> pendingTx;

    ConcurrentHashMap<CKeyID, std::pair<CKey, CPubKey>> keyBook;

    ThreadPool threadPool;

    // WalletStore walletStore_;

    std::pair<uint256, Coin> minerInfo_{uint256{}, Coin(0)};

    std::atomic<size_t> totalBalance;

    void AddInput(Transaction& tx, utxo_info& utxo);

    void SpendUTXO(UTXOKey utxoKey);

    std::pair<Coin, std::vector<utxo_info>> Select(const Coin& amount) const;

    TxInput CreateSignedVin(CKeyID targetAddr, TxOutPoint outpoint, std::string& msg);
};

extern std::shared_ptr<Wallet> WALLET;
#endif // EPIC_WALLET_H
