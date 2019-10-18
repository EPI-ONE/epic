#!/usr/bin/env bash
set -ev
EPIC_LIBEVENT_VERSION="2.1.11"
EPIC_LIBEVENT_DIR=""libevent-"$EPIC_LIBEVENT_VERSION"

if [ ${TRAVIS_OS_NAME} == 'osx' ]; then
  export OPENSSL_ROOT_DIR="/usr/local/opt/openssl@1.1"
  echo $OPENSSL_ROOT_DIR 
fi

if [[ ! -d ${EPIC_LIBEVENT_DIR} ]];then
    sudo rm -rf libevent*
    git clone -b release-${EPIC_LIBEVENT_VERSION}-stable --single-branch https://github.com/libevent/libevent.git ${EPIC_LIBEVENT_DIR}
    cd ${EPIC_LIBEVENT_DIR} && mkdir build && cd build
    cmake ..
    make -j6
else
    cd ${EPIC_LIBEVENT_DIR}/build
fi

sudo make install
cd ../..
