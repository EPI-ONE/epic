#!/usr/bin/env bash
set -ev
EPIC_OPENSSL_VERSION="1_1_1c"
EPIC_OPENSSL_DIR=""openssl-"$EPIC_OPENSSL_VERSION"

if [[ ! -d ${EPIC_OPENSSL_DIR} ]];then
    sudo rm -rf openssl*
    git clone -b OpenSSL_${EPIC_OPENSSL_VERSION} https://github.com/openssl/openssl.git ${EPIC_OPENSSL_DIR}
    cd ${EPIC_OPENSSL_DIR}
    ./config && make -j4
else
    cd ${EPIC_OPENSSL_DIR}
fi

sudo make install_sw
cd ..
