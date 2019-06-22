#include "caterpillar.h"

Caterpillar::Caterpillar(const std::string& dbPath)
    : verifyThread_(1), obcThread_(1), dbStore_(dbPath), obcEnabled_(false) {
    verifyThread_.Start();
    obcThread_.Start();
    file::SetDataDirPrefix(dbPath);
}

Caterpillar::~Caterpillar() {
    verifyThread_.Stop();
    obcThread_.Stop();
}

bool Caterpillar::StoreRecord(const RecordPtr& rec) const {
    try {
        // Write files
        FilePos blkPos{currentBlkEpoch_, currentBlkName_, currentBlkSize_};
        FileWriter fs1{file::BLK, blkPos};
        fs1 << *(rec->cblock);

        FilePos recPos{currentRecEpoch_, currentRecName_, currentRecSize_};
        FileWriter fs2{file::BLK, blkPos};
        fs2 << *(rec);

        // Write db
    } catch (const std::exception&) {
        return false;
    }
    return true;
}

std::unique_ptr<UTXO> Caterpillar::GetTransactionOutput(const uint256&) {
    return nullptr;
}

bool Caterpillar::AddNewBlock(const ConstBlockPtr& blk, std::shared_ptr<Peer> peer) {
    // clang-format off
    return verifyThread_.Submit([&blk, peer, this]() {
        if (*blk == GENESIS) {
            return false;
        }

        if (Exists(blk->GetHash())) {
            return false;
        }

        //////////////////////////////////////////////////
        // Start of online verification

        if (!blk->Verify()) {
            return false;
        }

        // Check solidity ////////////////////////////////

        const uint256& msHash   = blk->GetMilestoneHash();
        const uint256& prevHash = blk->GetPrevHash();
        const uint256& tipHash  = blk->GetTipHash();

        static auto mask = [](bool m, bool p, bool t) -> uint8_t { return ((!m << 0) | (!p << 2) | (!t << 1)); };

        // First, check if we already received its preceding blocks
        if (IsWeaklySolid(blk)) {
            if (AnyLinkIsOrphan(blk)) {
                AddBlockToOBC(blk, mask(DAGExists(msHash), DAGExists(prevHash), DAGExists(tipHash)));
                return false;
            }
        } else {
            // We have not received at least one of its parents.

            // Drop if the block is too old
            RecordPtr ms = DAG->GetState(msHash);
            if (ms && !CheckPuntuality(blk, ms)) {
                return false;
            }
            AddBlockToOBC(blk, mask(DAGExists(msHash), DAGExists(prevHash), DAGExists(tipHash)));

            // Abort and send GetInv requests.
            if (peer) {
                DAG->RequestInv(uint256(), 5, peer);
            }

            return false;
        }

        // Check difficulty target ///////////////////////

        RecordPtr ms = DAG->GetState(msHash);
        if (!ms) {
            spdlog::info("Block has missing or invalid milestone link [{}]", std::to_string(blk->GetHash()));
            return false;
        }

        uint32_t expectedTarget = ms->snapshot->blockTarget.GetCompact();
        if (blk->GetDifficultyTarget() != expectedTarget) {
            spdlog::info("Block has unexpected change in difficulty: current {} v.s. expected {} [{}]",
                         blk->GetDifficultyTarget(), expectedTarget);
            return false;
        }

        // Check punctuality /////////////////////////////

        if (!CheckPuntuality(blk, ms)) {
            return false;
        }

        // End of online verification
        //////////////////////////////////////////////////

        Cache(blk);
        DAG->AddBlockToPending(blk);
        ReleaseBlocks(blk->GetHash());

        return true;
    }).get();
    // clang-format on
}


bool Caterpillar::CheckPuntuality(const ConstBlockPtr& blk, const RecordPtr& ms) const {
    if (ms == nullptr) {
        // Should not happen
        return false;
    }

    if (blk->IsFirstRegistration()) {
        return true;
    }

    if (blk->GetMilestoneHash() == GENESIS.GetHash()) {
        return true;
    }

    if (blk->GetTime() - ms->cblock->GetTime() > GetParams().punctualityThred) {
        spdlog::info("Block is too old [{}]", std::to_string(blk->GetHash()));
        return false;
    }
    return true;
}

void Caterpillar::AddBlockToOBC(const ConstBlockPtr& blk, const uint8_t& mask) {
    obcThread_.Execute([blk, mask, this]() {
        if (!obcEnabled_.load()) {
            return;
        }
        obc_.AddBlock(blk, mask);
    });
}

void Caterpillar::ReleaseBlocks(const uint256& blkHash) {
    obcThread_.Execute([&blkHash, this]() {
        auto releasedBlocks = obc_.SubmitHash(blkHash);
        if (releasedBlocks) {
            for (const auto& blk : *releasedBlocks) {
                AddNewBlock(blk, nullptr);
            }
        }
    });
}

void Caterpillar::EnableOBC() {
    static bool flag = false;
    if (obcEnabled_.compare_exchange_strong(flag, true)) {
        spdlog::info("OBC enabled.");
    }
}

void Caterpillar::DisableOBC() {
    static bool flag = true;
    if (obcEnabled_.compare_exchange_strong(flag, false)) {
        spdlog::info("OBC disabled.");
    }
}

StoredRecord Caterpillar::GetMilestoneAt(size_t height) const {
    return dbStore_.GetMsRecordAt(height);
}

StoredRecord Caterpillar::GetRecord(const uint256& blkHash) const {
    return dbStore_.GetRecord(blkHash);
}

ConstBlockPtr Caterpillar::GetBlockCache(const uint256& blkHash) const {
    try {
        return blockCache_.at(blkHash);
    } catch (std::exception&) {
        return nullptr;
    }
}

std::vector<StoredRecord> Caterpillar::GetLevelSetAt(size_t height) const {
    return dbStore_.GetLevelSetAt(height);
}

size_t Caterpillar::GetHeight(const uint256& blkHash) const {
    return dbStore_.GetHeight(blkHash);
}

bool Caterpillar::Exists(const uint256& blkHash) const {
    return blockCache_.contains(blkHash) || dbStore_.Exists(blkHash) || obc_.IsOrphan(blkHash);
}

bool Caterpillar::DAGExists(const uint256& blkHash) const {
    return blockCache_.contains(blkHash) || dbStore_.Exists(blkHash);
}

bool Caterpillar::DBExists(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash);
}

bool Caterpillar::IsMilestone(const uint256& blkHash) const {
    return dbStore_.IsMilestone(blkHash);
}

bool Caterpillar::IsWeaklySolid(const ConstBlockPtr& blk) const {
    return Exists(blk->GetMilestoneHash()) && Exists(blk->GetPrevHash()) && Exists(blk->GetTipHash());
}

bool Caterpillar::IsSolid(const ConstBlockPtr& blk) const {
    return DAGExists(blk->GetMilestoneHash()) && DAGExists(blk->GetPrevHash()) && DAGExists(blk->GetTipHash());
}

bool Caterpillar::AnyLinkIsOrphan(const ConstBlockPtr& blk) const {
    return obc_.IsOrphan(blk->GetMilestoneHash()) || obc_.IsOrphan(blk->GetPrevHash()) ||
           obc_.IsOrphan(blk->GetTipHash());
}

void Caterpillar::Cache(const ConstBlockPtr& blk) {
    blockCache_.emplace(blk->GetHash(), blk);
}

void Caterpillar::Stop() {
    while (verifyThread_.GetTaskSize() > 0 || obcThread_.GetTaskSize() > 0) {
        std::this_thread::yield();
    }
    obcThread_.Stop();
    verifyThread_.Stop();
}
