cmake_minimum_required(VERSION 3.28)

project(CoriEngine)

    # hides all compiler warnings caused by subprojects
    set(HIDE_DEPENDANCY_WARNINGS ON CACHE BOOL "")
    
    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "") 

    # SDL3
    set(SDL_SHARED OFF)
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/SDL3 EXCLUDE_FROM_ALL)

    # SDL_mixer
    set(SDLMIXER_MIDI_NATIVE OFF)
    set(SDLMIXER_GME OFF)    
    set(SDLMIXER_MOD OFF)
    set(SDLMIXER_OPUS OFF)
    set(SDLMIXER_VENDORED ON)
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/SDL3_mixer EXCLUDE_FROM_ALL)

    # SDL_image
    set(SDLIMAGE_VENDORED ON)
    set(SDLIMAGE_AVIF OFF)
    set(SDLIMAGE_BMP OFF)
    set(SDLIMAGE_JPEG OFF)
    set(SDLIMAGE_WEBP OFF)
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/SDL3_image EXCLUDE_FROM_ALL)

    # glm
    set(GLM_ENABLE_CXX_20 ON CACHE BOOL "")
    set(GLM_ENABLE_SIMD_AVX2 ON CACHE BOOL "")
    set(GLM_ENABLE_FAST_MATH OFF CACHE BOOL "")
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/glm EXCLUDE_FROM_ALL)

    # nlohmann_json
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/nlohmann_json EXCLUDE_FROM_ALL)

    # tmxlite
    set(TMXLITE_STATIC_LIB ON)
    set(PROJECT_STATIC_RUNTIME ON CACHE BOOL "")
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/tmxlite/tmxlite EXCLUDE_FROM_ALL)

    # freetype
    set(FT_ENABLE_ERROR_STRINGS ON CACHE BOOL "")
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/freetype EXCLUDE_FROM_ALL)

    # dear imgui
    add_library(imgui STATIC
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui/imgui.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui/imgui_demo.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui/imgui_draw.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui/imgui_tables.cpp
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui/imgui_widgets.cpp
    # graphical backend v
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)
    target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/imgui)    

    # glad 2
    set(GLAD_SOURCES_DIR ${PROJECT_SOURCE_DIR}/thirdparty/glad2)

    add_subdirectory(${GLAD_SOURCES_DIR}/cmake glad_cmake EXCLUDE_FROM_ALL)

    glad_add_library(glad_gl_core_46 STATIC API gl:core=4.6)

    # spdlog

    set(SPDLOG_BUILD_PIC ON CACHE BOOL "")
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "")
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/spdlog EXCLUDE_FROM_ALL)

    # magic_enum

    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/magic_enum EXCLUDE_FROM_ALL)

# project's binary setup vvv

    # varible sets
    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")

    set(ENGINE_LIBRARY_NAME ${PROJECT_NAME}_shared)
    set(EXPORT_HEADER_NAME ${PROJECT_NAME}_export.hpp)
    set(CORI_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
    set(CORI_HEADERS_DIR ${PROJECT_SOURCE_DIR}/internal_headers)
    set(EXPORT_HEADER_FILE ${CORI_INCLUDE_DIR}/${EXPORT_HEADER_NAME})

    # source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS 
    ${PROJECT_SOURCE_DIR}/src/*.cpp
    )
    
    # my shared lib
    add_library(${ENGINE_LIBRARY_NAME} SHARED ${MY_SOURCES})

    target_compile_definitions(${ENGINE_LIBRARY_NAME} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
    )

    # compile flags just for the engine dll
    target_compile_options(${ENGINE_LIBRARY_NAME} PRIVATE -Wall -Wextra)

    # build binary output dir
    set_target_properties(${ENGINE_LIBRARY_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
    )

    # export header auto generation
    file(TO_CMAKE_PATH ${CORI_INCLUDE_DIR} CORI_INCLUDE_DIR)
    
    add_custom_command(
        OUTPUT ${CORI_INCLUDE_DIR}/.keep
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CORI_INCLUDE_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${CORI_INCLUDE_DIR}/.keep
        COMMENT "Creating export header directory"
    )

    include(GenerateExportHeader)
    generate_export_header(${ENGINE_LIBRARY_NAME}
        BASE_NAME ${PROJECT_NAME}
        EXPORT_MACRO_NAME CORI_ENGINE_API
        EXPORT_FILE_NAME ${CORI_INCLUDE_DIR}/${EXPORT_HEADER_NAME}
    )

    target_compile_definitions(${ENGINE_LIBRARY_NAME} PRIVATE ${PROJECT_NAME}_EXPORTS)

    # includes
    target_include_directories(${ENGINE_LIBRARY_NAME} PUBLIC 
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui
    ${CORI_INCLUDE_DIR}
    )

    target_include_directories(${ENGINE_LIBRARY_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src/*.hpp
    ${CORI_HEADERS_DIR}
    )

    # pch

    target_precompile_headers(${ENGINE_LIBRARY_NAME} PRIVATE ${CORI_HEADERS_DIR}/Cori_pch.hpp)

    # disable compilation warnings from thirdparty libs

    if(HIDE_DEPENDANCY_WARNINGS)
        target_compile_options(SDL3-static PRIVATE -w)
        target_compile_options(SDL3_mixer-static PRIVATE -w)
        target_compile_options(SDL3_image-static PRIVATE -w)
        target_compile_options(glm PRIVATE -w)
        target_compile_options(nlohmann_json INTERFACE -w)
        target_compile_options(tmxlite PRIVATE -w)
        target_compile_options(imgui PRIVATE -w)
        target_compile_options(freetype PRIVATE -w)
        target_compile_options(glad_gl_core_46 PRIVATE -w)
        if(TARGET TIFF::tiff)
            # introduced in CMake 3.28
            # TIFF::TIFF still exists, but it is an INTERFACE library linking to TIFF::tiff (no ALIAS library)
            set(SDLIMAGE_TIF_TARGET "tiff")
        else()
            set(SDLIMAGE_TIF_TARGET "TIFF")
        endif()
        target_compile_options(${SDLIMAGE_TIF_TARGET} PRIVATE -w)
        target_compile_options(wavpack PRIVATE -w)
        #target_compile_options(spdlog PRIVATE -w)
    endif()

    # lib linking

    # should be public
    target_link_libraries(${ENGINE_LIBRARY_NAME} PUBLIC spdlog::spdlog)

    # can be private 

    # idk will be private until it breaks something
    target_link_libraries(${ENGINE_LIBRARY_NAME} PRIVATE SDL3::SDL3-static SDL3_mixer::SDL3_mixer-static SDL3_image::SDL3_image-static glm::glm nlohmann_json::nlohmann_json tmxlite imgui freetype glad_gl_core_46 magic_enum::magic_enum)