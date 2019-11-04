// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_BASIC_BLOCK_EXPLORER_H
#define EPIC_BASIC_BLOCK_EXPLORER_H

#include "rpc_service.h"

class BasicBlockExplorerRPCServiceImpl final : public BasicBlockExplorerRPC::Service {
public:
    grpc::Status GetBlock(grpc::ServerContext* context,
                          const GetBlockRequest* request,
                          GetBlockResponse* reply) override;

    grpc::Status GetNewMilestoneSince(grpc::ServerContext* context,
                                      const GetNewMilestoneSinceRequest* request,
                                      GetNewMilestoneSinceResponse* reply) override;
    
    grpc::Status GetLatestMilestone(grpc::ServerContext* context,
                                    const EmptyRequest* request,
                                    GetLatestMilestoneResponse* reply) override;

    grpc::Status GetLevelSet(grpc::ServerContext* context,
                             const GetLevelSetRequest* request,
                             GetLevelSetResponse* reply) override;

    grpc::Status GetLevelSetSize(grpc::ServerContext* context,
                                 const GetLevelSetSizeRequest* request,
                                 GetLevelSetSizeResponse* reply) override;

    grpc::Status GetVertex(grpc::ServerContext* context,
                           const GetVertexRequest* request,
                           GetVertexResponse* response) override;

    grpc::Status GetMilestone(grpc::ServerContext* context,
                              const GetBlockRequest* request,
                              GetMilestoneResponse* response) override;

    grpc::Status GetForks(grpc::ServerContext* context,
                          const EmptyRequest* request,
                          GetForksResponse* response) override;

    grpc::Status GetPeerChains(grpc::ServerContext* context,
                               const EmptyRequest* request,
                               GetPeerChainsResponse* response) override;

    grpc::Status GetRecentStat(grpc::ServerContext* context,
                               const EmptyRequest* request,
                               GetRecentStatResponse* response) override;

    grpc::Status Statistic(grpc::ServerContext* context,
                           const EmptyRequest* request,
                           StatisticResponse* response) override;

    ~BasicBlockExplorerRPCServiceImpl() = default;
};

#endif // EPIC_BASIC_BLOCK_EXPLORER_H
