// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef EPIC_CLEANSE_H
#define EPIC_CLEANSE_H

#define __STDC_WANT_LIB_EXT1__ 1

#include <cassert>
#include <cstring>
#include <immintrin.h>

void* memset_zero_tmp(void* ptr, std::size_t len);
void* memset_zero_ntmp(void* ptr, std::size_t len);

typedef void* (*memset_zero_t)(void*, std::size_t);
static volatile memset_zero_t memset_safe_zero_tmp  = memset_zero_tmp;
static volatile memset_zero_t memset_safe_zero_ntmp = memset_zero_ntmp;

template <bool FinalCleanse = false>
inline void memory_cleanse(void* ptr, std::size_t len) {
    if (FinalCleanse) {
        memset_safe_zero_ntmp(ptr, len);
    } else {
        memset_safe_zero_tmp(ptr, len);
    }
}

#endif
