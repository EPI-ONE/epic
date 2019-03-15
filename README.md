# epic


* Dependency
  1. google test  https://github.com/google/googletest
  2. openssl https://github.com/openssl/openssl (for mac you can brew install openssl, it may work)
* You should create your own config.toml and put it in the bin dir
 Current config.toml:
```toml
title = "EPIC Config"

[logs]
use_file_logger = true
path = "../"
filename = "Debug.log"

```