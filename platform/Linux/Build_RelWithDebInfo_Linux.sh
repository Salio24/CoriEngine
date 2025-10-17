#!/bin/bash

cmake -S . -B out/RelWithDebInfo -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build out/RelWithDebInfo --parallel
