#ifndef __SRC_CONSENSUS_H__
#define __SRC_CONSENSUS_H__

#include "block.h"
#include "utxo.h"

class NodeRecord;
typedef std::shared_ptr<NodeRecord> RecordPtr;
extern const NodeRecord GENESIS_RECORD;

class ChainState {
public:
    uint64_t height;
    arith_uint256 chainwork;
    uint64_t lastUpdateTime;
    arith_uint256 milestoneTarget;
    arith_uint256 blockTarget;
    uint64_t hashRate;

    // constructor of a chain state of genesis.
    ChainState();
    // constructor of a chain state with all data fields
    ChainState(std::shared_ptr<ChainState>, const ConstBlockPtr&, std::vector<uint256>&&);
    // constructor of a chain state by vstream
    ChainState(VStream&);
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

    inline uint64_t GetBlockDifficulty() const {
        return (arith_uint256(params.maxTarget) / (blockTarget + 1)).GetLow64();
    }

    inline uint64_t GetMsDifficulty() const {
        return (arith_uint256(params.maxTarget) / (milestoneTarget + 1)).GetLow64();
    }

    const std::vector<uint256>& GetRecordHashes() const {
        return lvsHashes_;
    }

    const uint256& GetMilestoneHash() const {
        return lvsHashes_.back(); 
    }

    const TXOC& GetTXOC() const {
        return txoc_;
    }

    void UpdateTXOC(TXOC&&, std::unordered_map<uint256, UTXO>&, std::unordered_set<uint256>&); 

    ADD_SERIALIZE_METHODS;
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
    // a vector of TXOC: changes on transaction outputs from previous chain state
    TXOC txoc_;

    void UpdateDifficulty(uint64_t blockUpdateTime);
};

typedef std::shared_ptr<ChainState> ChainStatePtr;

ChainStatePtr make_shared_ChainState();
ChainStatePtr make_shared_ChainState(ChainStatePtr previous, NodeRecord& record, std::vector<uint256>&& hashes);

/*
 * A structure that contains a shared_ptr<const BlockNet> that will
 * be passed to different chains
 */
class NodeRecord {
public:
    // transaction validity
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

    enum RedemptionStatus : uint8_t {
        IS_NOT_REDEMPTION = 0, 
        NOT_YET_REDEEMED,
        IS_REDEEMED,
    };

    ConstBlockPtr cblock;

    Coin cumulativeReward;
    Coin fee;
    uint64_t minerChainHeight = 0;

    RedemptionStatus isRedeemed = RedemptionStatus::IS_NOT_REDEMPTION;
    uint256 prevRedemHash       = Hash::GetDoubleZeroHash();

    bool isMilestone = false;
    ChainStatePtr snapshot = nullptr;

    Validity validity = Validity::UNKNOWN;
    size_t optimalStorageSize = 0;

    NodeRecord();
    NodeRecord(const ConstBlockPtr&);
    NodeRecord(const BlockNet&);
    NodeRecord(VStream&);

    void LinkChainState(const ChainStatePtr&);
    size_t GetOptimalStorageSize();
    void InvalidateMilestone();
    void UpdateReward(const Coin&, bool);

    void Serialize(VStream& s) const;
    void Deserialize(VStream& s);

    bool operator==(const NodeRecord& another) const {
        return std::tie(*cblock, cumulativeReward, minerChainHeight) ==
                   std::tie(*(another.cblock), another.cumulativeReward, another.minerChainHeight) &&
               ((snapshot == nullptr || another.snapshot == nullptr) ? true : *snapshot == *(another.snapshot));
    }

    bool operator!=(const NodeRecord& another) const {
        return !(*this == another);
    }

    static NodeRecord CreateGenesisRecord();
};

namespace std {
string to_string(const ChainState&);
string to_string(const NodeRecord& rec, bool showtx = false);
} // namespace std

#endif // __SRC_CONSENSUS_H__
