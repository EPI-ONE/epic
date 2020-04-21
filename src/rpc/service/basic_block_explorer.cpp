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
                                                        const rpc::Hash* request,
                                                        rpc::Block* reply) {
    auto vertex = DAG->GetMainChainVertex(uintS<256>(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }

    ToRPCBlock(*(vertex->cblock), reply);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSet(grpc::ServerContext* context,
                                                           const rpc::HashOrHeight* request,
                                                           rpc::BlockList* reply) {
    std::vector<ConstBlockPtr> result;
    if (request->key_case() == request->kHash) {
        result = DAG->GetMainChainLevelSet(uintS<256>(request->hash()));
    } else if (request->key_case() == request->kHeight) {
        result = DAG->GetMainChainLevelSet(request->height());
    }

    if (result.empty()) {
        return grpc::Status::OK;
    }

    auto blocks = reply->mutable_blocks();
    blocks->Reserve(result.size());
    for (const auto& localBlock : result) {
        blocks->AddAllocated(ToRPCBlock(*localBlock, nullptr));
    }
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetLevelSetSize(grpc::ServerContext* context,
                                                               const rpc::HashOrHeight* request,
                                                               rpc::UintMessage* reply) {
    std::vector<ConstBlockPtr> result;
    if (request->key_case() == request->kHash) {
        result = DAG->GetMainChainLevelSet(uintS<256>(request->hash()));
    } else if (request->key_case() == request->kHeight) {
        result = DAG->GetMainChainLevelSet(request->height());
    }

    if (result.empty()) {
        return grpc::Status::OK;
    }
    reply->set_value(result.size());
    return grpc::Status::OK;
}


grpc::Status BasicBlockExplorerRPCServiceImpl::GetLatestMilestone(grpc::ServerContext* context,
                                                                  const EmptyMessage* request,
                                                                  rpc::Milestone* reply) {
    auto vertex = DAG->GetMilestoneHead();
    ToRPCMilestone(*vertex, reply);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetMilestone(grpc::ServerContext* context,
                                                            const rpc::HashOrHeight* request,
                                                            rpc::Milestone* response) {
    const auto chain  = DAG->GetBestChain();
    const auto msHash = uintS<256>(request->hash());
    if (!chain->IsMilestone(msHash)) {
        return grpc::Status::OK;
    }

    ToRPCMilestone(*chain->GetVertex(msHash), response);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetMilestonesFromHead(grpc::ServerContext* context,
                                                                     const rpc::MsLocator* request,
                                                                     rpc::MilestoneList* response) {
    auto head_height = DAG->GetBestMilestoneHeight();
    if (request->offset_from_head() + request->size() > head_height) {
        return grpc::Status::OK;
    }


    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetVertex(grpc::ServerContext* context,
                                                         const rpc::Hash* request,
                                                         rpc::Vertex* response) {
    auto vertex = DAG->GetMsVertex(uintS<256>(request->hash()));
    if (!vertex) {
        return grpc::Status::OK;
    }

    ToRPCVertex(*vertex, response);
    return grpc::Status::OK;
}

grpc::Status BasicBlockExplorerRPCServiceImpl::GetForks(grpc::ServerContext* context,
                                                        const EmptyMessage* request,
                                                        rpc::MsChainList* response) {
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
                                                             rpc::ChainList* response) {
    const auto bestChain = DAG->GetBestChain();
    const auto& heads    = bestChain->GetPeerChainHead();
    auto rpc_heads       = response->mutable_chains();
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