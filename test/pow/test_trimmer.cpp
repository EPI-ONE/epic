// Copyright (c) 2019 EPI-ONE Core Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <gtest/gtest.h>

#include "test_env.h"
#include "trimmer.h"
#include "utilstrencodings.h"

class TestTrimmer : public testing::Test {
public:
    uint32_t nonce = 0;

    void SetUp() override {
        SetLogLevel(SPDLOG_LEVEL_TRACE);
    }
    void TearDown() override {
        ResetLogLevel();
    }
};

TEST_F(TestTrimmer, CPU) {
    // Create solver context
    SolverParams params;
    params.nthreads = 16; // should be powers of 2
    params.ntrims   = EDGEBITS >= 30 ? 96 : 68;

    auto ctx = CreateCSolverCtx(params);

    VStream header(GENESIS);

    spdlog::info("Looking for {}-cycle on cuckaroo{}(\"{}\", {}) with 50\% edges", PROOFSIZE, EDGEBITS,
                 header.str().c_str(), nonce);

    uint64_t sbytes = ctx->sharedbytes();
    uint32_t tbytes = ctx->threadbytes();
    int sunit, tunit;
    for (sunit = 0; sbytes >= 10240; sbytes >>= 10, sunit++)
        ;
    for (tunit = 0; tbytes >= 10240; tbytes >>= 10, tunit++)
        ;
    spdlog::info("Using {}{}B bucket memory at {},", sbytes, " KMGT"[sunit], (uint64_t) ctx -> trimmer.buckets);
    spdlog::info("{}x{}{}B thread memory at {},", params.nthreads, tbytes, " KMGT"[tunit],
                 (uint64_t) ctx -> trimmer.tbuckets);
    spdlog::info("{}-way siphash, and {} buckets.", NSIPHASH, NX);

    // Generate graph and start trimming
    uint64_t time0, time1;
    uint32_t timems;

    time0 = timestamp();

    ctx->SetHeader(header);
    spdlog::trace("nonce {} k0 k1 k2 k3 {:X} {:X} {:X} {:X}", nonce, ctx->trimmer.sipkeys.k0, ctx->trimmer.sipkeys.k1,
                  ctx->trimmer.sipkeys.k2, ctx->trimmer.sipkeys.k3);
    uint32_t nsols = ctx->solve();

    time1  = timestamp();
    timems = (time1 - time0) / 1000000;
    spdlog::trace("Time: {} ms\n", timems);

    // Verify trim result
    for (unsigned s = 0; s < nsols; s++) {
        spdlog::trace("Solution");
        word_t* prf = &ctx->sols[s * PROOFSIZE];
        for (uint32_t i = 0; i < PROOFSIZE; i++)
            spdlog::trace(" {}", (uintmax_t) prf[i]);
        spdlog::trace("\n");
        int pow_rc = VerifyProof(prf, ctx->trimmer.sipkeys);
        if (pow_rc == POW_OK) {
            spdlog::trace("Verified with cyclehash ");
            auto cyclehash = HashBLAKE2<256>(prf, sizeof(Proof));
            spdlog::trace("cycle hash {}", std::to_string(cyclehash));
        } else {
            spdlog::trace("FAILED due to {}", ErrStr[pow_rc]);
            ASSERT_TRUE(false);
        }
    }
    spdlog::trace("{} total solutions", nsols);

    DestroySolverCtx(ctx);
}

#ifdef __CUDA_ENABLED__
#undef checkCudaErrors
#define checkCudaErrors(ans)                          \
    ({                                                \
        gpuAssert((ans), (char*) __FILE__, __LINE__); \
        if (ans != cudaSuccess)                       \
            exit(ans);                                \
    })

TEST_F(TestTrimmer, GPU) {
    trimparams tp;
    VStream header(GENESIS);

    // Check GPU status and create solver context
    SolverParams params;
    FillDefaultGPUParams(params);

    spdlog::info("SolverParams: cuckaroo{} -d {} -h \"\" -m {} -n {} -U {} -u "
                 "{} -v {} -w {} -y {} -Z {} -z {}",
                 EDGEBITS, params.device, tp.ntrims, nonce, tp.genA.blocks, tp.genA.tpb, tp.genB.tpb, tp.trim.tpb,
                 tp.tail.tpb, tp.recover.blocks, tp.recover.tpb);

    cudaDeviceReset();

    int nDevices;
    checkCudaErrors(cudaGetDeviceCount(&nDevices));
    assert(params.device < nDevices);

    cudaDeviceProp prop;
    checkCudaErrors(cudaGetDeviceProperties(&prop, params.device));
    uint64_t dbytes = prop.totalGlobalMem;
    int dunit;
    for (dunit = 0; dbytes >= 102040; dbytes >>= 10, dunit++)
        ;
    spdlog::info("{} with {}{}B @ {} bits x {}MHz", prop.name, (uint32_t) dbytes, " KMGT"[dunit], prop.memoryBusWidth,
                 prop.memoryClockRate / 1000);

    spdlog::info("Looking for {}-cycle on cuckaroo{}(\"{}\", {}) with 50\% edges, {}*{} buckets, {} trims, and {} "
                 "thread blocks.",
                 PROOFSIZE, EDGEBITS, header.str().c_str(), nonce, NX, NY, params.ntrims, NX);

    auto ctx = CreateGSolverCtx(params);

    uint64_t bytes = ctx->trimmer.globalbytes();
    int unit;
    for (unit = 0; bytes >= 102400; bytes >>= 10, unit++)
        ;
    spdlog::info("Using {}{}B of global memory.", (uint32_t) bytes, " KMGT"[unit]);

    uint64_t time0, time1;
    uint32_t timems;

    if (!ctx || !ctx->trimmer.initsuccess) {
        spdlog::debug("Error initialising trimmer. Aborting.");
        ASSERT_TRUE(false);
    }

    // Generate graph and start trimming
    time0 = timestamp();

    ctx->SetHeader(header.data(), header.size());
    spdlog::trace("nonce {} k0 k1 k2 k3 {:X} {:X} {:X} {:X}", nonce, ctx->trimmer.sipkeys.k0, ctx->trimmer.sipkeys.k1,
                  ctx->trimmer.sipkeys.k2, ctx->trimmer.sipkeys.k3);
    uint32_t nsols = ctx->solve();

    time1  = timestamp();
    timems = (time1 - time0) / 1000000;
    spdlog::trace("Time: {} ms", timems);

    // Verify trim result
    for (unsigned s = 0; s < nsols; s++) {
        spdlog::trace("Solution");
        uint32_t* prf = &ctx->sols[s * PROOFSIZE];
        for (uint32_t i = 0; i < PROOFSIZE; i++)
            spdlog::trace(" {}", (uintmax_t) prf[i]);
        int pow_rc = VerifyProof(prf, ctx->trimmer.sipkeys);
        if (pow_rc == POW_OK) {
            spdlog::trace("Verified with cyclehash ");
            unsigned char cyclehash[32];
            HashBLAKE2((char*) prf, sizeof(Proof), cyclehash, sizeof(cyclehash));
            for (int i = 0; i < 32; i++)
                spdlog::trace("{}", cyclehash[i]);
        } else {
            spdlog::trace("FAILED due to {}", ErrStr[pow_rc]);
            ASSERT_TRUE(false);
        }
    }
    spdlog::trace("{} total solutions", nsols);

    DestroySolverCtx(ctx);
}
#endif
