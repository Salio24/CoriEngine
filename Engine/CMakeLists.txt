cmake_minimum_required(VERSION 3.28)

project(CoriEngine)
    # hides all compiler warnings caused by subprojects
    get_filename_component(CORI_ENGINE_SOURCE_DIR ${PROJECT_SOURCE_DIR} ABSOLUTE)

    set(HIDE_DEPENDENCY_WARNINGS ON CACHE BOOL "")
    
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "") 

    #jinja2
    find_package(Python 3.9 REQUIRED COMPONENTS Interpreter)

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
    #set(SDLMIXER_VENDORED ON)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/SDL3_mixer EXCLUDE_FROM_ALL)

    # SDL_image
    set(SDLIMAGE_VENDORED ON)
    set(SDLIMAGE_AVIF OFF)
    set(SDLIMAGE_JPEG OFF)
    set(SDLIMAGE_WEBP OFF)
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/SDL3_image EXCLUDE_FROM_ALL)

    # glm
    set(GLM_ENABLE_CXX_20 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_AVX2 ON CACHE BOOL "")
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
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_sdl3.cpp
    )
    target_include_directories(imgui PUBLIC ${CORI_ENGINE_SOURCE_DIR}/thirdparty/imgui)
    target_link_libraries(imgui PRIVATE SDL3::SDL3-static)

    # glad 2
    set(GLAD_SOURCES_DIR ${CORI_ENGINE_SOURCE_DIR}/thirdparty/glad2)

    add_subdirectory(${GLAD_SOURCES_DIR}/cmake glad_cmake)

    glad_add_library(glad_gl_core_46 STATIC API gl:core=4.6)

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
    set(ENTT_BUILD_DOCS ON CACHE BOOL "")
    set(ENTT_INCLUDE_HEADERS ON CACHE BOOL "")
    set(ENTT_INCLUDE_TOOLS ON CACHE BOOL "")
    set(ENTT_INSTALL OFF CACHE BOOL "")
    if (CORI_CLANG_TIDY)
        set(ENTT_USE_CLANG_TIDY ON CACHE BOOL "")
    endif()
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/entt EXCLUDE_FROM_ALL)

    #box2d
    set(BOX2D_AVX2 ON CACHE BOOL "")
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

    FetchContent_MakeAvailable(freetype)

    #utfcpp
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/utfcpp EXCLUDE_FROM_ALL)

    #msdf-atlas-gen
    set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "")
    set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "")
    set(MSDF_ATLAS_INSTALL ON CACHE BOOL "")
    set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "")
    add_subdirectory(${CORI_ENGINE_SOURCE_DIR}/thirdparty/msdf-atlas-gen EXCLUDE_FROM_ALL)

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

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g)
        #add_compile_options(-fsanitize=address)
    endif()

    # my shared lib
    add_library(${ENGINE_LIBRARY_NAME} STATIC ${MY_SOURCES})

    target_compile_definitions(${ENGINE_LIBRARY_NAME} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
    )

    target_compile_definitions(${ENGINE_LIBRARY_NAME} PUBLIC
        $<$<CONFIG:Debug>:CORI_ENABLE_PROFILING>
        $<$<CONFIG:RelWithDebInfo>:CORI_ENABLE_PROFILING>
    )

    # compile flags just for the engine dll
    target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wall -Wextra)

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
        target_compile_options(glad_gl_core_46 PRIVATE -w)
        target_compile_options(msdf-atlas-gen PRIVATE -w)
        target_compile_options(msdfgen-core PRIVATE -w)
        target_compile_options(tmxlite PRIVATE -w)
        if(TARGET TIFF::tiff)
            set(SDLIMAGE_TIF_TARGET "tiff")
        else()
            set(SDLIMAGE_TIF_TARGET "TIFF")
        endif()
        target_compile_options(${SDLIMAGE_TIF_TARGET} PRIVATE -w)
        target_compile_options(spdlog PRIVATE -w)
    endif()

    #target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wno-unused-variable)
    #target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wno-unused-parameter)

    # lib linking

    # should be public
    target_link_libraries(${ENGINE_LIBRARY_NAME} PUBLIC EnTT::EnTT spdlog::spdlog magic_enum::magic_enum imgui glm::glm nlohmann_json::nlohmann_json box2d::box2d stduuid utf8cpp tmxlite)

    # can be private 
    target_link_libraries(${ENGINE_LIBRARY_NAME} PRIVATE SDL3::SDL3-static SDL3_mixer::SDL3_mixer-static SDL3_image::SDL3_image-static msdf-atlas-gen glad_gl_core_46)

    if(CORI_BUILD_DOCS)
        message(STATUS "CoriEngine documentation build is enabled.")
        add_subdirectory(docs)
        add_dependencies(${ENGINE_LIBRARY_NAME} CoriEngine-docs)
    endif()