#ifndef __SRC_RPC_TOOLS_H__
#define __SRC_RPC_TOOLS_H__

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

#include "block.h"

uint256 ToHash(const rpc::Hash&);

rpc::Hash* ToRPCHash(const uint256&);

rpc::Block* ToRPCBlock(const Block&);

Block ToBlock(const rpc::Block&);

#endif //__SRC_RPC_TOOLS_H__
