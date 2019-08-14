# Install

## Basic tools via system installation

We provide instructions for three operating systems: Ubuntu, CentOS and MacOS.

### Ubuntu 18.04

0. Install some basics including `gcc` version 7.4 and build tools.

    ```bash
    sudo apt-get install build-essential 
    sudo apt-get install autoconf libtool pkg-config
    ```

2. `clang` version 8

    Add repository. If you have a different version of Ubuntu other than 18.04, see https://apt.llvm.org for corresponding repos.

    ```bash
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
    sudo apt-add-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main"
    sudo apt-get update
    ```

    The following is instructed in https://apt.llvm.org

    ```bash
    # LLVM
    sudo apt-get install libllvm-8-ocaml-dev libllvm8 llvm-8 llvm-8-dev llvm-8-doc llvm-8-examples llvm-8-runtime
    # Clang and co
    sudo apt-get install clang-8 clang-tools-8 clang-8-doc libclang-common-8-dev libclang-8-dev libclang1-8 clang-format-8 
    ```

    Update alternatives

    ```bash
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-8 100
    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-8 100
    ```

3. (Optional) for `libsecp256k1` 

    ```bash
    sudo apt-get install libgmp-dev
    ```

### CentOS 7.6

0. Basic build tools

    ```bash
    sudo yum install libtool autoconf automake 
    ```

1. `gcc` version 7.4 
    ```bash
    sudo yum install centos-release-scl
    sudo yum install devtoolset-7-gcc-c++
    scl enable devtoolset-7 bash/zsh
    ```

2. `clang` version 5.0

    ```bash
    sudo yum install llvm-toolset-7 
    scl enable llvm-toolset-7
    ```

3. (Optional) for `libsecp256k1`
    ```bash
    sudo yum install gmp-devel
    ```

### Mac OS X 10.14

0. Xcode and [Brew](https://brew.sh)

    ```bash
    xcode-select --install
    ```
    Mac provides its own C/C++ compiler and lib, `Apple LLVM version 10.0.1 (clang-1001.0.46.4)`
    ```bash
    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
    ```

1. Some configure/make tools

    ```bash
    brew install automake autoconf libtool 
    ```

2. llvm
    ```bash
    brew install llvm
    # You may choose to put the following in your .zshrc or .bashrc
    export PATH="/usr/local/opt/llvm/bin:$PATH"
    ```
  
3. (Optional) for `libsecp256k1`

    ```bash
    brew install gmp
    ```

4. `gRPC` requires `c-ares` `grpc` and `perftools`

    ```bash
    brew install c-ares
    brew install gflags
    brew install gperftools
    brew link gperftools
    ```
    It is important that the above link is successful.

    Need to export the following `PATH` for installing `gRPC` from source.

    ```bash
    export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"
    export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
    ```

5. Then some C header files are required. This step is complicated since MacOS Mojave removed some system headers. First, you may check whether you already have the required C headers by

    ```bash
    ls /Library/Developer/CommandLineTools/Packages
    ```

    If you have something like `macOS_SDK_headers_for_macOS_10.14.pkg` in this folder, then you can skip the next step. Otherwise, please go to [Apple Developer](https://developer.apple.com/download/more/) and download `Command Line Tools (macOS 10.14) for Xcode 10.2.1.dmg` and install. After this, you will find `/Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg`. After install this package, you will have the required system header files. Now, you can build from source. 

## Dependencies installation from source

Some depencencies needs to be installed from the source. The following are detailed instructions, with slight variation depending on Linux (Ubuntu/CentOS) or Mac. 

#### CMake

You can easily install `cmake` version 3.15.2 on Mac by 

```bash
brew install cmake
```

You have to `cmake` from source on Linux.

```bash
git clone -b v3.15.2 --single-branch https://github.com/Kitware/CMake.git
cd CMake
./bootstrap && make -j && sudo make install
```

#### openssl

This is required by `libevent` and `secp256k1`. Version 1.1.1c can be installed on Mac by 

```bash
brew install openssl@1.1
# The following is needed for libevent install
export OPENSSL_ROOT_DIR="/usr/local/Cellar/openssl@1.1/1.1.1c"
```

For recent version, you need to install it from source on linux.On CentOS you can just try your local openssl first

```bash
git clone -b OpenSSL_1_1_1c https://github.com/openssl/openssl.git
cd openssl
./config && make -j && sudo make install
```

#### secp256k1

```bash
git clone https://github.com/bitcoin-core/secp256k1.git 
cd secp256k1
./autogen.sh && ./configure --enable-module-recovery=yes
make -j && sudo make install
```

#### libevent

```bash
git clone -b release-2.1.8-stable --single-branch https://github.com/libevent/libevent.git
cd libevent
mkdir build && cd build
cmake ..
make -j && sudo make install
```

<aside class="warning">
The brew-installed `libevent` on Mac does not work. So please compile as instructed in the above. 
</aside>

#### GoogleTest

```bash
git clone -b release-1.8.1 --single-branch https://github.com/google/googletest.git
cd googletest && mkdir build && cd build
cmake ..
make -j && sudo make install
```

#### protobuf

`protobuf` version 3.7.0 can be installed simply via brew on Mac

```bash
brew install protobuf
```

For recent version, you need to install it from srouce on linux. 

```bash
git clone -b v3.7.0 --single-branch https://github.com/protocolbuffers/protobuf.git
cd protobuf
git submodule update --init --recursive
./autogen.sh && ./configure && make -j 
make check
sudo make install
sudo ldconfig # refresh shared library cache.
```

If "make check" fails, you can still install, but it is likely that some features of this library will not work correctly on your system. Proceed at your own risk.[https://github.com/protocolbuffers/protobuf/blob/master/src/README.md](https://github.com/protocolbuffers/protobuf/blob/master/src/README.md)

#### gRPC

The following instruction is from [https://github.com/grpc/grpc/blob/master/BUILDING.md](https://github.com/grpc/grpc/blob/master/BUILDING.md)

```bash
git clone -b v1.20.0 --single-branch https://github.com/grpc/grpc.git
cd grpc

# please use the following for Mac
LIBTOOL=glibtool LIBTOOLIZE=glibtoolize make -j
# please use the following for Linux
git submodule update --init
make -j 
sudo make install
```

<aside class="warning">
The brew-installed `gRPC` on Mac does not work. So please compile as instructed in the above. 
Also, `gRPC` v1.22.x seems to have issues. So we still use v1.20.0 for now despite the existence of more recent releases. 
</aside>


#### RocksDB

`RocksDB` version 6.1.2 can be installed simply via brew on Mac

```bash
brew install rocksdb
```

The following is for linux.

```bash
git clone -b v6.2.2 --single-branch https://github.com/facebook/rocksdb.git
cd rocksdb
make shared_lib -j
sudo make install
```
