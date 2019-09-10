#ifndef EPIC_CUCKAROO_TRIMMER_H
#define EPIC_CUCKAROO_TRIMMER_H

#ifdef __CUDA_ENABLED__
#include "mean.cuh"

extern SolverCtx* CreateSolverCtx(SolverParams& params);
extern void FillDefaultGPUParams(SolverParams& params);

inline void DestroySolverCtx(SolverCtx* ctx) {
    delete ctx;
}

#endif // __CUDA_ENABLED__
#endif // EPIC_CUCKAROO_TRIMMER_H
