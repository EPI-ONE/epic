#include "caterpillar.h"

Caterpillar::Caterpillar(std::string& dbPath) : dbStore_(dbPath), verifyThread_(1), obcThread_(1) {
    verifyThread_.Start();
    obcThread_.Start();
}

Caterpillar::~Caterpillar() {
    verifyThread_.Stop();
    obcThread_.Stop();
    dbStore_.~RocksDBStore();
    obc_.~OrphanBlocksContainer();
}

bool Caterpillar::StoreRecord(const RecordPtr& rec) const {
    return dbStore_.WriteRecord(rec);
}

bool Caterpillar::AddNewBlock(const ConstBlockPtr& blk, const Peer* peer) {
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

    bool mDB  = dbStore_.Exists(msHash);
    bool pDB  = dbStore_.Exists(prevHash);
    bool tDB  = dbStore_.Exists(tipHash);
    bool mOBC = obc_.IsOrphan(msHash);
    bool pOBC = obc_.IsOrphan(prevHash);
    bool tOBC = obc_.IsOrphan(tipHash);

    static auto mask = [](bool m, bool p, bool t) {
        // random comment to break clang format
        return ((m << 0) | (p << 2) | (t << 1));
    };

    // First, check if we already received its preceding blocks
    if ((mDB || mOBC) && (pDB || pOBC) && (tDB || tOBC)) {
        if (mOBC || pOBC || tOBC) {
            obcThread_.Execute(std::bind(&OrphanBlocksContainer::AddBlock, &obc_, blk, mask(mDB, pDB, tDB)));
            return false;
        }
    } else {
        // We have not received at least one of its parents.

        // Drop if the block is too old
        DBRecord ms = dbStore_.GetRecord(msHash);
        if (ms && !CheckPuntuality(blk, ms)) {
            return false;
        }
        // Abort and send GetBlock requests.
        obcThread_.Execute(std::bind(&OrphanBlocksContainer::AddBlock, &obc_, blk, mask(mDB, pDB, tDB)));

        if (peer) {
            DAG->RequestInv(Hash::GetZeroHash(), 5, peer);
        }

        return false;
    }

    // Check difficulty target ///////////////////////

    DBRecord ms = dbStore_.GetRecord(msHash);
    if (!ms) {
        spdlog::info(strprintf(
            "Block %s has missing milestone link %s", std::to_string(blk->GetHash()), std::to_string(msHash)));
        return false;
    }

    if (!(ms->snapshot)) {
        spdlog::info(strprintf("Block %s has invalid milestone link", std::to_string(blk->GetHash())));
    }

    uint32_t expectedTarget = ms->snapshot->blockTarget.GetCompact();
    if (blk->GetDifficultyTarget() != expectedTarget) {
        spdlog::info(strprintf("Block %s has unexpected change in difficulty: current %s v.s. expected %s",
            std::to_string(blk->GetHash()), blk->GetDifficultyTarget(), expectedTarget));
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
}

bool Caterpillar::CheckPuntuality(const ConstBlockPtr& blk, const DBRecord& ms) const {
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
        spdlog::info(strprintf("Block %s is too old.", std::to_string(blk->GetHash())));
        return false;
    }
    return true;
}

DBRecord Caterpillar::GetBlock(const uint256& blkHash) const {
    return dbStore_.GetRecord(blkHash);
}

void Caterpillar::ReleaseBlocks(const uint256& blkHash) {
    obcThread_.Execute([&blkHash, this]() {
        auto releasedBlocks = obc_.SubmitHash(blkHash);
        if (releasedBlocks) {
            for (const auto& blk : *releasedBlocks) {
                verifyThread_.Submit(std::bind(&Caterpillar::AddNewBlock, this, blk, nullptr));
            }
        }
    });
}
