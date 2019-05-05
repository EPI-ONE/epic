#include "caterpillar.h"

// TODO: add real functions
bool OrphanBlocksContainer::Add(const BlockPtr& b) {
    return false;
}

void OrphanBlocksContainer::Remove(const BlockPtr& b) {}

bool OrphanBlocksContainer::Contains(const uint256& blockHash) const {
    return false;
}

void OrphanBlocksContainer::ReleaseBlocks(const uint256& blockHash) {}
// end of TODO

OrphanBlocksContainer::~OrphanBlocksContainer() {
    thread_.Stop();
    ODict_.clear();
    solidDegree_.clear();
}

Caterpillar::Caterpillar(std::string& dbPath) : dbStore_(dbPath), thread_(1) {
    thread_.Start();
}

bool Caterpillar::AddNewBlock(const BlockPtr& blk, const Peer& peer) {
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

    // First, check if we already received its preceding blocks
    if (Exists(msHash) && Exists(prevHash) && Exists(tipHash)) {
        if (obc_.Contains(msHash) || obc_.Contains(prevHash) || obc_.Contains(tipHash)) {
            obc_.Add(blk);
            return false;
        }
    } else {
        // We have not received at least one of its parents.

        // Drop if the block is too old
        if (dbStore_.Exists(msHash) && !CheckPuntuality(blk)) {
            return false;
        }
        // Abort and send GetBlock requests.
        obc_.Add(blk);

        DAG.RequestInv(Hash::GetZeroHash(), 5, peer);

        return false;
    }

    // Check difficulty target ///////////////////////

    std::unique_ptr<Block> ms = dbStore_.GetBlock(msHash);
    if (ms == nullptr) {
        spdlog::info(strprintf("Block %s has missing milestone link %s", std::to_string(blk->GetHash()),
            std::to_string(blk->GetMilestoneHash())));
        return false;
    }

    if (ms->GetMilestoneInstance() == nullptr) {
        spdlog::info(strprintf("Block %s has invalid milestone link", std::to_string(blk->GetHash())));
    }

    uint32_t expectedTarget = ms->GetMilestoneInstance()->blockTarget_.GetCompact();
    if (blk->GetDifficultyTarget() != expectedTarget) {
        spdlog::info(strprintf("Block %s has unexpected change in difficulty: current %s v.s. expected %s",
            std::to_string(blk->GetHash()), blk->GetDifficultyTarget(), expectedTarget));
        return false;
    }

    // Check punctuality /////////////////////////////

    if (!CheckPuntuality(blk, std::move(ms))) {
        return false;
    }

    // End of online verification
    //////////////////////////////////////////////////

    DAG.AddBlockToPending(blk);
    obc_.ReleaseBlocks(blk->GetHash());

    return true;
}

bool Caterpillar::CheckPuntuality(const BlockPtr& blk, std::unique_ptr<Block> ms) const {
    if (ms == nullptr) {
        ms.reset(dbStore_.GetBlock(blk->GetMilestoneHash()).release());
        if (ms == nullptr) {
            // Should not happen
            return false;
        }
    }

    if (blk->IsFirstRegistration()) {
        return true;
    }

    if (blk->GetMilestoneHash() == GENESIS.GetHash()) {
        return true;
    }

    if (blk->GetTime() - ms->GetTime() > params.punctualityThred) {
        spdlog::info(strprintf("Block %s is too old.", std::to_string(blk->GetHash())));
        return false;
    }
    return true;
}

Caterpillar::~Caterpillar() {
    thread_.Stop();
    dbStore_.~RocksDBStore();
    obc_.~OrphanBlocksContainer();
}
