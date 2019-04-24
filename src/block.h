#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include "arith_uint256.h"
#include "coin.h"
#include "hash.h"
#include "milestone.h"
#include "serialize.h"
#include "transaction.h"
#include "uint256.h"
#include <cstdint>

static constexpr int HEADER_SIZE = 112;
static const arith_uint256 LARGEST_HASH =
    arith_uint256("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

class Transaction;
class BlockHeader {
public:
    BlockHeader() {
        SetNull();
    }

    // Initialize with all the header fields
    BlockHeader(uint32_t version,
        uint256 milestoneHash,
        uint256 prevBlockHash,
        uint256 tipBlockHash,
        uint32_t time,
        uint32_t difficultyTarget,
        uint32_t nonce)
        : version_(version), milestoneBlockHash_(milestoneHash), prevBlockHash_(prevBlockHash),
          tipBlockHash_(tipBlockHash), time_(time), diffTarget_(difficultyTarget), nonce_(nonce) {}

    void SetNull() {
        version_ = 0;
        milestoneBlockHash_.SetNull();
        prevBlockHash_.SetNull();
        tipBlockHash_.SetNull();
        time_       = 0;
        diffTarget_ = 0;
        nonce_      = 0;
    }

    bool IsNull() const {
        return (time_ == 0);
    }

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

enum MilestoneStatus : uint8_t {
    IS_NOT_MILESTONE  = 0,
    IS_TRUE_MILESTONE = 1,
    IS_FAKE_MILESTONE = 2,
};

class Block : public BlockHeader {
public:
    // parameter restrictions

    // constructor and destructor
    Block() {
        SetNull();
    }

    Block(const Block&) = default;
    Block(const BlockHeader& header) {
        SetNull();
        *(static_cast<BlockHeader*>(this)) = header;
    }
    Block(uint32_t version);

    void SetNull() {
        BlockHeader::SetNull();
        transaction_.clear();
    }

    ~Block(){};

    // verify block content syntactically
    bool Verify();
    void AddTransaction(Transaction& tx);
    bool HasTransaction() const {
        return !transaction_.empty();
    }
    void SetMinerChainHeight(uint32_t height) {
        minerChainHeight_ = height;
    }
    void ResetReward() {
        cumulativeReward_ = ZERO_COIN;
    }
    void InvalidateMilestone() {
        isMilestone_ = false;
    }
    void SetMilestoneInstance(Milestone& ms) {
        milestoneInstance_ = &ms;
        isMilestone_       = true;
    }

    /**
     * TODO: serialize the header and compute the hash of the block
     * Currently is only a placeholder.
     */
    const uint256& GetHash() {
        if (!hash_.IsNull()) {
            return hash_;
        }
        VStream s;
        SerializeToHash(s);
        hash_ = Hash<1>(s);
        return hash_;
    }
    const uint256& GetTxHash();
    // TODO
    size_t GetOptimalEncodingSize() const {
        return 0;
    }
    /**
     * Checks whether the block is a registration block.
     */
    bool IsRegistration() const;
    /**
     * Checks whether the block is the first registration block on some peer
     * chain.
     */
    bool IsFirstRegistration() const;
    /**
     * Returns the chainwork represented by this block, which is defined as
     * the number of tries needed to solve a block in the on average.
     * It is calculated by the largest possible hash divided by the difficulty
     * target.
     */
    arith_uint256 GetChainWork() const {
        arith_uint256 target = GetTargetAsInteger();
        return LARGEST_HASH / (target + 1);
    }
    /**
     * Returns the difficulty target as a 256 bit value that can be compared
     * to a SHA-256 hash. Inside a block the target is represented using a
     * compact form. If this form decodes to a value that is out of bounds,
     * an exception is thrown.
     */
    arith_uint256 GetTargetAsInteger() const;
    /**
     * A simple solver for nonce that makes the blocks hash lower than the
     * difficulty target. For test purposes only.
     */
    void Solve();
    /**
     * Proves the block was as difficult to make as it claims to be.
     * Note that this function only checks the block hash is no greater than
     * the difficulty target contained in the header.
     * A complete syntax checking should also check the validity of the diff
     * target which is not considered here.
     */
    bool CheckPOW(bool throwException);
    /**
     * Returns a multi-line string containing a description of the contents of
     * the block. Use for debugging purposes only.
     */
    std::string ToString();

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(BlockHeader, *this);
        READWRITE(transaction_);
    }

    void SerializeToDB(VStream& s) const;
    void UnserializeFromDB(VStream& s);

    template <typename Stream>
    inline void SerializeToHash(Stream& s) {
        static_cast<BlockHeader>(*this).Serialize(s);
        GetTxHash().Serialize(s);
    }

    // get & set methods

private:
    uint256 hash_;
    // to be serialized to db only
    Coin cumulativeReward_;
    uint64_t minerChainHeight_;
    Milestone* milestoneInstance_;
    bool isMilestone_ = false;

    // content
    std::vector<Transaction> transaction_;
};

#endif //__SRC_BLOCK_H__
