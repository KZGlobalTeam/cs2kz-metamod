#!/bin/bash

apt update
apt install clang-16
update-alternatives --install /usr/bin/clang clang /usr/bin/clang-16 100
update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-16 100

cd build
python3 ../configure.py
ambuild
