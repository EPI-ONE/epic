#include "caterpillar.h"

#define EXISTS(hash) (dbStore_.Exists(hash) || obc_.IsOrphan(hash))
#define DBEXISTS(hash) (dbStore_.Exists(hash))
#define ALL_EXISTS(m, p, t) (EXISTS(m) && EXISTS(p) && EXISTS(t))
#define ANY_IS_ORPHAN(m, p, t) (obc_.IsOrphan(m) || obc_.IsOrphan(p) || obc_.IsOrphan(t))

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
            // clang-format on
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

            uint256 msHash   = blk->GetMilestoneHash();
            uint256 prevHash = blk->GetPrevHash();
            uint256 tipHash  = blk->GetTipHash();

            static auto mask = [](bool m, bool p, bool t) { return ((!m << 0) | (!p << 2) | (!t << 1)); };

            // First, check if we already received its preceding blocks
            if (ALL_EXISTS(msHash, prevHash, tipHash)) {
                if (ANY_IS_ORPHAN(msHash, prevHash, tipHash)) {
                    obcThread_.Execute(std::bind(&OrphanBlocksContainer::AddBlock, &obc_, blk,
                        mask(DBEXISTS(msHash), DBEXISTS(prevHash), DBEXISTS(tipHash))));
                    return false;
                }
            } else {
                // We have not received at least one of its parents.

                // Drop if the block is too old
                StoredRecord ms = dbStore_.GetRecord(msHash);
                if (ms && !CheckPuntuality(blk, ms)) {
                    return false;
                }
                // Abort and send GetBlock requests.
                obcThread_.Execute(std::bind(&OrphanBlocksContainer::AddBlock, &obc_, blk,
                    mask(DBEXISTS(msHash), DBEXISTS(prevHash), DBEXISTS(tipHash))));

                if (peer) {
                    DAG->RequestInv(Hash::GetZeroHash(), 5, peer);
                }

                return false;
            }

            // Check difficulty target ///////////////////////

            StoredRecord ms = dbStore_.GetRecord(msHash);
            if (!ms) {
                spdlog::info("Block has missing milestone link {} [{}]", std::to_string(msHash),
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

            dbStore_.WriteBlock(blk);
            DAG->AddBlockToPending(blk);
            ReleaseBlocks(blk->GetHash());

            return true;
        })
        .get();
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
