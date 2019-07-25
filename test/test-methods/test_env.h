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
    }

    static void SetUpDAG(std::string dirPath) {
        std::ostringstream os;
        os << time(nullptr);
        file::SetDataDirPrefix(dirPath + os.str());
        CAT = std::make_unique<Caterpillar>(dirPath + os.str());
        DAG = std::make_unique<DAGManager>();

        // Initialize DB with genesis
        std::vector<RecordPtr> genesisLvs = {std::make_shared<NodeRecord>(GENESIS_RECORD)};
        CAT->StoreLevelSet(genesisLvs);
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

inline void SetLogLevel(int level) {
    spdlog::set_level((spdlog::level::level_enum) level);
}

inline void ResetLogLevel() {
    SetLogLevel(SPDLOG_LEVEL_INFO);
}

#endif // __TEST_ENV__
