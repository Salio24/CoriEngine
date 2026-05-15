cmake_minimum_required(VERSION 3.28)

project(CoriEngine)
    #get_filename_component(CORI_ENGINE_SOURCE_DIR ${PROJECT_SOURCE_DIR} ABSOLUTE)
    set(CORI_ENGINE_SOURCE_DIR ${PROJECT_SOURCE_DIR})

    # hides all compiler warnings caused by thirdparty libs
    set(HIDE_DEPENDENCY_WARNINGS ON CACHE BOOL "")
    
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")

    # SDL3
    set(SDL_SHARED OFF)
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
    target_include_directories(imgui PUBLIC ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui)
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
        set(TRACY_ON_DEMAND ON CACHE BOOL "")
        add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/tracy EXCLUDE_FROM_ALL)
    endif ()

    #ExprTK
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/ExprTK EXCLUDE_FROM_ALL)

    #boost/pfr
    set(BUILD_TESTING OFF)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/pfr EXCLUDE_FROM_ALL)


    #add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/Vulkan-Headers)

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
        target_compile_options(${ENGINE_LIBRARY_NAME} PUBLIC -fsanitize=address)
        target_link_options(${ENGINE_LIBRARY_NAME} PUBLIC -fsanitize=address)
    endif ()
    #target_link_libraries()

    target_compile_options(${ENGINE_LIBRARY_NAME} PUBLIC -freflection)

    # should be public
    target_link_libraries(${ENGINE_LIBRARY_NAME} PUBLIC EnTT::EnTT spdlog::spdlog magic_enum::magic_enum imgui glm::glm nlohmann_json::nlohmann_json box2d::box2d stduuid utf8cpp tmxlite exprtk boost_pfr Vulkan::Headers GPUOpen::VulkanMemoryAllocator VulkanMemoryAllocator-Hpp::VulkanMemoryAllocator-Hpp Vulkan::Loader gch::small_vector sul::dynamic_bitset fast_obj_lib glaze_glaze)

    if (CORI_ENABLE_TRACY_PROFILER)
        target_link_libraries(${ENGINE_LIBRARY_NAME} PUBLIC Tracy::TracyClient)
    endif ()

    # can be private 
    target_link_libraries(${ENGINE_LIBRARY_NAME} PRIVATE SDL3::SDL3-static SDL3_mixer::SDL3_mixer-static SDL3_image::SDL3_image-static msdf-atlas-gen)

    if(CORI_BUILD_DOCS)
        message(STATUS "CoriEngine documentation build is enabled.")
        add_subdirectory(docs)
        add_dependencies(${ENGINE_LIBRARY_NAME} CoriEngine-docs)
    endif()

