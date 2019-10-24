# Install

## Basic tools via system installation

We provide instructions for two operating systems: Ubuntu and MacOS. 

### Ubuntu 18.04

0. Install some basics build tools

    ```bash
    sudo apt-get install autoconf libtool pkg-config
    ```
    
1. Install gcc-8

    ```bash
    sudo apt-get install gcc-8 g++-8
    ```

    Update alternatives

    ```bash
    sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 100
    sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-8 100
    ```

0. Install clang-9

   Add repository. If you have a different version of Ubuntu other than 18.04, see https://apt.llvm.org for corresponding repos.

   ```bash
   wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
   sudo apt-add-repository "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main"
   sudo apt-get update 
   sudo apt-get install clang-9
   ```

   Update alternatives

   ```bash
   sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-9 100
   sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-9 100
   ```

1. (Optional) for `libsecp256k1`

    ```bash
    sudo apt-get install libgmp-dev
    ```

### Mac OS X 10.15

0. XCode11

     ```bash
     xcode-select --install
     ```
     Mac provides its own C/C++ compiler and lib via XCode, `Apple clang version 11.0.0 (clang-1100.0.33.8)`. Mac OS X must upgrade to 10.15.
     
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

## Dependencies installation from source

Some depencencies need to be installed from the source. The following is the detailed instruction, with slight variation depending on Linux (Ubuntu/CentOS) or Mac.

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

This is required by `libevent` and `secp256k1`. 

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

#### libevent

```bash
git clone -b release-2.1.11-stable --single-branch https://github.com/libevent/libevent.git
cd libevent
mkdir build && cd build
cmake ..
make -j && sudo make install
```

>   The brew-installed `libevent` (version 2.1.11_1) on Mac does not work smoothly for now. So please compile as instructed in the above.

#### Google Test

```bash
git clone -b release-1.10.0 --single-branch https://github.com/google/googletest.git
cd googletest && mkdir build && cd build
cmake ..
make -j && sudo make install
```

#### Secp256k1

```bash
git clone https://github.com/bitcoin-core/secp256k1.git
cd secp256k1
./autogen.sh && ./configure --enable-module-recovery=yes
make -j && sudo make install
```
