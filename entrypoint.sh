#!/bin/bash

rm -rf build
cd build
python3 ../configure.py --hl2sdk-root "../"
ambuild
