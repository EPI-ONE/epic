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

    rpc::Block GetBlock(const uint256&);

    int GetLevelSet();

    int GetLevelSetSize();

    int GetNewMilestoneSince();

    int GetLatestMilestone();

private:
    std::unique_ptr<BasicBlockExplorerRPC::Stub> stub_;
};

#endif //__SRC_RPCCLIENT_H__
