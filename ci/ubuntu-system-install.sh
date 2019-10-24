#!/usr/bin/env bash

export LANG="en_US.UTF-8"
export LC_ALL=

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

./install-cmake.sh 
./install-openssl.sh

./install-secp256k1.sh &
./install-libevent.sh &
./install-googletest.sh &
./install-protobuf.sh &
./install-grpc.sh &
./install-rocksdb.sh &

