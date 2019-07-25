#ifndef __SRC_UTILS_CUCKAROO_BITMAP_H__
#define __SRC_UTILS_CUCKAROO_BITMAP_H__

#include <cassert>
#include <cstdint>

template <typename word_t>
class bitmap {
public:
    word_t SIZE;
    word_t BITMAP_WORDS;
#ifdef ATOMIC
    typedef std::atomic<word_t> aword_t;
#else
    typedef word_t aword_t;
#endif
    aword_t* bits;
    const uint32_t BITS_PER_WORD = sizeof(word_t) * 8;

    bitmap(word_t size) {
        SIZE         = size;
        BITMAP_WORDS = SIZE / BITS_PER_WORD;
        bits         = new aword_t[BITMAP_WORDS];
        assert(bits != 0);
    }
    ~bitmap() {
        freebits();
    }
    void freebits() {
        delete[] bits;
        bits = 0;
    }
    void clear() {
        assert(bits);
        memset((word_t*) bits, 0, BITMAP_WORDS * sizeof(word_t));
    }
    void prefetch(uint32_t u) const {
#ifdef PREFETCH
        __builtin_prefetch((const void*) (&bits[u / BITS_PER_WORD]), /*READ=*/0, /*TEMPORAL=*/0);
#endif
    }
    void set(uint32_t u) {
        uint32_t idx = u / BITS_PER_WORD;
        word_t bit   = (word_t) 1 << (u % BITS_PER_WORD);
#ifdef ATOMIC
        std::atomic_fetch_or_explicit(&bits[idx], bit, std::memory_order_relaxed);
#else
        bits[idx] |= bit;
#endif
    }
    void reset(uint32_t u) {
        uint32_t idx = u / BITS_PER_WORD;
        word_t bit   = (word_t) 1 << (u % BITS_PER_WORD);
#ifdef ATOMIC
        std::atomic_fetch_and_explicit(&bits[idx], ~bit, std::memory_order_relaxed);
#else
        bits[idx] &= ~bit;
#endif
    }
    bool test(uint32_t u) const {
        uint32_t idx = u / BITS_PER_WORD;
        uint32_t bit = u % BITS_PER_WORD;
#ifdef ATOMIC
        return (bits[idx].load(std::memory_order_relaxed) >> bit) & 1;
#else
        return (bits[idx] >> bit) & 1;
#endif
    }
    word_t block(uint32_t n) const {
        uint32_t idx = n / BITS_PER_WORD;
        return bits[idx];
    }
};

#endif // __SRC_UTILS_CUCKAROO_BITMAP_H__
