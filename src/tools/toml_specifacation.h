// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_TOML_SPECIFACATION_H
#define EPIC_TOML_SPECIFACATION_H

#include "cpptoml.h"
#include "pubkey.h"
#include "vertex.h"

#include <algorithm>
#include <vector>

std::string parseCKeyID(const tasm::Listing& data) {
    std::string addrString;
    VStream stream(data.data);
    stream >> addrString;
    auto key = DecodeAddress(addrString);
    if (key) {
        return key->GetHex();
    }
    return {};
}

auto TxInputToToml(const TxInput& input) {
    auto root = cpptoml::make_table();

    // output point
    root->insert("output_point_hash", std::to_string(input.outpoint.bHash));
    root->insert("output_point_txIndex", input.outpoint.txIndex);
    root->insert("output_point_outputIndex", input.outpoint.outIndex);

    // TODO signature, msg, pubkey

    return root;
}

auto TxOutputToToml(const TxOutput& output) {
    auto root = cpptoml::make_table();

    // address
    root->insert("address", parseCKeyID(output.listingContent));

    // value
    root->insert("value", output.value.GetValue());

    return root;
}

auto TxToToml(const Transaction& tx) {
    auto root = cpptoml::make_table();

    // redemption
    root->insert("isRedemption", !tx.IsFirstRegistration() && tx.IsRegistration());

    // first reg
    root->insert("isFirstReg", tx.IsFirstRegistration());

    // hash
    root->insert("tx_hash", std::to_string(tx.GetHash()));

    // inputs
    auto inputs_table = cpptoml::make_table_array();
    for (auto& input : tx.GetInputs()) {
        auto input_table = TxInputToToml(input);
        inputs_table->push_back(input_table);
    }
    root->insert("inputs", inputs_table);

    // outputs
    auto outputs_table = cpptoml::make_table_array();
    for (auto& output : tx.GetOutputs()) {
        auto output_table = TxOutputToToml(output);
        outputs_table->push_back(output_table);
    }
    root->insert("outputs", outputs_table);

    return root;
}

auto BlockToToml(ConstBlockPtr& block, const std::vector<uint8_t>& validity) {
    auto root = cpptoml::make_table();
    // block hash
    root->insert("block_hash", std::to_string(block->GetHash()));

    // 3 hash pointers
    root->insert("prev_hash", std::to_string(block->GetPrevHash()));
    root->insert("milestone_hash", std::to_string(block->GetMilestoneHash()));
    root->insert("tip_hash", std::to_string(block->GetTipHash()));

    // pow info
    root->insert("diff_target", block->GetDifficultyTarget());
    root->insert("nonce", block->GetNonce());
    root->insert("time", block->GetTime());

    // tx
    const auto& txns = block->GetTransactions();
    for (size_t i = 0; i < txns.size(); ++i) {
        root->insert("transaction", TxToToml(*txns[i]));
        root->insert("status", validity[i] == 0 ? "UNKNOWN" : validity[i] == 1 ? "VALID" : "INVALID");
    }

    return root;
}

auto VertexToToml(const VertexPtr& vertex) {
    auto root = cpptoml::make_table();

    root->insert("height", vertex->height);
    root->insert("cumulative_reward", vertex->cumulativeReward.GetValue());
    //    root->insert("fee", vertex->fee.GetValue());
    root->insert("is_milestone", vertex->isMilestone);
    root->insert("is_redeemed",
                 vertex->isRedeemed == 0 ? "not redemption" : vertex->isRedeemed == 1 ? "not yet" : "redeemed");
    root->insert("miner_chain_height", vertex->minerChainHeight);

    // state
    if (vertex->isMilestone) {
        auto state_info = cpptoml::make_table();

        state_info->insert("chain_work", vertex->snapshot->chainwork.GetCompact(false));
        state_info->insert("block_diff_target", vertex->snapshot->blockTarget.GetCompact(false));
        state_info->insert("ms_diff_target", vertex->snapshot->milestoneTarget.GetCompact(false));
        state_info->insert("hash_rate", vertex->snapshot->hashRate);
        state_info->insert("last_update_time", vertex->snapshot->lastUpdateTime);

        // TODO regChange
        root->insert("state_info", state_info);
    }

    // block
    root->insert("block", BlockToToml(vertex->cblock, vertex->validity));
    return root;
}

auto LvsWithVtxToToml(std::vector<VertexPtr>& vertices) {
    auto root  = cpptoml::make_table();
    auto array = cpptoml::make_table_array(false);
    std::swap(vertices.front(), vertices.back());
    for (auto& vtx : vertices) {
        array->push_back(VertexToToml(vtx));
    }
    root->insert("vertices", array);
    return root;
}
#endif // EPIC_TOML_SPECIFACATION_H
