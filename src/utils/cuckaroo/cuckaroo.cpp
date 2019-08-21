// Cuckoo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2016 John Tromp

#include "cuckaroo.h"

// arbitrary length of header hashed into siphash key
#ifndef HEADERLEN
#define HEADERLEN 112
#endif

uint64_t sipblock(siphash_keys& keys, word_t edge, uint64_t* buf) {
    siphash_state<> shs(keys);
    word_t edge0 = edge & ~EDGE_BLOCK_MASK;
    for (uint32_t i = 0; i < EDGE_BLOCK_SIZE; i++) {
        shs.hash24(edge0 + i);
        buf[i] = shs.xor_lanes();
    }
    const uint64_t last = buf[EDGE_BLOCK_MASK];
    for (uint32_t i = 0; i < EDGE_BLOCK_MASK; i++) {
        buf[i] ^= last;
    }
    return buf[edge & EDGE_BLOCK_MASK];
}

int verify(word_t edges[PROOFSIZE], siphash_keys& keys) {
    word_t xor0 = 0, xor1 = 0;
    uint64_t sips[EDGE_BLOCK_SIZE];
    word_t uvs[2 * PROOFSIZE];

    for (uint32_t n = 0; n < PROOFSIZE; n++) {
        if (edges[n] > EDGEMASK)
            return POW_TOO_BIG;
        if (n && edges[n] <= edges[n - 1])
            return POW_TOO_SMALL;
        uint64_t edge          = sipblock(keys, edges[n], sips);
        xor0 ^= uvs[2 * n]     = edge & EDGEMASK;
        xor1 ^= uvs[2 * n + 1] = (edge >> 32) & EDGEMASK;
    }
    if (xor0 | xor1) // optional check for obviously bad proofs
        return POW_NON_MATCHING;
    uint32_t n = 0, i = 0, j;
    do { // follow cycle
        for (uint32_t k = j = i; (k = (k + 2) % (2 * PROOFSIZE)) != i;) {
            if (uvs[k] == uvs[i]) { // find other edge endpoint identical to one at i
                if (j != i)         // already found one before
                    return POW_BRANCH;
                j = k;
            }
        }
        if (j == i)
            return POW_DEAD_END; // no matching endpoint
        i = j ^ 1;
        n++;
    } while (i != 0); // must cycle back to start or we would have found branch
    return n == PROOFSIZE ? POW_OK : POW_SHORT_CYCLE;
}

void setheader(const char* header, uint32_t headerlen, siphash_keys* keys) {
    // SHA256((unsigned char *)header, headerlen, (unsigned char *)hdrkey);
    unsigned char hdrkey[32];
    HashBLAKE2(header, headerlen, hdrkey, sizeof(hdrkey));
    keys->setkeys((char*) hdrkey);
}
