#ifndef __SRC_CONSENSUS_H__
#define __SRC_CONSENSUS_H__

#include "block.h"

class NodeRecord;
extern const NodeRecord GENESIS_RECORD;
typedef std::shared_ptr<NodeRecord> RecordPtr;

class ChainState {
public:
    uint64_t height;
    arith_uint256 chainwork;
    uint64_t lastUpdateTime;
    arith_uint256 milestoneTarget;
    arith_uint256 blockTarget;
    uint64_t hashRate;


    // TODO: remove these two pointers as ChainState
    //       instances will be stored in deque in Chain
    std::shared_ptr<ChainState> pprevious;
    std::weak_ptr<ChainState> pnext;

    // a vector consists of blocks in the level set of this chain state
    std::vector<RecordPtr> vblockstore;
    std::vector<uint256> pubkeySnapshot;

    // constructor of a chain state of genesis.
    ChainState();
    // constructor of a chain state with all data fields
    ChainState(RecordPtr pblock, std::shared_ptr<ChainState> previous);
    // copy constructor
    ChainState(const ChainState&) = default;

    /* simple constructor (now for test only) */
    ChainState(uint64_t lastUpdateTime,
        uint64_t hashRate,
        uint64_t height,
        arith_uint256 milestoneTarget,
        arith_uint256 blockTarget,
        arith_uint256 chainwork)
        : height(height), chainwork(chainwork), lastUpdateTime(lastUpdateTime), milestoneTarget(milestoneTarget),
          blockTarget(blockTarget), hashRate(hashRate) {}

    inline bool IsDiffTransition() const {
        return ((height + 1) % params.interval) == 0;
    }

    void UpdateDifficulty(uint64_t blockUpdateTime);

    inline uint64_t GetBlockDifficulty() const {
        return (arith_uint256(params.maxTarget) / (blockTarget + 1)).GetLow64();
    }

    inline uint64_t GetMsDifficulty() const {
        return (arith_uint256(params.maxTarget) / (milestoneTarget + 1)).GetLow64();
    }

    void Serialize(VStream& s) const {
        ::Serialize(s, VARINT(height));
        ::Serialize(s, chainwork.GetCompact());
        ::Serialize(s, lastUpdateTime);
        ::Serialize(s, milestoneTarget.GetCompact());
        ::Serialize(s, blockTarget.GetCompact());
        ::Serialize(s, VARINT(hashRate));
    }

    void Deserialize(VStream& s) {
        ::Deserialize(s, VARINT(height));
        chainwork.SetCompact(ser_readdata32(s));
        lastUpdateTime = ser_readdata64(s);
        milestoneTarget.SetCompact(ser_readdata32(s));
        blockTarget.SetCompact(ser_readdata32(s));
        ::Deserialize(s, VARINT(hashRate));
    }

    bool operator==(const ChainState& rhs) const {
        return lastUpdateTime               == rhs.lastUpdateTime &&
               chainwork.GetCompact()       == rhs.chainwork.GetCompact() &&
               hashRate                     == rhs.hashRate &&
               milestoneTarget.GetCompact() == rhs.milestoneTarget.GetCompact() &&
               blockTarget.GetCompact()     == rhs.blockTarget.GetCompact();
    }
};

/*
 * A structure that contains a shared_ptr<const BlockNet> that will
 * be passed to different chains
 */
class NodeRecord {
public:
    enum Validity : uint8_t {
        UNKNOWN = 0,
        VALID,
        INVALID,
    };

    enum MilestoneStatus : uint8_t {
        IS_NOT_MILESTONE = 0,
        IS_TRUE_MILESTONE,
        IS_FAKE_MILESTONE,
    };

    ConstBlockPtr cBlock;

    Coin cumulativeReward;
    uint64_t minerChainHeight;

    bool isMilestone = false;
    std::shared_ptr<ChainState> snapshot;

    Validity validity;

    size_t optimalStorageSize;

    NodeRecord();
    NodeRecord(const ConstBlockPtr&);
    NodeRecord(const BlockNet&);
    NodeRecord(VStream&);

    void LinkChainState(ChainState&);
    size_t GetOptimalStorageSize();
    void InvalidateMilestone();

    void Serialize(VStream& s) const;
    void Deserialize(VStream& s);

    bool operator==(const NodeRecord& another) const {
        return std::tie(*cBlock, cumulativeReward, minerChainHeight) ==
                   std::tie(*(another.cBlock), another.cumulativeReward, another.minerChainHeight) &&
               ((snapshot == nullptr || another.snapshot == nullptr) ? true : (*(snapshot) == *(another.snapshot)));
    }

    static NodeRecord CreateGenesisRecord();
};

namespace std {
string to_string(const NodeRecord&);
} // namespace std

#endif /* ifndef __SRC_CONSENSUS_H__ */
