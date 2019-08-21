#!/usr/bin/env bash
set -ev
EPIC_SECP256k1_VERSION="version"
EPIC_SECP256k1_DIR=""secp256k1-"$EPIC_SECP256k1_VERSION"

if [[ ! -d ${EPIC_SECP256k1_DIR} ]];then
    sudo rm -rf secp256k1*
    git clone https://github.com/bitcoin-core/secp256k1.git ${EPIC_SECP256k1_DIR}
    cd ${EPIC_SECP256k1_DIR}
    ./autogen.sh && ./configure --enable-module-recovery=yes
    make -j4
else
    cd ${EPIC_SECP256k1_DIR}
fi

sudo make install
cd ..
