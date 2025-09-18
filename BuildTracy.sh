cmake -B tools/TracyGUI -S Engine/thirdparty/tracy/profiler -DCMAKE_BUILD_TYPE=Release
cmake --build tools/TracyGUI --config Release --parallel
