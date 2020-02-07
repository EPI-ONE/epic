#!/usr/bin/env bash
set -ev
EPIC_ROCKSDB_VERSION="6.3.6"
EPIC_ROCKSDB_DIR=""rocksdb-"$EPIC_ROCKSDB_VERSION"

if [[ ! -d ${EPIC_ROCKSDB_DIR} ]];then
    sudo rm -rf rocksdb*
    git clone -b v${EPIC_ROCKSDB_VERSION} --single-branch https://github.com/facebook/rocksdb.git ${EPIC_ROCKSDB_DIR}
    cd ${EPIC_ROCKSDB_DIR}
    CFLAGS='-Wno-error' CXXFLAGS='-Wno-error' make shared_lib -j4
else
    cd ${EPIC_ROCKSDB_DIR}
fi

CFLAGS='-Wno-error' CXXFLAGS='-Wno-error' sudo make install
cd ..


