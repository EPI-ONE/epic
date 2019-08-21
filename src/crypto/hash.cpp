#include "hash.h"

const uint256& Hash::GetZeroHash() {
    static uint256 ZERO_HASH = HashSHA2<1>(VStream());
    return ZERO_HASH;
}

const uint256& Hash::GetDoubleZeroHash() {
    static uint256 ZERO_HASH_DOUBLE = HashSHA2<2>(VStream());
    return ZERO_HASH_DOUBLE;
}
