#ifndef __SRC_CONSENSUS_H__
#define __SRC_CONSENSUS_H__

#include "block.h"
#include "utxo.h"

enum MilestoneStatus : uint8_t {
    IS_NOT_MILESTONE = 0,
    IS_TRUE_MILESTONE,
    IS_FAKE_MILESTONE,
};

class NodeRecord;
typedef std::shared_ptr<NodeRecord> RecordPtr;

class ChainState {
public:
    uint64_t height;
    arith_uint256 chainwork;
    uint32_t lastUpdateTime;
    arith_uint256 milestoneTarget;
    arith_uint256 blockTarget;
    uint64_t hashRate;

    // Incremental change of the last registration block on each peer chain,
    // whose elements are pairs consisting of:
    //   <peer chain head, hash of the last registration block on this peer chain>
    RegChange regChange;

    // constructor of a chain state of genesis.
    ChainState() = default;
    // constructor of a chain state with all data fields
    ChainState(const std::shared_ptr<ChainState>&, const ConstBlockPtr&, std::vector<uint256>&&);
    // constructor of a chain state by vstream
    ChainState(VStream&);
    // copy constructor
    ChainState(const ChainState&) = default;

    /* simple constructor (now for test only) */
    ChainState(uint64_t height,
               arith_uint256 chainwork,
               uint32_t lastUpdateTime,
               arith_uint256 milestoneTarget,
               arith_uint256 blockTarget,
               uint64_t hashRate,
               std::vector<uint256>&& lvsHashes)
        : height(height), chainwork(chainwork), lastUpdateTime(lastUpdateTime), milestoneTarget(milestoneTarget),
          blockTarget(blockTarget), hashRate(hashRate), lvsHashes_(std::move(lvsHashes)) {}

    inline bool IsDiffTransition() const {
        return ((height + 1) % GetParams().interval) == 0;
    }

    inline uint64_t GetBlockDifficulty() const {
        return (arith_uint256(GetParams().maxTarget) / (blockTarget + 1)).GetLow64();
    }

    inline uint64_t GetMsDifficulty() const {
        return (arith_uint256(GetParams().maxTarget) / (milestoneTarget + 1)).GetLow64();
    }

    const std::vector<uint256>& GetRecordHashes() const {
        return lvsHashes_;
    }

    void PushHash(const uint256& hash) {
        lvsHashes_.emplace_back(hash);
    }

    const uint256& GetMilestoneHash() const {
        return lvsHashes_.back();
    }

    const TXOC& GetTXOC() const {
        return txoc_;
    }

    void UpdateTXOC(TXOC&&);

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(height));
        READWRITE(VARINT(hashRate));
        READWRITE(lastUpdateTime);
        if (ser_action.ForRead()) {
            chainwork.SetCompact(ser_readdata32(s));
            milestoneTarget.SetCompact(ser_readdata32(s));
            blockTarget.SetCompact(ser_readdata32(s));
        } else {
            ::Serialize(s, chainwork.GetCompact());
            ::Serialize(s, milestoneTarget.GetCompact());
            ::Serialize(s, blockTarget.GetCompact());
        }
    }

    bool operator==(const ChainState& rhs) const {
        // clang-format off
        return lastUpdateTime               == rhs.lastUpdateTime &&
               chainwork.GetCompact()       == rhs.chainwork.GetCompact() &&
               hashRate                     == rhs.hashRate &&
               milestoneTarget.GetCompact() == rhs.milestoneTarget.GetCompact() &&
               blockTarget.GetCompact()     == rhs.blockTarget.GetCompact();
        // clang-format on
    }

    bool operator!=(const ChainState& another) const {
        return !(*this == another);
    }

private:
    // a vector consists of hashes of blocks in level set of this chain state
    std::vector<uint256> lvsHashes_;
    // TXOC: changes on transaction outputs from previous chain state
    TXOC txoc_;

    void UpdateDifficulty(uint32_t blockUpdateTime);
};

typedef std::shared_ptr<ChainState> ChainStatePtr;

ChainStatePtr CreateNextChainState(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes);

/*
 * A structure that contains a shared_ptr<const Block> that will
 * be passed to different chains
 */
class NodeRecord {
public:
    // transaction validity
    enum Validity : uint8_t {
        UNKNOWN = 0,
        VALID   = 1,
        INVALID = 2,
    };

    enum RedemptionStatus : uint8_t {
        IS_NOT_REDEMPTION = 0, // double zero hash
        NOT_YET_REDEEMED  = 1, // hash of previous redemption block
        IS_REDEEMED       = 2, // null hash
    };

    ConstBlockPtr cblock;

    uint64_t height = 0;

    Coin cumulativeReward;
    Coin fee;
    uint64_t minerChainHeight = 0;

    uint8_t isRedeemed = RedemptionStatus::IS_NOT_REDEMPTION;
    uint256 prevRedemHash;

    bool isMilestone       = false;
    ChainStatePtr snapshot = nullptr;

    uint8_t validity = Validity::UNKNOWN;

    NodeRecord();
    NodeRecord(const NodeRecord&) = default;
    explicit NodeRecord(const ConstBlockPtr&);
    explicit NodeRecord(const Block&);
    explicit NodeRecord(Block&&);
    explicit NodeRecord(VStream&);

    void LinkChainState(const ChainStatePtr&);
    size_t GetOptimalStorageSize();
    void UpdateReward(const Coin&);

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(VARINT(height));
        READWRITE(cumulativeReward);
        READWRITE(VARINT(minerChainHeight));
        READWRITE(validity);
        READWRITE(isRedeemed);

        if (ser_action.ForRead()) {
            auto msFlag = static_cast<MilestoneStatus>(ser_readdata8(s));
            isMilestone = (msFlag == IS_TRUE_MILESTONE);
            if (msFlag > 0) {
                ChainState cs{};
                ::Deserialize(s, cs);
                snapshot = std::make_shared<ChainState>(std::move(cs));
            }
        } else {
            if (isMilestone) {
                ::Serialize(s, static_cast<uint8_t>(IS_TRUE_MILESTONE));
            } else if (snapshot != nullptr) {
                ::Serialize(s, static_cast<uint8_t>(IS_FAKE_MILESTONE));
            } else {
                ::Serialize(s, static_cast<uint8_t>(IS_NOT_REDEMPTION));
            }
            if (snapshot != nullptr) {
                ::Serialize(s, *snapshot);
            }
        }
    }

    bool operator==(const NodeRecord& another) const {
        // clang-format off
        return height           == another.height &&
               cumulativeReward == another.cumulativeReward &&
               minerChainHeight == another.minerChainHeight &&
               validity         == another.validity &&
               isRedeemed       == another.isRedeemed &&
               prevRedemHash    == another.prevRedemHash &&
               isMilestone      == another.isMilestone &&
               ((snapshot == nullptr || another.snapshot == nullptr) ? true : *snapshot == *(another.snapshot)) &&
               ((cblock == nullptr || another.cblock == nullptr) ? true : *cblock == *(another.cblock));
        // clang-format on
    }

    bool operator!=(const NodeRecord& another) const {
        return !(*this == another);
    }

private:
    size_t optimalStorageSize_ = 0;
};

namespace std {
string to_string(const ChainState&);
string to_string(const NodeRecord& rec, bool showtx = false);
} // namespace std

extern NodeRecord GENESIS_RECORD;

#endif // __SRC_CONSENSUS_H__
