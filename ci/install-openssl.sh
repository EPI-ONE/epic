#!/usr/bin/env bash
set -ev
EPIC_OPENSSL_VERSION="1_1_1d"
EPIC_OPENSSL_DIR=""openssl-"$EPIC_OPENSSL_VERSION"

if [ ${TRAVIS_OS_NAME} == 'osx' ]; then
    brew install openssl@1.1
    export OPENSSL_ROOT_DIR="/usr/local/opt/openssl@1.1"
    export OPENSSL_INCLUDE_DIR="/usr/local/opt/openssl@1.1/include"
    echo $OPENSSL_ROOT_DIR $OPENSSL_INCLUDE_DIR
else
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
fi
