#!/bin/sh

clang-format -i $(find src -type f -name '*.cpp' -o -name '*.h' -not -path 'src/sdk/*')
