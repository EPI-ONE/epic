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
                          const rpc::Hash* request,
                          rpc::Block* reply) override;

    grpc::Status GetLatestMilestone(grpc::ServerContext* context,
                                    const rpc::EmptyMessage* request,
                                    rpc::Milestone* reply) override;

    grpc::Status GetLevelSet(grpc::ServerContext* context,
                             const rpc::HashOrHeight* request,
                             rpc::BlockList* reply) override;

    grpc::Status GetLevelSetSize(grpc::ServerContext* context,
                                 const rpc::HashOrHeight* request,
                                 rpc::UintMessage* reply) override;

    grpc::Status GetVertex(grpc::ServerContext* context,
                           const rpc::Hash* request,
                           rpc::Vertex* response) override;

    grpc::Status GetMilestone(grpc::ServerContext* context,
                              const rpc::HashOrHeight* request,
                              rpc::Milestone* response) override;

    grpc::Status GetForks(grpc::ServerContext* context,
                          const rpc::EmptyMessage* request,
                          rpc::MsChainList* response) override;

    grpc::Status GetPeerChains(grpc::ServerContext* context,
                               const rpc::EmptyMessage* request,
                               rpc::ChainList* response) override;

    grpc::Status GetRecentStat(grpc::ServerContext* context,
                               const rpc::EmptyMessage* request,
                               rpc::GetRecentStatResponse* response) override;

    grpc::Status Statistic(grpc::ServerContext* context,
                           const rpc::EmptyMessage* request,
                           rpc::StatisticResponse* response) override;

    grpc::Status GetMilestonesFromHead(grpc::ServerContext* context,
                                       const rpc::MsLocator* request,
                                       rpc::MilestoneList* response) override;

    ~BasicBlockExplorerRPCServiceImpl() = default;
};

#endif // EPIC_BASIC_BLOCK_EXPLORER_H
