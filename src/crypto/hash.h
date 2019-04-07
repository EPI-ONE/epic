// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef __SRC_CRYPTO_HASH__
#define __SRC_CRYPTO_HASH__

#include "crypto/sha_hash_verifier.h"
#include <vector>

// Compute the hash of a byte array
uint256 Hash256(const unsigned char bytes[], size_t size);

// compute the hash of a byte array and return the first 20 bytes (160 bits) of the hash
uint160 Hash160(const unsigned char bytes[], size_t size);

// Compute the 256-bit hash of an object
template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend) {
    static const unsigned char pblank[1] = {};
    uint256 result;

    SHAHasher256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

// Compute the 256-bit hash of the concatenation of two objects
template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end) {
    static const unsigned char pblank[1] = {};
    uint256 result;

    SHAHasher256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
              .Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
              .Finalize((unsigned char*)&result);
    return result;
}

static const unsigned char *EMPTY_BYTE = new unsigned char[32]();
static const uint256 ZERO_HASH = Hash(EMPTY_BYTE, EMPTY_BYTE + 32);

#endif
