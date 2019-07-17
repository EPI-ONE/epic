#ifndef __SRC_RPC_TOOLS_H__
#define __SRC_RPC_TOOLS_H__

#include <grpc++/grpc++.h>
#include <rpc.grpc.pb.h>
#include <rpc.pb.h>

#include "block.h"
#include "net_address.h"

uint256 RPCHashToHash(const rpc::Hash&);

rpc::Hash* HashToRPCHash(const uint256&);

rpc::Block* BlockToRPCBlock(const Block&);

#endif //__SRC_RPC_TOOLS_H__
