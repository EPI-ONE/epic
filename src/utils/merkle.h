// Copyright (c) 2015-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SRC_MERKLE_H
#define SRC_MERKLE_H

#include "big_uint.h"

uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated = nullptr);

#endif // SRC_MERKLE_H
