#ifndef EPIC_CUCKAROO_TRIMMER_H
#define EPIC_CUCKAROO_TRIMMER_H

#include "mean.h"

#include <memory>

extern inline CSolverCtx* CreateCSolverCtx(SolverParams& params) {
    if (params.nthreads == 0) {
        params.nthreads = 1;
    }
    if (params.ntrims == 0) {
        params.ntrims = EDGEBITS >= 30 ? 96 : 68;
    }

    return new CSolverCtx(params.nthreads, params.ntrims, params.allrounds);
}

template <typename CTX>
extern inline void DestroySolverCtx(CTX* ctx) {
    delete ctx;
}

#ifdef __CUDA_ENABLED__

#include "mean.cuh"
typedef GSolverCtx CTX;

#else

typedef CSolverCtx CTX;

#endif // __CUDA_ENABLED__

extern inline CTX* CreateSolverCtx(SolverParams& params) {
#ifndef __CUDA_ENABLED__
    return CreateCSolverCtx(params);
#else
    return CreateGSolverCtx(params);
#endif
}

#endif // EPIC_CUCKAROO_TRIMMER_H
