#ifndef __SRC_UTILS_CUCKAROO_COMPRESS_H__
#define __SRC_UTILS_CUCKAROO_COMPRESS_H__

#include <cstdint>
#include <new>

#include "cuckaroo.h"

// compressor for cuckaroo nodes where edgetrimming
// has left at most a fraction 2^-compressbits nodes in each partition
template <typename word_t>
class compressor {
public:
    uint32_t NODEBITS;
    uint32_t SHIFTBITS;
    uint32_t SIZEBITS;
    word_t SIZE;
    word_t SIZE2;
    word_t MASK;
    word_t MASK2;
    word_t nnodes;
    const word_t NIL = ~(word_t) 0;
    word_t* nodes;
    bool sharedmem;

    compressor(uint32_t nodebits, uint32_t compressbits, char* bytes) {
        NODEBITS  = nodebits;
        SHIFTBITS = compressbits;
        SIZEBITS  = NODEBITS - compressbits;
        SIZE      = (word_t) 1 << SIZEBITS;
        SIZE2     = (word_t) 2 << SIZEBITS;
        nodes     = new (bytes) word_t[SIZE2];
        sharedmem = true;
        MASK      = SIZE - 1;
        MASK2     = SIZE2 - 1;
    }

    compressor(uint32_t nodebits, uint32_t compressbits) {
        NODEBITS  = nodebits;
        SHIFTBITS = compressbits;
        SIZEBITS  = NODEBITS - compressbits;
        SIZE      = (word_t) 1 << SIZEBITS;
        SIZE2     = (word_t) 2 << SIZEBITS;
        nodes     = new word_t[SIZE2];
        sharedmem = false;
        MASK      = SIZE - 1;
        MASK2     = SIZE2 - 1;
    }

    ~compressor() {
        delete[] nodes;
    }

    uint64_t bytes() {
        return sizeof(word_t[SIZE2]);
    }

    void reset() {
        memset(nodes, (char) NIL, sizeof(word_t[SIZE2]));
        nnodes = 0;
    }

    word_t compress(word_t u) {
        word_t ui = u >> SHIFTBITS;
        for (;; ui = (ui + 1) & MASK2) {
            word_t cu = nodes[ui];
            if (cu == NIL) {
                if (nnodes >= SIZE) {
                    spdlog::trace("NODE OVERFLOW at {}", u);
                    return 0;
                }
                nodes[ui] = u << SIZEBITS | nnodes;
                return nnodes++;
            }
            if ((cu & ~MASK) == u << SIZEBITS) {
                return cu & MASK;
            }
        }
    }
};

#endif /* ifndef __SRC_UTILS_CUCKAROO_COMPRESS_H__ */
