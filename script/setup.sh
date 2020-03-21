#!/bin/bash
sudo apt install build-essential cmake libfuse3-3 libfuse3-dev python3 ipython3 python3-matplotlib bison flex texlive texlive-xetex
cd ..
mkdir build
cd build
cmake ..
make
mkdir mount
mkdir datadir
mkdir metadir
cd ../../
wget https://github.com/filebench/filebench/releases/download/1.5-alpha3/filebench-1.5-alpha3.tar.gz
tar -xzvf filebench-1.5-alpha3.tar.gz
rm filebench-1.5-alpha3.tar.gz
cd filebench-1.5-alpha3
mkdir build
./configure --prefix=/root/filebench-1.5-alpha3/build
make install
echo 0 > /proc/sys/kernel/randomize_va_space
cd ../KTableFS/script