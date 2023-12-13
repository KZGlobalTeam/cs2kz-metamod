#!/bin/bash

cd build
python3 ../configure.py --hl2sdk-root "../" --mms_path "../metamod-source" --hl2sdk-manifest "/metamod-source/hl2sdk-manifests" -s cs2 --targets x86_64
ambuild
