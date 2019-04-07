#ifndef __SRC_CRYPTO_SHAHASHER__
#define __SRC_CRYPTO_SHAHASHER__

#include "crypto/sha256.h"

// A hasher class for Bitcoin's 256-bit hash (double SHA-256)
class SHAHasher256 {
    private:
        CSHA256 sha;
    public:
        static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

        void Finalize(unsigned char hash[OUTPUT_SIZE]);

        SHAHasher256& Write(const unsigned char *data, size_t len);

        SHAHasher256& Reset();
};

#endif
