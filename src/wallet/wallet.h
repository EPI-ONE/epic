#ifndef EPIC_WALLET_H
#define EPIC_WALLET_H

#include "concurrent_container.h"
#include "consensus.h"
#include "dag_service.h"
#include "key.h"
#include "threadpool.h"

#include <utility>

using UTXOKey     = uint256;
using TxHash      = uint256;
using OutputIndex = uint32_t;

constexpr uint64_t MIN_FEE = 1;

enum { CKEY_ID = 0, OUTPUT_INDEX, COIN };

using utxo_info = std::pair<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>>;

class Wallet : public OnLvsConfirmedInterface {
public:
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

    void GenerateNewKey();

    const Coin GetBalance() const {
        return Coin(totalBalance.load());
    }

    CKeyID GetRandomAddress();

    void ImportKey(CKey& key, CPubKey& pubKey);

    CKeyID CreateNewKey(bool compressed);

    ConstTxPtr CreateRedemption(CKeyID targetAddr, CKeyID nextAddr, std::string& msg);
    ConstTxPtr CreateTx(std::vector<std::pair<Coin, CKeyID>> outputs);

private:
    ConcurrentHashMap<UTXOKey, std::tuple<CKeyID, OutputIndex, uint64_t>> unspent, pending, spent;

    ConcurrentHashMap<TxHash, ConstTxPtr> pendingTx;

    ConcurrentHashMap<CKeyID, std::pair<CKey, CPubKey>> keyBook;

    ThreadPool threadPool;

    std::pair<uint256, Coin> minerInfo_;

    std::atomic<size_t> totalBalance;

    void AddInput(Transaction& tx, utxo_info& utxo);

    void SpendUTXO(UTXOKey utxoKey);

    void UpdateBalance();

    void TraceTx();

    TxInput CreateSignedVin(CKeyID targetAddr, TxOutPoint outpoint, std::string& msg);
};

extern std::shared_ptr<Wallet> WALLET;
#endif // EPIC_WALLET_H
