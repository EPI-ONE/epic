#!/usr/bin/env bash
set -ev
EPIC_PROTOBUF_VERSION="3.10.0"
EPIC_PROTOBUF_DIR=""protobuf-"$EPIC_PROTOBUF_VERSION"

if [ ${TRAVIS_OS_NAME} == 'osx' ]; then
    brew install protobuf 
else
    if [[ ! -d ${EPIC_PROTOBUF_DIR} ]];then
        sudo rm -rf protobuf*
        git clone -b v${EPIC_PROTOBUF_VERSION} --single-branch https://github.com/protocolbuffers/protobuf.git ${EPIC_PROTOBUF_DIR}
        cd ${EPIC_PROTOBUF_DIR}
        git submodule update --init --recursive
        ./autogen.sh && ./configure && make -j6
        #make check
    else
        cd ${EPIC_PROTOBUF_DIR}
    fi

    sudo make install
    sudo ldconfig # refresh shared library cache.
    cd ..
fi
