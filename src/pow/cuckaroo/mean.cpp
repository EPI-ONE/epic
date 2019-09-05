// Cuckaroo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2019 John Tromp

#include "mean.h"

#include <chrono>
#include <iostream>
#include <unistd.h>

void* etworker(void* vp) {
    thread_ctx* tp = (thread_ctx*) vp;
    tp->et->trimmer(tp->id);
    pthread_exit(NULL);
    return 0;
}

void* matchworker(void* vp) {
    match_ctx* tp = (match_ctx*) vp;
    tp->solver->matchUnodes(tp);
    pthread_exit(NULL);
    return 0;
}

/////////////////////////////// CEdgeTrimmer /////////////////////////////////////

CEdgeTrimmer::CEdgeTrimmer(const uint32_t& n_threads, const uint32_t& n_trims, bool show_all) : barry(n_threads) {
    assert(sizeof(matrix<ZBUCKETSIZE>) == NX * sizeof(yzbucket<ZBUCKETSIZE>));
    assert(sizeof(matrix<TBUCKETSIZE>) == NX * sizeof(yzbucket<TBUCKETSIZE>));
    nthreads = n_threads;
    ntrims   = n_trims;
    showall  = show_all;
    buckets  = new yzbucket<ZBUCKETSIZE>[NX];
    touch((uint8_t*) buckets, sizeof(matrix<ZBUCKETSIZE>));
    tbuckets = new yzbucket<TBUCKETSIZE>[nthreads];
    touch((uint8_t*) tbuckets, sizeof(yzbucket<TBUCKETSIZE>[nthreads]));
    tdegs   = new zbucket8[nthreads];
    tcounts = new offset_t[nthreads];
    threads = new thread_ctx[nthreads];
}

CEdgeTrimmer::~CEdgeTrimmer() {
    delete[] threads;
    delete[] buckets;
    delete[] tbuckets;
    delete[] tdegs;
    delete[] tcounts;
}

void CEdgeTrimmer::touch(uint8_t* p, const offset_t& n) {
    for (offset_t i = 0; i < n; i += 4096) {
        *(uint32_t*) (p + i) = 0;
    }
}

offset_t CEdgeTrimmer::count() const {
    offset_t cnt = 0;
    for (uint32_t t = 0; t < nthreads; t++) {
        cnt += tcounts[t];
    }
    return cnt;
}

void CEdgeTrimmer::genUVnodes(const uint32_t& id) {
    uint64_t rdtsc0, rdtsc1;
    const uint32_t NEBS = NSIPHASH * EDGE_BLOCK_SIZE;
    alignas(NSIPHASH > 4 ? 4 * NSIPHASH : 8) uint64_t buf[NEBS];

    rdtsc0              = __rdtsc();
    uint8_t const* base = (uint8_t*) buckets;
    indexer<ZBUCKETSIZE> dst;
    const uint32_t starty = NY * id / nthreads;
    const uint32_t endy   = NY * (id + 1) / nthreads;
    uint32_t edge0 = starty << YZBITS, endedge0 = edge0 + NYZ;
    offset_t sumsize = 0;
    for (uint32_t my = starty; my < endy; my++, endedge0 += NYZ) {
        dst.matrixv(my);
        for (; edge0 < endedge0; edge0 += NEBS) {
#if NSIPHASH == 1
            siphash_state<> shs(sip_keys);
            for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                shs.hash24(edge0 + e);
                buf[e] = shs.xor_lanes();
            }
#elif NSIPHASH == 4
            const uint64_t e1        = edge0;
            const __m128i vpacketinc = _mm_set1_epi64x(1);
            __m128i v0, v1, v2, v3, v4, v5, v6, v7;
            v7 = v3 = _mm_set1_epi64x(sip_keys.k3);
            v4 = v0 = _mm_set1_epi64x(sip_keys.k0);
            v5 = v1 = _mm_set1_epi64x(sip_keys.k1);
            v6 = v2          = _mm_set1_epi64x(sip_keys.k2);
            __m128i vpacket0 = _mm_set_epi64x(e1 + EDGE_BLOCK_SIZE, e1 + 0);
            __m128i vpacket1 = _mm_set_epi64x(e1 + 3 * EDGE_BLOCK_SIZE, e1 + 2 * EDGE_BLOCK_SIZE);
            for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                v3 = XOR(v3, vpacket0);
                v7 = XOR(v7, vpacket1);
                SIPROUNDX2N;
                SIPROUNDX2N;
                v0 = XOR(v0, vpacket0);
                v4 = XOR(v4, vpacket1);
                v2 = XOR(v2, _mm_set1_epi64x(0xffLL));
                v6 = XOR(v6, _mm_set1_epi64x(0xffLL));
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                vpacket0 = _mm_add_epi64(vpacket0, vpacketinc);
                vpacket1 = _mm_add_epi64(vpacket1, vpacketinc);
                _mm_store_si128((__m128i*) (buf + e), XOR(XOR(v0, v1), XOR(v2, v3)));
                _mm_store_si128((__m128i*) (buf + e + 2), XOR(XOR(v4, v5), XOR(v6, v7)));
            }
#elif NSIPHASH == 8
            const uint64_t e1        = edge0;
            const __m256i vinit      = _mm256_load_si256((__m256i*) &sip_keys);
            const __m256i vpacketinc = _mm256_set1_epi64x(1);
            __m256i v0, v1, v2, v3, v4, v5, v6, v7;
            v7 = v3 = _mm256_permute4x64_epi64(vinit, 0xFF);
            v4 = v0 = _mm256_permute4x64_epi64(vinit, 0x00);
            v5 = v1 = _mm256_permute4x64_epi64(vinit, 0x55);
            v6 = v2 = _mm256_permute4x64_epi64(vinit, 0xAA);
            __m256i vpacket0 =
                _mm256_set_epi64x(e1 + 3 * EDGE_BLOCK_SIZE, e1 + 2 * EDGE_BLOCK_SIZE, e1 + EDGE_BLOCK_SIZE, e1 + 0);
            __m256i vpacket1 = _mm256_set_epi64x(e1 + 7 * EDGE_BLOCK_SIZE, e1 + 6 * EDGE_BLOCK_SIZE,
                                                 e1 + 5 * EDGE_BLOCK_SIZE, e1 + 4 * EDGE_BLOCK_SIZE);
            for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                v3 = XOR(v3, vpacket0);
                v7 = XOR(v7, vpacket1);
                SIPROUNDX2N;
                SIPROUNDX2N;
                v0 = XOR(v0, vpacket0);
                v4 = XOR(v4, vpacket1);
                v2 = XOR(v2, _mm256_set1_epi64x(0xffLL));
                v6 = XOR(v6, _mm256_set1_epi64x(0xffLL));
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                vpacket0 = _mm256_add_epi64(vpacket0, vpacketinc);
                vpacket1 = _mm256_add_epi64(vpacket1, vpacketinc);
                _mm256_store_si256((__m256i*) (buf + e), XOR(XOR(v0, v1), XOR(v2, v3)));
                _mm256_store_si256((__m256i*) (buf + e + 4), XOR(XOR(v4, v5), XOR(v6, v7)));
            }
#endif
            for (uint32_t ns = 0; ns < NSIPHASH; ns++) {
                const uint64_t buflast    = buf[NEBS - NSIPHASH + ns];
                buf[NEBS - NSIPHASH + ns] = 0;
                for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                    const uint64_t nodes = buf[e + ns] ^ buflast;
                    const uint32_t node0 = nodes & EDGEMASK;
                    const uint32_t node1 = (nodes >> 32) & EDGEMASK;
                    const uint32_t ux    = node0 >> YZBITS;
                    // bit        50...22     21..15    14..0
                    // write      VXXYYZZ     UYYYYY    UZZZZ
                    *(BIGTYPE0*) (base + dst.index[ux]) = (BIGTYPE0) node1 << YZBITS | (node0 & YZMASK);
                    dst.index[ux] += BIGSIZE0;
                }
            }
        }
        sumsize += dst.storev(buckets, my);
    }
    rdtsc1 = __rdtsc();
    if (!id)
        spdlog::trace("genUVnodes size {} rdtsc: {}", sumsize / BIGSIZE0, rdtsc1 - rdtsc0);
    tcounts[id] = sumsize / BIGSIZE0;
}

void CEdgeTrimmer::sortVnodes(const uint32_t& id, const uint32_t& uorv) {
    uint64_t rdtsc0, rdtsc1;
    indexer<ZBUCKETSIZE> dst;
    indexer<TBUCKETSIZE> small;

    rdtsc0                 = __rdtsc();
    offset_t sumsize       = 0;
    uint8_t const* base    = (uint8_t*) buckets;
    uint8_t const* small0  = (uint8_t*) tbuckets[id];
    const uint32_t startux = NX * id / nthreads;
    const uint32_t endux   = NX * (id + 1) / nthreads;
    for (uint32_t ux = startux; ux < endux; ux++) { // matrix x == ux
        small.matrixu(0);
        for (uint32_t my = 0; my < NY; my++) {
            uint8_t* readbig          = buckets[ux][my].bytes;
            uint8_t const* endreadbig = readbig + buckets[ux][my].size;
            for (; readbig < endreadbig; readbig += BIGSIZE0) {
                // bit        50...22     21..15    14..0
                // write      VXXYYZZ     UYYYYY    UZZZZ   within UX
                // partition
                BIGTYPE0 e = *(BIGTYPE0*) readbig;
                e &= BIGSLOTMASK0;
                uint32_t vxyz     = e >> YZBITS;
                const uint32_t uy = (e >> ZBITS) & YMASK;
                // bit         43...15     14..0
                // write       VXXYYZZ     UZZZZ   within UX UY partition
                *(uint64_t*) (small0 + small.index[uy]) = ((uint64_t) vxyz << ZBITS) | (e & ZMASK);
                small.index[uy] += SMALLSIZE;
            }
        }
        uint8_t* degs = tdegs[id];
        small.storeu(tbuckets + id, 0);
        dst.matrixu(ux);
        for (uint32_t uy = 0; uy < NY; uy++) {
            assert(NZ <= sizeof(zbucket8));
            memset(degs, 0xff, NZ);
            uint8_t *readsmall = tbuckets[id][uy].bytes, *endreadsmall = readsmall + tbuckets[id][uy].size;
            for (uint8_t* rdsmall = readsmall; rdsmall < endreadsmall; rdsmall += SMALLSIZE)
                degs[*(uint32_t*) rdsmall & ZMASK]++;

            uint64_t uy37 = (int64_t) uy << YZZBITS;
            for (uint8_t* rdsmall = readsmall; rdsmall < endreadsmall; rdsmall += SMALLSIZE) {
                // bit         43...15     14..0
                // write       VXXYYZZ     UZZZZ   within UX UY partition
                const uint64_t e  = *(uint64_t*) rdsmall;
                const uint32_t vx = (e >> YZZBITS) & XMASK;
                const uint32_t uz = e & ZMASK;
                // bit     43/39..37    36..22     21..15     14..0
                // write      UYYYYY    UZZZZZ     VYYYYY     VZZZZ   within
                // UX VX partition
                *(uint64_t*) (base + dst.index[vx]) = uy37 | ((uint64_t) uz << YZBITS) | ((e >> ZBITS) & YZMASK);
                dst.index[vx] += degs[uz] ? BIGSIZE : 0;
            }
        }
        sumsize += dst.storeu(buckets, ux);
    }
    rdtsc1 = __rdtsc();
    if (!id)
        spdlog::trace("sortVnodes round {} size {} rdtsc: {}", uorv, sumsize / BIGSIZE, rdtsc1 - rdtsc0);
    tcounts[id] = sumsize / BIGSIZE;
}

void CEdgeTrimmer::trim() {
    barry.clear();
    for (uint32_t t = 0; t < nthreads; t++) {
        threads[t].id = t;
        threads[t].et = this;
        int err       = pthread_create(&threads[t].thread, NULL, etworker, (void*) &threads[t]);
        assert(err == 0);
    }
    for (uint32_t t = 0; t < nthreads; t++) {
        int err = pthread_join(threads[t].thread, NULL);
        assert(err == 0);
    }
}

void CEdgeTrimmer::trimmer(uint32_t id) {
    genUVnodes(id);
    barrier();
    sortVnodes(id, 1);
    for (uint32_t round = 2; round < ntrims - 2; round += 2) {
        if (aborted()) {
            return;
        }
        barrier();

        if (round < COMPRESSROUND) {
            if (round < EXPANDROUND)
                trimedges<BIGSIZE, BIGSIZE, true>(id, round);
            else if (round == EXPANDROUND)
                trimedges<BIGSIZE, BIGGERSIZE, true>(id, round);
            else
                trimedges<BIGGERSIZE, BIGGERSIZE, true>(id, round);
        } else if (round == COMPRESSROUND) {
            trimrename<BIGGERSIZE, BIGGERSIZE, true>(id, round);
        } else
            trimedges1<true>(id, round);

        if (aborted()) {
            return;
        }
        barrier();

        if (round < COMPRESSROUND) {
            if (round + 1 < EXPANDROUND)
                trimedges<BIGSIZE, BIGSIZE, false>(id, round + 1);
            else if (round + 1 == EXPANDROUND)
                trimedges<BIGSIZE, BIGGERSIZE, false>(id, round + 1);
            else
                trimedges<BIGGERSIZE, BIGGERSIZE, false>(id, round + 1);
        } else if (round == COMPRESSROUND) {
            trimrename<BIGGERSIZE, sizeof(uint32_t), false>(id, round + 1);
        } else
            trimedges1<false>(id, round + 1);
    }

    if (aborted()) {
        return;
    }
    barrier();

    trimrename1<true>(id, ntrims - 2);

    if (aborted()) {
        return;
    }
    barrier();

    trimrename1<false>(id, ntrims - 1);
}

//////////////////////////// end of CEdgeTrimmer /////////////////////////////////

//////////////////////////////// CSolverCtx //////////////////////////////////////

CSolverCtx::CSolverCtx(uint32_t nthreads, uint32_t n_trims, bool allrounds)
    : trimmer(nthreads, n_trims, allrounds), cg(MAXEDGES, MAXEDGES, MAXSOLS, (char*) trimmer.tbuckets) {
    assert(cg.bytes() <= sizeof(yzbucket<TBUCKETSIZE>[nthreads])); // check that graph cg can
                                                                   // fit in tbucket's memory
}

void CSolverCtx::setheader(const char* header, uint32_t len) {
    ::setheader(header, len, &trimmer.sip_keys);
    sols.clear();
}

void CSolverCtx::setheader(const VStream& header) {
    setheader(header.data(), header.size());
}

uint64_t CSolverCtx::sharedbytes() const {
    return sizeof(matrix<ZBUCKETSIZE>);
}

uint32_t CSolverCtx::threadbytes() const {
    return sizeof(thread_ctx) + sizeof(yzbucket<TBUCKETSIZE>) + sizeof(zbucket8) + sizeof(zbucket16) +
           sizeof(zbucket32);
}

void CSolverCtx::recordedge(const uint32_t& i, const uint32_t& u1, const uint32_t& v2) {
    const uint32_t ux = u1 >> YZ2BITS;
    uint32_t uyz      = trimmer.buckets[ux][(u1 >> Z2BITS) & YMASK].renameu1[u1 & Z2MASK];
    const uint32_t v1 = v2 - MAXEDGES;
    const uint32_t vx = v1 >> YZ2BITS;
    uint32_t vyz      = trimmer.buckets[(v1 >> Z2BITS) & YMASK][vx].renamev1[v1 & Z2MASK];
#if COMPRESSROUND > 0
    uyz = trimmer.buckets[ux][uyz >> Z1BITS].renameu[uyz & Z1MASK];
    vyz = trimmer.buckets[vyz >> Z1BITS][vx].renamev[vyz & Z1MASK];
#endif
    const uint32_t u = cycleus[i] = (ux << YZBITS) | uyz;
    cyclevs[i]                    = (vx << YZBITS) | vyz;
    uxymap[u >> ZBITS]            = 1;
}

void CSolverCtx::solution(const proof& sol) {
    for (uint32_t i = 0; i < PROOFSIZE; i++)
        recordedge(i, cg.links[2 * sol[i]].to, cg.links[2 * sol[i] + 1].to);
    sols.resize(sols.size() + PROOFSIZE);
    match_ctx* threads = new match_ctx[trimmer.nthreads];
    for (uint32_t t = 0; t < trimmer.nthreads; t++) {
        threads[t].id     = t;
        threads[t].solver = this;
        int err           = pthread_create(&threads[t].thread, NULL, matchworker, (void*) &threads[t]);
        assert(err == 0);
    }
    for (uint32_t t = 0; t < trimmer.nthreads; t++) {
        int err = pthread_join(threads[t].thread, NULL);
        assert(err == 0);
    }
    delete[] threads;
    qsort(&sols[sols.size() - PROOFSIZE], PROOFSIZE, sizeof(uint32_t), nonce_cmp);
}

void CSolverCtx::findcycles() {
    uint64_t rdtsc0, rdtsc1;

    rdtsc0 = __rdtsc();
    cg.reset();
    for (uint32_t vx = 0; vx < NX; vx++) {
        for (uint32_t ux = 0; ux < NX; ux++) {
            zbucket<ZBUCKETSIZE>& zb = trimmer.buckets[ux][vx];
            uint32_t *readbig = zb.words, *endreadbig = readbig + zb.size / sizeof(uint32_t);
            for (; readbig < endreadbig; readbig++) {
                // bit        21..11     10...0
                // write      UYYZZZ'    VYYZZ'   within VX partition
                const uint32_t e = *readbig;
                const uint32_t u = (ux << YZ2BITS) | (e >> YZ2BITS);
                const uint32_t v = (vx << YZ2BITS) | (e & YZ2MASK);
                cg.add_edge(u, v);
            }
        }
    }
    for (uint32_t s = 0; s < cg.nsols; s++) {
        solution(cg.sols[s]);
    }
    rdtsc1 = __rdtsc();
    spdlog::trace("findcycles rdtsc: {}", rdtsc1 - rdtsc0);
}

int CSolverCtx::solve() {
    trimmer.trim();
    if (!trimmer.aborted()) {
        findcycles();
    }
    return sols.size() / PROOFSIZE;
}

void* CSolverCtx::matchUnodes(match_ctx* mc) {
    uint64_t rdtsc0, rdtsc1;
    const uint32_t NEBS = NSIPHASH * EDGE_BLOCK_SIZE;
    alignas(NSIPHASH > 4 ? 4 * NSIPHASH : 8) uint64_t buf[NEBS];

    rdtsc0                = __rdtsc();
    const uint32_t starty = NY * mc->id / trimmer.nthreads;
    const uint32_t endy   = NY * (mc->id + 1) / trimmer.nthreads;
    uint32_t edge0 = starty << YZBITS, endedge0 = edge0 + NYZ;
    for (uint32_t my = starty; my < endy; my++, endedge0 += NYZ) {
        for (; edge0 < endedge0; edge0 += NEBS) {
#if NSIPHASH == 1
            siphash_state<> shs(trimmer.sip_keys);
            for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                shs.hash24(edge0 + e);
                buf[e] = shs.xor_lanes();
            }
#elif NSIPHASH == 4
            const uint64_t e1        = edge0;
            const __m128i vpacketinc = _mm_set1_epi64x(1);
            __m128i v0, v1, v2, v3, v4, v5, v6, v7;
            v7 = v3 = _mm_set1_epi64x(trimmer.sip_keys.k3);
            v4 = v0 = _mm_set1_epi64x(trimmer.sip_keys.k0);
            v5 = v1 = _mm_set1_epi64x(trimmer.sip_keys.k1);
            v6 = v2          = _mm_set1_epi64x(trimmer.sip_keys.k2);
            __m128i vpacket0 = _mm_set_epi64x(e1 + EDGE_BLOCK_SIZE, e1 + 0);
            __m128i vpacket1 = _mm_set_epi64x(e1 + 3 * EDGE_BLOCK_SIZE, e1 + 2 * EDGE_BLOCK_SIZE);
            for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                v3 = XOR(v3, vpacket0);
                v7 = XOR(v7, vpacket1);
                SIPROUNDX2N;
                SIPROUNDX2N;
                v0 = XOR(v0, vpacket0);
                v4 = XOR(v4, vpacket1);
                v2 = XOR(v2, _mm_set1_epi64x(0xffLL));
                v6 = XOR(v6, _mm_set1_epi64x(0xffLL));
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                vpacket0 = _mm_add_epi64(vpacket0, vpacketinc);
                vpacket1 = _mm_add_epi64(vpacket1, vpacketinc);
                _mm_store_si128((__m128i*) (buf + e), XOR(XOR(v0, v1), XOR(v2, v3)));
                _mm_store_si128((__m128i*) (buf + e + 2), XOR(XOR(v4, v5), XOR(v6, v7)));
            }
#elif NSIPHASH == 8
            const uint64_t e1        = edge0;
            const __m256i vinit      = _mm256_load_si256((__m256i*) &trimmer.sip_keys);
            const __m256i vpacketinc = _mm256_set1_epi64x(1);
            __m256i v0, v1, v2, v3, v4, v5, v6, v7;
            v7 = v3 = _mm256_permute4x64_epi64(vinit, 0xFF);
            v4 = v0 = _mm256_permute4x64_epi64(vinit, 0x00);
            v5 = v1 = _mm256_permute4x64_epi64(vinit, 0x55);
            v6 = v2 = _mm256_permute4x64_epi64(vinit, 0xAA);
            __m256i vpacket0 =
                _mm256_set_epi64x(e1 + 3 * EDGE_BLOCK_SIZE, e1 + 2 * EDGE_BLOCK_SIZE, e1 + EDGE_BLOCK_SIZE, e1 + 0);
            __m256i vpacket1 = _mm256_set_epi64x(e1 + 7 * EDGE_BLOCK_SIZE, e1 + 6 * EDGE_BLOCK_SIZE,
                                                 e1 + 5 * EDGE_BLOCK_SIZE, e1 + 4 * EDGE_BLOCK_SIZE);
            for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                v3 = XOR(v3, vpacket0);
                v7 = XOR(v7, vpacket1);
                SIPROUNDX2N;
                SIPROUNDX2N;
                v0 = XOR(v0, vpacket0);
                v4 = XOR(v4, vpacket1);
                v2 = XOR(v2, _mm256_set1_epi64x(0xffLL));
                v6 = XOR(v6, _mm256_set1_epi64x(0xffLL));
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                SIPROUNDX2N;
                vpacket0 = _mm256_add_epi64(vpacket0, vpacketinc);
                vpacket1 = _mm256_add_epi64(vpacket1, vpacketinc);
                _mm256_store_si256((__m256i*) (buf + e), XOR(XOR(v0, v1), XOR(v2, v3)));
                _mm256_store_si256((__m256i*) (buf + e + 4), XOR(XOR(v4, v5), XOR(v6, v7)));
            }
#endif
            for (uint32_t ns = 0; ns < NSIPHASH; ns++) {
                const uint64_t buflast    = buf[NEBS - NSIPHASH + ns];
                buf[NEBS - NSIPHASH + ns] = 0;
                for (uint32_t e = 0; e < NEBS; e += NSIPHASH) {
                    const uint64_t nodes = buf[e + ns] ^ buflast;
                    const uint32_t node0 = nodes & EDGEMASK;
                    const uint32_t node1 = (nodes >> 32) & EDGEMASK;
                    if (uxymap[node0 >> ZBITS]) {
                        for (uint32_t j = 0; j < PROOFSIZE; j++) {
                            if (cycleus[j] == node0 && cyclevs[j] == node1) {
                                sols[sols.size() - PROOFSIZE + j] = edge0 + ns * EDGE_BLOCK_SIZE + e / NSIPHASH;
                            }
                        }
                    }
                }
            }
        }
    }
    rdtsc1 = __rdtsc();
    if (trimmer.showall || !mc->id)
        spdlog::trace("matchUnodes id {} rdtsc: {}", mc->id, rdtsc1 - rdtsc0);
    return 0;
}

///////////////////////////// end of CSolverCtx //////////////////////////////////
