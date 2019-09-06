// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "storage.h"

BlockStore::BlockStore(const std::string& dbPath) : obcThread_(1), dbStore_(dbPath), obcEnabled_(false) {
    obcThread_.Start();

    currentBlkEpoch_ = dbStore_.GetInfo<uint32_t>("blkE");
    currentVtxEpoch_ = dbStore_.GetInfo<uint32_t>("vtxE");
    currentBlkName_  = dbStore_.GetInfo<uint16_t>("blkN");
    currentVtxName_  = dbStore_.GetInfo<uint16_t>("vtxN");
    currentBlkSize_  = dbStore_.GetInfo<uint32_t>("blkS");
    currentVtxSize_  = dbStore_.GetInfo<uint32_t>("vtxS");
}

BlockStore::~BlockStore() {
    obcThread_.Stop();
}

void BlockStore::AddBlockToOBC(const ConstBlockPtr& blk, const uint8_t& mask) {
    obcThread_.Execute([blk, mask, this]() {
        spdlog::trace("AddBlockToOBC {}", blk->GetHash().to_substr());
        if (!obcEnabled_.load()) {
            return;
        }
        obc_.AddBlock(blk, mask);
    });
}

void BlockStore::ReleaseBlocks(const uint256& blkHash) {
    obcThread_.Execute([blkHash, this]() {
        auto releasedBlocks = obc_.SubmitHash(blkHash);
        if (releasedBlocks) {
            for (auto& blk : *releasedBlocks) {
                DAG->AddNewBlock(blk, nullptr);
            }
        }
    });
}

void BlockStore::EnableOBC() {
    bool flag = false;
    if (obcEnabled_.compare_exchange_strong(flag, true)) {
        spdlog::info("OBC enabled.");
    }
}

void BlockStore::DisableOBC() {
    bool flag = true;
    if (obcEnabled_.compare_exchange_strong(flag, false)) {
        spdlog::info("OBC disabled.");
    }
}

ConstBlockPtr BlockStore::GetBlockCache(const uint256& blkHash) const {
    auto cache_iter = blockCache_.find(blkHash);
    if (cache_iter != blockCache_.end()) {
        return cache_iter->second;
    }

    return nullptr;
}

ConstBlockPtr BlockStore::FindBlock(const uint256& blkHash) const {
    if (auto cache = GetBlockCache(blkHash)) {
        return cache;
    }

    if (dbStore_.Exists(blkHash)) {
        return GetVertex(blkHash)->cblock;
    }

    return nullptr;
}

VertexPtr BlockStore::GetMilestoneAt(size_t height) const {
    VertexPtr vtx = ConstructNRFromFile(dbStore_.GetMsPos(height));
    vtx->snapshot->PushBlkToLvs(vtx);
    return vtx;
}

VertexPtr BlockStore::GetVertex(const uint256& blkHash, bool withBlock) const {
    VertexPtr vtx = ConstructNRFromFile(dbStore_.GetVertexPos(blkHash), withBlock);
    if (vtx && vtx->isMilestone) {
        vtx->snapshot->PushBlkToLvs(vtx);
    }
    return vtx;
}

std::vector<VertexPtr> BlockStore::GetLevelSetVtcsAt(size_t height, bool withBlock) const {
    // Get vertices
    auto vs = GetRawLevelSetAt(height, file::FileType::VTX);

    if (vs.empty()) {
        return {};
    }

    std::vector<VertexPtr> result;
    while (vs.in_avail()) {
        result.emplace_back(std::make_shared<Vertex>(vs));
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

StoredVertex BlockStore::ConstructNRFromFile(std::optional<std::pair<FilePos, FilePos>>&& value, bool withBlock) const {
    if (!value) {
        StoredVertex ret{nullptr, [](Vertex* ptr) {}};
        return ret;
    }

    auto [blkPos, vtxPos] = *value;

    std::shared_ptr<Block> blk = nullptr;
    if (withBlock) {
        FileReader blkReader{file::BLK, blkPos};
        Block b{};
        blkReader >> b;
        blk = std::make_shared<Block>(std::move(b));
    }

    StoredVertex vertex =
        std::unique_ptr<Vertex, std::function<void(Vertex*)>>(new Vertex{std::move(blk)}, [pos = vtxPos](Vertex* ptr) {
            if (pos == FilePos{}) {
                return;
            }
            if (ptr->isRedeemed == Vertex::IS_REDEEMED) {
                FileModifier vtxmod{file::VTX, pos};
                vtxmod << *ptr;
            }
            delete ptr;
        });

    FileReader vtxReader{file::VTX, vtxPos};
    vtxReader >> *vertex;

    return vertex;
}

std::vector<ConstBlockPtr> BlockStore::GetLevelSetBlksAt(size_t height) const {
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

VStream BlockStore::GetRawLevelSetAt(size_t height, file::FileType fType) const {
    return GetRawLevelSetBetween(height, height, fType);
}

VStream BlockStore::GetRawLevelSetBetween(size_t height1, size_t height2, file::FileType fType) const {
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
    } else if (fType == file::FileType::VTX) {
        if (left) {
            leftPos = left->second;
        }
        if (right) {
            rightPos = right->second;
        }
    } else {
        spdlog::error(
            "Wrong argument: the third argument can only be either file::FileType::BLK or file::FileType::VTX.");
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

size_t BlockStore::GetHeight(const uint256& blkHash) const {
    return dbStore_.GetHeight(blkHash);
}

uint64_t BlockStore::GetHeadHeight() const {
    return dbStore_.GetInfo<uint64_t>("headHeight");
}

bool BlockStore::SaveHeadHeight(uint64_t height) const {
    return dbStore_.WriteInfo("headHeight", height);
}

uint256 BlockStore::GetBestChainWork() const {
    return dbStore_.GetInfo<uint256>("chainwork");
}

bool BlockStore::SaveBestChainWork(const uint256& chainwork) const {
    return dbStore_.WriteInfo("chainwork", chainwork);
}

uint256 BlockStore::GetMinerChainHead() const {
    return dbStore_.GetInfo<uint256>("minerHead");
}

bool BlockStore::SaveMinerChainHead(const uint256& h) const {
    return dbStore_.WriteInfo("minerHead", h);
}

bool BlockStore::ExistsUTXO(const uint256& key) const {
    return dbStore_.ExistsUTXO(key);
}

std::unique_ptr<UTXO> BlockStore::GetUTXO(const uint256& key) const {
    return dbStore_.GetUTXO(key);
}

bool BlockStore::AddUTXO(const uint256& key, const UTXOPtr& utxo) const {
    return dbStore_.WriteUTXO(key, utxo);
}

bool BlockStore::RemoveUTXO(const uint256& key) const {
    return dbStore_.RemoveUTXO(key);
}

uint256 BlockStore::GetPrevRedemHash(const uint256& peerChainHeadHash) const {
    return dbStore_.GetLastReg(peerChainHeadHash);
}

bool BlockStore::UpdatePrevRedemHashes(const RegChange& change) const {
    return dbStore_.UpdateReg(change);
}
bool BlockStore::RollBackPrevRedemHashes(const RegChange& change) const {
    return dbStore_.RollBackReg(change);
}

bool BlockStore::StoreLevelSet(const std::vector<VertexWPtr>& lvs) {
    try {
        // Function to sum up storage sizes for blk and vtx in this lvs
        auto sumSize = [](const std::pair<uint32_t, uint32_t>& prevSum,
                          const VertexWPtr& vtx) -> std::pair<uint32_t, uint32_t> {
            return std::make_pair(prevSum.first + (*vtx.lock()).cblock->GetOptimalEncodingSize(),
                                  prevSum.second + (*vtx.lock()).GetOptimalStorageSize());
        };

        // pair of (total block size, total vertex size)
        std::pair<uint32_t, uint32_t> totalSize =
            std::accumulate(lvs.begin(), lvs.end(), std::make_pair(0, 0), sumSize);

        CarryOverFileName(totalSize);

        FilePos msBlkPos{loadCurrentBlkEpoch(), loadCurrentBlkName(), loadCurrentBlkSize()};
        FilePos msVtxPos{loadCurrentVtxEpoch(), loadCurrentVtxName(), loadCurrentVtxSize()};
        FileWriter blkFs{file::BLK, msBlkPos};
        FileWriter vtxFs{file::VTX, msVtxPos};

        uint32_t blkOffset;
        uint32_t vtxOffset;
        uint32_t msBlkOffset = msBlkPos.nOffset;
        uint32_t msVtxOffset = msVtxPos.nOffset;

        const auto& ms  = (*lvs.front().lock());
        uint64_t height = ms.snapshot->height;

        for (const auto& vtx_wpt : lvs) {
            // Write to file
            const auto& vtx = (*vtx_wpt.lock());
            blkOffset       = blkFs.GetOffset() - msBlkOffset;
            vtxOffset       = vtxFs.GetOffset() - msVtxOffset;
            blkFs << *(vtx.cblock);
            blkFs.Flush();
            vtxFs << vtx;
            vtxFs.Flush();

            // Write positions to db
            dbStore_.WriteVtxPos(vtx.cblock->GetHash(), height, blkOffset, vtxOffset);
        }

        // Write ms position at last to enable search for all blocks in the lvs
        dbStore_.WriteMsPos(height, ms.cblock->GetHash(), msBlkPos, msVtxPos);

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

bool BlockStore::StoreLevelSet(const std::vector<VertexPtr>& lvs) {
    std::vector<VertexWPtr> wLvs;
    std::transform(lvs.begin(), lvs.end(), std::back_inserter(wLvs), [](VertexPtr p) {
        VertexWPtr wptr = p;
        return wptr;
    });
    return StoreLevelSet(wLvs);
}

void BlockStore::UnCache(const uint256& blkHash) {
    blockCache_.erase(blkHash);
}

bool BlockStore::DBExists(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash);
}

bool BlockStore::DAGExists(const uint256& blkHash) const {
    return blockCache_.contains(blkHash) || DBExists(blkHash);
}

bool BlockStore::Exists(const uint256& blkHash) const {
    return obc_.Contains(blkHash) || DAGExists(blkHash);
}

bool BlockStore::IsMilestone(const uint256& blkHash) const {
    return dbStore_.IsMilestone(blkHash);
}

bool BlockStore::IsWeaklySolid(const ConstBlockPtr& blk) const {
    return Exists(blk->GetMilestoneHash()) && Exists(blk->GetPrevHash()) && Exists(blk->GetTipHash());
}

bool BlockStore::IsSolid(const ConstBlockPtr& blk) const {
    return DAGExists(blk->GetMilestoneHash()) && DAGExists(blk->GetPrevHash()) && DAGExists(blk->GetTipHash());
}

bool BlockStore::AnyLinkIsOrphan(const ConstBlockPtr& blk) const {
    return obc_.Contains(blk->GetMilestoneHash()) || obc_.Contains(blk->GetPrevHash()) ||
           obc_.Contains(blk->GetTipHash());
}

void BlockStore::Cache(const ConstBlockPtr& blk) {
    blockCache_.emplace(blk->GetHash(), blk);
}

void BlockStore::Wait() {
    while (obc_.Size() > 0 || !obcThread_.IsIdle()) {
        std::this_thread::yield();
    }
}

void BlockStore::Stop() {
    Wait();
    obcThread_.Stop();
}

void BlockStore::SetFileCapacities(uint32_t fileCapacity, uint16_t epochCapacity) {
    fileCapacity_  = fileCapacity;
    epochCapacity_ = epochCapacity;
}

uint32_t BlockStore::loadCurrentBlkEpoch() {
    return currentBlkEpoch_.load(std::memory_order_seq_cst);
}

uint32_t BlockStore::loadCurrentVtxEpoch() {
    return currentVtxEpoch_.load(std::memory_order_seq_cst);
}

uint16_t BlockStore::loadCurrentBlkName() {
    return currentBlkName_.load(std::memory_order_seq_cst);
}

uint16_t BlockStore::loadCurrentVtxName() {
    return currentVtxName_.load(std::memory_order_seq_cst);
}

uint32_t BlockStore::loadCurrentBlkSize() {
    return currentBlkSize_.load(std::memory_order_seq_cst);
}

uint32_t BlockStore::loadCurrentVtxSize() {
    return currentVtxSize_.load(std::memory_order_seq_cst);
}

void BlockStore::CarryOverFileName(std::pair<uint32_t, uint32_t> addon) {
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

    if (loadCurrentVtxSize() > 0 && loadCurrentVtxSize() + addon.second > fileCapacity_) {
        currentVtxName_.fetch_add(1, std::memory_order_seq_cst);
        currentVtxSize_.store(0, std::memory_order_seq_cst);
        dbStore_.WriteInfo("vtxS", (uint32_t) 0);
        if (loadCurrentVtxName() == epochCapacity_) {
            currentVtxEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentVtxName_.store(0, std::memory_order_seq_cst);
            dbStore_.WriteInfo("vtxE", loadCurrentVtxEpoch());
        }

        dbStore_.WriteInfo("vtxN", loadCurrentVtxName());
    }
}

void BlockStore::AddCurrentSize(std::pair<uint32_t, uint32_t> size) {
    currentBlkSize_.fetch_add(size.first, std::memory_order_seq_cst);
    currentVtxSize_.fetch_add(size.second, std::memory_order_seq_cst);

    dbStore_.WriteInfo("blkS", loadCurrentBlkSize());
    dbStore_.WriteInfo("vtxS", loadCurrentVtxSize());
}

FilePos& BlockStore::NextFile(FilePos& pos) const {
    if (pos.nName == epochCapacity_ - 1) {
        pos.nName = 0;
        pos.nEpoch++;
    } else {
        pos.nName++;
    }
    pos.nOffset = 0;
    return pos;
}
