// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_BASIC_RPC_SERVER_H
#define EPIC_BASIC_RPC_SERVER_H

#include <grpc++/grpc++.h>

class BasicRPCServer {
public:
    BasicRPCServer() = delete;

    explicit BasicRPCServer(const std::string& addr);

    void Start(std::vector<grpc::Service*> services);

    void Shutdown();

    bool IsRunning();


private:
    std::string server_address_;
    std::atomic<bool> isRunning_;

    void LaunchServer(const std::vector<grpc::Service*>& services);
};


#endif // EPIC_BASIC_RPC_SERVER_H
