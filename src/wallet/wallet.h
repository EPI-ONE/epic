// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_WALLET_H
#define EPIC_WALLET_H

#include "concurrent_container.h"
#include "key.h"
#include "node.h"
#include "scheduler.h"
#include "tasm.h"
#include "threadpool.h"
#include "wallet_store.h"

#include <atomic>
#include <utility>

constexpr uint64_t MIN_FEE = 1;

std::optional<CKeyID> ParseAddrFromScript(const Tasm::Listing& content);

class Wallet {
private:
    using UTXOKey     = uint256;
    using TxHash      = uint256;
    using TxIndex     = uint32_t;
    using OutputIndex = uint32_t;
    using utxo_info   = std::pair<UTXOKey, std::tuple<CKeyID, TxIndex, OutputIndex, uint64_t>>;
    enum { CKEY_ID = 0, TX_INDEX, OUTPUT_INDEX, COIN };

public:
    Wallet(std::string walletPath, uint32_t period)
        : threadPool_(2), walletStore_(walletPath), storePeriod_(period), totalBalance_{0} {
        Load();
    }

    virtual ~Wallet() = default;

    void Start();
    void Stop();
    void Load();

    void OnLvsConfirmed(std::vector<RecordPtr> records,
                        std::unordered_map<uint256, UTXOPtr> UTXOs,
                        std::unordered_set<uint256> STXOs);

    CKeyID GetRandomAddress();

    void ImportKey(CKey& key, CPubKey& pubKey);

    CKey CreateNewKey(bool compressed);
    ConstTxPtr CreateFirstRegistration(const CKeyID&);
    ConstTxPtr CreateRedemption(const CKeyID&, const CKeyID&, const std::string&);
    ConstTxPtr CreateTx(const std::vector<std::pair<Coin, CKeyID>>& outputs, const Coin& fee = 0);
    ConstTxPtr CreateTxAndSend(const std::vector<std::pair<Coin, CKeyID>>& outputs, const Coin& fee = 0);

    void CreateRandomTx(size_t size);

    Coin GetCurrentMinerReward() const;

    bool SendTxToMemPool(ConstTxPtr txPtr);

    Coin GetBalance() const {
        return Coin(totalBalance_.load());
    }

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
    ConcurrentHashMap<UTXOKey, std::tuple<CKeyID, TxIndex, OutputIndex, uint64_t>> unspent, pending;
    ConcurrentHashMap<TxHash, ConstTxPtr> pendingTx;
    ConcurrentHashMap<TxHash, ConstTxPtr> pendingRedemption;
    ConcurrentHashMap<CKeyID, std::pair<CKey, CPubKey>> keyBook;

    ThreadPool threadPool_;

    WalletStore walletStore_;

    std::atomic_bool stopFlag_;
    uint32_t storePeriod_;
    Scheduler scheduler_;
    std::thread scheduleTask_;

    mutable std::shared_mutex lock_;
    std::pair<uint256, Coin> minerInfo_{uint256{}, Coin(0)};
    CKeyID lastRedemAddress_;

    std::atomic<size_t> totalBalance_;
    std::atomic_bool hasSentFirstRegistration_{false};

    void ProcessUTXO(const UTXOKey& utxokey, const UTXOPtr& utxo);
    void ProcessSTXO(const UTXOKey& stxo);
    void ProcessRecord(const RecordPtr& record);

    void AddInput(Transaction& tx, const utxo_info& utxo);

    void SpendUTXO(const UTXOKey& utxoKey);

    std::pair<Coin, std::vector<utxo_info>> Select(const Coin& amount) const;

    TxInput CreateSignedVin(const CKeyID&, TxOutPoint, const std::string&);

    void SendingStorageTask(uint32_t);

    bool CanRedeem();

    std::pair<uint256, Coin> GetMinerInfo() const;
    void UpdateMinerInfo(uint256 blockHash, const Coin& value);

    const CKeyID GetLastRedemAddress() const;
    void SetLastRedemAddress(const CKeyID& lastRedemAddress);
};

extern std::shared_ptr<Wallet> WALLET;
#endif // EPIC_WALLET_H
