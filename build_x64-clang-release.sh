#!/bin/bash

cmake . --preset x64-clang-release
ninja -C out/build/x64-clang-release