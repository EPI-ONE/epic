#include "utxo.h"
#include "caterpillar.h"

//////////////////////
// UTXO
//
uint256 UTXO::GetContainingBlkHash() const {
    return output_.GetParentTx()->GetParentBlock()->GetHash();
}

uint256 UTXO::GetKey() const {
    return ComputeUTXOKey(GetContainingBlkHash(), index_);
}

uint64_t UTXO::HashCode() const {
    return std::hash<uint256>()(GetContainingBlkHash()) ^ index_;
}

//////////////////////
// TXOC
//
void TXOC::AddToCreated(const UTXOPtr& putxo) {
    increment_.Create(putxo->GetKey());
}

void TXOC::AddToCreated(const uint256& blkHash, uint32_t index) {
    increment_.Create(ComputeUTXOKey(blkHash, index));
}

void TXOC::AddToSpent(const TxInput& input) {
    auto& outpoint = input.outpoint;
    increment_.Remove(ComputeUTXOKey(outpoint.bHash, outpoint.index));
}

void TXOC::Merge(TXOC txoc) {
    increment_.Merge(std::move(txoc.increment_));
}

bool TXOC::Empty() {
    return increment_.GetCreated().empty() && increment_.GetRemoved().empty();
}

TXOC CreateTXOCFromInvalid(const Block& invalid) {
    const size_t nOuts = invalid.GetTransaction()->GetOutputs().size();
    std::unordered_set<uint256> invalidUTXO;
    invalidUTXO.reserve(nOuts);

    for (size_t i = 0; i < nOuts; i++) {
        invalidUTXO.emplace(ComputeUTXOKey(invalid.GetHash(), i));
    }
    return TXOC{{}, std::move(invalidUTXO)};
}

//////////////////////
// ChainLedger
//
void ChainLedger::AddToPending(UTXOPtr putxo) {
    pending_.insert({putxo->GetKey(), putxo});
}

UTXOPtr ChainLedger::GetFromPending(const uint256& xorkey) {
    auto query = pending_.find(xorkey);
    if (query != pending_.end()) {
        return query->second;
    }
    return nullptr;
}

UTXOPtr ChainLedger::FindSpendable(const uint256& xorkey) {
    auto query = removed_.find(xorkey);
    if (query != removed_.end()) {
        return nullptr; // nullptr as it is found in map of removed utxos
    }
    query = confirmed_.find(xorkey);
    if (query != confirmed_.end()) {
        return query->second;
    }
    return CAT->GetUTXO(xorkey);
}

UTXOPtr ChainLedger::FindFromLedger(const uint256& xorkey) {
    auto query = confirmed_.find(xorkey);
    if (query != confirmed_.end()) {
        return query->second;
    }
    query = removed_.find(xorkey);
    if (query != removed_.end()) {
        return query->second;
    }
    return nullptr; // should not happen
}

void ChainLedger::Invalidate(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetSpent()) {
        removed_.insert(pending_.extract(utxokey));
    }
}

void ChainLedger::Update(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetCreated()) {
        confirmed_.insert(pending_.extract(utxokey));
    }
    for (const auto& utxokey : txoc.GetSpent()) {
        removed_.insert(confirmed_.extract(utxokey));
    }
}

void ChainLedger::Remove(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetCreated()) {
        auto size = confirmed_.erase(utxokey);
        if (size == 0) {
            removed_.erase(utxokey);
        }
    }
    for (const auto& utxokey : txoc.GetSpent()) {
        removed_.erase(utxokey);
    }
}

void ChainLedger::Rollback(const TXOC& txoc) {
    for (const auto& utxokey : txoc.GetCreated()) {
        pending_.insert(confirmed_.extract(utxokey));
    }
    for (const auto& utxokey : txoc.GetSpent()) {
        confirmed_.insert(removed_.extract(utxokey));
    }
}

bool ChainLedger::IsSpendable(const uint256& utxokey) const {
    if (confirmed_.find(utxokey) != confirmed_.end()) {
        return true;
    }
    if (removed_.find(utxokey) != removed_.end()) {
        return false;
    }
    return CAT->ExistsUTXO(utxokey);
}

//////////////////////
// to_string methods
//
std::string std::to_string(const UTXO& utxo) {
    std::string s;
    s += "UTXO { \n";
    s += "   " + std::to_string(utxo.output_) + "with index " + std::to_string(utxo.index_);
    s += "   }";
    return s;
}

std::string std::to_string(const TXOC& txoc) {
    std::string s;
    s += "TXOC { \n";
    for (const auto& utxo : txoc.GetCreated()) {
        s += std::to_string(utxo);
    }
    for (const auto& utxo : txoc.GetSpent()) {
        s += std::to_string(utxo) + "\n";
    }
    s += "   }";
    return s;
}

std::string std::to_string(const ChainLedger& ledger) {
    std::string s;
    s += "Ledger { \n";

    s += strprintf("   pending utxo size: %i", ledger.pending_.size());
    if (!ledger.pending_.empty()) {
        s += "  {\n";
        for (const auto& ledgerPair : ledger.pending_) {
            s += std::to_string(*ledgerPair.second);
            s += "\n";
        }
        s += "   }\n";
    }

    s += strprintf("   confirmed utxo size: %i", ledger.confirmed_.size());
    if (!ledger.confirmed_.empty()) {
        s += "  {\n";
        for (const auto& ledgerPair : ledger.confirmed_) {
            s += std::to_string(*ledgerPair.second);
            s += "\n";
        }
        s += "   }\n";
    }

    s += strprintf("   removed utxo size: %i", ledger.removed_.size());
    if (!ledger.removed_.empty()) {
        s += "  {\n";
        for (const auto& ledgerPair : ledger.removed_) {
            s += std::to_string(*ledgerPair.second);
            s += "\n";
        }
        s += "   }\n";
    }

    s += "\n }";
    return s;
}
