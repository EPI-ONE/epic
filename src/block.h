#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <atomic>
#include <ctime>
#include <unordered_map>

#include "arith_uint256.h"
#include "pubkey.h"
#include "threadpool.h"
#include "transaction.h"
#include "utilstrencodings.h"

class Block;
namespace std {
string to_string(Block& block);
} // namespace std

enum MilestoneStatus : uint8_t {
    IS_NOT_MILESTONE = 0,
    IS_TRUE_MILESTONE,
    IS_FAKE_MILESTONE,
};

struct Milestone {
    uint64_t lastUpdateTime_;
    uint64_t hashRate_;
    uint64_t height_;

    arith_uint256 milestoneTarget_;
    arith_uint256 blockTarget_;
    arith_uint256 chainwork_;

    std::shared_ptr<Milestone> pprevious_;
    std::shared_ptr<Milestone> pnext_;

    // a vector consists of blocks with its offset w.r.t this level set of this milestone
    std::vector<std::pair<std::shared_ptr<Block>, uint64_t>> vblockstore_;
    std::unordered_map<uint256, uint256> pubkeySnapshot_;

    /* deault constructor */
    Milestone() = default;

    /* standard constructor that only
     * initializes the first six members */
    Milestone(uint64_t lastUpdateTime,
        uint64_t hashRate,
        uint64_t height,
        arith_uint256 milestoneTarget,
        arith_uint256 blockTarget,
        arith_uint256 chainwork)
        : lastUpdateTime_(lastUpdateTime), hashRate_(hashRate), height_(height), milestoneTarget_(milestoneTarget),
          blockTarget_(blockTarget), chainwork_(chainwork) {}

    bool operator==(const Milestone& another) {
        return lastUpdateTime_ == another.lastUpdateTime_ && chainwork_ == another.chainwork_ &&
               hashRate_ == another.hashRate_ && milestoneTarget_ == another.milestoneTarget_ &&
               blockTarget_ == another.blockTarget_;
    }
};

class Block {
public:
    Block();

    Block(const Block&) = default;

    Block(uint32_t versionNum);

    Block(uint32_t version,
        uint256 milestoneHash,
        uint256 prevBlockHash,
        uint256 tipBlockHash,
        uint32_t time,
        uint32_t difficultyTarget,
        uint32_t nonce)
        : version_(version), milestoneBlockHash_(milestoneHash), prevBlockHash_(prevBlockHash),
          tipBlockHash_(tipBlockHash), time_(time), diffTarget_(difficultyTarget), nonce_(nonce) {}

    void SetNull();

    bool IsNull() const;

    uint256 GetMilestoneHash() const;

    uint256 GetPrevHash() const;

    uint256 GetTipHash() const;

    void SetMilestoneHash(const uint256&);

    void SetPrevHash(const uint256&);

    void SetTIPHash(const uint256&);

    void UnCache();

    bool Verify() const;

    void AddTransaction(Transaction);

    bool HasTransaction() const;

    std::optional<Transaction>& GetTransaction();

    void SetMinerChainHeight(uint32_t);

    void SetCumulativeReward(Coin coin);

    void ResetReward();

    void SetDifficultyTarget(uint32_t);

    const uint32_t GetDifficultyTarget() const;

    void SetTime(uint64_t);

    const uint64_t GetTime() const;

    void SetNonce(uint32_t);

    const uint32_t GetNonce() const;

    void InvalidateMilestone();

    void SetMilestoneInstance(Milestone&);

    const std::shared_ptr<Milestone>& GetMilestoneInstance() const;

    const uint256& GetHash() const;

    void FinalizeHash() const;

    void CalculateHash() const;

    const uint256& GetTxHash() const;

    virtual size_t GetOptimalEncodingSize();

    /*
     * Checks whether the block is a registration block.
     */
    bool IsRegistration() const;

    /*
     * Checks whether the block is the first registration block on some peer
     * chain.
     */
    bool IsFirstRegistration() const;

    /*
     * Returns the chainwork represented by this block, which is defined as
     * the number of tries needed to solve a block in the on average.
     * It is calculated by the largest possible hash divided by the difficulty
     * target.
     */
    arith_uint256 GetChainWork() const;

    /*
     * Returns the difficulty target as a 256 bit value that can be compared
     * to a SHA-256 hash. Inside a block the target is represented using a
     * compact form. If this form decodes to a value that is out of bounds,
     * an exception is thrown.
     */
    arith_uint256 GetTargetAsInteger() const;

    /*
     * Proves the block was as difficult to make as it claims to be.
     * Note that this function only checks the block hash is no greater than
     * the difficulty target contained in the header.
     * A complete syntax checking should also check the validity of the diff
     * target which is not considered here.
     */
    bool CheckPOW() const;

    /*
     * Only to be used for debugging when validity of the
     * block does not matter e.g DFS testing
     */
    void RandomizeHash() const;

    /*
     * A simple solver for nonce that makes the blocks hash lower than the
     * difficulty target. For test purposes only.
     */
    void Solve();

    /*
     * A multi-thread solver for nonce that makes the blocks hash lower than the
     * difficulty target. For test purposes only.
     */
    void Solve(ThreadPool& pool);

    /*
     * Sets parents for elements contained in the block all at once
     */
    void SetParents();

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version_);
        READWRITE(milestoneBlockHash_);
        READWRITE(prevBlockHash_);
        READWRITE(tipBlockHash_);
        READWRITE(time_);
        READWRITE(diffTarget_);
        READWRITE(nonce_);
    }

    friend std::string std::to_string(Block& block);

    static void SerializeMilestone(VStream&, Milestone&);
    static void DeserializeMilestone(VStream&, Milestone&);

    static Block CreateGenesis();

protected:
    uint256 milestoneBlockHash_;
    uint256 prevBlockHash_;
    uint256 tipBlockHash_;

    uint32_t diffTarget_;
    uint32_t version_;
    uint32_t nonce_;

    uint64_t time_;

    std::optional<Transaction> transaction_;
    Coin cumulativeReward_;
    mutable uint256 hash_;

    std::shared_ptr<Milestone> milestone_;
    uint64_t minerChainHeight_;
    bool isMilestone_ = false;

    size_t optimalEncodingSize_ = 0;
};

class BlockNet : public Block {
public:
    using Block::Block;

    BlockNet(const BlockNet&) = default;

    BlockNet(const Block& b);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(Block, *this);
        READWRITE(transaction_);
        if (ser_action.ForRead() == true) {
            SetParents();
        }
    }

    size_t GetOptimalEncodingSize();
};

class BlockDag : public BlockNet {
public:
    using BlockNet::BlockNet;

    BlockDag(const BlockDag&) = default;

    BlockDag(const BlockNet& b);

    void Serialize(VStream& s) const;

    void Deserialize(VStream& s);

    size_t GetOptimalEncodingSize();
};

extern const Block GENESIS;
static constexpr std::size_t HEADER_SIZE                 = 116;
static constexpr std::size_t MAX_ADDITIONAL_STORAGE_SIZE = 58;

typedef std::shared_ptr<const Block> BlockPtr;

#endif //__SRC_BLOCK_H__
