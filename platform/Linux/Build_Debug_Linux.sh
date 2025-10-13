#!/bin/bash

cmake -S . -B out/Debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build out/Debug --parallel
