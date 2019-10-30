// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mempool.h"
#include "dag_manager.h"

#include <algorithm>

#define READER_LOCK(mu) std::shared_lock<std::shared_mutex> reader(mu);
#define WRITER_LOCK(mu) std::unique_lock<std::shared_mutex> writer(mu);

bool MemPool::Insert(ConstTxPtr value) {
    WRITER_LOCK(mutex_)
    return std::get<1>(mempool_.emplace(std::move(value)));
}

bool MemPool::Contains(const ConstTxPtr& value) const {
    READER_LOCK(mutex_)
    return mempool_.find(value) != mempool_.end();
}

bool MemPool::Erase(const ConstTxPtr& value) {
    if (value) {
        WRITER_LOCK(mutex_)
        return mempool_.erase(value) > 0;
    }
    return false;
}

void MemPool::Erase(const std::vector<ConstTxPtr>& values) {
    WRITER_LOCK(mutex_)
    for (const auto& v : values) {
        mempool_.erase(v);
    }
}

std::size_t MemPool::Size() const {
    READER_LOCK(mutex_)
    return mempool_.size();
}

bool MemPool::Empty() const {
    return Size() == 0;
}

bool MemPool::ReceiveTx(const ConstTxPtr& tx) {
    // mempool only receives normal transaction
    if (tx->IsRegistration()) {
        return false;
    }

    // note that we allow transactions that have double spending with other tx in mempool
    // check the transaction is not from no spent TXOs
    if (!DAG->GetBestChain().IsTxFitsLedger(tx)) {
        return false;
    }

    return Insert(tx);
}

void MemPool::ReleaseTxFromConfirmed(const ConstTxPtr& tx, bool valid) {
    // first erase this transaction
    Erase(tx);
    if (!valid) {
        return;
    }

    // then prepare inputs data for searching
    auto& inputs = tx->GetInputs();
    std::unordered_set<uint256> fromTXOs;
    fromTXOs.reserve(inputs.size());
    for (const auto& input : inputs) {
        fromTXOs.emplace(input.outpoint.GetOutKey());
    }

    // finally erase conflicting transactions
    WRITER_LOCK(mutex_)
    for (auto iter = mempool_.cbegin(); iter != mempool_.cend();) {
        bool flag = false;
        for (const auto& input : (*iter)->GetInputs()) {
            if (fromTXOs.find(input.outpoint.GetOutKey()) != fromTXOs.end()) {
                flag = true;
                break;
            }
        }
        if (flag) {
            iter = mempool_.erase(iter);
        } else {
            iter++;
        }
    }
}

std::vector<ConstTxPtr> MemPool::ExtractTransactions(const uint256& blkHash, double threshold, size_t limit) {
    arith_uint256 base_hash = UintToArith256(blkHash);
    std::vector<ConstTxPtr> result;
    WRITER_LOCK(mutex_)

    auto it = mempool_.begin();
    while (it != mempool_.end() && result.size() < limit) {
        auto dist = base_hash ^ UintToArith256((*it)->GetHash());
        if (PartitionCmp(dist, threshold)) {
            result.emplace_back(*it);
            it = mempool_.erase(it);
        } else {
            ++it;
        }
    }

    if (!result.empty()) {
        spdlog::debug("Transactions {}are packed", [&result]() -> std::string {
            std::string txHashes{};
            for (auto& tx : result) {
                txHashes += tx->GetHash().to_substr();
                txHashes += " ";
            }
            return txHashes;
        }());
    }

    return result;
}

void MemPool::PushRedemptionTx(ConstTxPtr redemption) {
    READER_LOCK(mutex_)
    redemptionTxQueue_.Put(redemption);
}

ConstTxPtr MemPool::GetRedemptionTx(bool IsFirstReg) {
    READER_LOCK(mutex_)
    if (!redemptionTxQueue_.Empty()) {
        ConstTxPtr ptr;
        redemptionTxQueue_.Take(ptr);
        if (IsFirstReg && !ptr->IsFirstRegistration()) {
            return nullptr;
        }
        return ptr;
    }
    return nullptr;
}

void MemPool::ClearRedemptions() {
    redemptionTxQueue_.Clear();
}
