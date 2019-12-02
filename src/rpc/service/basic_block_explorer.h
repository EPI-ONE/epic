// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_BASIC_BLOCK_EXPLORER_H
#define EPIC_BASIC_BLOCK_EXPLORER_H

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

class BasicBlockExplorerRPCServiceImpl final : public rpc::BasicBlockExplorerRPC::Service {
public:
    grpc::Status GetBlock(grpc::ServerContext* context,
                          const rpc::GetBlockRequest* request,
                          rpc::GetBlockResponse* reply) override;

    grpc::Status GetNewMilestoneSince(grpc::ServerContext* context,
                                      const rpc::GetNewMilestoneSinceRequest* request,
                                      rpc::GetNewMilestoneSinceResponse* reply) override;
    
    grpc::Status GetLatestMilestone(grpc::ServerContext* context,
                                    const rpc::EmptyRequest* request,
                                    rpc::GetLatestMilestoneResponse* reply) override;

    grpc::Status GetLevelSet(grpc::ServerContext* context,
                             const rpc::GetLevelSetRequest* request,
                             rpc::GetLevelSetResponse* reply) override;

    grpc::Status GetLevelSetSize(grpc::ServerContext* context,
                                 const rpc::GetLevelSetSizeRequest* request,
                                 rpc::GetLevelSetSizeResponse* reply) override;

    grpc::Status GetVertex(grpc::ServerContext* context,
                           const rpc::GetVertexRequest* request,
                           rpc::GetVertexResponse* response) override;

    grpc::Status GetMilestone(grpc::ServerContext* context,
                              const rpc::GetBlockRequest* request,
                              rpc::GetMilestoneResponse* response) override;

    grpc::Status GetForks(grpc::ServerContext* context,
                          const rpc::EmptyRequest* request,
                          rpc::GetForksResponse* response) override;

    grpc::Status GetPeerChains(grpc::ServerContext* context,
                               const rpc::EmptyRequest* request,
                               rpc::GetPeerChainsResponse* response) override;

    grpc::Status GetRecentStat(grpc::ServerContext* context,
                               const rpc::EmptyRequest* request,
                               rpc::GetRecentStatResponse* response) override;

    grpc::Status Statistic(grpc::ServerContext* context,
                           const rpc::EmptyRequest* request,
                           rpc::StatisticResponse* response) override;

    ~BasicBlockExplorerRPCServiceImpl() = default;
};

#endif // EPIC_BASIC_BLOCK_EXPLORER_H
