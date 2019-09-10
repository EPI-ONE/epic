#!/bin/bash --login
mkdir build && cd build
echo $PATH
cmake ..
make -j
./../bin/epictest
