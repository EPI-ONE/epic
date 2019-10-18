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
    scl enable llvm-toolset-7 bash
    ```

3. (Optional) for `libsecp256k1`
    ```bash
    sudo yum install gmp-devel
    ```

### Mac OS X 10.15

0. XCode11

     ```bash
     xcode-select --install
     ```
     Mac provides its own C/C++ compiler and lib via XCode, `Apple clang version 11.0.0 (clang-1100.0.33.8)`
     >   Note: if you use XCode10 (or lower) and on OS X 10.14, you need to install llvm by using `brew install llvm` and manually add some required C header files. This step is complicated since MacOS Mojave removed some system headers. You may check whether you already have the required C headers by
     >
     >   `ls /Library/Developer/CommandLineTools/Packages`
     >
     >   If you have something like `macOS_SDK_headers_for_macOS_10.14.pkg` in this folder, then you can skip the next step. Otherwise, please go to [Apple Developer](https://developer.apple.com/download/more/) and download `Command Line Tools (macOS 10.14) for Xcode 10.2.1.dmg` and install. After this, you will find `/Library/Developer/CommandLineTools/Packages/macOS_SDK_headers_for_macOS_10.14.pkg`. After install this package, you will have the required system header files. Now, you can build from source.
     
1. Brew

    ```bash
    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
    ```

2. Some configure/make tools

    ```bash
    brew install automake autoconf libtool
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
    It is important that the above link is successful. In fact, it is import for all `brew link` to be successful. 


## Dependencies installation from source

Some depencencies needs to be installed from the source. The following is the detailed instruction, with slight variation depending on Linux (Ubuntu/CentOS) or Mac.

#### CMake

Mac:

```bash
brew install cmake
# version 3.15.4 will be installed
```

Linux:

```bash
git clone -b v3.15.4 --single-branch https://github.com/Kitware/CMake.git
cd CMake
./bootstrap && make -j && sudo make install
```

#### OpenSSL

This is required by `libevent` and `secp256k1`. 

Mac:

```bash
brew install openssl@1.1
# The following is needed for libevent install
export OPENSSL_ROOT_DIR="/usr/local/opt/openssl@1.1"
```

Linux:

```bash
git clone -b OpenSSL_1_1_1d https://github.com/openssl/openssl.git
cd openssl
./config && make -j && sudo make install
```

#### Secp256k1

```bash
git clone https://github.com/bitcoin-core/secp256k1.git
cd secp256k1
./autogen.sh && ./configure --enable-module-recovery=yes
make -j && sudo make install
```

#### libevent

```bash
git clone -b release-2.1.11-stable --single-branch https://github.com/libevent/libevent.git
cd libevent
mkdir build && cd build
cmake ..
make -j && sudo make install
```

>   The brew-installed `libevent` on Mac does not work for now. So please compile as instructed in the above.

#### Google Test

```bash
git clone -b release-1.10.0 --single-branch https://github.com/google/googletest.git
cd googletest && mkdir build && cd build
cmake ..
make -j && sudo make install
```

#### Protocol Buffers

Mac:

```bash
brew install protobuf
# version 3.10.0 will be installed
```

Linux:

```bash
git clone -b v3.10.0 --single-branch https://github.com/protocolbuffers/protobuf.git
cd protobuf
git submodule update --init --recursive
./autogen.sh && ./configure && make -j
make check
sudo make install
sudo ldconfig # refresh shared library cache.
```

If "make check" fails, you can still install, but it is likely that some features of this library will not work correctly on your system. Proceed at your own risk.  [https://github.com/protocolbuffers/protobuf/blob/master/src/README.md](https://github.com/protocolbuffers/protobuf/blob/master/src/README.md)

#### gRPC

Mac:

```bash
brew install grpc
# version 1.23.0 will be installed
```

Linux:

```bash
git clone -b v1.24.0 --single-branch https://github.com/grpc/grpc.git
cd grpc
git submodule update --init
make -j
sudo make install
```

The above instruction is from [https://github.com/grpc/grpc/blob/master/BUILDING.md](https://github.com/grpc/grpc/blob/master/BUILDING.md).

#### RocksDB

Mac:

```bash
brew install rocksdb
# version 6.1.2 will be installed
```

Linux:

```bash
git clone -b v6.3.6 --single-branch https://github.com/facebook/rocksdb.git
cd rocksdb
make shared_lib -j
sudo make install
```
