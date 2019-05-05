#include "caterpillar.h"

Caterpillar::Caterpillar(std::string& dbPath) {
    dbStore_ = RocksDBStore(dbPath);
    thread_.Start();
}

bool Caterpillar::AddNewBlock(const BlockPtr& blk, const Peer& peer) {
    if (Exisis(blk->GetHash())) {
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
    if (Exisis(msHash) && Exisis(prevHash) && Exisis(tipHash)) {
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

        // TODO: Send sync request to DAG
        DAG.RequestInv(Hash::GetZeroHash(), 5, peer);

        return false;
    }

    // Check difficulty target ///////////////////////

    BlockPtr ms;
    if (!dbStore_.GetBlock(blk->GetMilestoneHash(), ms)) {
        spdlog::info(strprintf("Block %s has missing milestone link %s", blk->GetHash(), blk->GetMilestoneHash()));
        return false;
    }

    if (ms->GetMilestoneInstance() == nullptr) {
        spdlog::info(strprintf("Block %s has invalid milestone link", blk->GetHash()));
    }

    uint32_t expectedTarget = ms->GetMilestoneInstance()->blockTarget_.GetCompact();
    if (blk->GetDifficultyTarget() != expectedTarget) {
        spdlog::info(strprintf("Block %s has unexpected change in difficulty: current %s v.s. expected %s",
            blk->GetHash(), blk->GetDifficultyTarget(), expectedTarget));
        return false;
    }

    // Check punctuality /////////////////////////////

    if (!CheckPuntuality(blk, ms)) {
        return false;
    }

    // End of online verification
    //////////////////////////////////////////////////

    DAG.AddBlockToPending(blk);
    obc_.ReleaseBlocks(blk->GetHash());

    return true;
}

bool Caterpillar::CheckPuntuality(const BlockPtr& blk, BlockPtr ms) {
    if (ms == nullptr) {
        if (!dbStore_.GetBlock(blk->GetMilestoneHash(), ms)) {
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
        spdlog::info(strprintf("Block %s is too old.", blk->GetHash()));
        return false;
    }
    return true;
}

Caterpillar::~Caterpillar() {
    thread_.Stop();
    dbStore_.~RocksDBStore();
    obc_.~OrphanBlocksContainer();
}
