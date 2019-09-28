// Cuck(at)oo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2019 John Tromp

#pragma once

#include "blake2b.h"
#include "params.h"
#include "siphash.h"
#include "spdlog.h"

#ifndef MAXSOLS
#define MAXSOLS 4
#endif

#ifndef EDGE_BLOCK_BITS
#define EDGE_BLOCK_BITS 6
#endif
#define EDGE_BLOCK_SIZE (1 << EDGE_BLOCK_BITS)
#define EDGE_BLOCK_MASK (EDGE_BLOCK_SIZE - 1)

// proof-of-work parameters
#ifndef EDGEBITS
// the main parameter is the number of bits in an edge index,
// i.e. the 2-log of the number of edges
#define EDGEBITS 29
#endif

#ifndef CYCLELEN
// the next most important parameter is the (even) length
// of the cycle to be found. a minimum of 12 is recommended
#define CYCLELEN GetParams().cycleLen
#endif

#define MAXCYCLELEN 42

#if EDGEBITS > 30
typedef uint64_t word_t;
#elif EDGEBITS > 14
typedef uint32_t word_t;
#else // if EDGEBITS <= 14
typedef uint16_t word_t;
#endif

// number of edges
#define NEDGES ((word_t) 1 << EDGEBITS)
// used to mask siphash output
#define EDGEMASK ((word_t) NEDGES - 1)
#define NODEMASK EDGEMASK
#define NODE1MASK NODEMASK

// Common Solver parameters, to return to caller
struct SolverParams {
    uint32_t nthreads = 1;
    uint32_t ntrims   = 0;
    bool allrounds    = false;
    bool cpuload      = true;

    // Common cuda params
    uint32_t device = 0;

    // Cuda-mean specific params
    uint32_t expand        = 0;
    uint32_t genablocks    = 0;
    uint32_t genatpb       = 0;
    uint32_t genbtpb       = 0;
    uint32_t trimtpb       = 0;
    uint32_t tailtpb       = 0;
    uint32_t recoverblocks = 0;
    uint32_t recovertpb    = 0;
};

enum verify_code {
    POW_OK = 0,
    POW_HEADER_LENGTH,
    POW_TOO_BIG,
    POW_TOO_SMALL,
    POW_NON_MATCHING,
    POW_BRANCH,
    POW_DEAD_END,
    POW_SHORT_CYCLE
};

static const std::string ErrStr[] = {"OK",
                                     "wrong header length",
                                     "edge too big",
                                     "edges not ascending",
                                     "endpoints don't match up",
                                     "branch in cycle",
                                     "cycle dead ends",
                                     "cycle too short"};

// verify that edges are ascending and form a cycle in header-generated graph
int VerifyProof(const word_t* edges, const siphash_keys& keys);

// convenience function for extracting siphash keys from header
void SetHeader(const char* header, uint32_t headerlen, siphash_keys* keys);
