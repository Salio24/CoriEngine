#!/bin/bash

cmake . --preset x64-clang-debug
ninja -C out/build/x64-clang-debug