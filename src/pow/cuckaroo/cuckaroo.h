// Cuck(at)oo Cycle, a memory-hard proof-of-work
// Copyright (c) 2013-2019 John Tromp

#pragma once

#include "blake2b.h"
#include "params.h"
#include "siphash.h"
#include "spdlog.h"

// arbitrary length of header hashed into siphash key
#ifndef HEADERLEN
#define HEADERLEN 142
#endif

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
#ifndef PROOFSIZE
// the next most important parameter is the (even) length
// of the cycle to be found. a minimum of 12 is recommended
#define PROOFSIZE 4
#endif

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

// Solutions result structs to be instantiated by caller,
// and filled by solver if desired
struct Solution {
    uint64_t id    = 0;
    uint64_t nonce = 0;
    uint64_t proof[PROOFSIZE];
};

struct SolverSolutions {
    uint32_t edge_bits = 0;
    uint32_t num_sols  = 0;
    Solution sols[MAXSOLS];
};

#define MAX_NAME_LEN 256

// last error reason, to be picked up by stats
// to be returned to caller
static char LAST_ERROR_REASON[MAX_NAME_LEN];

// Solver statistics, to be instantiated by caller
// and filled by solver if desired
struct SolverStats {
    uint32_t device_id = 0;
    uint32_t edge_bits = 0;
    char plugin_name[MAX_NAME_LEN]; // will be filled in caller-side
    char device_name[MAX_NAME_LEN];
    bool has_errored = false;
    char error_reason[MAX_NAME_LEN];
    uint32_t iterations         = 0;
    uint64_t last_start_time    = 0;
    uint64_t last_end_time      = 0;
    uint64_t last_solution_time = 0;
};

enum verify_code {
    POW_OK,
    POW_HEADER_LENGTH,
    POW_TOO_BIG,
    POW_TOO_SMALL,
    POW_NON_MATCHING,
    POW_BRANCH,
    POW_DEAD_END,
    POW_SHORT_CYCLE
};

static const char* errstr[] = {"OK",
                               "wrong header length",
                               "edge too big",
                               "edges not ascending",
                               "endpoints don't match up",
                               "branch in cycle",
                               "cycle dead ends",
                               "cycle too short"};

// fills buffer with EDGE_BLOCK_SIZE siphash outputs for block containing edge in cuckaroo graph
// returns siphash output for given edge
uint64_t sipblock(const siphash_keys& keys, word_t edge, uint64_t* buf);

// verify that edges are ascending and form a cycle in header-generated graph
int verify(const word_t edges[PROOFSIZE], const siphash_keys& keys);

// convenience function for extracting siphash keys from header
void setheader(const char* header, uint32_t headerlen, siphash_keys* keys);

inline uint64_t timestamp() {
    using namespace std::chrono;
    high_resolution_clock::time_point now = high_resolution_clock::now();
    auto dn                               = now.time_since_epoch();
    return dn.count();
}
