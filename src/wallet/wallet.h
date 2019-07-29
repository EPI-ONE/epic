#ifndef EPIC_WALLET_H
#define EPIC_WALLET_H

#include "concurrent_container.h"
#include "consensus.h"
#include "dag_service.h"
#include "key.h"
#include "threadpool.h"
#include "wallet_store.h"

#include <atomic>
#include <utility>

constexpr uint64_t MIN_FEE = 1;

<<<<<<< HEAD
=======
enum { CKEY_ID = 0, OUTPUT_INDEX, COIN };

enum { UNSPENT = 0, PENDING = 1, SPENT = 2 };

using utxo_info = std::pair<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>>;

>>>>>>> 0232cd3... add methods of wallet store as well as print keys to a file
class Wallet : public OnLvsConfirmedInterface {
public:
    using UTXOKey     = uint256;
    using TxHash      = uint256;
    using OutputIndex = uint32_t;
    using utxo_info   = std::pair<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>>;
    enum { CKEY_ID = 0, OUTPUT_INDEX, COIN };

    Wallet(std::string walletPath, uint32_t period)
        : OnLvsConfirmedInterface(), threadPool_(2), walletStore_(walletPath), storePeriod_(period), totalBalance{0} {
        Load();
    }

    virtual ~Wallet() = default;

    void Start();
    void Stop();
    void Load();

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
    ConstTxPtr CreateFirstRegistration(const CKeyID&);
    ConstTxPtr CreateRedemption(CKeyID&, CKeyID&, std::string);
    ConstTxPtr CreateTx(const std::vector<std::pair<Coin, CKeyID>>& outputs, const Coin& fee = 0);

    Coin GetCurrentMinerReward() const;

    bool SendTxToMemPool(ConstTxPtr txPtr);

    const auto& GetUnspent() const {
        return unspent;
    }

    auto GetSpent() {
        return walletStore_.GetAllSpent();
    }

    const auto& GetPending() const {
        return pending;
    }

    const auto& GetPendingTx() const {
        return pendingTx;
    }

private:
    ConcurrentHashMap<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>> unspent, pending;
    ConcurrentHashMap<TxHash, ConstTxPtr> pendingTx;
    ConcurrentHashMap<CKeyID, std::pair<CKey, CPubKey>> keyBook;

    ThreadPool threadPool_;

    std::atomic<bool> stopFlag_;
    WalletStore walletStore_;
    uint32_t storePeriod_;

    std::pair<uint256, Coin> minerInfo_{uint256{}, Coin(0)};

    std::atomic<size_t> totalBalance;

    void AddInput(Transaction& tx, const utxo_info& utxo);

    void SpendUTXO(const UTXOKey& utxoKey);

    std::pair<Coin, std::vector<utxo_info>> Select(const Coin& amount) const;

    TxInput CreateSignedVin(const CKeyID&, TxOutPoint, const std::string&);

    void SendingStorageTask(uint32_t);
};

extern std::shared_ptr<Wallet> WALLET;
#endif // EPIC_WALLET_H
