// Copyright (c) 2015-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "merkle.h"
#include "hash.h"

uint256 ComputeMerkleRoot(std::vector<uint256> hashes, bool* mutated) {
    bool mutation = false;
    while (hashes.size() > 1) {
        if (mutated) {
            for (size_t pos = 0; pos + 1 < hashes.size(); pos += 2) {
                if (hashes[pos] == hashes[pos + 1])
                    mutation = true;
            }
        }

        if (hashes.size() & 1) {
            hashes.push_back(hashes.back());
        }

        SHA256D64(hashes[0].begin(), hashes[0].begin(), hashes.size() / 2);
        hashes.resize(hashes.size() / 2);
    }

    if (mutated) {
        *mutated = mutation;
    }

    if (hashes.empty()) {
        return uint256();
    }

    return hashes[0];
}
