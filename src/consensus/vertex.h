// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_VERTEX_H
#define EPIC_VERTEX_H

#include "block.h"
#include "milestone.h"

enum MilestoneStatus : uint8_t {
    IS_NOT_MILESTONE = 0,
    IS_TRUE_MILESTONE,
    IS_FAKE_MILESTONE,
};

/*
 * A structure that contains a shared_ptr<const Block> that will
 * be passed to different chains
 */
class Vertex {
public:
    // transaction validity
    enum Validity : uint8_t {
        UNKNOWN = 0,
        VALID   = 1,
        INVALID = 2,
    };

    enum RedemptionStatus : uint8_t {
        IS_NOT_REDEMPTION = 0,
        NOT_YET_REDEEMED  = 1,
        IS_REDEEMED       = 2,
    };

    ConstBlockPtr cblock;

    uint64_t height = 0;

    Coin cumulativeReward     = 0;
    Coin fee                  = 0;
    uint64_t minerChainHeight = 0;

    uint8_t isRedeemed = RedemptionStatus::IS_NOT_REDEMPTION;

    bool isMilestone                    = false;
    std::shared_ptr<Milestone> snapshot = nullptr;

    std::vector<uint8_t> validity;

    Vertex();
    Vertex(const Vertex&) = default;
    explicit Vertex(const ConstBlockPtr&);
    explicit Vertex(const Block&);
    explicit Vertex(Block&&);
    explicit Vertex(VStream&);

    void LinkMilestone(const std::shared_ptr<Milestone>&);
    size_t GetOptimalStorageSize();
    void UpdateReward(const Coin&);
    void UpdateMilestoneReward();

    // returns the number of valid transactions
    size_t GetNumOfValidTxns() const;

    ADD_SERIALIZE_METHODS
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(isRedeemed);
        READWRITE(VARINT(height));
        READWRITE(cumulativeReward);
        READWRITE(VARINT(minerChainHeight));
        READWRITE(validity);

        if (ser_action.ForRead()) {
            auto msFlag = static_cast<MilestoneStatus>(ser_readdata8(s));
            isMilestone = (msFlag == IS_TRUE_MILESTONE);
            if (msFlag > 0) {
                Milestone ms{};
                ::Deserialize(s, ms);
                snapshot         = std::make_shared<Milestone>(std::move(ms));
                snapshot->height = height;
                if (snapshot->IsDiffTransition() && cblock) {
                    snapshot->lastUpdateTime = cblock->GetTime();
                }
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

    bool operator==(const Vertex& another) const {
        // clang-format off
        return height           == another.height &&
               cumulativeReward == another.cumulativeReward &&
               minerChainHeight == another.minerChainHeight &&
               validity         == another.validity &&
               isRedeemed       == another.isRedeemed &&
               isMilestone      == another.isMilestone &&
               ((snapshot == nullptr || another.snapshot == nullptr) ? true : *snapshot == *(another.snapshot)) &&
               ((cblock == nullptr || another.cblock == nullptr) ? true : *cblock == *(another.cblock));
        // clang-format on
    }

    bool operator!=(const Vertex& another) const {
        return !(*this == another);
    }

private:
    size_t optimalStorageSize_ = 0;
};

typedef std::shared_ptr<Vertex> VertexPtr;
typedef std::weak_ptr<Vertex> VertexWPtr;

namespace std {
string to_string(const Vertex& vtx, bool showtx = false);
} // namespace std

extern std::shared_ptr<Vertex> GENESIS_VERTEX;

#endif // EPIC_VERTEX_H
