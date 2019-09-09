// Cuckaroo Cycle, a memory-hard proof-of-work by John Tromp
// Copyright (c) 2018-2019 Jiri Vadura (photon) and John Tromp
// This software is covered by the FAIR MINING license

#include "mean.cuh"

#ifndef MAXSOLS
#define MAXSOLS 4
#endif

#ifndef XBITS
#define XBITS 6
#endif

const uint32_t NX     = 1 << XBITS;
const uint32_t NX2    = NX * NX;
const uint32_t XMASK  = NX - 1;
const uint32_t YBITS  = XBITS;
const uint32_t NY     = 1 << YBITS;
const uint32_t YZBITS = EDGEBITS - XBITS;
const uint32_t ZBITS  = YZBITS - YBITS;
const uint32_t NZ     = 1 << ZBITS;
const uint32_t ZMASK  = NZ - 1;

#ifndef IDXSHIFT
// number of bits of compression of surviving edge endpoints
// reduces space used in cycle finding, but too high a value
// results in NODE OVERFLOW warnings and fake cycles
#define IDXSHIFT 12
#endif

const uint32_t MAXEDGES = NEDGES >> IDXSHIFT;

#ifndef NEPS_A
#define NEPS_A 133
#endif
#ifndef NEPS_B
#define NEPS_B 88
#endif
#define NEPS 128

// Number of Parts of BufferB, all but one of which will overlap BufferA
#ifndef NB
#define NB 2
#endif

#ifndef NA
#define NA ((NB * NEPS_A + NEPS_B - 1) / NEPS_B)
#endif

const uint32_t EDGES_A = NZ * NEPS_A / NEPS;
const uint32_t EDGES_B = NZ * NEPS_B / NEPS;

const uint32_t ROW_EDGES_A = EDGES_A * NY;
const uint32_t ROW_EDGES_B = EDGES_B * NY;

__constant__ uint2 recoveredges[16];
__constant__ uint2 e0 = {0, 0};

__device__ uint64_t dipblock(const siphash_keys& keys, const word_t edge, uint64_t* buf) {
    diphash_state<> shs(keys);
    word_t edge0 = edge & ~EDGE_BLOCK_MASK;
    uint32_t i;
    for (i = 0; i < EDGE_BLOCK_MASK; i++) {
        shs.hash24(edge0 + i);
        buf[i] = shs.xor_lanes();
    }
    shs.hash24(edge0 + i);
    buf[i] = 0;
    return shs.xor_lanes();
}

__device__ uint32_t endpoint(uint2 nodes, int uorv) {
    return uorv ? nodes.y : nodes.x;
}

#ifndef FLUSHA // should perhaps be in trimparams and passed as template
               // parameter
#define FLUSHA 16
#endif

template <int maxOut>
__global__ void SeedA(const siphash_keys& sipkeys, ulonglong4* __restrict__ buffer, uint32_t* __restrict__ indexes) {
    const int group    = blockIdx.x;
    const int dim      = blockDim.x;
    const int lid      = threadIdx.x;
    const int gid      = group * dim + lid;
    const int nthreads = gridDim.x * dim;
    const int FLUSHA2  = 2 * FLUSHA;

    __shared__ uint2 tmp[NX][FLUSHA2]; // needs to be ulonglong4 aligned
    const int TMPPERLL4 = sizeof(ulonglong4) / sizeof(uint2);
    __shared__ int counters[NX];
    uint64_t buf[EDGE_BLOCK_SIZE];

    for (int row = lid; row < NX; row += dim)
        counters[row] = 0;
    __syncthreads();

    const int col   = group % NX;
    const int loops = NEDGES / nthreads; // assuming THREADS_HAVE_EDGES checked
    for (int blk = 0; blk < loops; blk += EDGE_BLOCK_SIZE) {
        uint32_t nonce0     = gid * loops + blk;
        const uint64_t last = dipblock(sipkeys, nonce0, buf);
        for (uint32_t e = 0; e < EDGE_BLOCK_SIZE; e++) {
            uint64_t edge     = buf[e] ^ last;
            uint32_t node0    = edge & EDGEMASK;
            uint32_t node1    = (edge >> 32) & EDGEMASK;
            int row           = node0 >> YZBITS;
            int counter       = min((int) atomicAdd(counters + row, 1),
                              (int) (FLUSHA2 - 1)); // assuming ROWS_LIMIT_LOSSES checked
            tmp[row][counter] = make_uint2(node0, node1);
            __syncthreads();
            if (counter == FLUSHA - 1) {
                int localIdx = min(FLUSHA2, counters[row]);
                int newCount = localIdx % FLUSHA;
                int nflush   = localIdx - newCount;
                uint32_t grp = row * NX + col;
                int cnt      = min((int) atomicAdd(indexes + grp, nflush), (int) (maxOut - nflush));
                for (int i = 0; i < nflush; i += TMPPERLL4)
                    buffer[((uint64_t) grp * maxOut + cnt + i) / TMPPERLL4] = *(ulonglong4*) (&tmp[row][i]);
                for (int t = 0; t < newCount; t++) {
                    tmp[row][t] = tmp[row][t + nflush];
                }
                counters[row] = newCount;
            }
            __syncthreads();
        }
    }
    uint2 zero = make_uint2(0, 0);
    for (int row = lid; row < NX; row += dim) {
        int localIdx = min(FLUSHA2, counters[row]);
        uint32_t grp = row * NX + col;
        for (int j = localIdx; j % TMPPERLL4; j++)
            tmp[row][j] = zero;
        for (int i = 0; i < localIdx; i += TMPPERLL4) {
            int cnt = min((int) atomicAdd(indexes + grp, TMPPERLL4), (int) (maxOut - TMPPERLL4));
            buffer[((uint64_t) grp * maxOut + cnt) / TMPPERLL4] = *(ulonglong4*) (&tmp[row][i]);
        }
    }
}

template <typename Edge>
__device__ bool null(Edge e);

__device__ bool null(uint32_t nonce) {
    return nonce == 0;
}

__device__ bool null(uint2 nodes) {
    return nodes.x == 0 && nodes.y == 0;
}

#ifndef FLUSHB
#define FLUSHB 8
#endif

template <typename T>
__device__ __forceinline__ T ldg(const T* ptr) {
#if __CUDA_ARCH__ >= 350
    return __ldg(ptr);
#else
    return *ptr;
#endif
}

template <int maxOut>
__global__ void SeedB(const uint2* __restrict__ source,
                      ulonglong4* __restrict__ destination,
                      const uint32_t* __restrict__ srcIdx,
                      uint32_t* __restrict__ dstIdx) {
    const int group   = blockIdx.x;
    const int dim     = blockDim.x;
    const int lid     = threadIdx.x;
    const int FLUSHB2 = 2 * FLUSHB;

    __shared__ uint2 tmp[NX][FLUSHB2];
    const int TMPPERLL4 = sizeof(ulonglong4) / sizeof(uint2);
    __shared__ int counters[NX];

    for (int col = lid; col < NX; col += dim)
        counters[col] = 0;
    __syncthreads();
    const int row         = group / NX;
    const int bucketEdges = min((int) srcIdx[group], (int) maxOut);
    const int loops       = (bucketEdges + dim - 1) / dim;
    for (int loop = 0; loop < loops; loop++) {
        int col;
        int counter         = 0;
        const int edgeIndex = loop * dim + lid;
        if (edgeIndex < bucketEdges) {
            const int index = group * maxOut + edgeIndex;
            uint2 edge      = ldg(&source[index]);
            if (!null(edge)) {
                uint32_t node1    = edge.x;
                col               = (node1 >> ZBITS) & XMASK;
                counter           = min((int) atomicAdd(counters + col, 1),
                              (int) (FLUSHB2 - 1)); // assuming COLS_LIMIT_LOSSES checked
                tmp[col][counter] = edge;
            }
        }
        __syncthreads();
        if (counter == FLUSHB - 1) {
            int localIdx = min(FLUSHB2, counters[col]);
            int newCount = localIdx % FLUSHB;
            int nflush   = localIdx - newCount;
            uint32_t grp = row * NX + col;
#ifdef SYNCBUG
            if (grp == 0x2d6)
                printf("group %x size %d lid %d nflush %d\n", group, bucketEdges, lid, nflush);
#endif
            int cnt = min((int) atomicAdd(dstIdx + grp, nflush), (int) (maxOut - nflush));
            for (int i = 0; i < nflush; i += TMPPERLL4)
                destination[((uint64_t) grp * maxOut + cnt + i) / TMPPERLL4] = *(ulonglong4*) (&tmp[col][i]);
            for (int t = 0; t < newCount; t++) {
                tmp[col][t] = tmp[col][t + nflush];
            }
            counters[col] = newCount;
        }
        __syncthreads();
    }
    uint2 zero = make_uint2(0, 0);
    for (int col = lid; col < NX; col += dim) {
        int localIdx = min(FLUSHB2, counters[col]);
        uint32_t grp = row * NX + col;
#ifdef SYNCBUG
        if (group == 0x2f2 && grp == 0x2d6)
            printf("group %x size %d lid %d localIdx %d\n", group, bucketEdges, lid, localIdx);
#endif
        for (int j = localIdx; j % TMPPERLL4; j++)
            tmp[col][j] = zero;
        for (int i = 0; i < localIdx; i += TMPPERLL4) {
            int cnt = min((int) atomicAdd(dstIdx + grp, TMPPERLL4), (int) (maxOut - TMPPERLL4));
            destination[((uint64_t) grp * maxOut + cnt) / TMPPERLL4] = *(ulonglong4*) (&tmp[col][i]);
        }
    }
}

__device__ __forceinline__ void Increase2bCounter(uint32_t* ecounters, const int bucket) {
    int word          = bucket >> 5;
    unsigned char bit = bucket & 0x1F;
    uint32_t mask     = 1 << bit;

    uint32_t old = atomicOr(ecounters + word, mask) & mask;
    if (old)
        atomicOr(ecounters + word + NZ / 32, mask);
}

__device__ __forceinline__ bool Read2bCounter(uint32_t* ecounters, const int bucket) {
    int word          = bucket >> 5;
    unsigned char bit = bucket & 0x1F;

    return (ecounters[word + NZ / 32] >> bit) & 1;
}

template <int NP, int maxIn, int maxOut>
__global__ void Round(const int round,
                      const uint2* __restrict__ src,
                      uint2* __restrict__ dst,
                      const uint32_t* __restrict__ srcIdx,
                      uint32_t* __restrict__ dstIdx) {
    const int group        = blockIdx.x;
    const int dim          = blockDim.x;
    const int lid          = threadIdx.x;
    const int COUNTERWORDS = NZ / 16; // 16 2-bit counters per 32-bit word

    __shared__ uint32_t ecounters[COUNTERWORDS];

    for (int i = lid; i < COUNTERWORDS; i += dim)
        ecounters[i] = 0;
    __syncthreads();

    for (int i = 0; i < NP; i++, src += NX2 * maxIn, srcIdx += NX2) {
        const int edgesInBucket = min(srcIdx[group], maxIn);
        const int loops         = (edgesInBucket + dim - 1) / dim;

        for (int loop = 0; loop < loops; loop++) {
            const int lindex = loop * dim + lid;
            if (lindex < edgesInBucket) {
                const int index = maxIn * group + lindex;
                uint2 edge      = ldg(&src[index]);
                if (null(edge))
                    continue;
                uint32_t node = endpoint(edge, round & 1);
                Increase2bCounter(ecounters, node & ZMASK);
            }
        }
    }

    __syncthreads();

    src -= NP * NX2 * maxIn;
    srcIdx -= NP * NX2;
    for (int i = 0; i < NP; i++, src += NX2 * maxIn, srcIdx += NX2) {
        const int edgesInBucket = min(srcIdx[group], maxIn);
        const int loops         = (edgesInBucket + dim - 1) / dim;
        for (int loop = 0; loop < loops; loop++) {
            const int lindex = loop * dim + lid;
            if (lindex < edgesInBucket) {
                const int index = maxIn * group + lindex;
                uint2 edge      = ldg(&src[index]);
                if (null(edge))
                    continue;
                uint32_t node0 = endpoint(edge, round & 1);
                if (Read2bCounter(ecounters, node0 & ZMASK)) {
                    uint32_t node1                = endpoint(edge, (round & 1) ^ 1);
                    const int bucket              = node1 >> ZBITS;
                    const int bktIdx              = min(atomicAdd(dstIdx + bucket, 1), maxOut - 1);
                    dst[bucket * maxOut + bktIdx] = (round & 1) ? make_uint2(node1, node0) : make_uint2(node0, node1);
                }
            }
        }
    }
}

template <int maxIn>
__global__ void Tail(const uint2* source, uint2* destination, const uint32_t* srcIdx, uint32_t* dstIdx) {
    const int lid   = threadIdx.x;
    const int group = blockIdx.x;
    const int dim   = blockDim.x;
    int myEdges     = srcIdx[group];
    __shared__ int destIdx;

    if (lid == 0)
        destIdx = atomicAdd(dstIdx, myEdges);
    __syncthreads();
    for (int i = lid; i < myEdges; i += dim)
        destination[destIdx + lid] = source[group * maxIn + lid];
}

__global__ void Recovery(const siphash_keys& sipkeys, ulonglong4* buffer, int* indexes, int proofsize) {
    const int gid      = blockDim.x * blockIdx.x + threadIdx.x;
    const int lid      = threadIdx.x;
    const int nthreads = blockDim.x * gridDim.x;
    const int loops    = NEDGES / nthreads;
    __shared__ uint32_t nonces[4];
    uint64_t buf[EDGE_BLOCK_SIZE];

    if (lid < proofsize) {
        nonces[lid] = 0;
    }
    __syncthreads();
    for (int blk = 0; blk < loops; blk += EDGE_BLOCK_SIZE) {
        uint32_t nonce0     = gid * loops + blk;
        const uint64_t last = dipblock(sipkeys, nonce0, buf);
        for (int i = 0; i < EDGE_BLOCK_SIZE; i++) {
            uint64_t edge = buf[i] ^ last;
            uint32_t u    = edge & EDGEMASK;
            uint32_t v    = (edge >> 32) & EDGEMASK;
            for (int p = 0; p < proofsize; p++) { // YO
                if (recoveredges[p].x == u && recoveredges[p].y == v) {
                    nonces[p] = nonce0 + i;
                }
            }
        }
    }
    __syncthreads();
    if (lid < proofsize) {
        if (nonces[lid] > 0) {
            indexes[lid] = nonces[lid];
        }
    }
}

int gpuAssert(cudaError_t code, char* file, int line, bool abort) {
    int device_id;
    cudaGetDevice(&device_id);
    if (code != cudaSuccess) {
        spdlog::error("Device {} GPUassert({}): {} {} {}", device_id, code, cudaGetErrorString(code), file, line);

        cudaDeviceReset();
        if (abort)
            exit(code);
    }
    return code;
}

trimparams::trimparams() {
    ntrims         = 176;
    genA.blocks    = 4096;
    genA.tpb       = 256;
    genB.blocks    = NX2;
    genB.tpb       = 128;
    trim.blocks    = NX2;
    trim.tpb       = 512;
    tail.blocks    = NX2;
    tail.tpb       = 1024;
    recover.blocks = 1024;
    recover.tpb    = 1024;
}

GEdgeTrimmer::GEdgeTrimmer(const trimparams _tp)
    : tp(_tp), indexesSize(NX * NY * sizeof(uint32_t)), indexesE(new uint32_t*[1 + NB]) {
    checkCudaErrors_V(cudaMalloc((void**) &dt, sizeof(GEdgeTrimmer)));
    checkCudaErrors_V(cudaMalloc((void**) &uvnodes, CYCLELEN * 2 * sizeof(uint32_t)));
    checkCudaErrors_V(cudaMalloc((void**) &dipkeys, sizeof(siphash_keys)));
    for (int i = 0; i < 1 + NB; i++) {
        checkCudaErrors_V(cudaMalloc((void**) &indexesE[i], indexesSize));
    }
    sizeA                   = ROW_EDGES_A * NX * sizeof(uint2);
    sizeB                   = ROW_EDGES_B * NX * sizeof(uint2);
    const size_t bufferSize = sizeA + sizeB / NB;
    assert(bufferSize >= sizeB + sizeB / NB / 2); // ensure enough space for Round 1
    checkCudaErrors_V(cudaMalloc((void**) &bufferA, bufferSize));
    bufferAB = bufferA + sizeB / NB;
    bufferB  = bufferA + bufferSize - sizeB;
    assert(bufferA + sizeA == bufferB + sizeB * (NB - 1) / NB); // ensure alignment of overlap
    cudaMemcpy(dt, this, sizeof(GEdgeTrimmer), cudaMemcpyHostToDevice);
    initsuccess = true;
}

uint64_t GEdgeTrimmer::globalbytes() const {
    return (sizeA + sizeB / NB) + (1 + NB) * indexesSize + sizeof(siphash_keys) + CYCLELEN * 2 * sizeof(uint32_t) +
           sizeof(GEdgeTrimmer);
}

GEdgeTrimmer::~GEdgeTrimmer() {
    checkCudaErrors_V(cudaFree(bufferA));
    for (int i = 0; i < 1 + NB; i++) {
        checkCudaErrors_V(cudaFree(indexesE[i]));
    }
    checkCudaErrors_V(cudaFree(dipkeys));
    checkCudaErrors_V(cudaFree(uvnodes));
    checkCudaErrors_V(cudaFree(dt));
    cudaDeviceReset();
    delete[] indexesE;
}

uint32_t GEdgeTrimmer::trim() {
    cudaEvent_t start, stop;
    checkCudaErrors(cudaEventCreate(&start));
    checkCudaErrors(cudaEventCreate(&stop));
    cudaMemcpy(dipkeys, &sipkeys, sizeof(sipkeys), cudaMemcpyHostToDevice);

    cudaDeviceSynchronize();
    float durationA, durationB;
    cudaEventRecord(start, NULL);

    cudaMemset(indexesE[1], 0, indexesSize);

    SeedA<EDGES_A><<<tp.genA.blocks, tp.genA.tpb>>>(*dipkeys, (ulonglong4*) bufferAB, indexesE[1]);

    checkCudaErrors(cudaDeviceSynchronize());
    cudaEventRecord(stop, NULL);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&durationA, start, stop);
    if (abort) {
        return false;
    }
    cudaEventRecord(start, NULL);

    cudaMemset(indexesE[0], 0, indexesSize);

    uint32_t qA = sizeA / NA;
    uint32_t qE = NX2 / NA;
    for (uint32_t i = 0; i < NA; i++) {
        SeedB<EDGES_A><<<tp.genB.blocks / NA, tp.genB.tpb>>>(
            (uint2*) (bufferAB + i * qA), (ulonglong4*) (bufferA + i * qA), indexesE[1] + i * qE, indexesE[0] + i * qE);
        if (abort) {
            return false;
        }
    }

    checkCudaErrors(cudaDeviceSynchronize());
    cudaEventRecord(stop, NULL);
    cudaEventSynchronize(stop);
    cudaEventElapsedTime(&durationB, start, stop);
    checkCudaErrors(cudaEventDestroy(start));
    checkCudaErrors(cudaEventDestroy(stop));
    spdlog::trace("Seeding completed in {} + {} ms", durationA, durationB);
    if (abort) {
        return false;
    }

    for (uint32_t i = 0; i < NB; i++)
        cudaMemset(indexesE[1 + i], 0, indexesSize);

    qA              = sizeA / NB;
    const size_t qB = sizeB / NB;
    qE              = NX2 / NB;
    for (uint32_t i = NB; i--;) {
        Round<1, EDGES_A, EDGES_B / NB>
            <<<tp.trim.blocks / NB, tp.trim.tpb>>>(0, (uint2*) (bufferA + i * qA), (uint2*) (bufferB + i * qB),
                                                   indexesE[0] + i * qE, indexesE[1 + i]); // to .632
        if (abort) {
            return false;
        }
    }

    cudaMemset(indexesE[0], 0, indexesSize);

    Round<NB, EDGES_B / NB, EDGES_B / 2><<<tp.trim.blocks, tp.trim.tpb>>>(1, (const uint2*) bufferB, (uint2*) bufferA,
                                                                          indexesE[1],
                                                                          indexesE[0]); // to .296
    if (abort) {
        return false;
    }

    cudaMemset(indexesE[1], 0, indexesSize);

    Round<1, EDGES_B / 2, EDGES_A / 4><<<tp.trim.blocks, tp.trim.tpb>>>(2, (const uint2*) bufferA, (uint2*) bufferB,
                                                                        indexesE[0],
                                                                        indexesE[1]); // to .176
    if (abort) {
        return false;
    }

    cudaMemset(indexesE[0], 0, indexesSize);

    Round<1, EDGES_A / 4, EDGES_B / 4><<<tp.trim.blocks, tp.trim.tpb>>>(3, (const uint2*) bufferB, (uint2*) bufferA,
                                                                        indexesE[1],
                                                                        indexesE[0]); // to .117
    if (abort) {
        return false;
    }

    cudaDeviceSynchronize();

    for (int round = 4; round < tp.ntrims; round += 2) {
        cudaMemset(indexesE[1], 0, indexesSize);
        Round<1, EDGES_B / 4, EDGES_B / 4><<<tp.trim.blocks, tp.trim.tpb>>>(round, (const uint2*) bufferA,
                                                                            (uint2*) bufferB, indexesE[0], indexesE[1]);
        if (abort) {
            return false;
        }
        cudaMemset(indexesE[0], 0, indexesSize);
        Round<1, EDGES_B / 4, EDGES_B / 4><<<tp.trim.blocks, tp.trim.tpb>>>(round + 1, (const uint2*) bufferB,
                                                                            (uint2*) bufferA, indexesE[1], indexesE[0]);
        if (abort) {
            return false;
        }
    }

    cudaMemset(indexesE[1], 0, indexesSize);
    cudaDeviceSynchronize();

    Tail<EDGES_B / 4>
        <<<tp.tail.blocks, tp.tail.tpb>>>((const uint2*) bufferA, (uint2*) bufferB, indexesE[0], indexesE[1]);
    cudaMemcpy(&nedges, indexesE[1], sizeof(uint32_t), cudaMemcpyDeviceToHost);
    cudaDeviceSynchronize();
    return nedges;
}

SolverCtx::SolverCtx(const trimparams& tp) : trimmer(tp), cg(MAXEDGES, MAXEDGES, MAXSOLS, IDXSHIFT) {
    edges    = new uint2[MAXEDGES];
    soledges = new uint2[CYCLELEN];
}

int SolverCtx::findcycles(uint2* edges, uint32_t nedges) {
    cg.reset();
    for (uint32_t i = 0; i < nedges; i++) {
        cg.add_compress_edge(edges[i].x, edges[i].y);
    }

    for (uint32_t s = 0; s < cg.nsols; s++) {
        for (uint32_t j = 0; j < CYCLELEN; j++) {
            soledges[j] = edges[cg.sols[s][j]];
        }
        sols.resize(sols.size() + CYCLELEN);
        cudaMemcpyToSymbol(recoveredges, soledges, sizeof(uint2) * CYCLELEN);
        cudaMemset(trimmer.indexesE[1], 0, trimmer.indexesSize);
        Recovery<<<trimmer.tp.recover.blocks, trimmer.tp.recover.tpb>>>(*trimmer.dipkeys, (ulonglong4*) trimmer.bufferA,
                                                                        (int*) trimmer.indexesE[1], CYCLELEN);
        cudaMemcpy(&sols[sols.size() - CYCLELEN], trimmer.indexesE[1], CYCLELEN * sizeof(uint32_t),
                   cudaMemcpyDeviceToHost);
        checkCudaErrors(cudaDeviceSynchronize());
        qsort(&sols[sols.size() - CYCLELEN], CYCLELEN, sizeof(uint32_t), cg.nonce_cmp);
    }

    return 0;
}

int SolverCtx::solve() {
    trimmer.abort   = false;
    uint32_t nedges = trimmer.trim();
    if (!nedges)
        return 0;
    if (nedges > MAXEDGES) {
        spdlog::trace("OOPS; losing {} edges beyond MAXEDGES={}", nedges - MAXEDGES, MAXEDGES);
        nedges = MAXEDGES;
    }
    cudaMemcpy(edges, trimmer.bufferB, sizeof(uint2[nedges]), cudaMemcpyDeviceToHost);
    findcycles(edges, nedges);
    spdlog::trace("findcycles edges {}", nedges);
    return sols.size() / CYCLELEN;
}

void FillDefaultGPUParams(SolverParams& params) {
    trimparams tp;
    params.device        = 0;
    params.ntrims        = tp.ntrims;
    params.genablocks    = min(tp.genA.blocks, NEDGES / EDGE_BLOCK_SIZE / tp.genA.tpb);
    params.genatpb       = tp.genA.tpb;
    params.genbtpb       = tp.genB.tpb;
    params.trimtpb       = tp.trim.tpb;
    params.tailtpb       = tp.tail.tpb;
    params.recoverblocks = min(tp.recover.blocks, NEDGES / EDGE_BLOCK_SIZE / tp.recover.tpb);
    params.recovertpb    = tp.recover.tpb;
    params.cpuload       = false;
}

SolverCtx* CreateSolverCtx(SolverParams& params) {
    trimparams tp;
    tp.ntrims         = params.ntrims;
    tp.genA.blocks    = params.genablocks;
    tp.genA.tpb       = params.genatpb;
    tp.genB.tpb       = params.genbtpb;
    tp.trim.tpb       = params.trimtpb;
    tp.tail.tpb       = params.tailtpb;
    tp.recover.blocks = params.recoverblocks;
    tp.recover.tpb    = params.recovertpb;

    cudaDeviceProp prop;
    checkCudaErrors_N(cudaGetDeviceProperties(&prop, params.device));

    assert(tp.genA.tpb <= prop.maxThreadsPerBlock);
    assert(tp.genB.tpb <= prop.maxThreadsPerBlock);
    assert(tp.trim.tpb <= prop.maxThreadsPerBlock);
    // assert(tp.tailblocks <= prop.threadDims[0]);
    assert(tp.tail.tpb <= prop.maxThreadsPerBlock);
    assert(tp.recover.tpb <= prop.maxThreadsPerBlock);

    assert(tp.genA.blocks * tp.genA.tpb * EDGE_BLOCK_SIZE <= NEDGES);       // check THREADS_HAVE_EDGES
    assert(tp.recover.blocks * tp.recover.tpb * EDGE_BLOCK_SIZE <= NEDGES); // check THREADS_HAVE_EDGES
    assert(tp.genA.tpb / NX <= FLUSHA);                                     // check ROWS_LIMIT_LOSSES
    assert(tp.genB.tpb / NX <= FLUSHB);                                     // check COLS_LIMIT_LOSSES

    checkCudaErrors_N(cudaSetDevice(params.device));
    if (!params.cpuload) {
        checkCudaErrors_N(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync));
    }

    return new SolverCtx(tp);
}
