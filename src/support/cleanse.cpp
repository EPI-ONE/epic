#include "cleanse.h"

void* memset_tmp(void* ptr, int value, std::size_t len) {
    assert(ptr != nullptr);

#ifdef __STDC_LIB_EXT1__
    return memset_s(ptr, len, value, len);
#else
    return std::memset(ptr, value, len);
#endif
}

void* memset_ntmp(void* ptr, int value, std::size_t len) {
    assert(ptr != nullptr);

    std::size_t nbytes = len;

    while (nbytes >= 8) {
        nbytes -= 8;
        _mm_stream_si64((long long*) (static_cast<char*>(ptr) + nbytes), value);
    }

    while (nbytes >= 4) {
        nbytes -= 4;
        _mm_stream_si32((int*) (static_cast<char*>(ptr) + nbytes), value);
    }

    if (nbytes > 0) {
        memset_tmp(ptr, value, nbytes);
    }

    return ptr;
}
