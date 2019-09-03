#include "random.h"

#include <climits>
#include <openssl/rand.h>

void GetRDRandBytes(unsigned char* buf, size_t size) noexcept {
    unsigned long long x;
    uint_fast8_t j;

    for (size_t i = 0; i < size; i++) {
        j = i % 8;
        if (j == 0) {
            while (!_rdrand64_step(&x))
                ;
        }

        buf[i] = ((unsigned char*) &x)[j];
    }
}

bool GetOpenSSLRand(unsigned char* buf, size_t size) noexcept {
    return bool(RAND_bytes(buf, size));
}
