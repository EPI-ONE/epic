// Cuckaroo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2019 John Tromp
// The edge-trimming memory optimization is due to Dave Andersen
// http://da-data.blogspot.com/2014/03/a-public-review-of-cuckoo-cycle.html
// xenoncat demonstrated at https://github.com/xenoncat/cuckoo_pow
// how bucket sorting avoids random memory access latency
// my own cycle finding is run single threaded to avoid losing cycles
// to race conditions (typically takes under 1% of runtime)

#ifndef __SRC_UTILS_CUCKAROO_MEAN_H__
#define __SRC_UTILS_CUCKAROO_MEAN_H__

#include "barrier.h"
#include "cuckaroo.h"
#include "graph.h"
#include "hash.h"
#include "siphashxN.h"

#include <bitset>
#include <cassert>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h> // sleep
#include <vector>
#include <x86intrin.h>

// algorithm/performance parameters

// EDGEBITS/NEDGES/EDGEMASK defined in cuckaroo.h

// The node bits are logically split into 3 groups:
// XBITS 'X' bits (most significant), YBITS 'Y' bits, and ZBITS 'Z' bits (least
// significant) Here we have the default XBITS=YBITS=7, ZBITS=15 summing to
// EDGEBITS=29 nodebits
//
// XXXXXXX YYYYYYY ZZZZZZZZZZZZZZZ bit%10
// 8765432 1098765 432109876543210 bit/10
// 2222222 2211111 111110000000000

// The matrix solver stores all edges in a matrix of NX * NX buckets,
// where NX = 2^XBITS is the number of possible values of the 'X' bits.
// Edge i between nodes ui = siphash24(2*i) and vi = siphash24(2*i+1)
// resides in the bucket at (uiX,viX)
// In each trimming round, either a matrix row or a matrix column (NX buckets)
// is bucket sorted on uY or vY respectively, and then within each bucket
// uZ or vZ values are counted and edges with a count of only one are
// eliminated, while remaining edges are bucket sorted back on vX or uX
// respectively. When sufficiently many edges have been eliminated, a pair of
// compression rounds remap surviving Y,Z values in each row or column into 15
// bit combined YZ values, allowing the remaining rounds to avoid the sorting on
// Y, and directly count YZ values in a cache friendly 32KB. A final pair of
// compression rounds remap YZ values from 15 into 11 bits.

#ifndef XBITS
// 7 seems to give best performance
#define XBITS 7
//#define XBITS 4
#endif

#define YBITS XBITS

// need to fit 2 endpoints bucketed by one XBITS
#define BIGSIZE0 ((2 * EDGEBITS - XBITS + 7) / 8)
// size in bytes of a big bucket entry
// need to fit 2 endpoints bucketed by XBITS and YBITS
#define BIGSIZE1 ((2 * (EDGEBITS - XBITS) + 7) / 8)
// YZ compression round; must be even
#ifndef COMPRESSROUND
#define COMPRESSROUND 14
//#define COMPRESSROUND 8
#endif
// size in bytes of a small bucket entry
#define SMALLSIZE BIGSIZE1

#ifndef BIGSIZE
#if EDGEBITS <= 15
#define BIGSIZE 4
#else
#define BIGSIZE 5
#endif
#endif

#if EDGEBITS >= 30
typedef uint64_t offset_t;
#else
typedef uint32_t offset_t;
#endif

#if BIGSIZE0 > 4
typedef uint64_t BIGTYPE0;
#else
typedef uint32_t BIGTYPE0;
#endif

// node bits have two groups of bucketbits (X for big and Y for small) and a
// remaining group Z of degree bits
const uint32_t NX        = 1 << XBITS;
const uint32_t XMASK     = NX - 1;
const uint32_t NY        = 1 << YBITS;
const uint32_t YMASK     = NY - 1;
const uint32_t XYBITS    = XBITS + YBITS;
const uint32_t NXY       = 1 << XYBITS;
const uint32_t ZBITS     = EDGEBITS - XYBITS;
const uint32_t NZ        = 1 << ZBITS;
const uint32_t ZMASK     = NZ - 1;
const uint32_t YZBITS    = EDGEBITS - XBITS;
const uint32_t NYZ       = 1 << YZBITS;
const uint32_t YZMASK    = NYZ - 1;
const uint32_t YZ1BITS   = YZBITS < 15 ? YZBITS : 15; // compressed YZ bits
const uint32_t NYZ1      = 1 << YZ1BITS;
const uint32_t MAXNZNYZ1 = NZ < NYZ1 ? NYZ1 : NZ;
const uint32_t YZ1MASK   = NYZ1 - 1;
const uint32_t Z1BITS    = YZ1BITS - YBITS;
const uint32_t NZ1       = 1 << Z1BITS;
const uint32_t Z1MASK    = NZ1 - 1;
const uint32_t YZ2BITS   = YZBITS < 11 ? YZBITS : 11; // more compressed YZ bits
const uint32_t NYZ2      = 1 << YZ2BITS;
const uint32_t YZ2MASK   = NYZ2 - 1;
const uint32_t Z2BITS    = YZ2BITS - YBITS;
const uint32_t NZ2       = 1 << Z2BITS;
const uint32_t Z2MASK    = NZ2 - 1;
const uint32_t YZZBITS   = YZBITS + ZBITS;
const uint32_t YZZ1BITS  = YZ1BITS + ZBITS;

const uint32_t MAXEDGES = NX * NYZ2;

const uint32_t BIGSLOTBITS   = BIGSIZE * 8;
const uint32_t SMALLSLOTBITS = SMALLSIZE * 8;
const uint64_t BIGSLOTMASK   = (1ULL << BIGSLOTBITS) - 1ULL;
const uint64_t SMALLSLOTMASK = (1ULL << SMALLSLOTBITS) - 1ULL;
const uint32_t BIGSLOTBITS0  = BIGSIZE0 * 8;
const uint64_t BIGSLOTMASK0  = (1ULL << BIGSLOTBITS0) - 1ULL;

// for p close to 0, Pr(X>=k) < e^{-n*p*eps^2} where k=n*p*(1+eps)
// see https://en.wikipedia.org/wiki/Binomial_distribution#Tail_bounds
// eps should be at least 1/sqrt(n*p/64)
// to give negligible bad odds of e^-64.

// 1/32 reduces odds of overflowing z bucket on 2^30 nodes to 2^14*e^-32
// (less than 1 in a billion) in theory. not so in practice (fails first at
// mean30 -n 1549)
#ifndef BIGEPS
#define BIGEPS 3 / 64
#endif

// 176/256 is safely over 1-e(-1) ~ 0.63 trimming fraction
#ifndef TRIMFRAC256
#define TRIMFRAC256 176
#endif

const uint32_t NTRIMMEDZ = NZ * TRIMFRAC256 / 256;

const uint32_t ZBUCKETSLOTS = NZ + NZ * BIGEPS;
const uint32_t ZBUCKETSIZE  = ZBUCKETSLOTS * BIGSIZE0;
const uint32_t TBUCKETSIZE  = ZBUCKETSLOTS * BIGSIZE1;

template <uint32_t BUCKETSIZE>
struct zbucket {
    uint32_t size;
    // should avoid different values of RENAMESIZE in different threads of one
    // process
    static const uint32_t RENAMESIZE = 2 * NZ2 + 2 * (COMPRESSROUND ? NZ1 : 0);

    union alignas(16) {
        uint8_t bytes[BUCKETSIZE];
        struct {
            uint32_t words[BUCKETSIZE / sizeof(uint32_t) - RENAMESIZE];
            uint32_t renameu1[NZ2];
            uint32_t renamev1[NZ2];
            uint32_t renameu[COMPRESSROUND ? NZ1 : 0];
            uint32_t renamev[COMPRESSROUND ? NZ1 : 0];
        };
    };

    uint32_t setsize(uint8_t const* end) {
        size = end - bytes;
        assert(size <= BUCKETSIZE);
        return size;
    }
};

template <uint32_t BUCKETSIZE>
using yzbucket = zbucket<BUCKETSIZE>[NY];
template <uint32_t BUCKETSIZE>
using matrix = yzbucket<BUCKETSIZE>[NX];

template <uint32_t BUCKETSIZE>
struct indexer {
    offset_t index[NX];

    void matrixv(const uint32_t y) {
        const yzbucket<BUCKETSIZE>* foo = 0;
        for (uint32_t x = 0; x < NX; x++)
            index[x] = foo[x][y].bytes - (uint8_t*) foo;
    }

    offset_t storev(yzbucket<BUCKETSIZE>* buckets, const uint32_t y) {
        uint8_t const* base = (uint8_t*) buckets;
        offset_t sumsize    = 0;
        for (uint32_t x = 0; x < NX; x++)
            sumsize += buckets[x][y].setsize(base + index[x]);
        return sumsize;
    }

    void matrixu(const uint32_t x) {
        const yzbucket<BUCKETSIZE>* foo = 0;
        for (uint32_t y = 0; y < NY; y++)
            index[y] = foo[x][y].bytes - (uint8_t*) foo;
    }

    offset_t storeu(yzbucket<BUCKETSIZE>* buckets, const uint32_t x) {
        uint8_t const* base = (uint8_t*) buckets;
        offset_t sumsize    = 0;
        for (uint32_t y = 0; y < NY; y++)
            sumsize += buckets[x][y].setsize(base + index[y]);
        return sumsize;
    }
};

#define likely(x) __builtin_expect((x) != 0, 1)
#define unlikely(x) __builtin_expect((x), 0)

class CEdgeTrimmer; // avoid circular references

struct thread_ctx {
    uint32_t id;
    pthread_t thread;
    CEdgeTrimmer* et;
};

typedef uint8_t zbucket8[2 * MAXNZNYZ1];
typedef uint16_t zbucket16[NTRIMMEDZ];
typedef uint32_t zbucket32[NTRIMMEDZ];

void* etworker(void* vp);
void* matchworker(void* vp);

// maintains set of trimmable edges
class CEdgeTrimmer {
public:
    siphash_keys sip_keys;
    yzbucket<ZBUCKETSIZE>* buckets;
    yzbucket<TBUCKETSIZE>* tbuckets;
    zbucket8* tdegs   = nullptr;
    offset_t* tcounts = nullptr;
    uint32_t ntrims;
    uint32_t nthreads;
    bool showall;
    thread_ctx* threads = nullptr;
    trim_barrier barry;

    CEdgeTrimmer() = default;
    CEdgeTrimmer(const uint32_t& n_threads, const uint32_t& n_trims, bool show_all);
    ~CEdgeTrimmer();

    void touch(uint8_t* p, const offset_t& n);
    offset_t count() const;

    void genUVnodes(const uint32_t& id);
    void sortVnodes(const uint32_t& id, const uint32_t& uorv);

    template <uint32_t SRCSIZE, uint32_t DSTSIZE, bool TRIMONV>
    void trimedges(const uint32_t& id, const uint32_t& round) {
        const uint32_t SRCSLOTBITS = std::min(SRCSIZE * 8, 2 * YZBITS);
        const uint64_t SRCSLOTMASK = (1ULL << SRCSLOTBITS) - 1ULL;
        const uint32_t SRCPREFBITS = SRCSLOTBITS - YZBITS;
        const uint32_t SRCPREFMASK = (1 << SRCPREFBITS) - 1;
        const uint32_t DSTSLOTBITS = std::min(DSTSIZE * 8, 2 * YZBITS);
        const uint64_t DSTSLOTMASK = (1ULL << DSTSLOTBITS) - 1ULL;
        const uint32_t DSTPREFBITS = DSTSLOTBITS - YZZBITS;
        const uint32_t DSTPREFMASK = (1 << DSTPREFBITS) - 1;
        uint64_t rdtsc0, rdtsc1;
        indexer<ZBUCKETSIZE> dst;
        indexer<TBUCKETSIZE> small;

        rdtsc0                 = __rdtsc();
        offset_t sumsize       = 0;
        uint8_t const* base    = (uint8_t*) buckets;
        uint8_t const* small0  = (uint8_t*) tbuckets[id];
        const uint32_t startvx = NY * id / nthreads;
        const uint32_t endvx   = NY * (id + 1) / nthreads;
        for (uint32_t vx = startvx; vx < endvx; vx++) {
            small.matrixu(0);
            for (uint32_t ux = 0; ux < NX; ux++) {
                uint32_t uxyz            = ux << YZBITS;
                zbucket<ZBUCKETSIZE>& zb = TRIMONV ? buckets[ux][vx] : buckets[vx][ux];
                const uint8_t *readbig = zb.bytes, *endreadbig = readbig + zb.size;
                for (; readbig < endreadbig; readbig += SRCSIZE) {
                    // bit        43..37    36..22     21..15     14..0
                    // write      UYYYYY    UZZZZZ     VYYYYY     VZZZZ   within
                    // VX partition
                    const uint64_t e = *(uint64_t*) readbig & SRCSLOTMASK;
                    uxyz += ((uint32_t)(e >> YZBITS) - uxyz) & SRCPREFMASK;
                    const uint32_t vy = (e >> ZBITS) & YMASK;
                    // bit     43/39..37    36..30     29..15     14..0
                    // write      UXXXXX    UYYYYY     UZZZZZ     VZZZZ   within
                    // VX VY partition
                    *(uint64_t*) (small0 + small.index[vy]) = ((uint64_t) uxyz << ZBITS) | (e & ZMASK);
                    uxyz &= ~ZMASK;
                    small.index[vy] += DSTSIZE;
                }
                if (unlikely(uxyz >> YZBITS != ux)) {
                    spdlog::trace("OOPS3: id {} vx {} ux {} UXY {}\n", id, vx, ux, uxyz);
                    exit(0);
                }
            }
            uint8_t* degs = tdegs[id];
            small.storeu(tbuckets + id, 0);
            TRIMONV ? dst.matrixv(vx) : dst.matrixu(vx);
            for (uint32_t vy = 0; vy < NY; vy++) {
                const uint64_t vy34 = (uint64_t) vy << YZZBITS;
                memset(degs, 0xff, NZ);
                uint8_t *readsmall = tbuckets[id][vy].bytes, *endreadsmall = readsmall + tbuckets[id][vy].size;
                for (uint8_t* rdsmall = readsmall; rdsmall < endreadsmall; rdsmall += DSTSIZE)
                    degs[*(uint32_t*) rdsmall & ZMASK]++;
                uint32_t ux = 0;
                for (uint8_t* rdsmall = readsmall; rdsmall < endreadsmall; rdsmall += DSTSIZE) {
                    // bit        39..37    36..30     29..15     14..0 with
                    // XBITS==YBITS==7 read       UXXXXX    UYYYYY     UZZZZZ
                    // VZZZZ   within VX VY partition
                    const uint64_t e = *(uint64_t*) rdsmall & DSTSLOTMASK;
                    ux += ((uint32_t)(e >> YZZBITS) - ux) & DSTPREFMASK;
                    // bit       41/39..34    33..21     20..13     12..0
                    // write     VYYYYY       VZZZZZ     UYYYYY     UZZZZ
                    // within UX partition
                    *(uint64_t*) (base + dst.index[ux]) = vy34 | ((e & ZMASK) << YZBITS) | ((e >> ZBITS) & YZMASK);
                    dst.index[ux] += degs[e & ZMASK] ? DSTSIZE : 0;
                }
                if (unlikely(ux >> DSTPREFBITS != XMASK >> DSTPREFBITS)) {
                    spdlog::trace("OOPS4: id {} vx {} ux {} vs {}\n", id, vx, ux, XMASK);
                    exit(1);
                }
            }
            sumsize += TRIMONV ? dst.storev(buckets, vx) : dst.storeu(buckets, vx);
        }
        rdtsc1 = __rdtsc();
        if (showall || (!id && !(round & (round + 1))))
            spdlog::trace("trimedges id {} round {} size {} rdtsc: {}", id, round, sumsize / DSTSIZE, rdtsc1 - rdtsc0);
        tcounts[id] = sumsize / DSTSIZE;
    }

    template <uint32_t SRCSIZE, uint32_t DSTSIZE, bool TRIMONV>
    void trimrename(const uint32_t& id, const uint32_t& round) {
        const uint32_t SRCSLOTBITS  = std::min(SRCSIZE * 8, (TRIMONV ? YZBITS : YZ1BITS) + YZBITS);
        const uint64_t SRCSLOTMASK  = (1ULL << SRCSLOTBITS) - 1ULL;
        const uint32_t SRCPREFBITS  = SRCSLOTBITS - YZBITS;
        const uint32_t SRCPREFMASK  = (1 << SRCPREFBITS) - 1;
        const uint32_t SRCPREFBITS2 = SRCSLOTBITS - YZZBITS;
        const uint32_t SRCPREFMASK2 = (1 << SRCPREFBITS2) - 1;
        uint64_t rdtsc0, rdtsc1;
        indexer<ZBUCKETSIZE> dst;
        indexer<TBUCKETSIZE> small;
        uint32_t maxnnid = 0;

        rdtsc0                 = __rdtsc();
        offset_t sumsize       = 0;
        uint8_t const* base    = (uint8_t*) buckets;
        uint8_t const* small0  = (uint8_t*) tbuckets[id];
        const uint32_t startvx = NY * id / nthreads;
        const uint32_t endvx   = NY * (id + 1) / nthreads;
        for (uint32_t vx = startvx; vx < endvx; vx++) {
            barrier();
            small.matrixu(0);
            for (uint32_t ux = 0; ux < NX; ux++) {
                uint32_t uyz             = 0;
                zbucket<ZBUCKETSIZE>& zb = TRIMONV ? buckets[ux][vx] : buckets[vx][ux];
                const uint8_t *readbig = zb.bytes, *endreadbig = readbig + zb.size;
                for (; readbig < endreadbig; readbig += SRCSIZE) {
                    // bit        39..37    36..22     21..15     14..0
                    // write      UYYYYY    UZZZZZ     VYYYYY     VZZZZ   within
                    // VX partition  if TRIMONV
                    // bit            37...22     21..15     14..0
                    // write          VYYYZZ'     UYYYYY     UZZZZ   within UX
                    // partition  if !TRIMONV
                    const uint64_t e = *(uint64_t*) readbig & SRCSLOTMASK;
                    if (TRIMONV)
                        uyz += ((uint32_t)(e >> YZBITS) - uyz) & SRCPREFMASK;
                    else
                        uyz = e >> YZBITS;
                    const uint32_t vy = (e >> ZBITS) & YMASK;
                    // bit        39..37    36..30     29..15     14..0
                    // write      UXXXXX    UYYYYY     UZZZZZ     VZZZZ   within
                    // VX VY partition  if TRIMONV
                    // bit            37...31     30...15     14..0
                    // write          VXXXXXX     VYYYZZ'     UZZZZ   within UX
                    // UY partition  if !TRIMONV
                    *(uint64_t*) (small0 + small.index[vy]) =
                        ((uint64_t)(ux << (TRIMONV ? YZBITS : YZ1BITS) | uyz) << ZBITS) | (e & ZMASK);
                    if (TRIMONV)
                        uyz &= ~ZMASK;
                    small.index[vy] += SRCSIZE;
                }
            }
            uint16_t* degs = (uint16_t*) tdegs[id];
            small.storeu(tbuckets + id, 0);
            TRIMONV ? dst.matrixv(vx) : dst.matrixu(vx);
            uint32_t newnodeid   = 0;
            uint32_t* renames    = TRIMONV ? buckets[0][vx].renamev : buckets[vx][0].renameu;
            uint32_t* endrenames = renames + NZ1;
            for (uint32_t vy = 0; vy < NY; vy++) {
                assert(2 * NZ <= sizeof(zbucket8));
                memset(degs, 0xff, 2 * NZ); // sets each uint16_t entry to 0xffff
                uint8_t *readsmall = tbuckets[id][vy].bytes, *endreadsmall = readsmall + tbuckets[id][vy].size;
                for (uint8_t* rdsmall = readsmall; rdsmall < endreadsmall; rdsmall += SRCSIZE)
                    degs[*(uint32_t*) rdsmall & ZMASK]++;
                uint32_t ux       = 0;
                uint32_t nrenames = 0;
                for (uint8_t* rdsmall = readsmall; rdsmall < endreadsmall; rdsmall += SRCSIZE) {
                    // bit        39..37    36..30     29..15     14..0
                    // read       UXXXXX    UYYYYY     UZZZZZ     VZZZZ   within
                    // VX VY partition  if TRIMONV
                    // bit            37...31     30...15     14..0
                    // read           VXXXXXX     VYYYZZ'     UZZZZ   within UX
                    // UY partition  if !TRIMONV
                    const uint64_t e = *(uint64_t*) rdsmall & SRCSLOTMASK;
                    if (TRIMONV)
                        ux += ((uint32_t)(e >> YZZBITS) - ux) & SRCPREFMASK2;
                    else
                        ux = e >> YZZ1BITS;
                    const uint32_t vz = e & ZMASK;
                    uint16_t vdeg     = degs[vz];
                    if (vdeg) {
                        if (vdeg < 32) {
                            degs[vz] = vdeg = 32 + nrenames++;
                            *renames++      = vy << ZBITS | vz;
                            if (renames == endrenames) {
                                endrenames += (TRIMONV ? sizeof(yzbucket<ZBUCKETSIZE>) : sizeof(zbucket<ZBUCKETSIZE>)) /
                                              sizeof(uint32_t);
                                renames = endrenames - NZ1;
                            }
                        }
                        // bit       37..22     21..15     14..0
                        // write     VYYZZ'     UYYYYY     UZZZZ   within UX
                        // partition  if TRIMONV
                        if (TRIMONV)
                            *(uint64_t*) (base + dst.index[ux]) =
                                ((uint64_t)(newnodeid + vdeg - 32) << YZBITS) | ((e >> ZBITS) & YZMASK);
                        else
                            *(uint32_t*) (base + dst.index[ux]) =
                                ((newnodeid + vdeg - 32) << YZ1BITS) | ((e >> ZBITS) & YZ1MASK);
                        dst.index[ux] += DSTSIZE;
                    }
                }
                newnodeid += nrenames;
                if (TRIMONV && unlikely(ux >> SRCPREFBITS2 != XMASK >> SRCPREFBITS2)) {
                    spdlog::trace("OOPS6: id {} vx {} vy {} ux {} vs {}", id, vx, vy, ux, XMASK);
                    exit(0);
                }
            }
            if (newnodeid > maxnnid)
                maxnnid = newnodeid;
            sumsize += TRIMONV ? dst.storev(buckets, vx) : dst.storeu(buckets, vx);
        }
        rdtsc1 = __rdtsc();
        if (showall || !id)
            spdlog::trace("trimrename id {} round {} size {} rdtsc: {} maxnnid {}", id, round, sumsize / DSTSIZE,
                          rdtsc1 - rdtsc0, maxnnid);
        if (maxnnid >= NYZ1)
            spdlog::trace("maxnnid {} >= NYZ1 {}", maxnnid, NYZ1);
        assert(maxnnid < NYZ1);
        tcounts[id] = sumsize / DSTSIZE;
    }

    template <bool TRIMONV>
    void trimedges1(const uint32_t& id, const uint32_t& round) {
        uint64_t rdtsc0, rdtsc1;
        indexer<ZBUCKETSIZE> dst;

        rdtsc0                 = __rdtsc();
        offset_t sumsize       = 0;
        uint8_t* degs          = tdegs[id];
        uint8_t const* base    = (uint8_t*) buckets;
        const uint32_t startvx = NY * id / nthreads;
        const uint32_t endvx   = NY * (id + 1) / nthreads;
        for (uint32_t vx = startvx; vx < endvx; vx++) {
            TRIMONV ? dst.matrixv(vx) : dst.matrixu(vx);
            assert(NYZ1 <= sizeof(zbucket8));
            memset(degs, 0xff, NYZ1);
            for (uint32_t ux = 0; ux < NX; ux++) {
                zbucket<ZBUCKETSIZE>& zb = TRIMONV ? buckets[ux][vx] : buckets[vx][ux];
                uint32_t *readbig = zb.words, *endreadbig = readbig + zb.size / sizeof(uint32_t);
                for (; readbig < endreadbig; readbig++)
                    degs[*readbig & YZ1MASK]++;
            }
            for (uint32_t ux = 0; ux < NX; ux++) {
                zbucket<ZBUCKETSIZE>& zb = TRIMONV ? buckets[ux][vx] : buckets[vx][ux];
                uint32_t *readbig = zb.words, *endreadbig = readbig + zb.size / sizeof(uint32_t);
                for (; readbig < endreadbig; readbig++) {
                    // bit       31...16     15...0
                    // read      UYYZZZ'     VYYZZ'   within VX partition
                    const uint32_t e   = *readbig;
                    const uint32_t vyz = e & YZ1MASK;
                    // bit       31...16     15...0
                    // write     VYYZZZ'     UYYZZ'   within UX partition
                    *(uint32_t*) (base + dst.index[ux]) = (vyz << YZ1BITS) | (e >> YZ1BITS);
                    dst.index[ux] += degs[vyz] ? sizeof(uint32_t) : 0;
                }
            }
            sumsize += TRIMONV ? dst.storev(buckets, vx) : dst.storeu(buckets, vx);
        }
        rdtsc1 = __rdtsc();
        if (showall || (!id && !(round & (round + 1))))
            spdlog::trace("trimedges1 id {} round {} size {} rdtsc: {}", id, round, sumsize / sizeof(uint32_t),
                          rdtsc1 - rdtsc0);
        tcounts[id] = sumsize / sizeof(uint32_t);
    }

    template <bool TRIMONV>
    void trimrename1(const uint32_t& id, const uint32_t& round) {
        uint64_t rdtsc0, rdtsc1;
        indexer<ZBUCKETSIZE> dst;
        uint32_t maxnnid = 0;

        rdtsc0                 = __rdtsc();
        offset_t sumsize       = 0;
        uint16_t* degs         = (uint16_t*) tdegs[id];
        uint8_t const* base    = (uint8_t*) buckets;
        const uint32_t startvx = NY * id / nthreads;
        const uint32_t endvx   = NY * (id + 1) / nthreads;
        for (uint32_t vx = startvx; vx < endvx; vx++) {
            TRIMONV ? dst.matrixv(vx) : dst.matrixu(vx);
            memset(degs, 0xff, 2 * NYZ1); // sets each uint16_t entry to 0xffff
            for (uint32_t ux = 0; ux < NX; ux++) {
                zbucket<ZBUCKETSIZE>& zb = TRIMONV ? buckets[ux][vx] : buckets[vx][ux];
                uint32_t *readbig = zb.words, *endreadbig = readbig + zb.size / sizeof(uint32_t);
                for (; readbig < endreadbig; readbig++)
                    degs[*readbig & YZ1MASK]++;
            }
            uint32_t newnodeid   = 0;
            uint32_t* renames    = TRIMONV ? buckets[0][vx].renamev1 : buckets[vx][0].renameu1;
            uint32_t* endrenames = renames + NZ2;
            for (uint32_t ux = 0; ux < NX; ux++) {
                zbucket<ZBUCKETSIZE>& zb = TRIMONV ? buckets[ux][vx] : buckets[vx][ux];
                uint32_t *readbig = zb.words, *endreadbig = readbig + zb.size / sizeof(uint32_t);
                for (; readbig < endreadbig; readbig++) {
                    // bit       31...16     15...0
                    // read      UYYYZZ'     VYYZZ'   within VX partition
                    const uint32_t e   = *readbig;
                    const uint32_t vyz = e & YZ1MASK;
                    uint16_t vdeg      = degs[vyz];
                    if (vdeg) {
                        if (vdeg < 32) {
                            degs[vyz] = vdeg = 32 + newnodeid++;
                            *renames++       = vyz;
                            if (renames == endrenames) {
                                endrenames += (TRIMONV ? sizeof(yzbucket<ZBUCKETSIZE>) : sizeof(zbucket<ZBUCKETSIZE>)) /
                                              sizeof(uint32_t);
                                renames = endrenames - NZ2;
                                assert(renames < buckets[NX][0].renameu1);
                            }
                        }
                        // bit       26...16     15...0
                        // write     VYYZZZ"     UYYZZ'   within UX partition
                        *(uint32_t*) (base + dst.index[ux]) =
                            ((vdeg - 32) << (TRIMONV ? YZ1BITS : YZ2BITS)) | (e >> YZ1BITS);
                        dst.index[ux] += sizeof(uint32_t);
                    }
                }
            }
            if (newnodeid > maxnnid)
                maxnnid = newnodeid;
            sumsize += TRIMONV ? dst.storev(buckets, vx) : dst.storeu(buckets, vx);
        }
        rdtsc1 = __rdtsc();
        if (showall || !id)
            spdlog::trace("trimrename1 id {} round {} size {} rdtsc: {} maxnnid {}", id, round,
                          sumsize / sizeof(uint32_t), rdtsc1 - rdtsc0, maxnnid);
        if (maxnnid >= NYZ2)
            spdlog::trace("maxnnid {} >= NYZ2 {}", maxnnid, NYZ2);
        assert(maxnnid < NYZ2);
        tcounts[id] = sumsize / sizeof(uint32_t);
    }

    void trim();

    inline void abort() {
        barry.abort();
    }

    inline bool aborted() {
        return barry.aborted();
    }

    inline void barrier() {
        barry.wait();
    }

#ifdef EXPANDROUND
#define BIGGERSIZE BIGSIZE + 1
#else
#define BIGGERSIZE BIGSIZE
#define EXPANDROUND COMPRESSROUND
#endif
    void trimmer(uint32_t id);
};

#define NODEBITS (EDGEBITS + 1)

inline int nonce_cmp(const void* a, const void* b) {
    return *(uint32_t*) a - *(uint32_t*) b;
}

typedef word_t proof[PROOFSIZE];

// break circular reference with forward declaration
class CSolverCtx;

struct match_ctx {
    uint32_t id;
    pthread_t thread;
    CSolverCtx* solver;
};

class CSolverCtx {
public:
    CEdgeTrimmer trimmer;
    graph<word_t> cg;
    proof cycleus;
    proof cyclevs;
    std::bitset<NXY> uxymap;
    std::vector<word_t> sols; // concatanation of all proof's indices

    CSolverCtx() = default;
    CSolverCtx(uint32_t nthreads, uint32_t n_trims, bool allrounds);

#if NSIPHASH > 4
    /* ensure correct alignment for _mm256_load_si256
     * of sip_keys at start of trimmer struct
     */
    void* operator new(size_t size) noexcept {
        void* newobj;
        int tmp = posix_memalign(&newobj, NSIPHASH * sizeof(uint32_t), sizeof(CSolverCtx));
        if (tmp != 0)
            return nullptr;
        return newobj;
    }
#endif

    void setheader(const VStream& header);
    void setheader(const char* header, uint32_t len);
    uint64_t sharedbytes() const;
    uint32_t threadbytes() const;

    void recordedge(const uint32_t& i, const uint32_t& u1, const uint32_t& v2);

    void solution(const proof& sol);
    void findcycles();
    int solve();

    inline void abort() {
        trimmer.abort();
    }

    void* matchUnodes(match_ctx* mc);
};

#endif // __SRC_UTILS_CUCKAROO_MEAN_H__
