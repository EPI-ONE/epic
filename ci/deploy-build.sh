#!/bin/bash --login
mkdir build && cd build
echo $PATH
cmake ..
make -j
./../bin/epictest --gtest_filter="-TestTrimmer*:TestMiner.SolveCuckaroo*"


