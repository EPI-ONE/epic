// Copyright (c) 2014-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_COMMON_H
#define BITCOIN_CRYPTO_COMMON_H

#include "portable_endian.h"

#include <cstdint>
#include <cstring>

uint16_t extern inline ReadLE16(const unsigned char* ptr) {
    uint16_t x;
    memcpy((char*) &x, ptr, 2);
    return le16toh(x);
}

uint32_t extern inline ReadLE32(const unsigned char* ptr) {
    uint32_t x;
    memcpy((char*) &x, ptr, 4);
    return le32toh(x);
}

uint64_t extern inline ReadLE64(const unsigned char* ptr) {
    uint64_t x;
    memcpy((char*) &x, ptr, 8);
    return le64toh(x);
}

void extern inline WriteLE16(void* ptr, uint16_t x) {
    uint16_t v = htole16(x);
    memcpy(ptr, (char*) &v, 2);
}

void extern inline WriteLE32(void* ptr, uint32_t x) {
    uint32_t v = htole32(x);
    memcpy(ptr, (char*) &v, 4);
}

void extern inline WriteLE64(void* ptr, uint64_t x) {
    uint64_t v = htole64(x);
    memcpy(ptr, (char*) &v, 8);
}

uint32_t extern inline ReadBE32(const unsigned char* ptr) {
    uint32_t x;
    memcpy((char*) &x, ptr, 4);
    return be32toh(x);
}

uint64_t extern inline ReadBE64(const unsigned char* ptr) {
    uint64_t x;
    memcpy((char*) &x, ptr, 8);
    return be64toh(x);
}

void extern inline WriteBE32(void* ptr, uint32_t x) {
    uint32_t v = htobe32(x);
    memcpy(ptr, (char*) &v, 4);
}

void extern inline WriteBE64(void* ptr, uint64_t x) {
    uint64_t v = htobe64(x);
    memcpy(ptr, (char*) &v, 8);
}

// Return the smallest number n such that (x >> n) == 0 (or 64 if the highest bit in x is set
uint64_t extern inline CountBits(uint64_t x) {
#if HAVE_DECL___BUILTIN_CLZL
    if (sizeof(unsigned long) >= sizeof(uint64_t)) {
        return x ? 8 * sizeof(unsigned long) - __builtin_clzl(x) : 0;
    }
#endif
#if HAVE_DECL___BUILTIN_CLZLL
    if (sizeof(unsigned long long) >= sizeof(uint64_t)) {
        return x ? 8 * sizeof(unsigned long long) - __builtin_clzll(x) : 0;
    }
#endif
    int ret = 0;
    while (x) {
        x >>= 1;
        ++ret;
    }

    return ret;
}

// Rotation to the right
uint32_t extern inline ROTR32(const uint32_t w, const unsigned c) {
    return (w >> c) | (w << (32 - c));
}

uint64_t extern inline ROTR64(const uint64_t w, const unsigned c) {
    return (w >> c) | (w << (64 - c));
}

#endif // BITCOIN_CRYPTO_COMMON_H
