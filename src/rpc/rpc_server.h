// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_EPIC_RPC_SERVER_H
#define EPIC_EPIC_RPC_SERVER_H

#include "basic_rpc_server.h"
#include "net_address.h"

#include <unordered_set>

enum RPCServiceType { BLOCK_EXPLORER_SERVER, COMMAND_LINE_SERVER, MINER_SOLVER };

class RPCServer {
public:
    RPCServer(const NetAddress& address, const std::vector<RPCServiceType>& services);

    void Start();

    void Shutdown();

    bool IsRunning();

    ~RPCServer();

private:
    BasicRPCServer server;
    std::vector<std::unique_ptr<grpc::Service>> service_impls;
};

extern std::unique_ptr<RPCServer> RPC;

#endif // EPIC_EPIC_RPC_SERVER_H
