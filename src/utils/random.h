#ifndef EPIC_UTIL_RANDOM
#define EPIC_UTIL_RANDOM

#include <cstddef>

void GetRDRandBytes(unsigned char* buf, size_t size) noexcept;
bool GetOpenSSLRand(unsigned char* buf, size_t size) noexcept;

#endif // EPIC_UTIL_RANDOM
