#include "caterpillar.h"

Caterpillar::Caterpillar(const std::string& dbPath) : dbStore_(dbPath), verifyThread_(1), obcThread_(1) {
    verifyThread_.Start();
    obcThread_.Start();
}

Caterpillar::~Caterpillar() {
    verifyThread_.Stop();
    obcThread_.Stop();
}

bool Caterpillar::StoreRecord(const RecordPtr& rec) const {
    return dbStore_.WriteRecord(rec);
}

bool Caterpillar::AddNewBlock(const ConstBlockPtr& blk, const Peer* peer) {
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

        static auto mask = [](bool m, bool p, bool t) { return ((!m << 0) | (!p << 2) | (!t << 1)); };

        // First, check if we already received its preceding blocks
        if (IsWeaklySolid(blk)) {
            if (AnyLinkIsOrphan(blk)) {
                obcThread_.Execute(std::bind(&OrphanBlocksContainer::AddBlock, &obc_, blk,
                    mask(IsSolid(msHash), IsSolid(prevHash), IsSolid(tipHash))));
                return false;
            }
        } else {
            // We have not received at least one of its parents.

            // Drop if the block is too old
            StoredRecord ms = GetBlock(msHash);
            if (ms && !CheckPuntuality(blk, ms)) {
                return false;
            }
            // Abort and send GetBlock requests.
            obcThread_.Execute(std::bind(&OrphanBlocksContainer::AddBlock, &obc_, blk,
                mask(IsSolid(msHash), IsSolid(prevHash), IsSolid(tipHash))));

            if (peer) {
                DAG->RequestInv(Hash::GetZeroHash(), 5, peer);
            }

            return false;
        }

        // Check difficulty target ///////////////////////

        StoredRecord ms = GetBlock(msHash);
        if (!ms) {
            spdlog::info("Block is missing milestone link {} [{}]", std::to_string(msHash),
                std::to_string(blk->GetHash()));
            return false;
        }

        if (!(ms->snapshot)) {
            spdlog::info("Block has invalid milestone link [{}]", std::to_string(blk->GetHash()));
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

bool Caterpillar::CheckPuntuality(const ConstBlockPtr& blk, const StoredRecord& ms) const {
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

    if (blk->GetTime() - ms->cBlock->GetTime() > params.punctualityThred) {
        spdlog::info("Block is too old [{}]", std::to_string(blk->GetHash()));
        return false;
    }
    return true;
}

StoredRecord Caterpillar::GetBlock(const uint256& blkHash) const {
    return dbStore_.GetRecord(blkHash);
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

bool Caterpillar::Exists(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash) || obc_.IsOrphan(blkHash);
}

bool Caterpillar::IsSolid(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash);
}

bool Caterpillar::IsWeaklySolid(const ConstBlockPtr& blk) const {
    return Exists(blk->GetMilestoneHash()) && Exists(blk->GetPrevHash()) && Exists(blk->GetTipHash());
}

bool Caterpillar::AnyLinkIsOrphan(const ConstBlockPtr& blk) const {
    return obc_.IsOrphan(blk->GetMilestoneHash()) || obc_.IsOrphan(blk->GetPrevHash()) ||
           obc_.IsOrphan(blk->GetTipHash());
}

void Caterpillar::Cache(const ConstBlockPtr& blk) const {
    dbStore_.WriteBlock(blk);
}

void Caterpillar::Stop() {
    while (obcThread_.GetTaskSize() > 0) {
        std::this_thread::yield();
    }
    obcThread_.Stop();

    while (verifyThread_.GetTaskSize() > 0) {
        std::this_thread::yield();
    }
    verifyThread_.Stop();
}
