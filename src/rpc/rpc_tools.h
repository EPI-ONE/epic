// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_RPC_TOOLS_H
#define EPIC_RPC_TOOLS_H

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>
#include <string>

class Transaction;
class Block;
class Vertex;

// functions below will pass the ownvership of the allocated object to rpc to handle its lifetime
rpc::Transaction* ToRPCTx(const Transaction& tx); 
rpc::Block* ToRPCBlock(const Block&);
rpc::Vertex* ToRPCVertex(const Vertex&);
rpc::Chain* ToRPCChain(const Vertex&);
rpc::Milestone* ToRPCMilestone(const Vertex&);
rpc::MsChain* ToRPCMsChain(const Vertex&);

#endif // EPIC_RPC_TOOLS_H
