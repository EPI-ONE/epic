// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc_tools.h"

//////////////////// CPP internal methods ///////////////////////////

rpc::Outpoint* ToRPCOutPoint(const TxOutPoint& outpoint) {
    auto rpcoutponit = new rpc::Outpoint();
    rpcoutponit->set_allocated_fromblock(ToRPCHash(outpoint.bHash));
    rpcoutponit->set_txidx(outpoint.txIndex);
    rpcoutponit->set_outidx(outpoint.outIndex);
    return rpcoutponit;
}

std::unique_ptr<rpc::Input> ToRPCInput(const TxInput& input) {
    auto rpcin = std::make_unique<rpc::Input>();
    rpcin->set_allocated_outpoint(ToRPCOutPoint(input.outpoint));
    VStream vs(input.listingContent);
    rpcin->set_listing(vs.str());
    return rpcin;
}

std::unique_ptr<rpc::Output> ToRPCOutput(const TxOutput& output) {
    auto rpcout = std::make_unique<rpc::Output>();
    rpcout->set_money(output.value.GetValue());
    VStream vs(output.listingContent);
    rpcout->set_address(vs.str());
    return rpcout;
}

std::unique_ptr<rpc::Transaction> ToRPCTx(const Transaction& tx) {
    auto rpctx = std::make_unique<rpc::Transaction>();
    for (const auto& in : tx.GetInputs()) {
        auto inptr = rpctx->add_inputs();
        *inptr     = *ToRPCInput(in);
    }

    for (const auto& out : tx.GetOutputs()) {
        auto outptr = rpctx->add_outputs();
        *outptr     = *ToRPCOutput(out);
    }
    return rpctx;
}

TxOutPoint ToOutPoint(const rpc::Outpoint& rop) {
    return TxOutPoint(ToHash(rop.fromblock()), rop.txidx(), rop.outidx());
}

std::vector<TxInput> ToInputs(const ::google::protobuf::RepeatedPtrField<::rpc::Input>& rins) {
    std::vector<TxInput> inputs;
    for (const auto& in : rins) {
        Tasm::Listing l;
        VStream vs(in.listing().data(), in.listing().data() + in.listing().size());
        vs >> l;
        inputs.emplace_back(ToOutPoint(in.outpoint()), l);
    }
    return inputs;
}

std::vector<TxOutput> ToOutputs(const ::google::protobuf::RepeatedPtrField<::rpc::Output>& routs) {
    std::vector<TxOutput> outputs;
    for (const auto& out : routs) {
        Tasm::Listing l;
        VStream vs(out.address().data(), out.address().data() + out.address().size());
        vs >> l;
        outputs.emplace_back(out.money(), l);
    }
    return outputs;
}

std::vector<ConstTxPtr> ToTxns(const ::google::protobuf::RepeatedPtrField<::rpc::Transaction>& rtxns) {
    std::vector<ConstTxPtr> txns;
    for (const auto& t : rtxns) {
        txns.emplace_back(std::make_shared<const Transaction>(ToInputs(t.inputs()), ToOutputs(t.outputs())));
    }

    return txns;
}

////////////////// End of internal methods ////////////////////////

uint256 ToHash(const rpc::Hash& h) {
    uint256 result = uintS<256>(h.hash());
    return result;
}

rpc::Hash* ToRPCHash(const uint256& h) {
    auto rpch = new rpc::Hash();
    rpch->set_hash(std::to_string(h));
    return rpch;
}

rpc::Block* ToRPCBlock(const Block& b) {
    auto rpcb = new rpc::Block();
    rpcb->set_version(b.GetVersion());
    auto milestoneBlockHash = ToRPCHash(b.GetMilestoneHash());
    rpcb->set_allocated_milestoneblockhash(milestoneBlockHash);
    auto prevBlockHash = ToRPCHash(b.GetPrevHash());
    rpcb->set_allocated_prevblockhash(prevBlockHash);
    auto tipBlockHash = ToRPCHash(b.GetTipHash());
    rpcb->set_allocated_tipblockhash(tipBlockHash);
    rpcb->set_difftarget(b.GetDifficultyTarget());
    rpcb->set_nonce(b.GetNonce());
    rpcb->set_time(b.GetTime());
    for (auto e : b.GetProof()) {
        rpcb->add_proof(e);
    }
    // Transaction transactions = 9;
    auto txns = b.GetTransactions();
    if (!txns.empty()) {
        for (const auto& tx : txns) {
            auto txptr = rpcb->add_transactions();
            *txptr     = *ToRPCTx(*tx);
        }
    }

    return rpcb;
}

Block ToBlock(const rpc::Block& rb) {
    std::vector<word_t> proof;
    proof.reserve(rb.proof().Capacity());
    for (auto e : rb.proof()) {
        proof.emplace_back(e);
    }
    Block blk(rb.version(), ToHash(rb.milestoneblockhash()), ToHash(rb.prevblockhash()), ToHash(rb.tipblockhash()),
              uint256(), rb.time(), rb.difftarget(), rb.nonce(), std::move(proof));
    blk.AddTransactions(ToTxns(rb.transactions()));
    blk.SetMerkle();
    blk.FinalizeHash();
    return blk;
}
