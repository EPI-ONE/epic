#!/usr/bin/env bash
set -v
EPIC_SYS_DEP_DIR="sys-dep-install"
EPIC_SYS_INFO="$(uname -v)"

echo $EPIC_SYS_INFO

export LANG="en_US.UTF-8"
export LC_ALL=

if [[ ! -d ${EPIC_SYS_DEP_DIR} ]]
then
  mkdir ${EPIC_SYS_DEP_DIR}
fi

cd ${EPIC_SYS_DEP_DIR} 

if [[ ${EPIC_SYS_INFO} == *"Darwin"* ]] && [[ ${EPIC_SYS_INFO} == *"Version 19."* ]]
then
  brew install autoconf automake libtool
  brew install openssl@1.1
  export OPENSSL_ROOT_DIR="/usr/local/opt/openssl@1.1"
  brew install cmake
  brew install gmp
  brew install grpc
  brew install rocksdb
  brew install protobuf

  ../install-secp256k1.sh &
  ../install-libevent.sh &
  ../install-googletest.sh &

elif [[ ${EPIC_SYS_INFO} == *"Ubuntu"* ]] && [[ ${EPIC_SYS_INFO} == *"18.04"* ]]
then
  sudo apt-get install autoconf libtool pkg-config
  sudo apt-get install libgmp-dev

  # install gcc 8
  sudo apt-get install gcc-8 g++-8
  # update alternatives
  sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100
  sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 100

  # install llvm clang
  wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
  sudo apt-add-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main"
  sudo apt-get update 
  sudo apt-get install clang-9
  # update alternatives
  sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-9 100
  sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-9 100

  ../install-cmake.sh 
  ../install-openssl.sh

  ../install-secp256k1.sh &
  ../install-libevent.sh &
  ../install-googletest.sh &
  ../install-protobuf.sh &
  ../install-grpc.sh &
  ../install-rocksdb.sh &
else
  echo "This script only supports Mac OS X and Ubuntu Linux 18.04"
fi



