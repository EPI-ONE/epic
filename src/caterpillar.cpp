#include "caterpillar.h"

Caterpillar::Caterpillar(std::string dbPath) : verifyThread_(1), obcThread_(1), dbStore_(dbPath), obcEnabled_(false) {
    verifyThread_.Start();
    obcThread_.Start();
    file::SetDataDirPrefix(dbPath + "/");
}

Caterpillar::~Caterpillar() {
    verifyThread_.Stop();
    obcThread_.Stop();
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

ConstBlockPtr Caterpillar::GetBlockCache(const uint256& blkHash) const {
    try {
        return blockCache_.at(blkHash);
    } catch (std::exception&) {
        return nullptr;
    }
}

StoredRecord Caterpillar::GetMilestoneAt(size_t height) const {
    return ConstructNRFromFile(dbStore_.GetMsPos(height));
}

StoredRecord Caterpillar::GetRecord(const uint256& blkHash) const {
    return ConstructNRFromFile(dbStore_.GetRecordPos(blkHash));
}

StoredRecord Caterpillar::ConstructNRFromFile(std::optional<std::pair<FilePos, FilePos>>&& value) const {
    if (!value) {
        return nullptr;
    }

    auto [blkPos, recPos] = *value;

    FileReader blkReader{file::BLK, blkPos};
    Block blk{};
    blkReader >> blk;


    StoredRecord record = std::make_unique<NodeRecord>(std::move(blk));

    FileReader recReader{file::REC, recPos};
    recReader >> *record;

    return record;
}

std::vector<ConstBlockPtr> Caterpillar::GetLevelSetAt(size_t height) const {
    auto vs = GetRawLevelSetAt(height);
    std::vector<ConstBlockPtr> blocks;
    while (!vs->empty()) {
        blocks.emplace_back(std::make_shared<const Block>(*vs));
    }
    return blocks;
}

std::unique_ptr<VStream> Caterpillar::GetRawLevelSetAt(size_t height) const {
    return GetRawLevelSetBetween(height, height);
}

std::unique_ptr<VStream> Caterpillar::GetRawLevelSetBetween(size_t height1, size_t height2) const {
    assert(height1 <= height2);
    auto leftPos  = dbStore_.GetMsBlockPos(height1);
    auto rightPos = dbStore_.GetMsBlockPos(height2 + 1);

    auto result = std::make_unique<VStream>();
    if (!leftPos) {
        return result;
    }

    FileReader reader(file::BLK, *leftPos);
    auto leftOffset  = leftPos->nOffset;
    auto rightOffset = rightPos ? rightPos->nOffset : 0;

    if (rightPos && leftPos->SameFileAs(*rightPos)) {
        reader.read(rightOffset - leftOffset, *result);
        return result;
    }

    // Read all of the first file
    auto size = reader.Size();
    reader.read(size - leftOffset, *result);
    reader.Close();

    if (rightPos) {
        // Read files between leftPos and rightPos (exclusive)
        auto file = NextFile(*leftPos);
        while (file < *rightPos && !file.SameFileAs(*rightPos)) {
            FileReader cursor(file::BLK, file);
            size = cursor.Size();
            cursor.read(size, *result);
            NextFile(file);
        }

        // Read the last file
        FileReader cursor(file::BLK, file);
        cursor.read(rightOffset, *result);
        return result;
    }

    // Read all the rest of files
    static const size_t nFilesMax = 20;

    auto file     = NextFile(*leftPos);
    size_t nFiles = 0;
    while (CheckFileExist(file::GetFilePath(file::BLK, file)) && nFiles < nFilesMax) {
        FileReader cursor(file::BLK, file);
        size = cursor.Size();
        cursor.read(size, *result);
        NextFile(file);
        nFiles++;
    }
    return result;
}

size_t Caterpillar::GetHeight(const uint256& blkHash) const {
    return dbStore_.GetHeight(blkHash);
}

std::unique_ptr<UTXO> Caterpillar::GetTransactionOutput(const uint256&) {
    return nullptr;
}

bool Caterpillar::StoreRecords(const std::vector<RecordPtr>& lvs) {
    try {
        // Function to sum up storage sizes for blk and rec in this lvs
        auto sumSize = [](const std::pair<uint32_t, uint32_t>& prevSum,
                          const RecordPtr& rec) -> std::pair<uint32_t, uint32_t> {
            return std::make_pair(prevSum.first + rec->cblock->GetOptimalEncodingSize(),
                                  prevSum.second + rec->GetOptimalStorageSize());
        };

        // std::pair<total block size, total record size>
        std::pair<uint32_t, uint32_t> totalSize =
            std::accumulate(lvs.begin(), lvs.end(), std::make_pair(0, 0), sumSize);

        CarryOverFileName(totalSize);

        FilePos msBlkPos{loadCurrentBlkEpoch(), loadCurrentBlkName(), loadCurrentBlkSize()};
        FilePos msRecPos{loadCurrentRecEpoch(), loadCurrentRecName(), loadCurrentRecSize()};
        FileWriter blkFs{file::BLK, msBlkPos};
        FileWriter recFs{file::REC, msRecPos};

        uint32_t blkOffset;
        uint32_t recOffset;
        uint32_t msBlkOffset = msBlkPos.nOffset;
        uint32_t msRecOffset = msRecPos.nOffset;

        uint64_t height = lvs.front()->snapshot->height;

        for (const auto& rec : lvs) {
            // Write to file
            blkOffset = blkFs.GetOffset() - msBlkOffset;
            recOffset = recFs.GetOffset() - msRecOffset;
            blkFs << *(rec->cblock);
            blkFs.Flush();
            recFs << *rec;
            recFs.Flush();

            // Write positions to db
            dbStore_.WriteRecPos(rec->cblock->GetHash(), height, blkOffset, recOffset);
        }

        // Write ms position at last to enable search for all blocks in the lvs
        dbStore_.WriteMsPos(height, lvs.front()->cblock->GetHash(), msBlkPos, msRecPos);

        AddCurrentSize(totalSize);

    } catch (const std::exception&) {
        return false;
    }

    return true;
}

bool Caterpillar::Exists(const uint256& blkHash) const {
    return blockCache_.contains(blkHash) || obc_.IsOrphan(blkHash) || dbStore_.Exists(blkHash);
}

bool Caterpillar::DAGExists(const uint256& blkHash) const {
    return blockCache_.contains(blkHash) || dbStore_.Exists(blkHash);
}

bool Caterpillar::DBExists(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash) || blkHash == GENESIS.GetHash();
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

void Caterpillar::SetFileCapacities(uint32_t fileCapacity, uint16_t epochCapacity) {
    fileCapacity_  = fileCapacity;
    epochCapacity_ = epochCapacity;
}

uint32_t Caterpillar::loadCurrentBlkEpoch() {
    return currentBlkEpoch_.load(std::memory_order_seq_cst);
}

uint32_t Caterpillar::loadCurrentRecEpoch() {
    return currentRecEpoch_.load(std::memory_order_seq_cst);
}

uint16_t Caterpillar::loadCurrentBlkName() {
    return currentBlkName_.load(std::memory_order_seq_cst);
}

uint16_t Caterpillar::loadCurrentRecName() {
    return currentRecName_.load(std::memory_order_seq_cst);
}

uint32_t Caterpillar::loadCurrentBlkSize() {
    return currentBlkSize_.load(std::memory_order_seq_cst);
}

uint32_t Caterpillar::loadCurrentRecSize() {
    return currentRecSize_.load(std::memory_order_seq_cst);
}

void Caterpillar::CarryOverFileName(std::pair<uint32_t, uint32_t> addon) {
    if (loadCurrentBlkSize() > 0 && loadCurrentBlkSize() + addon.first > fileCapacity_) {
        currentBlkName_.fetch_add(1, std::memory_order_seq_cst);
        currentBlkSize_.store(0, std::memory_order_seq_cst);
        if (loadCurrentBlkName() == epochCapacity_) {
            currentBlkEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentBlkName_.store(0, std::memory_order_seq_cst);
        }
    }

    if (loadCurrentRecSize() > 0 && loadCurrentRecSize() + addon.second > fileCapacity_) {
        currentRecName_.fetch_add(1, std::memory_order_seq_cst);
        currentRecSize_.store(0, std::memory_order_seq_cst);
        if (loadCurrentRecName() == epochCapacity_) {
            currentRecEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentRecName_.store(0, std::memory_order_seq_cst);
        }
    }
}

void Caterpillar::AddCurrentSize(std::pair<uint32_t, uint32_t> size) {
    currentBlkSize_.fetch_add(size.first, std::memory_order_seq_cst);
    currentRecSize_.fetch_add(size.second, std::memory_order_seq_cst);
}

FilePos& Caterpillar::NextFile(FilePos& pos) const {
    if (pos.nName == epochCapacity_ - 1) {
        pos.nName = 0;
        pos.nEpoch++;
    } else {
        pos.nName++;
    }
    pos.nOffset = 0;
    return pos;
}
