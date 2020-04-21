// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_tools.h"

#include "block.h"
#include "milestone.h"
#include "vertex.h"

//////////////////// CPP internal methods ///////////////////////////

rpc::Outpoint* ToRPCOutPoint(const TxOutPoint& outpoint) {
    auto rpc_outpoint = new rpc::Outpoint();
    rpc_outpoint->set_from_block(std::to_string(outpoint.bHash));
    rpc_outpoint->set_tx_idx(outpoint.txIndex);
    rpc_outpoint->set_out_idx(outpoint.outIndex);
    return rpc_outpoint;
}

rpc::Input* ToRPCInput(const TxInput& input) {
    auto rpc_input = new rpc::Input;
    rpc_input->set_allocated_outpoint(ToRPCOutPoint(input.outpoint));
    rpc_input->set_listing(std::to_string(input.listingContent));
    return rpc_input;
}

rpc::Output* ToRPCOutput(const TxOutput& output) {
    auto rpc_output = new rpc::Output;
    rpc_output->set_listing(std::to_string(output.listingContent));
    rpc_output->set_money(output.value.GetValue());
    return rpc_output;
}

////////////////// End of internal methods ////////////////////////

rpc::Transaction* ToRPCTx(const Transaction& tx) {
    auto rpc_tx    = new rpc::Transaction();
    auto rpc_input = rpc_tx->mutable_inputs();
    for (const auto& in : tx.GetInputs()) {
        rpc_input->AddAllocated(ToRPCInput(in));
    }

    auto rpc_output = rpc_tx->mutable_outputs();
    for (const auto& out : tx.GetOutputs()) {
        rpc_output->AddAllocated(ToRPCOutput(out));
    }

    return rpc_tx;
}

rpc::Block* ToRPCBlock(const Block& b, rpc::Block* res) {
    rpc::Block* rpcb = nullptr;
    if (res) {
        rpcb = res;
    } else {
        rpcb = new rpc::Block();
    }

    rpcb->set_hash(std::to_string(b.GetHash()));
    rpcb->set_version(b.GetVersion());

    rpcb->set_mshash(std::to_string(b.GetMilestoneHash()));
    rpcb->set_prevhash(std::to_string(b.GetPrevHash()));
    rpcb->set_tiphash(std::to_string(b.GetTipHash()));

    rpcb->set_difftarget(b.GetDifficultyTarget());
    rpcb->set_nonce(b.GetNonce());
    rpcb->set_time(b.GetTime());

    for (auto&& e : b.GetProof()) {
        rpcb->add_proof(e);
    }

    auto txns = b.GetTransactions();
    if (!txns.empty()) {
        auto rpc_tx = rpcb->mutable_transactions();
        rpc_tx->Reserve(txns.size());
        for (const auto& tx : txns) {
            rpc_tx->AddAllocated(ToRPCTx(*tx));
        }
    }

    return rpcb;
}

rpc::Vertex* ToRPCVertex(const Vertex& vertex, rpc::Vertex* res) {
    rpc::Vertex* rpc_vertex;
    if (res) {
        rpc_vertex = res;
    } else {
        rpc_vertex = new rpc::Vertex();
    }
    auto* rpc_block_p = ToRPCBlock(*(vertex.cblock), nullptr);

    rpc_vertex->set_allocated_block(rpc_block_p);
    rpc_vertex->set_height(vertex.height);
    rpc_vertex->set_ismilestone(vertex.isMilestone);
    rpc_vertex->set_redemptionstatus(vertex.isRedeemed);

    auto txStatus = rpc_vertex->mutable_txstatus();
    txStatus->Reserve(vertex.validity.size());
    for (const auto& val : vertex.validity) {
        txStatus->Add(val == Vertex::Validity::VALID);
    }

    rpc_vertex->set_rewards(vertex.cumulativeReward.GetValue());

    return rpc_vertex;
}

rpc::Chain* ToRPCChain(const Vertex& vertex) {
    auto rpc_chain = new rpc::Chain();

    rpc_chain->set_headhash(std::to_string(vertex.cblock->GetHash()));
    rpc_chain->set_pcheight(vertex.minerChainHeight);
    rpc_chain->set_time(vertex.cblock->GetTime());

    return rpc_chain;
}

rpc::Milestone* ToRPCMilestone(const Vertex& msVer, rpc::Milestone* res) {
    rpc::Milestone* rpc_milestone;
    if (res) {
        rpc_milestone = res;
    } else {
        rpc_milestone = new rpc::Milestone();
    }

    const auto ms = msVer.snapshot;
    rpc_milestone->set_height(msVer.height);
    rpc_milestone->set_chainwork(std::to_string(ms->chainwork));
    rpc_milestone->set_blkdiff(ms->GetBlockDifficulty());
    rpc_milestone->set_msdiff(ms->GetMsDifficulty());
    rpc_milestone->set_hashrate(static_cast<uint64_t>(ms->hashRate));
    rpc_milestone->set_hash(std::to_string(msVer.cblock->GetHash()));
    rpc_milestone->set_time(msVer.cblock->GetTime());

    return rpc_milestone;
}

rpc::MsChain* ToRPCMsChain(const Vertex& msVer) {
    auto rpc_ms_chain = new rpc::MsChain();

    rpc_ms_chain->set_allocated_chain(ToRPCChain(msVer));
    rpc_ms_chain->set_allocated_milestone(ToRPCMilestone(msVer));

    return rpc_ms_chain;
}
