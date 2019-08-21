#!/usr/bin/env bash
set -ev
EPIC_CMAKE_VERSION="3.15.2"
EPIC_CMAKE_DIR=""cmake-"$EPIC_CMAKE_VERSION"

if [ ${TRAVIS_OS_NAME} == 'osx' ]; then
    brew uninstall cmake
    brew install https://raw.githubusercontent.com/Homebrew/homebrew-core/bb8c8bef46027924a2df271d1843b0f3cecc784a/Formula/cmake.rb
else
    if [[ ! -d ${EPIC_CMAKE_DIR} ]];then
        sudo rm -rf cmake*
        git clone -b v${EPIC_CMAKE_VERSION} --single-branch https://github.com/Kitware/CMake.git ${EPIC_CMAKE_DIR}
        cd ${EPIC_CMAKE_DIR}
        ./bootstrap && make -j6
    else
        cd ${EPIC_CMAKE_DIR}
    fi

    sudo make install
    cd ..
fi
