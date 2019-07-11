#ifndef __TEST_ENV__
#define __TEST_ENV__

#include <gtest/gtest.h>

#include "caterpillar.h"
#include "params.h"
#include "test_factory.h"

class EpicTestEnvironment : public ::testing::Environment {
public:
    static const TestFactory& GetFactory() {
        static const TestFactory fac{};
        return fac;
    }

    void SetUp() override {
        ECC_Start();
        handle = ECCVerifyHandle();
        SelectParams(ParamsType::UNITTEST);
    }

    void TearDown() override {
        ECC_Stop();
        handle.~ECCVerifyHandle();
    }

    static void SetUpDAG(std::string dirPath) {
        std::ostringstream os;
        os << time(nullptr);
        file::SetDataDirPrefix(dirPath + os.str());
        CAT = std::make_unique<Caterpillar>(dirPath + os.str());
        DAG = std::make_unique<DAGManager>();
    }

    static void TearDownDAG(std::string dirPath) {
        CAT.reset();
        DAG.reset();
        std::string cmd = "exec rm -r " + dirPath;
        system(cmd.c_str());
    }

private:
    ECCVerifyHandle handle;
};

#endif // __TEST_ENV__
