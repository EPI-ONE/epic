#ifndef EPIC_RPC_HEADER_H
#define EPIC_RPC_HEADER_H
#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

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

using rpc::CommanderRPC;
using rpc::CreateRandomTxRequest;
using rpc::CreateRandomTxResponse;
using rpc::CreateTxRequest;
using rpc::CreateTxResponse;
using rpc::GenerateNewKeyRequest;
using rpc::GenerateNewKeyResponse;
using rpc::GetBalanceRequest;
using rpc::GetBalanceResponse;
using rpc::Output;
using rpc::StartMinerRequest;
using rpc::StartMinerResponse;
using rpc::StatusRequest;
using rpc::StatusResponse;
using rpc::StopMinerRequest;
using rpc::StopMinerResponse;
using rpc::StopRequest;
using rpc::StopResponse;
#endif // EPIC_RPC_HEADER_H
