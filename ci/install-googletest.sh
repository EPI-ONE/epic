#!/usr/bin/env bash
set -ev
EPIC_GTEST_VERSION="1.8.1"
EPIC_GTEST_DIR=""googletest-"$EPIC_GTEST_VERSION"

if [[ ! -d ${EPIC_GTEST_DIR} ]];then
    sudo rm -rf googletest*
    git clone -b release-${EPIC_GTEST_VERSION} --single-branch https://github.com/google/googletest.git ${EPIC_GTEST_DIR}
    cd ${EPIC_GTEST_DIR} && mkdir build && cd build
    cmake ..
    make -j6
else
    cd ${EPIC_GTEST_DIR}/build
fi

sudo make install
cd ../..
