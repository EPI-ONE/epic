#ifndef EPIC_CLEANSE_H
#define EPIC_CLEANSE_H

#define __STDC_WANT_LIB_EXT1__ 1
#include <cstring>

typedef void* (*memset_t)(void*, int, std::size_t);
static volatile memset_t memset_safe = std::memset;

inline void memory_cleanse(void* ptr, std::size_t len) {
#ifdef __STDC_LIB_EXT1__
    memset_s(pointer, size_to_remove, 0, size_to_remove);
#else
    memset_safe(ptr, 0, len);
#endif
}

#endif
