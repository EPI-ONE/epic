// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef __SRC_CRYPTO_HASH__
#define __SRC_CRYPTO_HASH__

#include "big_uint.h"
#include "blake2b.h"
#include "sha256.h"
#include "stream.h"

/* Compute the 256-bit hash of an object.
 * R: number of hashing rounds:
 * 1 = single hash
 * 2 = double hash */
template <std::size_t R>
inline uint256 HashSHA2(const void* pin, size_t inlen) {
    static const unsigned char emptyByte[0] = {};
    uint256 result;
    CSHA256 sha;

    sha.Write(inlen ? (const unsigned char*) pin : emptyByte, inlen);
    sha.Finalize((unsigned char*) &result);

#pragma clang loop unroll_count(R)
    for (size_t i = 1; i < R; i++) {
        sha.Reset();
        sha.Write((unsigned char*) &result, 32);
        sha.Finalize((unsigned char*) &result);
    }

    return result;
}

template <std::size_t R>
uint256 HashSHA2(const VStream& data) {
    return HashSHA2<R>(data.data(), data.size());
}

/* Compute the 160-bit hash an object
 * NOTE: R means the same as with uint256 Hash */
template <std::size_t R>
inline uint160 Hash160(const void* pin, size_t inlen) {
    return HashSHA2<R>(pin, inlen).GetUint160();
}

template <std::size_t R>
inline uint160 Hash160(const VStream& vch) {
    return Hash160<R>(vch.data(), vch.size());
}

namespace Hash {
const uint256& GetZeroHash();
const uint256& GetDoubleZeroHash();
static constexpr uint32_t SIZE = 32;
} // namespace Hash

template <unsigned int OUTPUT_SIZE>
inline base_blob<OUTPUT_SIZE> HashBLAKE2(const void* pin, size_t inlen) {
    base_blob<OUTPUT_SIZE> result;
    HashBLAKE2((char*) pin, inlen, result.begin(), OUTPUT_SIZE / 8);
    return result;
}

template <unsigned int OUTPUT_SIZE>
inline base_blob<OUTPUT_SIZE> HashBLAKE2(const VStream& data) {
    return HashBLAKE2<OUTPUT_SIZE>(data.data(), data.size());
}

#endif
