#ifndef __TEST_ENV__
#define __TEST_ENV__

#include <gtest/gtest.h>

#include "test_factory.h"
#include "params.h"

class EpicTestEnvironment : public ::testing::Environment {
public:
    static const TestFactory& GetFactory() {
        static const TestFactory fac{};
        return fac;
    }

    void SetUp() override {
        ECC_Start();
        handle = ECCVerifyHandle();
        SelectParams(ParamsType::TESTNET);
    }

    void TearDown() override {
        ECC_Stop();
        handle.~ECCVerifyHandle();
    }

private: 
    ECCVerifyHandle handle;
};

#endif // __TEST_ENV__
