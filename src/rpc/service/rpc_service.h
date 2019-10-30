// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_RPC_SERVICE_H
#define EPIC_RPC_SERVICE_H

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>
#include <rpc/proto-gen/rpc.grpc.pb.h>

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

using rpc::ChangePassphraseRequest;
using rpc::ChangePassphraseResponse;
using rpc::CommanderRPC;
using rpc::ConnectRequest;
using rpc::ConnectResponse;
using rpc::CreateFirstRegRequest;
using rpc::CreateFirstRegResponse;
using rpc::CreateRandomTxRequest;
using rpc::CreateRandomTxResponse;
using rpc::CreateTxRequest;
using rpc::CreateTxResponse;
using rpc::DisConnectAllRequest;
using rpc::DisConnectAllResponse;
using rpc::DisConnectPeerRequest;
using rpc::DisConnectPeerResponse;
using rpc::GenerateNewKeyRequest;
using rpc::GenerateNewKeyResponse;
using rpc::GetBalanceRequest;
using rpc::GetBalanceResponse;
using rpc::LoginRequest;
using rpc::LoginResponse;
using rpc::Output;
using rpc::SetPassphraseRequest;
using rpc::SetPassphraseResponse;
using rpc::StartMinerRequest;
using rpc::StartMinerResponse;
using rpc::StatusRequest;
using rpc::StatusResponse;
using rpc::StopMinerRequest;
using rpc::StopMinerResponse;
using rpc::StopRequest;
using rpc::StopResponse;

using rpc::POWResult;
using rpc::POWTask;
using rpc::RemoteSolver;
using rpc::StopTaskRequest;
using rpc::StopTaskResponse;
#endif // EPIC_RPC_SERVICE_H
