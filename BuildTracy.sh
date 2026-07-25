cmake -B Tools/TracyGUI -S Engine/thirdparty/tracy/profiler -DCMAKE_BUILD_TYPE=Release
cmake --build Tools/TracyGUI --config Release --parallel
