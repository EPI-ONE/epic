// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "basic_block_explorer.h"
#include "block_store.h"
#include "dag_manager.h"
#include "mempool.h"
#include "rpc_tools.h"

#include <numeric>

using namespace rpc;

grpc::Status BasicBlockExplorerRPCServiceImpl::GetBlock(grpc::ServerContext* context,
                                                        const GetBlockRequest* request,
                                                        GetBlockResponse* reply) {
    auto vertex = DAG->GetMainChainVertex(uintS<256>(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }
    reply->set_allocated_block(ToRPCBlock(*(vertex->cblock)));
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSet(grpc::ServerContext* context,
                                                           const GetLevelSetRequest* request,
                                                           GetLevelSetResponse* reply) {
    auto ls = DAG->GetMainChainLevelSet(uintS<256>(request->hash()));
    if (ls.empty()) {
        return grpc::Status::OK;
    }

    auto blocks = reply->mutable_blocks();
    blocks->Reserve(ls.size());
    for (const auto& localBlock : ls) {
        blocks->AddAllocated(ToRPCBlock(*localBlock));
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSetSize(grpc::ServerContext* context,
                                                               const GetLevelSetSizeRequest* request,
                                                               GetLevelSetSizeResponse* reply) {
    auto ls = DAG->GetMainChainLevelSet(uintS<256>(request->hash()));
    reply->set_size(ls.size());
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetNewMilestoneSince(grpc::ServerContext* context,
                                                                    const GetNewMilestoneSinceRequest* request,
                                                                    GetNewMilestoneSinceResponse* reply) {
    auto vertex = DAG->GetMsVertex(uintS<256>(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }

    auto ms_hashes = DAG->TraverseMilestoneForward(vertex, request->number());
    auto blocks    = reply->mutable_blocks();
    blocks->Reserve(ms_hashes.size());

    for (const auto& ms_hash : ms_hashes) {
        auto vtx = DAG->GetMsVertex(ms_hash);
        blocks->AddAllocated(ToRPCBlock(*(vtx->cblock)));
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLatestMilestone(grpc::ServerContext* context,
                                                                  const EmptyMessage* request,
                                                                  GetLatestMilestoneResponse* reply) {
    auto vertex   = DAG->GetMilestoneHead();
    rpc::Block* b = ToRPCBlock(*(vertex->cblock));
    reply->set_allocated_milestone(b);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetMilestone(grpc::ServerContext* context,
                                                            const GetBlockRequest* request,
                                                            GetMilestoneResponse* response) {
    const auto chain  = DAG->GetBestChain();
    const auto msHash = uintS<256>(request->hash());
    if (!chain->IsMilestone(msHash)) {
        return grpc::Status::OK;
    }

    response->set_allocated_milestone(ToRPCMilestone(*chain->GetVertex(msHash)));

    return grpc::Status::OK;
}


grpc::Status BasicBlockExplorerRPCServiceImpl::GetVertex(grpc::ServerContext* context,
                                                         const GetVertexRequest* request,
                                                         GetVertexResponse* response) {
    auto vertex = DAG->GetMsVertex(uintS<256>(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }

    response->set_allocated_vertex(ToRPCVertex(*vertex));
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetForks(grpc::ServerContext* context,
                                                        const EmptyMessage* request,
                                                        GetForksResponse* response) {
    const auto& chains = DAG->GetChains();
    auto rpc_chains    = response->mutable_chains();
    rpc_chains->Reserve(chains.size());

    for (const auto& chain : chains) {
        rpc_chains->AddAllocated(ToRPCMsChain(*(chain->GetChainHead()->GetMilestone())));
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetPeerChains(grpc::ServerContext* context,
                                                             const EmptyMessage* request,
                                                             GetPeerChainsResponse* response) {
    const auto bestChain = DAG->GetBestChain();
    const auto& heads    = bestChain->GetPeerChainHead();
    auto rpc_heads       = response->mutable_peerchains();
    rpc_heads->Reserve(heads.size());

    for (const auto& head : heads) {
        rpc_heads->AddAllocated(ToRPCChain(*(bestChain->GetVertex(head))));
    }

    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetRecentStat(grpc::ServerContext* context,
                                                             const EmptyMessage* request,
                                                             GetRecentStatResponse* response) {
    const auto chain = DAG->GetBestChain()->GetMilestones();

    response->set_timefrom(chain.front()->GetLevelSet().front().lock()->cblock->GetTime());
    response->set_timeto(chain.back()->GetMilestone()->cblock->GetTime());

    const auto [totalblk, totaltx] = std::accumulate(
        chain.begin(), chain.end(), std::pair{0, 0},
        [](std::pair<uint32_t, uint32_t> sum, auto chain_elem) -> std::pair<uint32_t, uint32_t> {
            return {sum.first + chain_elem->GetLevelSet().size(), sum.second + chain_elem->GetNumOfValidTxns()};
        });
    response->set_nblks(totalblk);
    response->set_ntxs(totaltx);

    auto tps = totaltx / static_cast<double>(response->timeto() - response->timefrom());
    response->set_tps(tps);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::Statistic(grpc::ServerContext* context,
                                                         const EmptyMessage* request,
                                                         StatisticResponse* response) {
    auto bestchain = DAG->GetBestChain();
    if (STORE->GetHeadHeight() > 0) {
        response->set_height(bestchain->GetChainHead()->height);
        const auto stat = DAG->GetStatData();
        response->set_nblks(stat.nBlkCnt);
        response->set_ntxs(stat.nTxCnt);

        const auto tEnd = STORE->GetLevelSetBlksAt(bestchain->GetMilestones().front()->height - 1).front()->GetTime();
        auto tps        = response->ntxs() / static_cast<double>(tEnd - stat.tStart);
        response->set_tps(tps);

        if (MEMPOOL) {
            response->set_mempool(MEMPOOL->Size());
        }
    }
    return grpc::Status::OK;
}
