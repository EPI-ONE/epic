#ifndef EPIC_CUCKAROO_TRIMMER_H
#define EPIC_CUCKAROO_TRIMMER_H

#include "mean.h"

#include <memory>

// arbitrary length of header hashed into siphash key
#ifndef HEADERLEN
#define HEADERLEN 112
#endif // HEADERLEN

extern inline std::unique_ptr<CSolverCtx> CreateCSolverCtx(SolverParams* params) {
    if (params->nthreads == 0)
        params->nthreads = 1;
    if (params->ntrims == 0)
        params->ntrims = EDGEBITS >= 30 ? 96 : 68;

    return std::make_unique<CSolverCtx>(params->nthreads, params->ntrims, params->allrounds);
}

template <typename CTX>
extern inline void DestroySolverCtx(std::unique_ptr<CTX>& ctx) {
    ctx.reset(nullptr);
}

template <typename CTX>
extern inline void StopSolver(const std::unique_ptr<CTX>& ctx) {
    ctx->abort();
}

#ifdef __CUDA_ENABLED__

#include "mean.cuh"
typedef GSolverCtx CTX;

#else

typedef CSolverCtx CTX;

#endif // __CUDA_ENABLED__

extern inline std::unique_ptr<CTX> CreateSolverCtx(SolverParams* params) {
#ifndef __CUDA_ENABLED__
    return CreateCSolverCtx(params);
#else
    return CreateGSolverCtx(params);
#endif
}

#endif // EPIC_CUCKAROO_TRIMMER_H
