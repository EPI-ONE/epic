#!/usr/bin/env bash
set -ev
EPIC_LIBEVENT_VERSION="2.1.11"
EPIC_LIBEVENT_DIR=""libevent-"$EPIC_LIBEVENT_VERSION"

sudo rm -rf libevent*
if [[ ! -d ${EPIC_LIBEVENT_DIR} ]];then
    sudo rm -rf libevent*
    git clone -b release-${EPIC_LIBEVENT_VERSION}-stable --single-branch https://github.com/libevent/libevent.git ${EPIC_LIBEVENT_DIR}
    cd ${EPIC_LIBEVENT_DIR} && mkdir build && cd build
    echo $OPENSSL_ROOT_DIR 
    cmake ..
    make -j6
else
    cd ${EPIC_LIBEVENT_DIR}/build
fi

sudo make install
cd ../..
