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

#ifdef __CUDA_ENABLED__
#undef checkCudaErrors
#define checkCudaErrors(ans)                          \
    ({                                                \
        gpuAssert((ans), (char*) __FILE__, __LINE__); \
        if (ans != cudaSuccess)                       \
            exit(ans);                                \
    })

uint64_t timestamp() {
    using namespace std::chrono;
    high_resolution_clock::time_point now = high_resolution_clock::now();
    auto dn                               = now.time_since_epoch();
    return dn.count();
}

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

    spdlog::info("Looking for {}-cycle on cuckaroo{}(\"{}\", {}) with 50\% edges, {} trims thread blocks.", CYCLELEN,
                 EDGEBITS, header.str().c_str(), nonce, params.ntrims);

    auto* ctx = CreateSolverCtx(params);

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
        uint32_t* prf = &ctx->sols[s * CYCLELEN];
        for (uint32_t i = 0; i < CYCLELEN; i++)
            spdlog::trace(" {}", (uintmax_t) prf[i]);
        int pow_rc = VerifyProof(prf, ctx->trimmer.sipkeys);
        if (pow_rc == POW_OK) {
            spdlog::trace("Verified with cyclehash ");
            auto cyclehash = HashBLAKE2<256>((char*) prf, PROOFSIZE);
            spdlog::trace("{}", std::to_string(cyclehash));
        } else {
            spdlog::trace("FAILED due to {}", ErrStr[pow_rc]);
            ASSERT_TRUE(false);
        }
    }
    spdlog::trace("{} total solutions", nsols);

    DestroySolverCtx(ctx);
}
#endif
