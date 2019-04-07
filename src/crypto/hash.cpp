#include "crypto/hash.h"

uint256 Hash256(const unsigned char bytes[], size_t size) {
    return Hash(bytes, bytes + size);
}

uint160 Hash160(const unsigned char bytes[], size_t size) {
    uint256 h = Hash(bytes, bytes + size);
    std::vector<unsigned char> v(h.begin(), h.begin() + 20);
    return uint160(v);
}

