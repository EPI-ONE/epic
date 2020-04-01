// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "block_store.h"
#include "crc32.h"

#include <filesystem>

template <typename P>
std::vector<std::shared_ptr<P>> DeserializeRawLvs(VStream&& vs) {
    if (vs.empty()) {
        return {};
    }

    std::vector<std::shared_ptr<P>> blocks;

    try {
        auto ms = std::make_shared<P>(vs);

        while (vs.in_avail()) {
            blocks.emplace_back(std::make_shared<P>(vs));
        }
        blocks.emplace_back(std::move(ms));
    } catch (const std::exception&) {
        spdlog::error("Error occurs deserializing raw level set: {} {}", __FILE__, __LINE__);
    }

    return blocks;
}

template std::vector<ConstBlockPtr> DeserializeRawLvs(VStream&&);
template std::vector<VertexPtr> DeserializeRawLvs(VStream&&);

BlockStore::BlockStore(const std::string& dbPath)
    : obcThread_(1), obcEnabled_(false), checksumCalThread_(1), lastUpdateTaskTime_(time(nullptr)), dbStore_(dbPath) {
    obcThread_.Start();
    obcTimeout_.AddPeriodTask(300, [this]() {
        obcThread_.Execute([this]() {
            auto n = obc_.Prune(3600);
            if (n) {
                spdlog::info("[OBC] Erased {} outdated entries from OBC.", n);
            }
        });
    });
    obcTimeout_.Start();
    checksumCalThread_.Start();
}

void BlockStore::AddBlockToOBC(ConstBlockPtr&& blk, const uint8_t& mask) {
    obcThread_.Execute([blk = std::move(blk), mask, this]() mutable {
        spdlog::trace("[OBC] AddBlockToOBC {}", blk->GetHash().to_substr());
        if (!obcEnabled_.load()) {
            return;
        }
        obc_.AddBlock(std::move(blk), mask);
    });
}

void BlockStore::ReleaseBlocks(const uint256& blkHash) {
    obcThread_.Execute([blkHash, this]() {
        auto releasedBlocks = obc_.SubmitHash(blkHash);
        for (auto& blk : releasedBlocks) {
            DAG->AddNewBlock(std::move(blk), nullptr);
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

const OrphanBlocksContainer& BlockStore::GetOBC() const {
    return obc_;
}

ConstBlockPtr BlockStore::GetBlockCache(const uint256& blkHash) const {
    auto cache_iter = blockPool_.find(blkHash);
    if (cache_iter != blockPool_.end()) {
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
    auto result = DeserializeRawLvs<Vertex>(GetRawLevelSetAt(height, file::FileType::VTX));
    assert(!result.empty());

    const auto& ms = result.back();
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

VertexPtr BlockStore::ConstructNRFromFile(std::optional<std::pair<FilePos, FilePos>>&& value, bool withBlock) const {
    if (!value) {
        return nullptr;
    }

    auto [blkPos, vtxPos] = *value;

    std::shared_ptr<Block> blk = nullptr;
    if (withBlock) {
        FileReader blkReader{file::BLK, blkPos};
        Block b{};
        blkReader >> b;
        blk = std::make_shared<Block>(std::move(b));
    }

    VertexPtr vertex = std::make_shared<Vertex>(std::move(blk));
    FileReader vtxReader{file::VTX, vtxPos};
    vtxReader >> *vertex;

    return vertex;
}

std::vector<ConstBlockPtr> BlockStore::GetLevelSetBlksAt(size_t height) const {
    return DeserializeRawLvs<const Block>(GetRawLevelSetAt(height));
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
            cursor.read(size - file::checksum_size, result);
            cursor.Close();
            NextFile(file);
        }

        // Read the last file
        if (file.nOffset > file::checksum_size) {
            FileReader cursor(fType, file);
            cursor.read(rightOffset - file::checksum_size, result);
            cursor.Close();
        }
        return result;
    }

    // Read at most 20 of the rest files
    static const size_t nFilesMax = 20;

    auto file     = NextFile(*leftPos);
    size_t nFiles = 0;
    while (CheckFileExist(file::GetFilePath(fType, file)) && nFiles < nFilesMax) {
        FileReader cursor(fType, file);
        size = cursor.Size();
        cursor.read(size - file::checksum_size, result);
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

CircularQueue<uint256> BlockStore::GetMinerChainHeads() const {
    return dbStore_.GetInfo<CircularQueue<uint256>>("minerHeads");
}

bool BlockStore::SaveMinerChainHeads(const CircularQueue<uint256>& q) const {
    return dbStore_.WriteInfo("minerHeads", q);
}

bool BlockStore::ExistsUTXO(const uint256& key) const {
    return dbStore_.ExistsUTXO(key);
}

std::unique_ptr<UTXO> BlockStore::GetUTXO(const uint256& key) const {
    return dbStore_.GetUTXO(key);
}

std::unordered_map<uint256, std::unique_ptr<UTXO>> BlockStore::GetAllUTXO() const {
    return dbStore_.GetAllUTXO();
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

bool BlockStore::UpdateRedemptionStatus(const uint256& key) const {
    auto pos = dbStore_.GetVertexPos(key);
    if (!pos) {
        return false;
    }
    FileModifier vtxmod{file::VTX, pos->second};
    vtxmod << static_cast<uint8_t>(Vertex::RedemptionStatus::IS_REDEEMED);
    vtxmod.Flush();
    vtxmod.Close();

    STORE->AddChecksumTask(pos->second);
    return true;
}

std::unordered_map<uint256, uint256> BlockStore::GetAllReg() const {
    return dbStore_.GetAllReg();
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

        // reserve space for checksum
        uint32_t init_checksum = 0;
        if (blkFs.Size() == 0) {
            currentBlkSize_.store(file::checksum_size);
            msBlkPos.nOffset = file::checksum_size;
            blkFs << init_checksum;
        }
        if (vtxFs.Size() == 0) {
            currentVtxSize_.store(file::checksum_size);
            msVtxPos.nOffset = file::checksum_size;
            vtxFs << init_checksum;
        }

        const auto& ms  = (*lvs.back().lock());
        uint64_t height = ms.height;

        // Store ms to file
        blkFs << *ms.cblock;
        vtxFs << ms;
        dbStore_.WriteVtxPos(ms.cblock->GetHash(), height, 0, 0);
        uint32_t blkOffset;
        uint32_t vtxOffset;

        for (size_t i = 0; i < lvs.size() - 1; ++i) {
            // Write to file
            const auto& vtx = (*lvs[i].lock());
            blkOffset       = blkFs.GetOffsetP() - msBlkPos.nOffset;
            vtxOffset       = vtxFs.GetOffsetP() - msVtxPos.nOffset;
            blkFs << *(vtx.cblock);
            vtxFs << vtx;

            // Write positions to db
            dbStore_.WriteVtxPos(vtx.cblock->GetHash(), height, blkOffset, vtxOffset);
        }

        blkFs.Flush();
        blkFs.Close();
        vtxFs.Flush();
        vtxFs.Close();

        // Write ms position at last to enable search for all blocks in the lvs
        dbStore_.WriteMsPos(height, ms.cblock->GetHash(), msBlkPos, msVtxPos);
        STORE->SaveBestChainWork(ArithToUint256(ms.snapshot->chainwork));

        AddCurrentSize(totalSize);

        spdlog::trace("[STORE] Storing LVS with MS hash {} of height {} with current file pos {}", ms.height, height,
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
    blockPool_.erase(blkHash);
}

bool BlockStore::DBExists(const uint256& blkHash) const {
    return dbStore_.Exists(blkHash);
}

bool BlockStore::DAGExists(const uint256& blkHash) const {
    return blockPool_.contains(blkHash) || DBExists(blkHash);
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
    blockPool_.emplace(blk->GetHash(), blk);
}

void BlockStore::Wait() {
    while (obc_.Size() > 0 || !obcThread_.IsIdle()) {
        std::this_thread::yield();
    }
}

void BlockStore::Stop() {
    spdlog::info("Stopping store...");
    obcThread_.Abort();
    obcThread_.Stop();
    obcTimeout_.Stop();
    while (!checksumTasks_.empty()) {
        spdlog::info("{} checksum tasks left, executing...", checksumTasks_.size());
        ExecuteChecksumTask();
    }
    checksumCalThread_.Stop();
    file::CalculateChecksum(file::BLK, FilePos{loadCurrentBlkEpoch(), loadCurrentBlkName(), 0});
    file::CalculateChecksum(file::VTX, FilePos{loadCurrentVtxEpoch(), loadCurrentVtxName(), 0});
    spdlog::info("Finish all checksum tasks");
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
        // calculate the checksum of the last block file immediately
        file::CalculateChecksum(file::BLK, FilePos{loadCurrentBlkEpoch(), loadCurrentBlkName(), 0});

        currentBlkName_.fetch_add(1, std::memory_order_seq_cst);
        currentBlkSize_.store(0, std::memory_order_seq_cst);
        if (loadCurrentBlkName() == epochCapacity_) {
            currentBlkEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentBlkName_.store(0, std::memory_order_seq_cst);
        }
    }

    if (loadCurrentVtxSize() > 0 && loadCurrentVtxSize() + addon.second > fileCapacity_) {
        // calculate the checksum of the last file, send it to the task set
        AddChecksumTask(FilePos{loadCurrentVtxEpoch(), loadCurrentVtxName(), 0});

        currentVtxName_.fetch_add(1, std::memory_order_seq_cst);
        currentVtxSize_.store(0, std::memory_order_seq_cst);
        if (loadCurrentVtxName() == epochCapacity_) {
            currentVtxEpoch_.fetch_add(1, std::memory_order_seq_cst);
            currentVtxName_.store(0, std::memory_order_seq_cst);
        }
    }
}

void BlockStore::AddCurrentSize(std::pair<uint32_t, uint32_t> size) {
    currentBlkSize_.fetch_add(size.first, std::memory_order_seq_cst);
    currentVtxSize_.fetch_add(size.second, std::memory_order_seq_cst);
}

FilePos& BlockStore::NextFile(FilePos& pos) const {
    if (pos.nName == epochCapacity_ - 1) {
        pos.nName = 0;
        pos.nEpoch++;
    } else {
        pos.nName++;
    }
    pos.nOffset = file::checksum_size;
    return pos;
}

bool BlockStore::CheckOneFile(file::FileType type, uint32_t epoch, uint32_t name) {
    auto filename = file::GetEpochPath(type, epoch) + '/' + file::GetFileName(type, name);
    if (!CheckFileExist(filename)) {
        spdlog::error("File {} doesn't exit", filename);
        return false;
    }
    if (!ValidateChecksum(type, FilePos{epoch, name, 0})) {
        spdlog::error("File {} can't pass the validation of checksum", filename);
        return false;
    }
    return true;
}

FileCheckInfo BlockStore::CheckOneType(file::FileType type) {
    FileCheckInfo result{false, 0, 0};
    auto all_epoches = file::GetAllEpoch(type);
    if (all_epoches.empty()) {
        spdlog::error("File {} doesn't exit", file::GetFilePath(type, FilePos(0, 0, 0)));
        return result;
    }
    for (size_t epoch = 0; epoch < all_epoches.size(); epoch++) {
        size_t end   = epochCapacity_;
        result.epoch = epoch;
        if (epoch == all_epoches.size() - 1) {
            auto all_names = file::GetAllName(epoch, type);
            if (all_names.empty()) {
                spdlog::error("File {} doesn't exit", file::GetFilePath(type, FilePos(epoch, 0, 0)));
                result.name = 0;
                return result;
            }
            end = all_names.size();
        }
        for (size_t name = 0; name < end; name++) {
            result.name = name;
            if (!CheckOneFile(type, epoch, name)) {
                return result;
            }
        }
    }
    result.valid = true;
    return result;
}

bool BlockStore::CheckFileSanity(bool prune) {
    auto blk_res              = CheckOneType(file::BLK);
    auto vtx_res              = CheckOneType(file::VTX);
    uint64_t minInvalidHeight = UINT64_MAX;
    auto headHeight           = GetHeadHeight();

    if (blk_res.valid && vtx_res.valid) {
        auto currentBLKHeight = GetlatestHeightFromFile(FilePos(blk_res.epoch, blk_res.name, 0), file::BLK);
        auto currentVTXHeight = GetlatestHeightFromFile(FilePos(vtx_res.epoch, vtx_res.name, 0), file::VTX);
        if (currentBLKHeight == currentVTXHeight && currentBLKHeight == headHeight) {
            FilePos vvtxPos{vtx_res.epoch, vtx_res.name, 0};
            FilePos vblkPos{blk_res.epoch, blk_res.name, 0};
            vvtxPos.nOffset = file::GetFileSize(file::VTX, vvtxPos);
            vblkPos.nOffset = file::GetFileSize(file::BLK, vblkPos);
            SetCurrentFilePos(file::VTX, vvtxPos);
            SetCurrentFilePos(file::BLK, vblkPos);
            spdlog::info("Pass the file sanity check, current blk epoch = {}, name = {}, offset = {} and current vtx "
                         "epoch = {}, name = {}, offset = {}",
                         currentBlkEpoch_, currentBlkName_, currentBlkSize_, currentVtxEpoch_, currentVtxName_,
                         currentVtxSize_);
            return true;
        } else {
            spdlog::error("current valid blk height is {}, vtx height is {}, head height is {}, which are not same.",
                          currentBLKHeight, currentVTXHeight, headHeight);
            minInvalidHeight = std::min(currentBLKHeight, currentVTXHeight) + 1;
        }
    } else {
        if (!blk_res.valid) {
            uint64_t invalidBlkHeight = GetHeightFromInvalidFile(FilePos{blk_res.epoch, blk_res.name, 0}, file::BLK);
            if (invalidBlkHeight == UINT64_MAX) {
                return false;
            }
            spdlog::error("BLK files starting from height {} errored", invalidBlkHeight);
            minInvalidHeight = invalidBlkHeight;
        }
        if (!vtx_res.valid) {
            uint64_t invalidVtxHeight = GetHeightFromInvalidFile(FilePos{vtx_res.epoch, vtx_res.name, 0}, file::VTX);
            if (invalidVtxHeight == UINT64_MAX) {
                return false;
            }
            spdlog::error("Vtx files starting from height {} errored", invalidVtxHeight);
            minInvalidHeight = std::min(minInvalidHeight, invalidVtxHeight);
        }
    }

    // try to locate the actual position at the min invalid height in DB
    minInvalidHeight = std::min(minInvalidHeight, headHeight + 1);
    auto pos_pair    = dbStore_.GetMsPos(minInvalidHeight);
    while (!pos_pair && minInvalidHeight > 0) {
        spdlog::debug("Failed to get the ms pos from the invalid height {}", minInvalidHeight);
        --minInvalidHeight;
        pos_pair = dbStore_.GetMsPos(minInvalidHeight);
    }
    spdlog::debug("The min invalid height is {}", minInvalidHeight);

    if (!prune) {
        return false;
    } else {
        spdlog::info("Start to prune invalid db records");
        // fix invalid DB records
        if (!FixDBRecords(minInvalidHeight)) {
            spdlog::error("Failed to prune invalid DB records");
            return false;
        }

        // delete invalid files
        auto [blkPos, vtxPos] = *pos_pair;
        if (!DeleteInvalidFiles(blkPos, file::BLK) || !DeleteInvalidFiles(vtxPos, file::VTX)) {
            spdlog::error("Failed to delete invalid files");
            return false;
        }

        // set correct DB records about meta data,reset head height to the safe height
        auto latestValidHeight = minInvalidHeight == 0 ? 0 : minInvalidHeight - 1;
        spdlog::debug("Reset current head height from {} to {}", headHeight, latestValidHeight);
        if (!SaveHeadHeight(latestValidHeight)) {
            spdlog::error("Failed to update head height");
            return false;
        }

        // set correct position info
        auto latestValidPos = dbStore_.GetMsPos(latestValidHeight);
        if (latestValidPos) {
            auto [vblkPos, vvtxPos] = *latestValidPos;
            vvtxPos.nOffset         = file::GetFileSize(file::VTX, vvtxPos);
            vblkPos.nOffset         = file::GetFileSize(file::BLK, vblkPos);
            SetCurrentFilePos(file::VTX, vvtxPos);
            SetCurrentFilePos(file::BLK, vblkPos);
        } else if (minInvalidHeight > 0) {
            // since DB has the record of minInvalidHeight, it should have the record of latestValidHeight unless
            // minInvalidHeight == 0
            spdlog::error("DB is not consistent, please delete all data files and restart the program");
            return false;
        }

        // deal with genesis case
        if (minInvalidHeight == 0) {
            spdlog::info("Restore genesis");
            FilePos empty_pos{0, 0, 0};
            SetCurrentFilePos(file::BLK, empty_pos);
            SetCurrentFilePos(file::VTX, empty_pos);
            std::vector<VertexPtr> genesisLvs = {GENESIS_VERTEX};
            StoreLevelSet(genesisLvs);
        }
        spdlog::info("Finish the pruning process, current blk epoch = {}, name = {}, offset = {} and current vtx "
                     "epoch = {}, name = {}, offset = {}",
                     currentBlkEpoch_, currentBlkName_, currentBlkSize_, currentVtxEpoch_, currentVtxName_,
                     currentVtxSize_);
    }
    return true;
}

size_t BlockStore::GetlatestHeightFromFile(FilePos search_pos, file::FileType type) {
    FileReader reader(type, search_pos);
    reader.SetOffsetP(file::checksum_size, std::ios_base::beg);
    if (type == file::BLK) {
        Block block;
        reader >> block;
        uint64_t height = GetHeight(block.GetHash()) + 1;
        if (height == 0) {
            spdlog::error("Can not find the record of the block {}, DB may be broken", block.GetHash().to_substr());
            return height;
        }
        while (true) {
            auto msPos = dbStore_.GetMsBlockPos(height);
            if (!msPos || msPos->nName != search_pos.nName) {
                return height - 1;
            }
            height++;
        }
    } else {
        Vertex vertex;
        reader >> vertex;

        uint64_t height = vertex.height + 1;
        while (true) {
            auto msPos = dbStore_.GetMsPos(height);
            if (!msPos || msPos->second.nName != search_pos.nName) {
                return height - 1;
            }
            height++;
        }
    }
}

size_t BlockStore::GetHeightFromInvalidFile(FilePos pos, file::FileType type) {
    if (pos.nEpoch == 0 && pos.nName == 0) {
        return 0;
    }
    FilePos search_pos;

    if (pos.nName == 0) {
        search_pos.nName  = epochCapacity_ - 1;
        search_pos.nEpoch = pos.nEpoch - 1;
    } else {
        search_pos.nName  = pos.nName - 1;
        search_pos.nEpoch = pos.nEpoch;
    }
    return GetlatestHeightFromFile(search_pos, type) + 1;
}

bool BlockStore::FixDBRecords(uint64_t height) {
    spdlog::debug("Start to rebuild UTXOs and Registrations");
    if (!RebuildConsensus(height)) {
        spdlog::error("Failed to rebuild consensus records");
        return false;
    }

    spdlog::debug("Start to delete invalid milestone records");
    if (!DeleteDBMs(height)) {
        spdlog::error("Failed to delete ms pos");
        return false;
    }

    spdlog::debug("Start to delete invalid block and vertex records");
    if (!DeleteDBBlks(height)) {
        spdlog::error("Failed ot delete all invalid blk/vtx records");
        return false;
    }

    return true;
}

bool BlockStore::DeleteDBMs(uint64_t height) {
    uint64_t currentHeight = GetHeadHeight();
    spdlog::info("Start to delete db ms record from {} to {}", height, currentHeight);
    for (uint64_t h = height; h <= currentHeight; h++) {
        auto res = dbStore_.DeleteMsPos(h);
        if (res) {
            spdlog::debug("Deleted Ms Pos at height {}", h);
        } else {
            spdlog::error("Failed to delete Ms Pos at height {}, DB record is not consistent", h);
            return false;
        }
    }
    return true;
}

bool BlockStore::DeleteDBBlks(uint64_t height) {
    return dbStore_.DeleteBatchVtxPos(height);
}

bool BlockStore::RebuildConsensus(uint64_t height) {
    // delete two columns in db  UTXO, Reg
    std::string column1 = "utxo";
    std::string column2 = "reg";
    auto statusU        = dbStore_.ClearColumn(column1);
    auto statusR        = dbStore_.ClearColumn(column2);
    if (!statusU || !statusR) {
        return false;
    }
    if (height <= 1) {
        return true;
    }
    arith_uint256 chainwork       = GENESIS_VERTEX->snapshot->chainwork;
    arith_uint256 previous_target = GENESIS_VERTEX->snapshot->milestoneTarget;
    for (uint64_t h = 1; h < height; h++) {
        auto levelset = GetLevelSetVtcsAt(h, true);
        if (!ConstructUTXOAndRegFromLvs(levelset)) {
            return false;
        }
        auto ms = levelset.back();
        chainwork += GetParams().maxTarget / previous_target;
        previous_target = ms->snapshot->milestoneTarget;
    }

    // save chainwork
    SaveBestChainWork(ArithToUint256(chainwork));
    return true;
}

bool BlockStore::ConstructUTXOAndRegFromLvs(std::vector<VertexPtr>& levelset) {
    for (auto& vertex : levelset) {
        if (!ConstructUTXOAndRegFromVtx(vertex)) {
            return false;
        }
    }
    return true;
}

bool BlockStore::ConstructUTXOAndRegFromVtx(const VertexPtr& vtx) {
    size_t size   = vtx->cblock->GetTransactionSize();
    auto blkHash  = vtx->cblock->GetHash();
    auto prevHash = vtx->cblock->GetPrevHash();

    RegChange regChange;
    TXOC txoc;
    std::vector<UTXOPtr> newUXTOs;
    if (vtx->cblock->IsFirstRegistration()) {
        regChange.Create(blkHash, blkHash);
    } else {
        // reg change
        auto oldRedempHash = STORE->GetPrevRedemHash(prevHash);
        if (oldRedempHash.IsNull()) {
            spdlog::error("Can't find redemption hash {}", oldRedempHash.GetHex());
            return false;
        }
        regChange.Remove(prevHash, oldRedempHash);
        if (size > 0 && vtx->cblock->IsRegistration() && vtx->validity[0] == Vertex::VALID) {
            regChange.Create(blkHash, blkHash);
        } else {
            regChange.Create(blkHash, oldRedempHash);
        }

        // utxo
        for (size_t txIndex = 0; txIndex < size; ++txIndex) {
            if (vtx->validity[txIndex] == Vertex::VALID) {
                auto tx = vtx->cblock->GetTransactions()[txIndex];
                // input, find spent utxo and new reg
                for (auto& input : tx->GetInputs()) {
                    if (!input.IsRegistration()) {
                        txoc.AddToSpent(input);
                    }
                }
                // output, find new utxo
                for (size_t outputIndex = 0; outputIndex < tx->GetOutputs().size(); ++outputIndex) {
                    auto utxoPtr = std::make_shared<UTXO>(tx->GetOutputs()[outputIndex], txIndex, outputIndex);
                    newUXTOs.emplace_back(utxoPtr);
                }
            }
        }
    }

    // delete spent utxo
    for (auto& utxokey : txoc.GetSpent()) {
        if (!RemoveUTXO(utxokey)) {
            spdlog::error("Failed to remove utxo {}", utxokey.GetHex());
            return false;
        }
    }

    // save new utxo
    for (auto& utxo : newUXTOs) {
        if (!AddUTXO(utxo->GetKey(), utxo)) {
            spdlog::error("Failed to add utxo {}", utxo->GetKey().GetHex());
            return false;
        }
    }

    // update reg change
    if (!UpdatePrevRedemHashes(regChange)) {
        spdlog::error("Failed to update reg");
        return false;
    }

    return true;
}

void BlockStore::SetCurrentFilePos(file::FileType type, FilePos pos) {
    switch (type) {
        case file::BLK: {
            currentBlkEpoch_ = pos.nEpoch;
            currentBlkName_  = pos.nName;
            currentBlkSize_  = pos.nOffset;
            break;
        }
        case file::VTX: {
            currentVtxEpoch_ = pos.nEpoch;
            currentVtxName_  = pos.nName;
            currentVtxSize_  = pos.nOffset;
            break;
        }
        default: {
            throw "invalid file type " + std::to_string(type);
        }
    }
}

void BlockStore::AddChecksumTask(FilePos pos) {
    pos.nOffset = 0;
    checksumTasks_.insert(pos);
    if (checksumTasks_.size() > 10 || time(nullptr) - lastUpdateTaskTime_ > 5) {
        ExecuteChecksumTask();
        lastUpdateTaskTime_ = time(nullptr);
    }
}

void BlockStore::ExecuteChecksumTask() {
    auto it = checksumTasks_.begin();
    if (it == checksumTasks_.end()) {
        return;
    }
    FilePos task = *it;
    checksumCalThread_.Execute([task]() { file::CalculateChecksum(file::VTX, task); });
    checksumTasks_.erase(it);
}
