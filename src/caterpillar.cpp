#include "caterpillar.h"

Caterpillar::Caterpillar(const std::string& dbPath) : obcThread_(1), dbStore_(dbPath), obcEnabled_(false) {
    obcThread_.Start();

    currentBlkEpoch_ = dbStore_.GetInfo<uint32_t>("blkE");
    currentRecEpoch_ = dbStore_.GetInfo<uint32_t>("recE");
    currentBlkName_  = dbStore_.GetInfo<uint16_t>("blkN");
    currentRecName_  = dbStore_.GetInfo<uint16_t>("recN");
    currentBlkSize_  = dbStore_.GetInfo<uint32_t>("blkS");
    currentRecSize_  = dbStore_.GetInfo<uint32_t>("recS");
}

Caterpillar::~Caterpillar() {
    obcThread_.Stop();
}

void Caterpillar::AddBlockToOBC(const ConstBlockPtr& blk, const uint8_t& mask) {
    obcThread_.Execute([blk, mask, this]() {
        spdlog::trace("AddBlockToOBC {}", blk->GetHash().to_substr());
        if (!obcEnabled_.load()) {
            return;
        }
        obc_.AddBlock(blk, mask);
    });
}

void Caterpillar::ReleaseBlocks(const uint256& blkHash) {
    obcThread_.Execute([blkHash, this]() {
        auto releasedBlocks = obc_.SubmitHash(blkHash);
        if (releasedBlocks) {
            for (auto& blk : *releasedBlocks) {
                DAG->AddNewBlock(blk, nullptr);
            }
        }
    });
}

void Caterpillar::EnableOBC() {
    bool flag = false;
    if (obcEnabled_.compare_exchange_strong(flag, true)) {
        spdlog::info("OBC enabled.");
    }
}

void Caterpillar::DisableOBC() {
    bool flag = true;
    if (obcEnabled_.compare_exchange_strong(flag, false)) {
        spdlog::info("OBC disabled.");
    }
}

ConstBlockPtr Caterpillar::GetBlockCache(const uint256& blkHash) const {
    auto cache_iter = blockCache_.find(blkHash);
    if (cache_iter != blockCache_.end()) {
        return cache_iter->second;
    }

    return nullptr;
}

ConstBlockPtr Caterpillar::FindBlock(const uint256& blkHash) const {
    if (auto cache = GetBlockCache(blkHash)) {
        return cache;
    }

    if (dbStore_.Exists(blkHash)) {
        return GetRecord(blkHash)->cblock;
    }

    return nullptr;
}

RecordPtr Caterpillar::GetMilestoneAt(size_t height) const {
    RecordPtr rec = ConstructNRFromFile(dbStore_.GetMsPos(height));
    rec->snapshot->PushBlkToLvs(rec);
    return rec;
}

RecordPtr Caterpillar::GetRecord(const uint256& blkHash, bool withBlock) const {
    RecordPtr rec = ConstructNRFromFile(dbStore_.GetRecordPos(blkHash), withBlock);
    if (rec && rec->isMilestone) {
        rec->snapshot->PushBlkToLvs(rec);
    }
    return rec;
}

std::vector<RecordPtr> Caterpillar::GetLevelSetRecsAt(size_t height, bool withBlock) const {
    // Get recs
    auto vs = GetRawLevelSetAt(height, file::FileType::REC);

    if (vs.empty()) {
        return {};
    }

    std::vector<RecordPtr> result;
    while (vs.in_avail()) {
        result.emplace_back(std::make_shared<NodeRecord>(vs));
    }

    assert(!result.empty());
    const auto& ms = result[0];
    for (const auto& b : result) {
        ms->snapshot->PushBlkToLvs(b);
    }

    // Get cblocks
    if (withBlock) {
        auto levelSetBlocks = GetLevelSetBlksAt(height);

        assert(result.size() == levelSetBlocks.size());
        for (size_t i = 0; i < result.size(); ++i) {
            result[i]->cblock = std::move(levelSetBlocks[i]);
        }
    }

    return result;
}

StoredRecord Caterpillar::ConstructNRFromFile(std::optional<std::pair<FilePos, FilePos>>&& value,
                                              bool withBlock) const {
    if (!value) {
        StoredRecord ret{nullptr, [](NodeRecord* ptr) {}};
        return ret;
    }

    auto [blkPos, recPos] = *value;

    std::shared_ptr<Block> blk = nullptr;
    if (withBlock) {
        FileReader blkReader{file::BLK, blkPos};
        Block b{};
        blkReader >> b;
        blk = std::make_shared<Block>(std::move(b));
    }

    StoredRecord record = std::unique_ptr<NodeRecord, std::function<void(NodeRecord*)>>(
        new NodeRecord{std::move(blk)}, [pos = recPos](NodeRecord* ptr) {
            if (pos == FilePos{}) {
                return;
            }
            if (ptr->isRedeemed == NodeRecord::IS_REDEEMED) {
                FileModifier recmod{file::REC, pos};
                recmod << *ptr;
            }
            delete ptr;
        });

    FileReader recReader{file::REC, recPos};
    recReader >> *record;

    return record;
}

std::vector<ConstBlockPtr> Caterpillar::GetLevelSetBlksAt(size_t height) const {
    auto vs = GetRawLevelSetAt(height);

    if (vs.empty()) {
        return {};
    }

    std::vector<ConstBlockPtr> blocks;
    while (vs.in_avail()) {
        blocks.emplace_back(std::make_shared<const Block>(vs));
    }
    return blocks;
}

VStream Caterpillar::GetRawLevelSetAt(size_t height, file::FileType fType) const {
    return GetRawLevelSetBetween(height, height, fType);
}

VStream Caterpillar::GetRawLevelSetBetween(size_t height1, size_t height2, file::FileType fType) const {
    assert(height1 <= height2);

    auto left  = dbStore_.GetMsPos(height1);
    auto right = dbStore_.GetMsPos(height2 + 1);

    std::optional<FilePos> leftPos = {}, rightPos = {};

    if (fType == file::FileType::BLK) {
        if (left) {
            leftPos = left->first;
        }
        if (right) {
            rightPos = right->first;
        }
    } else if (fType == file::FileType::REC) {
        if (left) {
            leftPos = left->second;
        }
        if (right) {
            rightPos = right->second;
        }
    } else {
        spdlog::error("Wrong argument: the third argument can only be either file::FileType::BLK or file::FileType::REC.");
        return {};
    }

    VStream result;
    if (!leftPos) {
        return result;
    }

    FileReader reader(fType, *leftPos);
    auto leftOffset  = leftPos->nOffset;
    auto rightOffset = rightPos ? rightPos->nOffset : 0;

    if (rightPos && leftPos->SameFileAs(*rightPos)) {
        reader.read(rightOffset - leftOffset, result);
        return result;
    }

    // Read all of the first file
    auto size = reader.Size();
    reader.read(size - leftOffset, result);
    reader.Close();

    if (rightPos) {
        // Read files between leftPos and rightPos (exclusive)
        auto file = NextFile(*leftPos);
        while (file < *rightPos && !file.SameFileAs(*rightPos)) {
            FileReader cursor(fType, file);
            size = cursor.Size();
            cursor.read(size, result);
            NextFile(file);
        }

        // Read the last file
        FileReader cursor(fType, file);
        cursor.read(rightOffset, result);
        return result;
    }

    // Read at most 20 of the rest files
    static const size_t nFilesMax = 20;

    auto file     = NextFile(*leftPos);
    size_t nFiles = 0;
    while (CheckFileExist(file::GetFilePath(fType, file)) && nFiles < nFilesMax) {
        FileReader cursor(fType, file);
        size = cursor.Size();
        cursor.read(size, result);
        NextFile(file);
        nFiles++;
    }
    return result;
}

size_t Caterpillar::GetHeight(const uint256& blkHash) const {
    return dbStore_.GetHeight(blkHash);
}

uint64_t Caterpillar::GetHeadHeight() const {
    return dbStore_.GetInfo<uint64_t>("headHeight");
}

bool Caterpillar::SaveHeadHeight(uint64_t height) const {
    return dbStore_.WriteInfo("headHeight", height);
}

uint256 Caterpillar::GetBestChainWork() const {
    return dbStore_.GetInfo<uint256>("chainwork");    
}

bool Caterpillar::SaveBestChainWork(const uint256& chainwork) const {
    return dbStore_.WriteInfo("chainwork", chainwork);
}

uint256 Caterpillar::GetMinerChainHead() const {
    return dbStore_.GetInfo<uint256>("minerHead");
}

bool Caterpillar::SaveMinerChainHead(const uint256& h) const {
    return dbStore_.WriteInfo("minerHead", h);
}

bool Caterpillar::ExistsUTXO(const uint256& key) const {
    return dbStore_.ExistsUTXO(key);
}

std::unique_ptr<UTXO> Caterpillar::GetUTXO(const uint256& key) const {
    return dbStore_.GetUTXO(key);
}

bool Caterpillar::AddUTXO(const uint256& key, const UTXOPtr& utxo) const {
    return dbStore_.WriteUTXO(key, utxo);
}

bool Caterpillar::RemoveUTXO(const uint256& key) const {
    return dbStore_.RemoveUTXO(key);
}

uint256 Caterpillar::GetPrevRedemHash(const uint256& peerChainHeadHash) const {
    return dbStore_.GetLastReg(peerChainHeadHash);
}

bool Caterpillar::UpdatePrevRedemHashes(const RegChange& change) const {
    return dbStore_.UpdateReg(change);
}
bool Caterpillar::RollBackPrevRedemHashes(const RegChange& change) const {
    return dbStore_.RollBackReg(change);
}

bool Caterpillar::StoreLevelSet(const std::vector<RecordWPtr>& lvs) {
    try {
        // Function to sum up storage sizes for blk and rec in this lvs
        auto sumSize = [](const std::pair<uint32_t, uint32_t>& prevSum,
                          const RecordWPtr& rec) -> std::pair<uint32_t, uint32_t> {
            return std::make_pair(prevSum.first + (*rec.lock()).cblock->GetOptimalEncodingSize(),
                                  prevSum.second + (*rec.lock()).GetOptimalStorageSize());
        };

        // pair of (total block size, total record size)
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

        const auto& ms  = (*lvs.front().lock());
        uint64_t height = ms.snapshot->height;

        for (const auto& rec_wpt : lvs) {
            // Write to file
            const auto& rec = (*rec_wpt.lock());
            blkOffset       = blkFs.GetOffset() - msBlkOffset;
            recOffset       = recFs.GetOffset() - msRecOffset;
            blkFs << *(rec.cblock);
            blkFs.Flush();
            recFs << rec;
            recFs.Flush();

            // Write positions to db
            dbStore_.WriteRecPos(rec.cblock->GetHash(), height, blkOffset, recOffset);
        }

        // Write ms position at last to enable search for all blocks in the lvs
        dbStore_.WriteMsPos(height, ms.cblock->GetHash(), msBlkPos, msRecPos);

        AddCurrentSize(totalSize);

        SaveHeadHeight(height);
        SaveBestChainWork(ArithToUint256(ms.snapshot->chainwork));

        spdlog::trace("Storing LVS with MS hash {} of height {} with current file pos {}", ms.height, height,
                      std::to_string(*dbStore_.GetMsBlockPos(height)));
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

bool Caterpillar::StoreLevelSet(const std::vector<RecordPtr>& lvs) {
    std::vector<RecordWPtr> wLvs;
    std::transform(lvs.begin(), lvs.end(), std::back_inserter(wLvs), [](RecordPtr p) {
        RecordWPtr wptr = p;
        return wptr;
    });
    return StoreLevelSet(wLvs);
}

void Caterpillar::UnCache(const uint256& blkHash) {
    blockCache_.erase(blkHash);
}

bool Caterpillar::DBExists(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash);
}

bool Caterpillar::DAGExists(const uint256& blkHash) const {
    return blockCache_.contains(blkHash) || DBExists(blkHash);
}

bool Caterpillar::Exists(const uint256& blkHash) const {
    return obc_.Contains(blkHash) || DAGExists(blkHash);
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
    return obc_.Contains(blk->GetMilestoneHash()) || obc_.Contains(blk->GetPrevHash()) ||
           obc_.Contains(blk->GetTipHash());
}

void Caterpillar::Cache(const ConstBlockPtr& blk) {
    blockCache_.emplace(blk->GetHash(), blk);
}

void Caterpillar::Wait() {
    while (obc_.Size() > 0 || !obcThread_.IsIdle()) {
        std::this_thread::yield();
    }
}

void Caterpillar::Stop() {
    Wait();
    obcThread_.Stop();
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
        dbStore_.WriteInfo("blkS", (uint32_t) 0);
        if (loadCurrentBlkName() == epochCapacity_) {
            currentBlkEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentBlkName_.store(0, std::memory_order_seq_cst);
            dbStore_.WriteInfo("blkE", loadCurrentBlkEpoch());
        }

        dbStore_.WriteInfo("blkN", loadCurrentBlkName());
    }

    if (loadCurrentRecSize() > 0 && loadCurrentRecSize() + addon.second > fileCapacity_) {
        currentRecName_.fetch_add(1, std::memory_order_seq_cst);
        currentRecSize_.store(0, std::memory_order_seq_cst);
        dbStore_.WriteInfo("recS", (uint32_t) 0);
        if (loadCurrentRecName() == epochCapacity_) {
            currentRecEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentRecName_.store(0, std::memory_order_seq_cst);
            dbStore_.WriteInfo("recE", loadCurrentRecEpoch());
        }

        dbStore_.WriteInfo("recN", loadCurrentRecName());
    }
}

void Caterpillar::AddCurrentSize(std::pair<uint32_t, uint32_t> size) {
    currentBlkSize_.fetch_add(size.first, std::memory_order_seq_cst);
    currentRecSize_.fetch_add(size.second, std::memory_order_seq_cst);

    dbStore_.WriteInfo("blkS", loadCurrentBlkSize());
    dbStore_.WriteInfo("recS", loadCurrentRecSize());
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
