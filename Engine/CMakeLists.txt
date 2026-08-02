cmake_minimum_required(VERSION 3.28)

project(CoriEngine)
    #get_filename_component(CORI_ENGINE_SOURCE_DIR ${PROJECT_SOURCE_DIR} ABSOLUTE)
    set(CORI_ENGINE_SOURCE_DIR ${PROJECT_SOURCE_DIR})

    # hides all compiler warnings caused by thirdparty libs
    set(HIDE_DEPENDENCY_WARNINGS ON CACHE BOOL "")
    
    set(BUILD_SHARED_LIBS OFF)

    # SDL3
    set(SDL_SHARED OFF)
    set(SDL_X11_XTEST OFF)
    if (CORI_CLANG_TIDY)
        set(SDL_CLANG_TIDY ON CACHE BOOL "")
    endif()
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/SDL3 EXCLUDE_FROM_ALL)

    # SDL_mixer
    set(SDLMIXER_GME OFF)
    set(SDLMIXER_MOD_XMP OFF)
    set(SDLMIXER_OPUS OFF)
    set(SDLMIXER_DEPS_SHARED OFF)
    #find_package(Threads REQUIRED)
    set(SDLMIXER_VENDORED OFF)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/SDL3_mixer EXCLUDE_FROM_ALL)

    # SDL_image
    set(SDLIMAGE_VENDORED OFF)
    set(SDLIMAGE_AVIF OFF)
    set(SDLIMAGE_JPEG OFF)
    set(SDLIMAGE_WEBP OFF)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/SDL3_image EXCLUDE_FROM_ALL)

    # glm
    set(GLM_ENABLE_CXX_20 ON CACHE BOOL "")
    if(CORI_USE_AVX2)
        set(GLM_ENABLE_SIMD_AVX2 ON CACHE BOOL "")
    endif ()
    set(GLM_ENABLE_FAST_MATH OFF CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/glm EXCLUDE_FROM_ALL)

    # nlohmann_json
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/nlohmann_json EXCLUDE_FROM_ALL)

    # tmxlite
    set(TMXLITE_STATIC_LIB ON)
    set(PROJECT_STATIC_RUNTIME ON CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/tmxlite/tmxlite EXCLUDE_FROM_ALL)

    # dear imgui
    add_library(imgui STATIC
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/imgui.cpp
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp
    # graphical backend v
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_vulkan.cpp
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_sdl3.cpp
    )
    target_include_directories(imgui PUBLIC ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui_club)
    target_link_libraries(imgui PRIVATE SDL3::SDL3-static)

    # spdlog
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "")
    if (CORI_CLANG_TIDY)
        set(SPDLOG_TIDY ON CACHE BOOL "")
    endif()
    set(SPDLOG_USE_STD_FORMAT OFF CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/spdlog EXCLUDE_FROM_ALL)


    # magic_enum

    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/magic_enum EXCLUDE_FROM_ALL)

    #entt
    set(ENTT_BUILD_DOCS OFF CACHE BOOL "")
    set(ENTT_INCLUDE_HEADERS ON CACHE BOOL "")
    set(ENTT_INCLUDE_TOOLS ON CACHE BOOL "")
    set(ENTT_INSTALL OFF CACHE BOOL "")
    if (CORI_CLANG_TIDY)
        set(ENTT_USE_CLANG_TIDY ON CACHE BOOL "")
    endif()
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/entt EXCLUDE_FROM_ALL)

    #box2d
    if(CORI_USE_AVX2)
        set(BOX2D_AVX2 ON CACHE BOOL "")
    endif ()
    set(BOX2D_DOCS OFF CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/box2d EXCLUDE_FROM_ALL)

    #stduuid
    #set(UUID_SYSTEM_GENERATOR ON CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/stduuid EXCLUDE_FROM_ALL)

    #freetype
    include(FetchContent)

    # a hack to support gpos kerning data in msdf-atlas-gen (disabled in freetype by default)

    FetchContent_Declare(
            freetype
            GIT_REPOSITORY https://github.com/freetype/freetype.git
            GIT_TAG VER-2-13-3
            PATCH_COMMAND ${CMAKE_COMMAND}
            -DPATCH_FILE=${CORI_ENGINE_SOURCE_DIR}/thirdparty/freetype-gpos.patch
            -DPATCH_BUILD_DIR=<BINARY_DIR>
            -P ${CORI_ENGINE_SOURCE_DIR}/cmake/patch-freetype.cmake
    )

    option(FT_DISABLE_ZLIB "DO NOT TOUCH" ON)
    option(FT_DISABLE_BZIP2 "DO NOT TOUCH" ON)
    option(FT_DISABLE_PNG "DO NOT TOUCH" ON)
    option(FT_DISABLE_HARFBUZZ "DO NOT TOUCH" ON)
    option(FT_DISABLE_BROTLI "DO NOT TOUCH" ON)

    FetchContent_MakeAvailable(freetype)

    add_library(Freetype::Freetype ALIAS freetype)

    #utfcpp
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/utfcpp EXCLUDE_FROM_ALL)

    #msdf-atlas-gen
    set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "")
    set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "")
    set(MSDF_ATLAS_INSTALL OFF CACHE BOOL "")
    set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "")
    set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/msdf-atlas-gen EXCLUDE_FROM_ALL)

    #tracy
    if (CORI_ENABLE_TRACY_PROFILER)
        set(TRACY_STATIC ON CACHE BOOL "")
        set(TRACY_ENABLE ON CACHE BOOL "")
        set(TRACY_NO_EXIT ON CACHE BOOL "")
        if (CORI_MY_CONFIG)
            set(TRACY_DEBUGINFOD ON CACHE BOOL "")
        endif ()
        add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/tracy EXCLUDE_FROM_ALL)
    endif ()

    #nsight aftermath
    # Binary-only SDK, not a submodule. Grab it from https://developer.nvidia.com/nsight-aftermath
    # and unpack it into thirdparty/ - either as 'NsightAftermath' or under its original versioned directory name, both are picked up.
    set(CORI_AFTERMATH_FOUND OFF)

    if (CORI_VK_DL_DEBUG_NVIDIA)
        set(CORI_AFTERMATH_DIR ${CORI_ENGINE_SOURCE_DIR}/thirdparty/NsightAftermath)

        if (NOT EXISTS ${CORI_AFTERMATH_DIR}/include/GFSDK_Aftermath.h)
            file(GLOB CORI_AFTERMATH_CANDIDATES ${CORI_ENGINE_SOURCE_DIR}/thirdparty/NVIDIA_Nsight_Aftermath_SDK_*)
            foreach (candidate ${CORI_AFTERMATH_CANDIDATES})
                if (EXISTS ${candidate}/include/GFSDK_Aftermath.h)
                    set(CORI_AFTERMATH_DIR ${candidate})
                    break()
                endif ()
            endforeach ()
        endif ()

        if (NOT EXISTS ${CORI_AFTERMATH_DIR}/include/GFSDK_Aftermath.h)
            message(STATUS "CORI_VK_DL_DEBUG_NVIDIA is ON but the Nsight Aftermath SDK was not found under '${CORI_ENGINE_SOURCE_DIR}/thirdparty'. GPU crash dumps will be disabled.")
        else ()
            if (WIN32)
                set(CORI_AFTERMATH_RUNTIME ${CORI_AFTERMATH_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.dll)
                set(CORI_AFTERMATH_IMPLIB ${CORI_AFTERMATH_DIR}/lib/x64/GFSDK_Aftermath_Lib.x64.lib)
            else ()
                set(CORI_AFTERMATH_RUNTIME ${CORI_AFTERMATH_DIR}/lib/x64/libGFSDK_Aftermath_Lib.x64.so)
                set(CORI_AFTERMATH_IMPLIB ${CORI_AFTERMATH_RUNTIME})
            endif ()

            if (NOT EXISTS ${CORI_AFTERMATH_RUNTIME})
                message(STATUS "Nsight Aftermath SDK headers were found, but '${CORI_AFTERMATH_RUNTIME}' is missing. GPU crash dumps will be disabled.")
            else ()
                add_library(Aftermath::Aftermath SHARED IMPORTED GLOBAL)
                set_target_properties(Aftermath::Aftermath PROPERTIES
                    IMPORTED_LOCATION ${CORI_AFTERMATH_RUNTIME}
                    IMPORTED_IMPLIB ${CORI_AFTERMATH_IMPLIB}
                    INTERFACE_INCLUDE_DIRECTORIES ${CORI_AFTERMATH_DIR}/include)

                set(CORI_AFTERMATH_FOUND ON)
                message(STATUS "Nsight Aftermath SDK found in '${CORI_AFTERMATH_DIR}', GPU crash dump collection is enabled.")
            endif ()
        endif ()
    endif ()

    # Puts the Aftermath runtime library next to <target>'s executable. Only needed on Windows,
    # elsewhere CMake adds the SDK directory to the build rpath of everything linking the engine.
    function(cori_deploy_aftermath TARGET_NAME)
        if (NOT TARGET Aftermath::Aftermath OR NOT WIN32)
            return()
        endif ()

        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:Aftermath::Aftermath> $<TARGET_FILE_DIR:${TARGET_NAME}>)
    endfunction()

    #ExprTK
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/ExprTK EXCLUDE_FROM_ALL)

    #boost/pfr
    set(BUILD_TESTING OFF)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/pfr EXCLUDE_FROM_ALL)


    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/Vulkan-Headers)

    #vma
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/VulkanMemoryAllocator-Hpp EXCLUDE_FROM_ALL)
    #note: this implicitly includes vulkan headers

    #vulkan loader
    set(LOADER_CODEGEN ON)
    set(LOADER_ENABLE_ADDRESS_SANITIZER OFF)
    set(LOADER_ENABLE_THREAD_SANITIZER OFF)

    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/Vulkan-Loader EXCLUDE_FROM_ALL)

    #small_vector
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/small_vector EXCLUDE_FROM_ALL)

    #dynamic_bitset
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/dynamic_bitset EXCLUDE_FROM_ALL)

    #fast obj
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/fast_obj EXCLUDE_FROM_ALL)

    #glaze
    set(glaze_ENABLE_REFLECTION26 ON CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/glaze EXCLUDE_FROM_ALL)

    #hwloc.
    set(CORI_HWLOC_VERSION 2.14.0 CACHE STRING "hwloc release to vendor")
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" CORI_HWLOC_SERIES ${CORI_HWLOC_VERSION})
    set(CORI_HWLOC_URL "https://download.open-mpi.org/release/hwloc/v${CORI_HWLOC_SERIES}/hwloc-${CORI_HWLOC_VERSION}.tar.gz")

    include(FetchContent)
    if (WIN32)
        set(HWLOC_SKIP_LSTOPO 1)
        set(HWLOC_SKIP_TOOLS 1)
        FetchContent_Declare(hwloc
            URL ${CORI_HWLOC_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            SOURCE_SUBDIR contrib/windows-cmake
            EXCLUDE_FROM_ALL)
        FetchContent_MakeAvailable(hwloc)
        add_library(HWLOC::hwloc_static ALIAS hwloc)
    else()
        FetchContent_Declare(hwloc
            URL ${CORI_HWLOC_URL}
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(hwloc)

        set(CORI_HWLOC_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/hwloc-install)
        set(CORI_HWLOC_LIB ${CORI_HWLOC_PREFIX}/lib/libhwloc.a)
        set(CORI_HWLOC_INC ${CORI_HWLOC_PREFIX}/include)

        include(ProcessorCount)
        ProcessorCount(CORI_HWLOC_JOBS)
        if (CORI_HWLOC_JOBS EQUAL 0)
            set(CORI_HWLOC_JOBS 1)
        endif()

        include(ExternalProject)
        ExternalProject_Add(hwloc_ext
            SOURCE_DIR ${hwloc_SOURCE_DIR}
            CONFIGURE_COMMAND ${hwloc_SOURCE_DIR}/configure
                --prefix=${CORI_HWLOC_PREFIX}
                --libdir=${CORI_HWLOC_PREFIX}/lib
                --enable-static --disable-shared
                --with-pic
                "CFLAGS=-O2 -g -fPIC"
                --disable-io --disable-libxml2 --disable-cairo --disable-libudev
                --disable-plugin-dlopen --disable-plugin-ltdl
                "CC=${CMAKE_C_COMPILER}"
            BUILD_COMMAND make -j${CORI_HWLOC_JOBS}
            INSTALL_COMMAND make install
            BUILD_BYPRODUCTS ${CORI_HWLOC_LIB})

        file(MAKE_DIRECTORY ${CORI_HWLOC_INC})

        add_library(HWLOC::hwloc_static STATIC IMPORTED GLOBAL)
        set_target_properties(HWLOC::hwloc_static PROPERTIES
            IMPORTED_LOCATION ${CORI_HWLOC_LIB}
            INTERFACE_INCLUDE_DIRECTORIES ${CORI_HWLOC_INC})
    endif()

    set(CORI_HWLOC_TARGET HWLOC::hwloc_static)

    set(TBB_TEST OFF CACHE BOOL "")
    set(TBB_STRICT OFF CACHE BOOL "")
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/oneTBB EXCLUDE_FROM_ALL)
    set(BUILD_SHARED_LIBS OFF)

    #explicit tbb dependency
    if (NOT WIN32 AND TARGET tbbbind)
        add_dependencies(tbbbind hwloc_ext)
    endif()


#find_package (Vulkan REQUIRED)

# project's binary setup vvv

    # varible sets
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

    set(ENGINE_LIBRARY_NAME ${PROJECT_NAME}_static)
    set(EXPORT_HEADER_NAME ${PROJECT_NAME}_export.hpp)
    set(CORI_INCLUDE_DIR ${CORI_ENGINE_SOURCE_DIR}/include)
    set(CORI_HEADERS_DIR ${CORI_ENGINE_SOURCE_DIR}/pchs)
    set(EXPORT_HEADER_FILE ${CORI_INCLUDE_DIR}/${EXPORT_HEADER_NAME})

    # source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS 
    ${CORI_ENGINE_SOURCE_DIR}/src/*.cpp
    )

    # my shared lib
    add_library(${ENGINE_LIBRARY_NAME} STATIC ${MY_SOURCES})
    if (CORI_MY_CONFIG)
        target_compile_definitions(${ENGINE_LIBRARY_NAME} PRIVATE
                $<$<CONFIG:Debug>:CORI_MY_CONFIG>
                $<$<CONFIG:RelWithDebInfo>:CORI_MY_CONFIG>
                $<$<CONFIG:Release>:CORI_MY_CONFIG>)
    endif ()

    if (CORI_VK_DL_DEBUG_AMD)
        target_compile_definitions(${ENGINE_LIBRARY_NAME} PUBLIC CORI_VK_DL_DEBUG_AMD)
    endif ()

    if (CORI_AFTERMATH_FOUND)
        target_compile_definitions(${ENGINE_LIBRARY_NAME} PUBLIC CORI_VK_DL_DEBUG_NVIDIA)
        target_link_libraries(${ENGINE_LIBRARY_NAME} PRIVATE Aftermath::Aftermath)
    endif ()

    target_compile_definitions(${ENGINE_LIBRARY_NAME} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
    )

    if (CORI_USE_AVX2 OR CORI_USE_NATIVE OR CORI_FORCE_AVX2)
        target_compile_definitions(${ENGINE_LIBRARY_NAME} PRIVATE
                $<$<CONFIG:Debug>:CORI_USE_SMID>
                $<$<CONFIG:RelWithDebInfo>:CORI_USE_SMID>
                $<$<CONFIG:Release>:CORI_USE_SMID>
        )
    endif()

    if (CORI_ENABLE_TRACY_PROFILER)
        target_compile_definitions(${ENGINE_LIBRARY_NAME} PUBLIC
            $<$<CONFIG:Debug>:CORI_ENABLE_PROFILING>
            $<$<CONFIG:RelWithDebInfo>:CORI_ENABLE_PROFILING>
                $<$<CONFIG:Debug>:TRACY_ENABLE>
                $<$<CONFIG:RelWithDebInfo>:TRACY_ENABLE>
        )
    endif()

    # compile flags just for the engine dll
    target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wall -Wextra -Wpedantic)

    # build binary output dir
    set_target_properties(${ENGINE_LIBRARY_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
    )

    # includes
    target_include_directories(${ENGINE_LIBRARY_NAME} PUBLIC
            ${CORI_ENGINE_SOURCE_DIR}/thirdparty/box2cpp/include
            ${CORI_ENGINE_SOURCE_DIR}/thirdparty/ska_sort
            ${CORI_ENGINE_SOURCE_DIR}/src
            ${CORI_INCLUDE_DIR}
    )

    target_include_directories(${ENGINE_LIBRARY_NAME} PRIVATE
            ${CORI_HEADERS_DIR}
    )

    # pch
    target_precompile_headers(${ENGINE_LIBRARY_NAME} PRIVATE ${CORI_HEADERS_DIR}/Cori_pch.hpp)
    target_precompile_headers(${ENGINE_LIBRARY_NAME} PUBLIC ${CORI_HEADERS_DIR}/App_pch.hpp)

    # disable compilation warnings from thirdparty libs

    if(HIDE_DEPENDENCY_WARNINGS)
        target_compile_options(SDL3-static PRIVATE -w)
        target_compile_options(SDL3_mixer-static PRIVATE -w)
        target_compile_options(SDL3_image-static PRIVATE -w)
        target_compile_options(glm PRIVATE -w)
        target_compile_options(tmxlite PRIVATE -w)
        target_compile_options(imgui PRIVATE -w)
        target_compile_options(msdf-atlas-gen PRIVATE -w)
        target_compile_options(msdfgen-core PRIVATE -w)
        target_compile_options(tmxlite PRIVATE -w)

        get_target_property(GLAZE_INCLUDES glaze_glaze INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(${ENGINE_LIBRARY_NAME} SYSTEM PRIVATE ${GLAZE_INCLUDES})

        get_target_property(VMA_INCLUDES GPUOpen::VulkanMemoryAllocator INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(${ENGINE_LIBRARY_NAME} SYSTEM PRIVATE ${VMA_INCLUDES})

        get_target_property(VMAHPP_INCLUDES VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(${ENGINE_LIBRARY_NAME} SYSTEM PRIVATE ${VMAHPP_INCLUDES})
        #target_compile_options(vulkan PRIVATE -w)

        #if(TARGET TIFF::tiff)
        #    set(SDLIMAGE_TIF_TARGET "tiff")
        #else()
        #    set(SDLIMAGE_TIF_TARGET "TIFF")
        #endif()
        #target_compile_options(${SDLIMAGE_TIF_TARGET} PRIVATE -w)
        target_compile_options(spdlog PRIVATE -w)
    endif()

    #target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wno-unused-variable)
    #target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wno-unused-parameter)

    # lib linking

    if (CORI_SANITIZE)
        #target_compile_options(${ENGINE_LIBRARY_NAME} PUBLIC -fsanitize=address)
        #target_link_options(${ENGINE_LIBRARY_NAME} PUBLIC -fsanitize=address)
    endif ()

    #target_compile_options(${ENGINE_LIBRARY_NAME} PUBLIC -fsanitize=thread)
    #target_link_options(${ENGINE_LIBRARY_NAME} PUBLIC -fsanitize=thread)
    #target_link_libraries()

    target_compile_options(${ENGINE_LIBRARY_NAME} PUBLIC -freflection)

    # should be public
    target_link_libraries(${ENGINE_LIBRARY_NAME} PUBLIC EnTT::EnTT spdlog::spdlog magic_enum::magic_enum imgui glm::glm nlohmann_json::nlohmann_json box2d::box2d stduuid utf8cpp tmxlite exprtk boost_pfr Vulkan::Headers GPUOpen::VulkanMemoryAllocator VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp Vulkan::Loader gch::small_vector sul::dynamic_bitset fast_obj_lib glaze_glaze tbb SDL3::SDL3-static stdc++exp)

    if (CORI_ENABLE_TRACY_PROFILER)
        target_link_libraries(${ENGINE_LIBRARY_NAME} PUBLIC Tracy::TracyClient)
    endif ()

    # can be private 
    target_link_libraries(${ENGINE_LIBRARY_NAME} PRIVATE SDL3_mixer::SDL3_mixer-static SDL3_image::SDL3_image-static msdf-atlas-gen ${CORI_HWLOC_TARGET})

    if (NOT WIN32 AND TARGET hwloc_ext)
        add_dependencies(${ENGINE_LIBRARY_NAME} hwloc_ext)
    endif()

    if (TARGET tbbbind)
        add_dependencies(${ENGINE_LIBRARY_NAME} tbbbind)
    endif()

    if(CORI_BUILD_DOCS)
        message(STATUS "CoriEngine documentation build is enabled.")
        add_subdirectory(docs)
        add_dependencies(${ENGINE_LIBRARY_NAME} CoriEngine-docs)
    endif()

