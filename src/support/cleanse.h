#ifndef EPIC_CLEANSE_H
#define EPIC_CLEANSE_H

#define __STDC_WANT_LIB_EXT1__ 1

#include <cassert>
#include <cstring>
#include <immintrin.h>

void* memset_tmp(void* ptr, int value, std::size_t len);
void* memset_ntmp(void* ptr, int value, std::size_t len);

typedef void* (*memset_t)(void*, int, std::size_t);
static volatile memset_t memset_safe_tmp  = memset_tmp;
static volatile memset_t memset_safe_ntmp = memset_ntmp;

template <bool FinalCleanse = false>
inline void memory_cleanse(void* ptr, std::size_t len) {
    if (FinalCleanse) {
        memset_safe_ntmp(ptr, 0, len);
    } else {
        memset_safe_tmp(ptr, 0, len);
    }
}

#endif
