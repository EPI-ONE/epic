#ifndef __SRC_CRYPTO_HASH__
#define __SRC_CRYPTO_HASH__

#include "crypto/sha256.h"
#include "stream.h"
#include "uint256.h"
#include <vector>

/* R: number of hashing rounds e.g 1 = single hash; 2 = double hash */
template <std::size_t R>
uint256 Hash(VStream& data) {
    uint256 hash;
    CSHA256 sha;

    static const unsigned char emptyByte[0] = {};
    sha.Write(data.empty() ? emptyByte : (const unsigned char*) data.data(), data.size());
    sha.Finalize((unsigned char*) &hash);

#pragma clang loop unroll_count(R)
    for (size_t i = 1; i < R; i++) {
        sha.Reset();
        sha.Write((unsigned char*) &hash, 32);
        sha.Finalize((unsigned char*) &hash);
    }

    return hash;
}
#endif
