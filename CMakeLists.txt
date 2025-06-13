cmake_minimum_required(VERSION 3.28)

set(PVS_STUDIO_ACTIVE OFF)

if(PVS_STUDIO_ACTIVE)
    include(FetchContent)
    FetchContent_Declare(
        PVS_CMakeModule
        GIT_REPOSITORY "https://github.com/viva64/pvs-studio-cmake-module.git"
        GIT_TAG        "master"
    )
    FetchContent_MakeAvailable(PVS_CMakeModule)
    include("${pvs_cmakemodule_SOURCE_DIR}/PVS-Studio.cmake")
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:RelWithDebInfo>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:>")

set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

project(main)
    if (NOT (
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/freetype/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/glad2/cmake/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/glm/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/nlohmann_json/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/SDL3/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/SDL3_image/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/SDL3_mixer/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/spdlog/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/tmxlite/tmxlite/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/entt/CMakeLists.txt") AND 
            (EXISTS "${PROJECT_SOURCE_DIR}/CoriEngine/thirdparty/imgui/LICENSE.txt")
            ))
        message(FATAL_ERROR "CoriEngine thirdparty dependencies not found. Are you sure you cloned the repo recursively? Try running 'git submodule update --init --recursive' to download dependencies.")
    endif()
    set(CMAKE_CXX_STANDARD 23)

    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Only Clang/LLVM compiler is supported")
    endif()

    if(MSVC)
        message(FATAL_ERROR "MSVC Compiler frontend is not supported, use Clang/LLVM with posix frontend")
    endif()

    add_compile_options(-mavx2)
    
    add_subdirectory(${PROJECT_SOURCE_DIR}/CoriEngine)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/Sandbox)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/RendererSandbox)

