#ifndef __SRC_UTIL_RANDOM__
#define __SRC_UTIL_RANDOM__

#include <immintrin.h>

void GetRDRandBytes(unsigned char* buf, size_t size) noexcept;
bool GetOpenSSLRand(unsigned char* buf, size_t size) noexcept;

#endif // __SRC_UTIL_RANDOM__
