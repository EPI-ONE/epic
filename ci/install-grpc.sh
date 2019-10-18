#!/usr/bin/env bash
set -ev
EPIC_GRPC_VERSION="1.23.0"
EPIC_GRPC_DIR=""grpc-"$EPIC_GRPC_VERSION"

if [ ${TRAVIS_OS_NAME} == 'osx' ]; then
    brew install grpc
else
    if [[ ! -d ${epic_grpc_dir} ]];then
        sudo rm -rf grpc*
        git clone -b v${epic_grpc_version} --single-branch https://github.com/grpc/grpc.git ${epic_grpc_dir}
        cd ${epic_grpc_dir}
        git submodule update --init
        make -j6
    else
        cd ${epic_grpc_dir}
    fi

    sudo make install
    cd ..
fi
