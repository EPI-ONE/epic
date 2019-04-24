# epic
### Installing Dependencies

#### google test v1.8.1
   Refer to link: https://github.com/google/googletest
#### spdlog v1.3.1
   - We only use header file and you can refer to it via https://github.com/gabime/spdlog
   - You should create your own config.toml and put it in the bin dir, if you don't create config.toml, the program will use the default_config.toml
#### libevent
   We specify the libevent version 2.1.8 for building a stable base network. You need to download the source code and compile it.  https://github.com/libevent/libevent/archive/release-2.1.8-stable.zip

 ```shell
  $ mkdir build && cd build
  $ cmake ..
  $ make
  ```
#### libsecp256k1
   This is a stand-alone library in the bitcoin-core project. Please clone the source code and compile it.

  ```shell
   $ git clone https://github.com/bitcoin-core/secp256k1.git
   $ cd secp256k1
   $ ./autogen.sh
   $ ./configure --enable-module-recovery=yes
   $ make
   $ ./tests
   $ sudo make install
   ```
   Please note that on *Mac OSX* you will have to install `gmp` via brew as `libsecp256k1` depends on it.
   ```
   brew install gmp
   ```
#### rocksdb
* For MacOS, it's as simple as `brew install rocksdb`. For Linux, please refer to the link https://github.com/facebook/rocksdb/blob/master/INSTALL.md

