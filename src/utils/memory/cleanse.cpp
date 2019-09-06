// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cleanse.h"

void* memset_zero_tmp(void* ptr, std::size_t len) {
    assert(ptr != nullptr);

#ifdef __STDC_LIB_EXT1__
    return memset_s(ptr, len, 0, len);
#else
    return std::memset(ptr, 0, len);
#endif
}

void* memset_zero_ntmp(void* ptr, std::size_t len) {
    assert(ptr != nullptr);

    std::size_t nbytes = len;

    while (nbytes >= 8) {
        nbytes -= 8;
        _mm_stream_si64((long long*) (static_cast<char*>(ptr) + nbytes), 0);
    }

    while (nbytes >= 4) {
        nbytes -= 4;
        _mm_stream_si32((int*) (static_cast<char*>(ptr) + nbytes), 0);
    }

    if (nbytes > 0) {
        memset_zero_tmp(ptr, nbytes);
    }

    return ptr;
}
