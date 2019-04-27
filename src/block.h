#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <cstdint>
#include <ctime>

#include "arith_uint256.h"
#include "coin.h"
#include "hash.h"
#include "milestone.h"
#include "transaction.h"
#include "uint256.h"

static constexpr std::size_t HEADER_SIZE = 116;
static const arith_uint256 LARGEST_HASH =
    arith_uint256("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

enum MilestoneStatus : uint_fast8_t {
    IS_NOT_MILESTONE = 0,
    IS_TRUE_MILESTONE,
    IS_FAKE_MILESTONE,
};

class Block;

namespace std {
/*
 * Returns a multi-line string containing a description of the contents of
 * the block. Use for debugging purposes only.
 */
string to_string(Block& block);
} // namespace std

class BlockHeader {
public:
    BlockHeader();

    BlockHeader(uint32_t version,
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

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version_);
        READWRITE(milestoneBlockHash_);
        READWRITE(prevBlockHash_);
        READWRITE(tipBlockHash_);
        READWRITE(*((int64_t*) &time_));
        READWRITE(diffTarget_);
        READWRITE(nonce_);
    }

protected:
    uint256 hash_;

    // To be serialized to net and db
    uint32_t version_;
    uint256 milestoneBlockHash_;
    uint256 prevBlockHash_;
    uint256 tipBlockHash_;
    time_t time_;
    uint32_t diffTarget_;
    uint32_t nonce_;
};

class Block : public BlockHeader {
public:
    Block();

    Block(const Block&) = default;

    Block(uint32_t version);

    Block(const BlockHeader& header);

    void SetNull();

    bool Verify();

    void AddTransaction(Transaction& tx);

    bool HasTransaction() const;

    void SetMinerChainHeight(uint32_t height);

    void ResetReward();

    void InvalidateMilestone();

    void SetMilestoneInstance(Milestone& ms);

    const uint256& GetHash();

    const uint256& GetTxHash();

    size_t GetOptimalEncodingSize() const;

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
     * A simple solver for nonce that makes the blocks hash lower than the
     * difficulty target. For test purposes only.
     */
    void Solve();

    /*
     * Proves the block was as difficult to make as it claims to be.
     * Note that this function only checks the block hash is no greater than
     * the difficulty target contained in the header.
     * A complete syntax checking should also check the validity of the diff
     * target which is not considered here.
     */
    bool CheckPOW(bool throwException);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(BlockHeader, *this);
        READWRITE(transaction_);
    }

    void SerializeToDB(VStream& s) const;

    void DeserializeFromDB(VStream& s);

    template <typename Stream>
    inline void SerializeToHash(Stream& s) {
        static_cast<BlockHeader>(*this).Serialize(s);
        GetTxHash().Serialize(s);
    }

    friend std::string std::to_string(Block& block);

private:
    std::optional<Transaction> transaction_;
    Coin cumulativeReward_;
    uint256 hash_;

    std::shared_ptr<Milestone> milestoneInstance_;
    uint64_t minerChainHeight_;
    bool isMilestone_ = false;
};


#endif //__SRC_BLOCK_H__
