cmake_minimum_required(VERSION 3.28)

set(CORI_CLANG_TIDY ON)

if(MSVC)
    message(FATAL_ERROR "MSVC Compiler frontend is not supported, use GCC or Clang (not clang-cl it has MSVC frontend)")
endif()

if(CMAKE_CXX_COMPILER MATCHES "/msys[36][24]/")
    set(CLANG_VARIANT "MSYS2" CACHE STRING "Detected MSYS2 environment")
else()
    set(CLANG_VARIANT "OTHER" CACHE STRING "Not MSYS2 environment")
endif()

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

set(THREADS_PREFER_STATIC_FLAG ON)

if(CLANG_VARIANT STREQUAL "MSYS2")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Bstatic -lc++ -lc++abi -Wl,-Bdynamic")
    endif()
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static")
        #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
    endif()
endif ()

option(CORI_BUILD_SANDBOX_APPS   "Build sandbox apps. (can break the compilation if enabled)" OFF)
option(CORI_BUILD_DOCS "Build CoriEngine documentation" OFF)
option(CORI_SANITIZE "Enable address sanitization" OFF)
option(CORI_USE_AVX2 "Allows the compiler to use AVX2 instruction set. Disabled by default, gcc/clang have -march=native set by default, msvc doesn't have a default /arch: set. Setting it to ON will force AVX2 on gcc/clang/msvc." OFF)

project(main)
    if(CLANG_VARIANT STREQUAL "OTHER" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:RelWithDebInfo>:Debug>")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:>")
    endif ()

    if (NOT (
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/glad2/cmake/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/glm/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/nlohmann_json/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/SDL3/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/SDL3_image/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/SDL3_mixer/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/spdlog/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/tmxlite/tmxlite/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/entt/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/imgui/LICENSE.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/stduuid/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/utfcpp/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/msdf-atlas-gen/CMakeLists.txt")
            ))
        message(FATAL_ERROR "CoriEngine thirdparty dependencies not found. Are you sure you cloned the repo recursively? Try running 'git submodule update --init --recursive' to download dependencies.")
    endif()
    set(CMAKE_CXX_STANDARD 23)

    if (NOT MSVC AND NOT CORI_USE_AVX2)
        add_compile_options(-march=native)
    endif()

    if (MSVC AND CORI_USE_AVX2)
        add_compile_options(/arch:AVX2)
    endif()
    if(NOT MSVC AND CORI_USE_AVX2)
        add_compile_options(-mavx2)
    endif ()
    #set(CMAKE_COLOR_DIAGNOSTICS ON CACHE BOOL "")

    add_subdirectory(${PROJECT_SOURCE_DIR}/Engine)

    if (CORI_BUILD_SANDBOX_APPS)
        add_subdirectory(${PROJECT_SOURCE_DIR}/Sandboxes/Sandbox)
        add_subdirectory(${PROJECT_SOURCE_DIR}/Sandboxes/RendererSandbox)
        add_subdirectory(${PROJECT_SOURCE_DIR}/Sandboxes/PhysicsSandbox)
    endif ()


