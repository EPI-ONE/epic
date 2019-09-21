// Cuckaroo Cycle, a memory-hard proof-of-work by John Tromp
// Copyright (c) 2018-2019 Jiri Vadura (photon) and John Tromp
// This software is covered by the FAIR MINING license

#pragma once

#include "blake2b.h"
#include "graph.h"
#include "siphash.cuh"
#include "spdlog.h"

int gpuAssert(cudaError_t code, char* file, int line, bool abort = true);

#define checkCudaErrors(ans)                                            \
    ({                                                                  \
        int retval = gpuAssert(ans, (char*) __FILE__, __LINE__, false); \
        if (retval != cudaSuccess)                                      \
            return retval;                                              \
    })

#define checkCudaErrors_N(ans)                                                \
    ({                                                                        \
        if (gpuAssert(ans, (char*) __FILE__, __LINE__, false) != cudaSuccess) \
            return nullptr;                                                   \
    })

#define checkCudaErrors_V(ans)                                                \
    ({                                                                        \
        if (gpuAssert(ans, (char*) __FILE__, __LINE__, false) != cudaSuccess) \
            return;                                                           \
    })

struct blockstpb {
    uint16_t blocks;
    uint16_t tpb;
};

struct trimparams {
    uint16_t ntrims;
    blockstpb genA;
    blockstpb genB;
    blockstpb trim;
    blockstpb tail;
    blockstpb recover;

    trimparams();
};

// Number of Parts of BufferB, all but one of which will overlap BufferA
#ifndef NB
#define NB 2
#endif

// maintains set of trimmable edges
struct GEdgeTrimmer {
    trimparams tp;
    GEdgeTrimmer* dt;
    size_t sizeA, sizeB;
    const size_t indexesSize;
    uint8_t* bufferA;
    uint8_t* bufferB;
    uint8_t* bufferAB;
    uint32_t* indexesE[1 + NB];
    uint32_t nedges;
    uint32_t* uvnodes;
    siphash_keys sipkeys, *dipkeys;
    std::atomic_bool abort;
    bool initsuccess = false;
    int cycle_len;

    GEdgeTrimmer(const trimparams _tp, int cyclelen);
    ~GEdgeTrimmer();
    uint64_t globalbytes() const;
    uint32_t trim();
};

struct SolverCtx {
    GEdgeTrimmer trimmer;
    uint2* edges;
    graph<word_t> cg;
    uint2* soledges;
    std::vector<uint32_t> sols; // concatenation of all proof's indices
    int cycle_len;

    SolverCtx(const trimparams&, int cycleLen);

    ~SolverCtx() {
        delete[] edges;
        delete[] soledges;
    }

    void SetHeader(char* header, uint32_t len) {
        ::SetHeader(header, len, &trimmer.sipkeys);
        sols.clear();
    }

    int findcycles(uint2* edges, uint32_t nedges);
    int solve();
    void abort() {
        trimmer.abort = true;
    }
};
