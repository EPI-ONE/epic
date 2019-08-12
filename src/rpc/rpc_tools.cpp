#include <rpc_tools.h>

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
    rpctx->set_allocated_transaction_hash(ToRPCHash(tx.GetHash()));
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

////////////////// End of internal methods ////////////////////////

uint256 ToHash(const rpc::Hash& h) {
    uint256 result = uint256S(h.hash());
    return result;
}

rpc::Hash* ToRPCHash(const uint256& h) {
    auto rpch = new rpc::Hash();
    rpch->set_hash(std::to_string(h));
    return rpch;
}

rpc::Block* ToRPCBlock(const Block& b) {
    // message Block
    auto rpcb = new rpc::Block();
    // Hash block_hash = 1;
    auto block_hash = ToRPCHash(b.GetHash());
    rpcb->set_allocated_block_hash(block_hash);
    // uint32 version = 2;
    // TODO rpcb.set_version(b.GetVersion());
    // Hash milestoneBlockHash = 3;
    auto milestoneBlockHash = ToRPCHash(b.GetMilestoneHash());
    rpcb->set_allocated_milestoneblockhash(milestoneBlockHash);
    // Hash prevBlockHash = 4;
    auto prevBlockHash = ToRPCHash(b.GetPrevHash());
    rpcb->set_allocated_milestoneblockhash(prevBlockHash);
    // Hash tipBlockHash = 5;
    auto tipBlockHash = ToRPCHash(b.GetTipHash());
    rpcb->set_allocated_tipblockhash(tipBlockHash);
    auto merkleRoot = ToRPCHash(b.GetMerkleRoot());
    rpcb->set_allocated_merkleroot(merkleRoot);
    // uint32 diffTarget = 6;
    rpcb->set_difftarget(b.GetDifficultyTarget());
    // uint32 nonce = 7;
    rpcb->set_nonce(b.GetNonce());
    // uint64 time = 8;
    rpcb->set_time(b.GetTime());
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
