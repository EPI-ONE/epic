// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "vertex.h"

#include <numeric>

Vertex::Vertex() : minerChainHeight(0), optimalStorageSize_(0) {}

Vertex::Vertex(const ConstBlockPtr& blk) : cblock(blk), minerChainHeight(0), optimalStorageSize_(0) {
    if (cblock) {
        auto n = cblock->GetTransactionSize();
        if (n > 0) {
            validity.resize(n);
            memset(&validity[0], UNKNOWN, n);
        }
    }
}

Vertex::Vertex(const Block& blk) : minerChainHeight(0), optimalStorageSize_(0) {
    cblock = std::make_shared<Block>(blk);
    auto n = cblock->GetTransactionSize();
    if (n > 0) {
        validity.resize(n);
        memset(&validity[0], UNKNOWN, n);
    }
}

Vertex::Vertex(Block&& blk) : minerChainHeight(0), optimalStorageSize_(0) {
    cblock = std::make_shared<Block>(std::move(blk));
    auto n = cblock->GetTransactionSize();
    if (n > 0) {
        validity.resize(n);
        memset(&validity[0], UNKNOWN, n);
    }
}

Vertex::Vertex(VStream& s) {
    s >> *this;
}

void Vertex::LinkMilestone(const MilestonePtr& pcs) {
    snapshot    = pcs;
    isMilestone = true;
}

void Vertex::UpdateReward(const Coin& prevReward) {
    // cumulative reward without fee; default for blocks except first registration
    cumulativeReward = prevReward + GetParams().reward;

    if (cblock->HasTransaction()) {
        if (cblock->IsRegistration()) {
            // remaining reward = last cumulative reward - redemption amount
            cumulativeReward -= cblock->GetTransactions()[0]->GetOutputs()[0].value;
        }

        cumulativeReward += fee;
    }
}

void Vertex::UpdateMilestoneReward() {
    if (!isMilestone) {
        return;
    }

    cumulativeReward += GetParams().reward * ((snapshot->GetLevelSet().size() - 1) / GetParams().msRewardCoefficient);
}

size_t Vertex::GetNumOfValidTxns() const {
    return std::accumulate(validity.cbegin(), validity.cend(), 0,
                           [](const size_t& sum, const uint8_t& v) { return sum + (v == Vertex::Validity::VALID); });
}

size_t Vertex::GetOptimalStorageSize() {
    if (optimalStorageSize_ > 0) {
        return optimalStorageSize_;
    }

    optimalStorageSize_ += (1                                                               // RedemptionStatus
                            + GetSizeOfVarInt(height)                                       // block height
                            + GetSizeOfVarInt(cumulativeReward.GetValue())                  // reward
                            + GetSizeOfVarInt(minerChainHeight)                             // miner chain height
                            + ::GetSizeOfCompactSize(validity.size()) + validity.size() * 1 // validity
                            + 1                                                             // MilestoneStatus
    );

    // Milestone
    if (snapshot != nullptr) {
        optimalStorageSize_ += (4   // hash rate
                                + 4 // ms target
                                + 4 // block target
        );
    }

    return optimalStorageSize_;
}

std::string std::to_string(const Vertex& vtx, bool showtx) {
    std::string s = "Vertex {\n";
    s += strprintf("   at height :   %i \n", vtx.height);
    s += strprintf("   is milestone: %s \n\n", vtx.isMilestone);

    if (vtx.snapshot) {
        s += "   with snapshot of ";
        s += to_string(*(vtx.snapshot));
    }

    if (vtx.cblock) {
        s += strprintf("   contains%s \n", std::to_string(*(vtx.cblock), showtx, vtx.validity));
    }

    s += strprintf("   miner chain height: %s \n", vtx.minerChainHeight);
    s += strprintf("   cumulative reward:  %s \n", vtx.cumulativeReward.GetValue());

    const std::array<std::string, 3> enumRedeemption{"IS_NOT_REDEMPTION", "NOT_YET_REDEEMED", "IS_REDEEMED"};
    s += strprintf("   redemption status:  %s \n", enumRedeemption[vtx.isRedeemed]);
    return s;
}
