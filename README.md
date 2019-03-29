# epic
* Dependency
1. google test v1.8.1 https://github.com/google/googletest
2. spdlog v1.3.1 we only use header file and you can refer to it via https://github.com/gabime/spdlog
* You should create your own config.toml and put it in the bin dir, if you don't create config.toml, the program will use the default_config.toml
3. libevent 
* We specify the libevent version 2.1.8 for building a stable base network. You need download the source code and compile it.  https://github.com/libevent/libevent/archive/release-2.1.8-stable.zip

* ```shell
  $ mkdir build && cd build
  $ cmake ..
  $ make
  ```
