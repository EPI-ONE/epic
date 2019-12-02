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
using rpc::EmptyRequest;
using rpc::GetBlockRequest;
using rpc::GetBlockResponse;
using rpc::GetLatestMilestoneResponse;
using rpc::GetLevelSetRequest;
using rpc::GetLevelSetResponse;
using rpc::GetLevelSetSizeRequest;
using rpc::GetLevelSetSizeResponse;
using rpc::GetNewMilestoneSinceRequest;
using rpc::GetNewMilestoneSinceResponse;
using rpc::GetVertexRequest;
using rpc::GetVertexResponse;

using rpc::GetMilestoneResponse;
using rpc::GetForksResponse;
using rpc::GetPeerChainsResponse;
using rpc::GetRecentStatResponse;
using rpc::StatisticResponse;

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
using rpc::DisconnectAllResponse;
using rpc::DisconnectPeerRequest;
using rpc::DisconnectPeerResponse;
using rpc::GenerateNewKeyResponse;
using rpc::GetBalanceResponse;
using rpc::LoginRequest;
using rpc::LoginResponse;
using rpc::Output;
using rpc::SetPassphraseRequest;
using rpc::SetPassphraseResponse;
using rpc::ShowPeerRequest;
using rpc::ShowPeerResponse;
using rpc::StartMinerResponse;
using rpc::StatusResponse;
using rpc::StopMinerResponse;
using rpc::StopResponse;
using rpc::SyncStatusResponse;

using rpc::POWResult;
using rpc::POWTask;
using rpc::RemoteSolver;
using rpc::StopTaskRequest;
using rpc::StopTaskResponse;

#endif // EPIC_RPC_SERVICE_H
