#include <gtest/gtest.h>

#include "test_factory.h"

class EpicTestEnvironment : public ::testing::Environment {
public:
    static TestFactory GetFactory() {
        static const TestFactory fac{};
        return fac;
    }

    void SetUp() override {
        ECC_Start();
        handle = ECCVerifyHandle();
    }

    void TearDown() override {
        ECC_Stop();
        handle.~ECCVerifyHandle();
    }

private: 
    ECCVerifyHandle handle;
};
