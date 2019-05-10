#include "block.h"

enum MilestoneStatus : uint8_t {
    IS_NOT_MILESTONE = 0,
    IS_TRUE_MILESTONE,
    IS_FAKE_MILESTONE,
};

class BlockStatus;
typedef std::shared_ptr<BlockStatus> BlkStatPtr;

class ChainState {
public:
    uint64_t height;
    uint64_t lastUpdateTime;
    uint64_t hashRate;

    arith_uint256 chainwork;
    arith_uint256 blockTarget;
    arith_uint256 milestoneTarget;

    std::shared_ptr<ChainState> pprevious;
    std::weak_ptr<ChainState> pnext;

    // a vector consists of blocks with its offset w.r.t this level set of this milestone
    std::vector<BlkStatPtr> vblockstore;
    std::vector<uint256> pubkeySnapshot;

    // constructor of a milestone of genesis.
    ChainState();
    // constructor of a milestone with all data fields
    ChainState(BlkStatPtr, std::shared_ptr<ChainState>);
    // copy constructor
    ChainState(const ChainState&) = default;

    /* simple constructor (now for test only) */
    ChainState(uint64_t lastUpdateTime,
        uint64_t hashRate,
        uint64_t height,
        arith_uint256 milestoneTarget,
        arith_uint256 blockTarget,
        arith_uint256 chainwork)
        : lastUpdateTime(lastUpdateTime), hashRate(hashRate), height(height), milestoneTarget(milestoneTarget),
          blockTarget(blockTarget), chainwork(chainwork) {}

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
};

class BlockStatus {
public:
    enum Validity : uint8_t {
        UNKNOWN = 0,
        VALID,
        INVALID,
    };

    cBlockPtr cBlock;

    Coin cumulativeReward;
    uint64_t minerChainHeight;

    bool isMilestone = false;
    std::shared_ptr<ChainState> snapshot;

    Validity validity;

    size_t optimalStorageSize;

    BlockStatus() : optimalStorageSize(0), minerChainHeight(0) {}
    BlockStatus(const cBlockPtr& blk) : cBlock(blk), optimalStorageSize(0), minerChainHeight(0) {}
    BlockStatus(const Block& blk) : optimalStorageSize(0), minerChainHeight(0) {
        cBlock = std::make_shared<BlockNet>(blk);
    }

    void LinkChainState(ChainState&);
    size_t GetOptimalStorageSize();
    void InvalidateMilestone();

    void Serialize(VStream& s) const;
    void Deserialize(VStream& s);

    static BlockStatus CreateGenesisStat();
};

extern const BlockStatus GENESISSTAT;
