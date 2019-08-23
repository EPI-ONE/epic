#ifndef __SRC_MERKLE_H__
#define __SRC_MERKLE_H__

#include "big_uint.h"

extern uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated = nullptr);

#endif // __SRC_MERKLE_H__
