#include <rpc_tools.h>

uint256 RPCHashToHash(const rpc::Hash& h) {
    uint256 result = uintS<256>(h.hash());
    return result;
}

rpc::Hash* HashToRPCHash(const uint256& h) {
    rpc::Hash* rpch = new rpc::Hash();
    rpch->set_hash(std::to_string(h));
    return rpch;
}

rpc::Transaction* TxToRPCTx(const Transaction& tx) {
    // message Transaction
    rpc::Transaction* rpctx = new rpc::Transaction();
    //    Hash transaction_hash = 1;
    auto tx_hash = HashToRPCHash(tx.GetHash());
    rpctx->set_allocated_transaction_hash(tx_hash);
    // TODO uint32 fee = 2;
    // TODO repeated Input inputs = 3;
    // TODO repeated Output outputs = 4;
    return rpctx;
}

rpc::Block* BlockToRPCBlock(const Block& b) {
    // message Block
    rpc::Block* rpcb = new rpc::Block();
    // Hash block_hash = 1;
    auto block_hash = HashToRPCHash(b.GetHash());
    rpcb->set_allocated_block_hash(block_hash);
    // uint32 version = 2;
    // TODO rpcb.set_version(b.GetVersion());
    // Hash milestoneBlockHash = 3;
    auto milestoneBlockHash = HashToRPCHash(b.GetMilestoneHash());
    rpcb->set_allocated_milestoneblockhash(milestoneBlockHash);
    // Hash prevBlockHash = 4;
    auto prevBlockHash = HashToRPCHash(b.GetPrevHash());
    rpcb->set_allocated_milestoneblockhash(prevBlockHash);
    // Hash tipBlockHash = 5;
    auto tipBlockHash = HashToRPCHash(b.GetTipHash());
    rpcb->set_allocated_tipblockhash(tipBlockHash);
    // uint32 diffTarget = 6;
    rpcb->set_difftarget(b.GetDifficultyTarget());
    // uint32 nonce = 7;
    rpcb->set_nonce(b.GetNonce());
    // uint64 time = 8;
    rpcb->set_time(b.GetTime());
    // Transaction transactions = 9;
    auto tx = b.GetTransaction();
    if (tx) {
        auto rpctx = TxToRPCTx(*tx);
        rpcb->set_allocated_transactions(rpctx);
    }
    return rpcb;
}
