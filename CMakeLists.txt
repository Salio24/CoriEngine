cmake_minimum_required(VERSION 3.13)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")

# prevent installing to system directories. 
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse4.1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1")


project(Sandbox)
    set(CMAKE_CXX_STANDARD 20)

    set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "") 

    set(SDL3_DIR ${CMAKE_CURRENT_SOURCE_DIR}/out/build/$<CONFIG>/CoriEngine/thirdparty/SDL3)

    # SDL3
    set(SDL_SHARED OFF)
    add_subdirectory(CoriEngine/thirdparty/SDL3 EXCLUDE_FROM_ALL)

    # SDL_mixer
    set(SDLMIXER_MIDI_NATIVE OFF)
    set(SDLMIXER_GME OFF)    
    set(SDLMIXER_MOD OFF)
    set(SDLMIXER_OPUS OFF)
    set(SDLMIXER_VENDORED ON)
    add_subdirectory(CoriEngine/thirdparty/SDL3_mixer EXCLUDE_FROM_ALL)

    set(SDLIMAGE_VENDORED ON)
    set(SDLIMAGE_AVIF OFF)
    set(SDLIMAGE_BMP OFF)
    set(SDLIMAGE_JPEG OFF)
    set(SDLIMAGE_WEBP OFF)
    add_subdirectory(CoriEngine/thirdparty/SDL3_image EXCLUDE_FROM_ALL)

    add_subdirectory(CoriEngine/thirdparty/glm EXCLUDE_FROM_ALL)

    set(JSON_BuildTests OFF CACHE INTERNAL "")

    add_subdirectory(CoriEngine/thirdparty/nlohmann_json EXCLUDE_FROM_ALL)

    set(TMXLITE_STATIC_LIB ON)
    add_subdirectory(CoriEngine/thirdparty/tmxlite/tmxlite EXCLUDE_FROM_ALL)

    add_subdirectory(CoriEngine/thirdparty/freetype EXCLUDE_FROM_ALL)

    add_library(imgui STATIC
    CoriEngine/thirdparty/imgui/imgui.cpp
    CoriEngine/thirdparty/imgui/imgui_demo.cpp
    CoriEngine/thirdparty/imgui/imgui_draw.cpp
    CoriEngine/thirdparty/imgui/imgui_tables.cpp
    CoriEngine/thirdparty/imgui/imgui_widgets.cpp
    # graphical backend v
    CoriEngine/thirdparty/imgui/backends/imgui_impl_opengl3.cpp
)
    
    set(GLAD_SOURCES_DIR CoriEngine/thirdparty/glad2)
    add_subdirectory(CoriEngine/thirdparty/glad2/cmake glad_cmake EXCLUDE_FROM_ALL)

    glad_add_library(glad_gl_core_46 STATIC API gl:core=4.6)

# Source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Apps/Sandbox/*.cpp")

    set(EXECUTABLE_NAME ${PROJECT_NAME})

# Set output directories
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Apps/Sandbox/Build/$<CONFIG>/bin)
    
    add_executable(${EXECUTABLE_NAME} "${MY_SOURCES}")
    
    target_include_directories(imgui PUBLIC CoriEngine/thirdparty/imgui)

    target_link_libraries(${EXECUTABLE_NAME} PUBLIC SDL3::SDL3-static SDL3_mixer::SDL3_mixer-static SDL3_image::SDL3_image-static glm::glm glad_gl_core_46)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE nlohmann_json::nlohmann_json tmxlite freetype imgui)