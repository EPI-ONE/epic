#!/usr/bin/env bash
set -ev
EPIC_PROTOBUF_VERSION="3.7.0"
EPIC_PROTOBUF_DIR=""protobuf-"$EPIC_PROTOBUF_VERSION"

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
if [ ${TRAVIS_OS_NAME} == 'linux' ]; then
    sudo ldconfig # refresh shared library cache.
fi
cd ..
