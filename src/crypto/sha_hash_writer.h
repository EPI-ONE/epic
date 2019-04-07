#ifndef __SRC_CRYPTO_SHAHASHWRITER__
#define __SRC_CRYPTO_SHAHASHWRITER__

#include "crypto/sha_hasher.h"
#include "crypto/common.h"
#include "uint256.h"

// A writer stream (for serialization) that computes a 256-bit hash
class SHAHashWriter {
    private:
        SHAHasher256 ctx;

        const int nType;
        const int nVersion;
    public:
        SHAHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {}

        int GetType() const;
        int GetVersion() const;
        void write(const char *pch, size_t size);

        // invalidates the object
        uint256 GetHash();

        // Returns the first 64 bits from the resulting hash.
        uint64_t GetCheapHash();
};

#endif
