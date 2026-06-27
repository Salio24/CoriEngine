cmake_minimum_required(VERSION 3.28)

if(MSVC)
    message(FATAL_ERROR "MSVC is not supported, use GCC or Clang (not clang-cl)")
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

option(CORI_CLANG_TIDY "Enable clang tidy for 3rd party libs where possible" OFF)
option(CORI_BUILD_SANDBOX_APPS "Build sandbox apps. (99% chance that it will fail, i rarely update them)" OFF)
option(CORI_BUILD_DOCS "Build CoriEngine documentation" OFF)
option(CORI_SANITIZE "Enable address sanitization" OFF)
option(CORI_USE_AVX2 "Allows the use of AVX2 in thirdparty libs. You need to do clean configuration+build after changing this." ON)
option(CORI_USE_NATIVE "Adds -march=native compiler flag if CORI_FORCE_AVX2 is OFF, ON by default" ON)
option(CORI_FORCE_AVX2 "Force AVX2, default OFF, -march=native is used by default. You need to do clean configuration+build after changing this." OFF)
option(CORI_ENABLE_TRACY_PROFILER "Enables Tracy profiler in debug builds. You need to do clean configuration+build after changing this." OFF)
option(CORI_USE_MOLD "Adds -fuse-ld=mold link flag globally to enable the use of mold linker, off by default." OFF)

if (NOT CORI_USE_AVX2)
    set(CORI_FORCE_AVX2 OFF)
endif()

if(CORI_FORCE_AVX2)
    set(CORI_USE_AVX2 ON)
endif()

project(main)
    if(CLANG_VARIANT STREQUAL "OTHER" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:Debug>")
    endif ()

    if (NOT (
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
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/msdf-atlas-gen/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/ExprTK/CMakeLists.txt") AND
            (EXISTS "${PROJECT_SOURCE_DIR}/Engine/thirdparty/pfr/CMakeLists.txt")

            ))
        message(FATAL_ERROR "CoriEngine thirdparty dependencies not found. Are you sure you cloned the repo recursively? Try running 'git submodule update --init --recursive' to download dependencies.")
    endif()
    set(CMAKE_CXX_STANDARD 26)

    if (NOT CORI_FORCE_AVX2 AND CORI_USE_NATIVE)
        add_compile_options(-march=native)
    else()
        add_compile_options(-mavx2)
    endif()

    if (CORI_USE_MOLD)
        add_link_options("-fuse-ld=mold")
    endif()

    #set(CMAKE_COLOR_DIAGNOSTICS ON CACHE BOOL "")

    add_subdirectory(${PROJECT_SOURCE_DIR}/Engine)

    if (CORI_BUILD_SANDBOX_APPS)
        add_subdirectory(${PROJECT_SOURCE_DIR}/Sandboxes/Sandbox)
        add_subdirectory(${PROJECT_SOURCE_DIR}/Sandboxes/RendererSandbox)
        add_subdirectory(${PROJECT_SOURCE_DIR}/Sandboxes/PhysicsSandbox)
    endif ()


