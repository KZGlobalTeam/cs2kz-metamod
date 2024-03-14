#!/bin/sh

clang-format -i ./src/*.{cpp,h} ./src/{kz,movement,utils}/**/*.{cpp,h} &>/dev/null
