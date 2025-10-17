#!/bin/bash

cmake -S . -B out/Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/Release --parallel
