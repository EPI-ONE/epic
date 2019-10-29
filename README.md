# epic

[![Build Status](https://travis-ci.com/EPI-ONE/epic.svg?token=xx2m4HADP8ipz4gYg3xd&branch=master)](https://travis-ci.com/EPI-ONE/epic)
[![Coverage Status](https://coveralls.io/repos/github/EPI-ONE/epic/badge.svg?branch=master&t=OvdAhL)](https://coveralls.io/github/EPI-ONE/epic?branch=master)

Epic is a pow chain that achieves an order of magnitude improvement on the throughput without sacrificing  security and decentralization.

Testnet `testnet 0.1.1` is released. You may join the testnet by following the [instruction](#compile-and-run).

Welcome to join the discussion at https://t.me/epi-one

## Design

The design of the consensus mechanism is based on a Structured DAG. Refer to the [paper](https://arxiv.org/abs/1901.02755) for details.

## Implementation

The implementation of this peoject is written in C++ 17.

- C/C++ compilier and library
    - `gcc` version 8 or up on Linux
    - Apple LLVM version 11.0.0 (clang-1100.0.33.8) provided by XCode 11 on OS X 10.15

- `cmake` version 3.15 or up

- (Recommended) `clang` version 8 or up as an alternative (or better) compiler.
  Note: the llvm `libc++` lacks support of some C++ 17, thus still need the `gcc` library.

Builds on several existing projects:

- Our source code has included the code from existing projects:
    - several pieces of code in the [bitcoin](https://github.com/bitcoin/bitcoin) project, some of which with our own modification.
    - `cxxopts` [v2.0.0](https://github.com/jarro2783/cxxopts)
    - `cpptoml` [v0.1.1](https://github.com/skystrife/cpptoml)
    - `spdlog` [v1.3.1](https://github.com/gabime/spdlog) 
- The following dependencies need to be installed on your system:
    - `openssl` [version 1.1](https://github.com/openssl/openssl)
    - `libsecp256k1`
    - `libevent` [version 2.1.11](https://github.com/libevent/libevent/releases)
    - `RocksDB` [version 6.1.2](https://github.com/facebook/rocksdb) or up
    - `protobuf` [version 3.10.0](https://github.com/protocolbuffers/protobuf) 
    - `gRPC` [version 1.23.0](https://github.com/grpc/grpc) or up
    - `GoogleTest` [version 1.10.0](https://github.com/google/googletest)

This project is tested on Ubuntu (18.04) and MacOS X (10.15). See [INSTALL.md](./INSTALL.md) for detailed installation instructions of the above dependencies.

## Compile and run

```bash
git clone https://github.com/epi-one/epic/
cd epic && mkdir build && cd build
# Debug mode by default
# cmake ..
# Release mode
cmake -DCAMKE_BUILD_TYPE=Release ..
make -j
cd ../bin
# you may run the following test
./epictest
# runing the daemon
./epic --configpath /path/to/toml
# send rpc command to daemon
./epic-cli [OPTIONS] [COMMAND]
# type ./epic-cli --help to see the options
```

A default [config.toml](./config.toml) with seed address for the testnet is provided. You may customize a number of things, e.g., how to use `spdlog`, by editing the config file.

### Mining

Once a daemon is started, you may start the mining process

```bash
# 1. set pass phrase
./epic-cli set-passphrase
# 2. login
./epic-cli login
# 3. (optional) create transactions
# ./epic-cli create-randomtx 50000
# ./epic-cli create-tx []
# 4. start miner
./epic-cli start-miner
```

#### GPU mining

The project is enabled with CPU mining by default, if no CUDA installation found on the system.
It compiles the CUDA code with NVCC if CUDA is found, and GPU mining is then enabled automatically.
To disable GPU mining at all, add the flag `-DEPIC_ENABLE_CUDA=OFF` to the `cmake` command before compiling the codes.

If you are experiencing the following runtime error: `GPUassert(2): out of memory <EPIC PATH>/src/miner.h 32` while you do have adequate GPU memory, set the following shell environment variable:


``` shell
$ export ASAN_OPTIONS="protect_shadow_gap=0"
```

## License

EPIC project is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see [https://opensource.org/licenses/MIT](https://opensource.org/licenses/MIT). For the submodule [Cuckaroo](src/cuckaroo), the original license from the source [Cuckoo](https://github.com/tromp/cuckoo/blob/master/LICENSE.txt) should be followed.