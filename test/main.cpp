#include <gtest/gtest.h>

#include "test_env.h"
//#include "caterpillar.h"
#include "params.h"

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new EpicTestEnvironment);

    //CAT = std::make_unique<Caterpillar>(config->GetDBPath());
    //const auto& params = UnitTestParams::GetParams();
    return RUN_ALL_TESTS();
}
