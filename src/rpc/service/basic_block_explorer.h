// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_BASIC_BLOCK_EXPLORER_H
#define EPIC_BASIC_BLOCK_EXPLORER_H

#include "dag_manager.h"
#include "rpc_tools.h"
#include "rpc_service.h"

class BasicBlockExplorerRPCServiceImpl final : public BasicBlockExplorerRPC::Service {
public:
    grpc::Status GetBlock(grpc::ServerContext* context,
                          const GetBlockRequest* request,
                          GetBlockResponse* reply) override {
        auto vertex = DAG->GetMainChainVertex(ToHash(request->hash()));
        if (!vertex) {
            return grpc::Status::OK;
        }
        rpc::Block* b = ToRPCBlock(*(vertex->cblock));
        reply->set_allocated_block(b);
        return grpc::Status::OK;
    }

    grpc::Status GetLevelSet(grpc::ServerContext* context,
                             const GetLevelSetRequest* request,
                             GetLevelSetResponse* reply) override {
        auto ls = DAG->GetMainChainLevelSet(ToHash(request->hash()));
        if (ls.empty()) {
            return grpc::Status::OK;
        }
        for (const auto& localBlock : ls) {
            auto newBlock   = reply->add_blocks();
            auto blockValue = ToRPCBlock(*localBlock);
            *newBlock       = *blockValue;
            delete blockValue;
        }
        return grpc::Status::OK;
    }

    grpc::Status GetLevelSetSize(grpc::ServerContext* context,
                                 const GetLevelSetSizeRequest* request,
                                 GetLevelSetSizeResponse* reply) override {
        auto ls = DAG->GetMainChainLevelSet(ToHash(request->hash()));
        reply->set_size(ls.size());
        return grpc::Status::OK;
    }

    grpc::Status GetNewMilestoneSince(grpc::ServerContext* context,
                                      const GetNewMilestoneSinceRequest* request,
                                      GetNewMilestoneSinceResponse* reply) override {
        auto vertex = DAG->GetState(ToHash(request->hash()));
        if (!vertex) {
            return grpc::Status::OK;
        }
        auto milestone_hashes = DAG->TraverseMilestoneForward(vertex, request->number());
        for (size_t i = 0; i < milestone_hashes.size(); ++i) {
            auto vtx        = DAG->GetState(milestone_hashes[i]);
            auto newBlock   = reply->add_blocks();
            auto blockValue = ToRPCBlock(*(vtx->cblock));
            *newBlock       = *blockValue;
            delete blockValue;
        }
        return grpc::Status::OK;
    }

    grpc::Status GetLatestMilestone(grpc::ServerContext* context,
                                    const GetLatestMilestoneRequest* request,
                                    GetLatestMilestoneResponse* reply) override {
        auto vertex   = DAG->GetMilestoneHead();
        rpc::Block* b = ToRPCBlock(*(vertex->cblock));
        reply->set_allocated_milestone(b);
        return grpc::Status::OK;
    }
};

#endif // EPIC_BASIC_BLOCK_EXPLORER_H
