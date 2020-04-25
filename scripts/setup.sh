#!/bin/bash
apt install build-essential cmake pkg-config libfuse3-3 libfuse3-dev python3 ipython3 python3-matplotlib bison flex texlive texlive-xetex fuse3 screen
cd ..
mkdir build
cd build
cmake ..
make
mkdir mount
mkdir datadir
mkdir metadir
cd ../third_party/filebench-1.5-alpha3
mkdir build
./configure --prefix=$(pwd)/build
make install
echo 0 > /proc/sys/kernel/randomize_va_space
cd ../../scripts