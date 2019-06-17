#ifndef __SRC_BLOCK_H__
#define __SRC_BLOCK_H__

#include <atomic>
#include <ctime>
#include <unordered_map>

#include "arith_uint256.h"
#include "pubkey.h"
#include "transaction.h"
#include "utilstrencodings.h"

static constexpr std::size_t HEADER_SIZE = 116;

namespace std {
string to_string(const Block& b, bool showtx = true);
} // namespace std

class Block {
public:
    Block();

    Block(const Block&);

    Block(uint32_t versionNum);

    Block(uint32_t version,
        uint256 milestoneHash,
        uint256 prevBlockHash,
        uint256 tipBlockHash,
        uint64_t time,
        uint32_t difficultyTarget,
        uint32_t nonce)
        : version_(version), milestoneBlockHash_(milestoneHash), prevBlockHash_(prevBlockHash),
          tipBlockHash_(tipBlockHash), time_(time), diffTarget_(difficultyTarget), nonce_(nonce) {
        CalculateOptimalEncodingSize();
    }

    void SetNull();

    bool IsNull() const;

    uint256 GetMilestoneHash() const;

    uint256 GetPrevHash() const;

    uint256 GetTipHash() const;

    void SetMilestoneHash(const uint256&);

    void SetPrevHash(const uint256&);

    void SetTipHash(const uint256&);

    void UnCache();

    bool Verify() const;

    void AddTransaction(const Transaction&);

    bool HasTransaction() const;

    const std::optional<Transaction>& GetTransaction() const;

    void SetDifficultyTarget(uint32_t target);

    uint32_t GetDifficultyTarget() const;

    void SetTime(uint64_t);

    uint64_t GetTime() const;

    void SetNonce(uint32_t);

    uint32_t GetNonce() const;

    void InvalidateMilestone();

    const uint256& GetHash() const;

    void FinalizeHash();

    void CalculateHash();

    const uint256& FinalizeTxHash();

    const uint256& GetTxHash() const;

    size_t GetOptimalEncodingSize() const;

    size_t CalculateOptimalEncodingSize();

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
     * A simple solver for nonce that makes the blocks hash lower than the
     * difficulty target. For test purposes only.
     */
    void Solve();

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

    bool operator==(const Block& another) const {
        if (GetHash().IsNull() || another.GetHash().IsNull()) {
            return false;
        }

        return hash_ == another.hash_;
    }

    friend std::string std::to_string(const Block& b, bool showtx);

    static Block CreateGenesis();

protected:
    uint256 hash_;
    size_t optimalEncodingSize_ = 0;

    uint32_t version_;
    uint256 milestoneBlockHash_;
    uint256 prevBlockHash_;
    uint256 tipBlockHash_;
    uint64_t time_;
    uint32_t diffTarget_;
    uint32_t nonce_;
    std::optional<Transaction> transaction_;
};

class BlockNet : public Block {
public:
    using Block::Block;

    BlockNet(const BlockNet&) = default;
    BlockNet(const Block& b);
    BlockNet(Block&& b);

    BlockNet(VStream&);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(Block, *this);
        READWRITE(transaction_);
        if (ser_action.ForRead() == true) {
            SetParents();
            FinalizeHash();
            CalculateOptimalEncodingSize();
        }
    }
};

typedef std::shared_ptr<const BlockNet> ConstBlockPtr;
extern Block GENESIS;

#endif //__SRC_BLOCK_H__
