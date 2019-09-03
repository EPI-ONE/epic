// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __SRC_RPC_TOOLS_H__
#define __SRC_RPC_TOOLS_H__

#include "block.h"

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

uint256 ToHash(const rpc::Hash&);

rpc::Hash* ToRPCHash(const uint256&);

rpc::Block* ToRPCBlock(const Block&);

Block ToBlock(const rpc::Block&);

#endif //__SRC_RPC_TOOLS_H__
