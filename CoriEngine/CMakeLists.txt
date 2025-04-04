cmake_minimum_required(VERSION 3.13)

project(CoriEngine)

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
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/glm EXCLUDE_FROM_ALL)

    # nlohmann_json
    set(JSON_BuildTests OFF CACHE INTERNAL "")
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/nlohmann_json EXCLUDE_FROM_ALL)

    # tmxlite
    set(TMXLITE_STATIC_LIB ON)
    add_subdirectory(${PROJECT_SOURCE_DIR}/thirdparty/tmxlite/tmxlite EXCLUDE_FROM_ALL)
    target_compile_options(tmxlite PRIVATE "/EHsc")

    # freetype
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

    # glad 2
    set(GLAD_SOURCES_DIR ${PROJECT_SOURCE_DIR}/thirdparty/glad2)

    add_subdirectory(${GLAD_SOURCES_DIR}/cmake glad_cmake EXCLUDE_FROM_ALL)

    glad_add_library(glad_gl_core_46 STATIC API gl:core=4.6)

    # project's binary setup vvv

    set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")


    set(EXECUTABLE_NAME ${PROJECT_NAME}_shared)

    # source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)
    
    add_library(${EXECUTABLE_NAME} SHARED ${MY_SOURCES})

    # build binary output dir
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>
    )

    set(EXPORT_HEADER_DIR ${PROJECT_SOURCE_DIR}/include)
    file(TO_CMAKE_PATH "${EXPORT_HEADER_DIR}" EXPORT_HEADER_DIR)
    
    add_custom_command(
        OUTPUT ${EXPORT_HEADER_DIR}/.keep
        COMMAND ${CMAKE_COMMAND} -E make_directory ${EXPORT_HEADER_DIR}
        COMMAND ${CMAKE_COMMAND} -E touch ${EXPORT_HEADER_DIR}/.keep
        COMMENT "Creating export header directory"
    )

    include(GenerateExportHeader)
    generate_export_header(CoriEngine_shared
        BASE_NAME CoriEngine
        EXPORT_MACRO_NAME CORI_ENGINE_API
        EXPORT_FILE_NAME ${EXPORT_HEADER_DIR}/coriengine_export.hpp
    )
    target_compile_definitions(CoriEngine_shared PRIVATE CORI_ENGINE_EXPORTS)

    target_include_directories(${EXECUTABLE_NAME} PUBLIC 
    ${PROJECT_SOURCE_DIR}/thirdparty/imgui
    ${PROJECT_SOURCE_DIR}/src
    ${EXPORT_HEADER_DIR}
    )

    target_include_directories(imgui PUBLIC ${PROJECT_SOURCE_DIR}/thirdparty/imgui)

    target_link_libraries(${EXECUTABLE_NAME} PUBLIC SDL3::SDL3-static SDL3_mixer::SDL3_mixer-static SDL3_image::SDL3_image-static glm::glm nlohmann_json::nlohmann_json tmxlite imgui)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE freetype glad_gl_core_46)