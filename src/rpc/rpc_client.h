#ifndef __SRC_RPCCLIENT_H__
#define __SRC_RPCCLIENT_H__

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>
#include <rpc_tools.h>

#include "block.h"
#include "net_address.h"

using rpc::BasicBlockExplorerRPC;
using rpc::GetBlockRequest;
using rpc::GetBlockResponse;
using rpc::GetLatestMilestoneRequest;
using rpc::GetLatestMilestoneResponse;
using rpc::GetLevelSetRequest;
using rpc::GetLevelSetResponse;
using rpc::GetLevelSetSizeRequest;
using rpc::GetLevelSetSizeResponse;
using rpc::GetNewMilestoneSinceRequest;
using rpc::GetNewMilestoneSinceResponse;

class RPCClient {
public:
    RPCClient(std::shared_ptr<grpc::Channel> channel);

    std::optional<rpc::Block> GetBlock(const uint256&);

    std::optional<std::vector<rpc::Block>> GetLevelSet(const uint256&);

    std::optional<size_t> GetLevelSetSize(const uint256&);

    std::optional<rpc::Block> GetLatestMilestone();

    std::optional<std::vector<rpc::Block>> GetNewMilestoneSince(const uint256&, size_t);


private:
    std::unique_ptr<BasicBlockExplorerRPC::Stub> stub_;
};

#endif //__SRC_RPCCLIENT_H__
