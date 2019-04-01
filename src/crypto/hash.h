#ifndef __SRC_CRYPTO_HASH__
#define __SRC_CRYPTO_HASH__

#include "sha256.h"
#include "stream.h"
#include "uint256.h"

#include <vector>

/* Compute the 256-bit hash of an object. */
template<std::size_t R, typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    uint256 hash;
    CSHA256 sha;

    static const unsigned char emptyByte[0] = {};
    sha.Write((pbegin == pend) ? emptyByte : (const unsigned char*) &pbegin[0],
        (pend - pbegin) * sizeof(pbegin[0]));
    sha.Finalize((unsigned char*) &hash);

#pragma clang loop unroll_count(R)
    for (size_t i = 1; i < R; i++) {
        sha.Reset();
        sha.Write((unsigned char*) &hash, 32);
        sha.Finalize((unsigned char*) &hash);
    }

    return hash;
}

/* R: number of hashing rounds e.g 1 = single hash; 2 = double hash */
template <std::size_t R>
uint256 Hash(VStream& data) {
    return Hash<R>(data.cbegin(), data.cend());
}

/** Compute the 160-bit hash an object. */
template <typename T1>
inline uint160 Hash160(const T1 pbegin, const T1 pend) {
    return Hash<2>(pbegin, pend).GetUint160();
}

/** Compute the 160-bit hash of a vector. */
inline uint160 Hash160(VStream& vch) {
    return Hash160(vch.cbegin(), vch.cend());
}

#endif
