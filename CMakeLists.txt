cmake_minimum_required(VERSION 3.28)

set(PVS_STUDIO_ACTIVE OFF)
set(CORI_CLANG_TIDY ON)

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

if(MSVC)
    message(FATAL_ERROR "MSVC Compiler frontend is not supported, use GCC or Clang (not clang-cl it has MSVC frontend)")
endif()

if(CMAKE_CXX_COMPILER MATCHES "/msys[36][24]/")
    set(CLANG_VARIANT "MSYS2" CACHE STRING "Detected MSYS2 environment")
else()
    set(CLANG_VARIANT "OTHER" CACHE STRING "Not MSYS2 environment")
endif()

if(CLANG_VARIANT STREQUAL "OTHER" AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:RelWithDebInfo>:Debug>")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:>")
endif ()

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

    add_compile_options(-mavx2)
    
    add_subdirectory(${PROJECT_SOURCE_DIR}/CoriEngine)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/Sandbox)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/RendererSandbox)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/PhysicsSandbox)


