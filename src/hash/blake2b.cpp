/*
   BLAKE2 reference source code package - reference C implementations

   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:

   - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0

   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
*/

#include "blake2b.h"
#include "zeroafterfree.h"
#include <cassert>

const uint64_t blake2b_IV[8] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL,
                                0xa54ff53a5f1d36f1ULL, 0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
                                0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};

const uint8_t blake2b_sigma[12][16] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
    {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4}, {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
    {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13}, {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
    {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11}, {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
    {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5}, {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3}};

void blake2b_set_lastnode(BLAKE2B::state& S) {
    S.f[1] = (uint64_t) -1;
}

int blake2b_is_lastblock(const BLAKE2B::state& S) {
    return S.f[0] != 0;
}

void blake2b_set_lastblock(BLAKE2B::state& S) {
    if (S.last_node)
        blake2b_set_lastnode(S);

    S.f[0] = (uint64_t) -1;
}

void blake2b_increment_counter(BLAKE2B::state& S, const uint64_t& inc) {
    S.t[0] += inc;
    S.t[1] += (S.t[0] < inc);
}

#define G(r, i, a, b, c, d)                         \
    do {                                            \
        a = a + b + m[blake2b_sigma[r][2 * i + 0]]; \
        d = ROTR64(d ^ a, 32);                      \
        c = c + d;                                  \
        b = ROTR64(b ^ c, 24);                      \
        a = a + b + m[blake2b_sigma[r][2 * i + 1]]; \
        d = ROTR64(d ^ a, 16);                      \
        c = c + d;                                  \
        b = ROTR64(b ^ c, 63);                      \
    } while (0)

#define ROUND(r)                           \
    do {                                   \
        G(r, 0, v[0], v[4], v[8], v[12]);  \
        G(r, 1, v[1], v[5], v[9], v[13]);  \
        G(r, 2, v[2], v[6], v[10], v[14]); \
        G(r, 3, v[3], v[7], v[11], v[15]); \
        G(r, 4, v[0], v[5], v[10], v[15]); \
        G(r, 5, v[1], v[6], v[11], v[12]); \
        G(r, 6, v[2], v[7], v[8], v[13]);  \
        G(r, 7, v[3], v[4], v[9], v[14]);  \
    } while (0)

void blake2b_compress(BLAKE2B::state& S, const uint8_t block[BLAKE2B::BLOCKBYTES]) {
    uint64_t m[16];
    uint64_t v[16];
    size_t i;

    for (i = 0; i < 16; ++i) {
        m[i] = ReadLE64(block + i * sizeof(m[i]));
    }

    for (i = 0; i < 8; ++i) {
        v[i] = S.h[i];
    }

    v[8]  = blake2b_IV[0];
    v[9]  = blake2b_IV[1];
    v[10] = blake2b_IV[2];
    v[11] = blake2b_IV[3];
    v[12] = blake2b_IV[4] ^ S.t[0];
    v[13] = blake2b_IV[5] ^ S.t[1];
    v[14] = blake2b_IV[6] ^ S.f[0];
    v[15] = blake2b_IV[7] ^ S.f[1];

    ROUND(0);
    ROUND(1);
    ROUND(2);
    ROUND(3);
    ROUND(4);
    ROUND(5);
    ROUND(6);
    ROUND(7);
    ROUND(8);
    ROUND(9);
    ROUND(10);
    ROUND(11);

    for (i = 0; i < 8; ++i) {
        S.h[i] = S.h[i] ^ v[i] ^ v[i + 8];
    }
}

#undef G
#undef ROUND

void blake2b_init0(BLAKE2B::state& S) {
    size_t i;
    memset(&S, 0, sizeof(BLAKE2B::state));

    for (i = 0; i < 8; ++i)
        S.h[i] = blake2b_IV[i];
}

///////////////////////
// BLAKE2B
///////////////////////

BLAKE2B::BLAKE2B(size_t outlen, const unsigned char* key, size_t keylen) {
    if (outlen > OUTBYTES) {
        throw "BLAKE2B initialize: output size out of range.";
    }

    if (!key && keylen > 0) {
        throw "BLAKE2B initialize: key ptr is null but key size > 0.";
    }

    if (keylen > 0) {
        InitializeKey(outlen, key, keylen);
    } else {
        Initialize(outlen);
    }
}

void BLAKE2B::InitParams(const params& P) {
    const uint8_t* p = (const uint8_t*) (&P);
    size_t i;

    blake2b_init0(s);

    /* IV XOR ParamBlock */
    for (i = 0; i < 8; ++i) {
        s.h[i] ^= ReadLE64(p + sizeof(s.h[i]) * i);
    }

    s.outlen = P.digest_length;
}

void BLAKE2B::InitializeKey(size_t outlen, const unsigned char* key, size_t keylen) {
    params P;

    if (!key || !keylen || keylen > KEYBYTES) {
        throw "BLAKE2B initialize key: null key, or zero key length, or key length out of range";
    }

    P.digest_length = outlen;
    P.key_length    = (uint8_t) keylen;
    P.fanout        = 1;
    P.depth         = 1;
    WriteLE32(&P.leaf_length, 0);
    WriteLE32(&P.node_offset, 0);
    WriteLE32(&P.xof_length, 0);
    P.node_depth   = 0;
    P.inner_length = 0;
    memset(P.reserved, 0, sizeof(P.reserved));
    memset(P.salt, 0, sizeof(P.salt));
    memset(P.personal, 0, sizeof(P.personal));

    InitParams(P);

    std::vector<uint8_t, zero_after_free_allocator<uint8_t>> block(BLOCKBYTES, 0);
    assert(block.size() == BLOCKBYTES);
    memcpy(block.data(), key, keylen);
    Write(block.data(), BLOCKBYTES);
}

void BLAKE2B::Initialize(size_t outlen) {
    params P;

    P.digest_length = outlen;
    P.key_length    = 0;
    P.fanout        = 1;
    P.depth         = 1;
    WriteLE32(&P.leaf_length, 0);
    WriteLE32(&P.node_offset, 0);
    WriteLE32(&P.xof_length, 0);
    P.node_depth   = 0;
    P.inner_length = 0;
    memset(P.reserved, 0, sizeof(P.reserved));
    memset(P.salt, 0, sizeof(P.salt));
    memset(P.personal, 0, sizeof(P.personal));

    InitParams(P);
}

BLAKE2B& BLAKE2B::Write(const unsigned char* pin, size_t inlen) {
    if (!pin && inlen > 0) {
        throw "BLAKE2B write: input ptr is null but length > 0.";
    }

    if (inlen > 0) {
        size_t left = s.buflen;
        size_t fill = BLOCKBYTES - left;
        if (inlen > fill) {
            s.buflen = 0;
            memcpy(s.buf + left, pin, fill); /* Fill buffer */
            blake2b_increment_counter(s, BLOCKBYTES);
            blake2b_compress(s, s.buf); /* Compress */
            pin += fill;
            inlen -= fill;
            while (inlen > BLOCKBYTES) {
                blake2b_increment_counter(s, BLOCKBYTES);
                blake2b_compress(s, pin);
                pin += BLOCKBYTES;
                inlen -= BLOCKBYTES;
            }
        }
        memcpy(s.buf + s.buflen, pin, inlen);
        s.buflen += inlen;
    }
    return *this;
}

void BLAKE2B::Finalize(const unsigned char* out) {
    std::vector<uint8_t, zero_after_free_allocator<uint8_t>> buffer((size_t) OUTBYTES);
    size_t i;

    if (blake2b_is_lastblock(s)) {
        throw "BLAKE2B finalize: start with the last block.";
    }

    blake2b_increment_counter(s, s.buflen);
    blake2b_set_lastblock(s);
    memset(s.buf + s.buflen, 0, BLOCKBYTES - s.buflen); /* Padding */
    blake2b_compress(s, s.buf);

    for (i = 0; i < 8; ++i) { /* Output full hash to temp buffer */
        WriteLE64(buffer.data() + sizeof(s.h[i]) * i, s.h[i]);
    }

    memcpy((void*) out, buffer.data(), s.outlen);
}

void HashBLAKE2(const char* pin, uint32_t inlen, const unsigned char* out, uint32_t outlen) {
    static const unsigned char emptyByte[0] = {};
    BLAKE2B blake(outlen);
    blake.Write(pin == pin + inlen ? emptyByte : (const unsigned char*) pin, inlen);
    blake.Finalize(out);
}

bool BLAKE2BSelfTest() {
#define BLAKE2_KAT_LENGTH 8
    static const uint8_t blake2b_keyed_kat[BLAKE2_KAT_LENGTH][BLAKE2B::OUTBYTES] = {
        {0x10, 0xEB, 0xB6, 0x77, 0x00, 0xB1, 0x86, 0x8E, 0xFB, 0x44, 0x17, 0x98, 0x7A, 0xCF, 0x46, 0x90,
         0xAE, 0x9D, 0x97, 0x2F, 0xB7, 0xA5, 0x90, 0xC2, 0xF0, 0x28, 0x71, 0x79, 0x9A, 0xAA, 0x47, 0x86,
         0xB5, 0xE9, 0x96, 0xE8, 0xF0, 0xF4, 0xEB, 0x98, 0x1F, 0xC2, 0x14, 0xB0, 0x05, 0xF4, 0x2D, 0x2F,
         0xF4, 0x23, 0x34, 0x99, 0x39, 0x16, 0x53, 0xDF, 0x7A, 0xEF, 0xCB, 0xC1, 0x3F, 0xC5, 0x15, 0x68},
        {0x96, 0x1F, 0x6D, 0xD1, 0xE4, 0xDD, 0x30, 0xF6, 0x39, 0x01, 0x69, 0x0C, 0x51, 0x2E, 0x78, 0xE4,
         0xB4, 0x5E, 0x47, 0x42, 0xED, 0x19, 0x7C, 0x3C, 0x5E, 0x45, 0xC5, 0x49, 0xFD, 0x25, 0xF2, 0xE4,
         0x18, 0x7B, 0x0B, 0xC9, 0xFE, 0x30, 0x49, 0x2B, 0x16, 0xB0, 0xD0, 0xBC, 0x4E, 0xF9, 0xB0, 0xF3,
         0x4C, 0x70, 0x03, 0xFA, 0xC0, 0x9A, 0x5E, 0xF1, 0x53, 0x2E, 0x69, 0x43, 0x02, 0x34, 0xCE, 0xBD},
        {0xDA, 0x2C, 0xFB, 0xE2, 0xD8, 0x40, 0x9A, 0x0F, 0x38, 0x02, 0x61, 0x13, 0x88, 0x4F, 0x84, 0xB5,
         0x01, 0x56, 0x37, 0x1A, 0xE3, 0x04, 0xC4, 0x43, 0x01, 0x73, 0xD0, 0x8A, 0x99, 0xD9, 0xFB, 0x1B,
         0x98, 0x31, 0x64, 0xA3, 0x77, 0x07, 0x06, 0xD5, 0x37, 0xF4, 0x9E, 0x0C, 0x91, 0x6D, 0x9F, 0x32,
         0xB9, 0x5C, 0xC3, 0x7A, 0x95, 0xB9, 0x9D, 0x85, 0x74, 0x36, 0xF0, 0x23, 0x2C, 0x88, 0xA9, 0x65},
        {0x33, 0xD0, 0x82, 0x5D, 0xDD, 0xF7, 0xAD, 0xA9, 0x9B, 0x0E, 0x7E, 0x30, 0x71, 0x04, 0xAD, 0x07,
         0xCA, 0x9C, 0xFD, 0x96, 0x92, 0x21, 0x4F, 0x15, 0x61, 0x35, 0x63, 0x15, 0xE7, 0x84, 0xF3, 0xE5,
         0xA1, 0x7E, 0x36, 0x4A, 0xE9, 0xDB, 0xB1, 0x4C, 0xB2, 0x03, 0x6D, 0xF9, 0x32, 0xB7, 0x7F, 0x4B,
         0x29, 0x27, 0x61, 0x36, 0x5F, 0xB3, 0x28, 0xDE, 0x7A, 0xFD, 0xC6, 0xD8, 0x99, 0x8F, 0x5F, 0xC1},
        {0xBE, 0xAA, 0x5A, 0x3D, 0x08, 0xF3, 0x80, 0x71, 0x43, 0xCF, 0x62, 0x1D, 0x95, 0xCD, 0x69, 0x05,
         0x14, 0xD0, 0xB4, 0x9E, 0xFF, 0xF9, 0xC9, 0x1D, 0x24, 0xB5, 0x92, 0x41, 0xEC, 0x0E, 0xEF, 0xA5,
         0xF6, 0x01, 0x96, 0xD4, 0x07, 0x04, 0x8B, 0xBA, 0x8D, 0x21, 0x46, 0x82, 0x8E, 0xBC, 0xB0, 0x48,
         0x8D, 0x88, 0x42, 0xFD, 0x56, 0xBB, 0x4F, 0x6D, 0xF8, 0xE1, 0x9C, 0x4B, 0x4D, 0xAA, 0xB8, 0xAC},
        {0x09, 0x80, 0x84, 0xB5, 0x1F, 0xD1, 0x3D, 0xEA, 0xE5, 0xF4, 0x32, 0x0D, 0xE9, 0x4A, 0x68, 0x8E,
         0xE0, 0x7B, 0xAE, 0xA2, 0x80, 0x04, 0x86, 0x68, 0x9A, 0x86, 0x36, 0x11, 0x7B, 0x46, 0xC1, 0xF4,
         0xC1, 0xF6, 0xAF, 0x7F, 0x74, 0xAE, 0x7C, 0x85, 0x76, 0x00, 0x45, 0x6A, 0x58, 0xA3, 0xAF, 0x25,
         0x1D, 0xC4, 0x72, 0x3A, 0x64, 0xCC, 0x7C, 0x0A, 0x5A, 0xB6, 0xD9, 0xCA, 0xC9, 0x1C, 0x20, 0xBB},
        {0x60, 0x44, 0x54, 0x0D, 0x56, 0x08, 0x53, 0xEB, 0x1C, 0x57, 0xDF, 0x00, 0x77, 0xDD, 0x38, 0x10,
         0x94, 0x78, 0x1C, 0xDB, 0x90, 0x73, 0xE5, 0xB1, 0xB3, 0xD3, 0xF6, 0xC7, 0x82, 0x9E, 0x12, 0x06,
         0x6B, 0xBA, 0xCA, 0x96, 0xD9, 0x89, 0xA6, 0x90, 0xDE, 0x72, 0xCA, 0x31, 0x33, 0xA8, 0x36, 0x52,
         0xBA, 0x28, 0x4A, 0x6D, 0x62, 0x94, 0x2B, 0x27, 0x1F, 0xFA, 0x26, 0x20, 0xC9, 0xE7, 0x5B, 0x1F},
        {0x7A, 0x8C, 0xFE, 0x9B, 0x90, 0xF7, 0x5F, 0x7E, 0xCB, 0x3A, 0xCC, 0x05, 0x3A, 0xAE, 0xD6, 0x19,
         0x31, 0x12, 0xB6, 0xF6, 0xA4, 0xAE, 0xEB, 0x3F, 0x65, 0xD3, 0xDE, 0x54, 0x19, 0x42, 0xDE, 0xB9,
         0xE2, 0x22, 0x81, 0x52, 0xA3, 0xC4, 0xBB, 0xBE, 0x72, 0xFC, 0x3B, 0x12, 0x62, 0x95, 0x28, 0xCF,
         0xBB, 0x09, 0xFE, 0x63, 0x0F, 0x04, 0x74, 0x33, 0x9F, 0x54, 0xAB, 0xF4, 0x53, 0xE2, 0xED, 0x52}};

    uint8_t key[BLAKE2B::KEYBYTES];
    uint8_t buf[BLAKE2_KAT_LENGTH];
    size_t i, step;

    for (i = 0; i < BLAKE2B::KEYBYTES; ++i) {
        key[i] = (uint8_t) i;
    }

    for (i = 0; i < BLAKE2_KAT_LENGTH; ++i) {
        buf[i] = (uint8_t) i;
    }

    /* Test simple API */
    for (i = 0; i < BLAKE2_KAT_LENGTH; ++i) {
        uint8_t hash[BLAKE2B::OUTBYTES];

        BLAKE2B hash_writer(BLAKE2B::OUTBYTES, key, BLAKE2B::KEYBYTES);
        hash_writer.Write(buf, i);
        hash_writer.Finalize(hash);

        if (0 != memcmp(hash, blake2b_keyed_kat[i], BLAKE2B::OUTBYTES)) {
            return false;
        }
    }

    /* Test streaming API */
    for (step = 1; step < BLAKE2B::BLOCKBYTES; ++step) {
        for (i = 0; i < BLAKE2_KAT_LENGTH; ++i) {
            uint8_t hash[BLAKE2B::OUTBYTES];
            uint8_t* p  = buf;
            size_t mlen = i;

            BLAKE2B hash_writer(BLAKE2B::OUTBYTES, key, BLAKE2B::KEYBYTES);

            while (mlen >= step) {
                hash_writer.Write(p, step);
                mlen -= step;
                p += step;
            }

            hash_writer.Write(p, mlen);
            hash_writer.Finalize(hash);

            if (0 != memcmp(hash, blake2b_keyed_kat[i], BLAKE2B::OUTBYTES)) {
                return false;
            }
        }
    }

    return true;
}
