#include "sha_hasher.h"

void SHAHasher256::Finalize(unsigned char hash[OUTPUT_SIZE]) {
    unsigned char buf[CSHA256::OUTPUT_SIZE];
    sha.Finalize(buf);
    sha.Reset().Write(buf, CSHA256::OUTPUT_SIZE).Finalize(hash);
}

SHAHasher256& SHAHasher256::Write(const unsigned char *data, size_t len) {
    sha.Write(data, len);
    return *this;
}

SHAHasher256& SHAHasher256::Reset() {
    sha.Reset();
    return *this;
}
