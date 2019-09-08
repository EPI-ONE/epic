// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_BLOCK_H
#define EPIC_BLOCK_H

#include "net_message.h"
#include "spdlog.h"
#include "transaction.h"

#include <atomic>
#include <ctime>

// maximum allowed block size in optimal encoding format
static constexpr uint32_t MAX_BLOCK_SIZE = 20 * 1000;
// exact number of size of a block without counting transaction
static constexpr std::size_t HEADER_SIZE = 110;
// maximum time in a block header allowed to be in advanced to the current time (sec)
static constexpr uint32_t ALLOWED_TIME_DRIFT = 1;

namespace std {
string to_string(const Block& b, bool showtx = true, std::vector<uint8_t> validity = {});
} // namespace std

class Block : public NetMessage {
public:
    Block();
    Block(const Block&);
    Block(Block&&) noexcept;
    explicit Block(uint16_t versionNum);
    explicit Block(VStream&);

    Block(uint16_t version,
          uint256 milestoneHash,
          uint256 prevBlockHash,
          uint256 tipBlockHash,
          uint256 merkle,
          uint32_t time,
          uint32_t difficultyTarget,
          uint32_t nonce)
        : NetMessage(BLOCK), version_(version), milestoneBlockHash_(milestoneHash), prevBlockHash_(prevBlockHash),
          tipBlockHash_(tipBlockHash), merkleRoot_(merkle), time_(time), diffTarget_(difficultyTarget), nonce_(nonce) {
        CalculateOptimalEncodingSize();
    }

    Block& operator=(const Block&) = default;

    void SetNull();
    bool IsNull() const;
    void UnCache();

    uint16_t GetVersion() const;
    uint256 GetMilestoneHash() const;
    uint256 GetPrevHash() const;
    uint256 GetTipHash() const;
    uint256 GetMerkleRoot() const;
    uint32_t GetDifficultyTarget() const;
    uint32_t GetTime() const;
    uint32_t GetNonce() const;

    void SetMilestoneHash(const uint256&);
    void SetPrevHash(const uint256&);
    void SetTipHash(const uint256&);
    void SetDifficultyTarget(uint32_t target);
    void SetTime(uint32_t);
    void SetNonce(uint32_t);

    void AddTransaction(const Transaction&);
    void AddTransaction(ConstTxPtr);
    void AddTransactions(std::vector<ConstTxPtr>&&);
    bool HasTransaction() const;
    const std::vector<ConstTxPtr>& GetTransactions() const;
    std::vector<ConstTxPtr> GetTransactions();
    size_t GetTransactionSize() const;

    std::vector<uint256> GetTxHashes() const;
    uint256 ComputeMerkleRoot(bool* mutated = nullptr) const;

    const uint256& GetHash() const;
    void CalculateHash();
    void FinalizeHash();

    size_t CalculateOptimalEncodingSize();
    size_t GetOptimalEncodingSize() const;

    /*
     * Checks whether the block is correct in format.
     */
    bool Verify() const;

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

    ADD_SERIALIZE_METHODS
    ADD_NET_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(version_);
        READWRITE(milestoneBlockHash_);
        READWRITE(prevBlockHash_);
        READWRITE(tipBlockHash_);
        READWRITE(time_);
        READWRITE(diffTarget_);
        READWRITE(nonce_);
        READWRITE(transactions_);
        if (ser_action.ForRead()) {
            SetParents();
            FinalizeHash();
            CalculateOptimalEncodingSize();
        }
    }

    bool operator==(const Block& another) const {
        if (GetHash().IsNull() || another.GetHash().IsNull()) {
            return false;
        }

        return hash_ == another.hash_;
    }

    bool operator!=(const Block& another) const {
        return !(*this == another);
    }

    friend std::string std::to_string(const Block& b, bool showtx, std::vector<uint8_t> validity);

protected:
    uint256 hash_;

private:
    uint16_t version_;
    uint256 milestoneBlockHash_;
    uint256 prevBlockHash_;
    uint256 tipBlockHash_;
    uint256 merkleRoot_;
    uint32_t time_;
    uint32_t diffTarget_;
    uint32_t nonce_;
    std::vector<ConstTxPtr> transactions_;

    size_t optimalEncodingSize_ = 0;
    bool mutated                = false;

public:
    enum Source : uint8_t { UNKNOWN = 0, NETWORK = 1, MINER = 2 };
    Source source = UNKNOWN;
};

typedef std::shared_ptr<const Block> ConstBlockPtr;
extern Block GENESIS;

#endif // EPIC_BLOCK_H
