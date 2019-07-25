#ifndef __SRC_CRYPTO_BLAKE2B_H__
#define __SRC_CRYPTO_BLAKE2B_H__

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "common.h"

// hasher class for BLAKE2B
class BLAKE2B {
public:
    static constexpr uint8_t BLOCKBYTES    = 128;
    static constexpr uint8_t OUTBYTES      = 64;
    static constexpr uint8_t KEYBYTES      = 64;
    static constexpr uint8_t SALTBYTES     = 16;
    static constexpr uint8_t PERSONALBYTES = 16;

    struct state {
        uint64_t h[8];
        uint64_t t[2];
        uint64_t f[2];
        uint8_t buf[BLOCKBYTES];
        size_t buflen;
        size_t outlen;
        uint8_t last_node;
    };

    struct params {
        uint8_t digest_length;           /* 1 */
        uint8_t key_length;              /* 2 */
        uint8_t fanout;                  /* 3 */
        uint8_t depth;                   /* 4 */
        uint32_t leaf_length;            /* 8 */
        uint32_t node_offset;            /* 12 */
        uint32_t xof_length;             /* 16 */
        uint8_t node_depth;              /* 17 */
        uint8_t inner_length;            /* 18 */
        uint8_t reserved[14];            /* 32 */
        uint8_t salt[SALTBYTES];         /* 48 */
        uint8_t personal[PERSONALBYTES]; /* 64 */
    };

    BLAKE2B(size_t outputlen, const unsigned char* key = nullptr, size_t keylen = 0);
    BLAKE2B& Write(const unsigned char* data, size_t datalen);
    void Finalize(const unsigned char* out);
    BLAKE2B& Reset();

    friend bool BLAKE2BSelfTest();

private:
    state s;

    void InitParams(const params&);
    void Initialize(size_t outlen);
    void InitializeKey(size_t outlen, const unsigned char* key, size_t keylen);
};

void HashBLAKE2(const char* pin, uint32_t inlen, const unsigned char* out, uint32_t outlen);

// checks if the implementation is correct
bool BLAKE2BSelfTest();

#endif // __SRC_CRYPTO_BLAKE2B_H__
