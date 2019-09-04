# epic

[![Build Status](https://travis-ci.com/EPI-ONE/epic.svg?token=xx2m4HADP8ipz4gYg3xd&branch=master)](https://travis-ci.com/EPI-ONE/epic)
[![Coverage Status](https://coveralls.io/repos/github/EPI-ONE/epic/badge.svg?branch=travis-190&t=OvdAhL)](https://coveralls.io/github/EPI-ONE/epic?branch=master)

Epic is a pow chain that achives an order of magnitude improvement on the throughput without sacrificing the security and decentralization.

## Design

The design of this chain is based on the [paper](https://arxiv.org/abs/1901.02755).

## Implementation

The implementation of this peoject is written in C++ 17.

- C/C++ compilier and library
    - `gcc` version 7 or up on Linux
    - Apple LLVM version 10.0.1 (clang-1001.0.46.4) provided by XCode

- `cmake` version 3.14 or up

- (Recommended) `clang` version 5 or up as an alternative (or better) compiler.
  Note: the llvm `libc++` lacks support of some C++ 17, thus still need the `gcc` (or Mac clang) library.

Builds on several esisting projects:

- Our source code has inculded the code from existing projects:
    - [bitcoin](https://github.com/bitcoin/bitcoin) and several pieces of code it uses, some of which with our own modification.
    - [cxxopts v2.0.0](https://github.com/jarro2783/cxxopts)
    - [cpptoml v0.1.1](https://github.com/skystrife/cpptoml)
    - [spdlog v1.3.1](https://github.com/gabime/spdlog): header file only. You may customize how to use it by editing the default [config.toml](./config.toml) file.
********
- The following need to be installed on your system:
    - `openssl` version 1.1.1c
    - `libsecp256k1`
    - `libevent` version 2.1.11
    - `RocksDB` version 6.2.2
    - `protobuf` version 3.7.1
    - `gRPC` version 1.20.0
    - `GoogleTest` version 1.8.1

We test this project on Ubuntu (18.04), CentOS (7.6) and MacOS X (10.14.6). See [INSTALL.md](./INSTALL.md) for detailed installation instructions.

## Compile and run

```bash
git clone https://github.com/epi-one/epic/
cd epic && mkdir build && cd build
# Debug mode by default
cmake ..
# Release mode
# cmake -DCAMKE_BUILD_TYPE=Release ..
make -j
# you may run the following test
../bin/epictest
# runing the daemon
../bin/epic
# send rpc command to daemon
../bin/epicc [OPTIONS] [COMMAND]
```

### GPU mining

The project is enabled with CPU mining by default, if no CUDA installation found on the system.
It compiles the CUDA code with NVCC if CUDA is found, and GPU mining is then enabled automatically.
To disable GPU mining at all, add the flag `-DEPIC_ENABLE_CUDA=OFF` to the `cmake` command before compiling the codes.

If you are experiencing the following runtime error: `GPUassert(2): out of memory <EPIC PATH>/src/miner.h 32` while you do have adequate GPU memory, set the following shell environment variable:


``` shell
$ export ASAN_OPTIONS="protect_shadow_gap=0"
```

## License

EPIC project is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see [https://opensource.org/licenses/MIT](https://opensource.org/licenses/MIT). For the submodule [Cuckaroo](src/cuckaroo), we follow the original license from the source [Cuckoo](https://github.com/tromp/cuckoo/blob/master/LICENSE.txt).
