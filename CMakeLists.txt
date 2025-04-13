cmake_minimum_required(VERSION 3.28)

if (NOT (
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/freetype/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/glad2/cmake/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/glm/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/nlohmann_json/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/SDL3/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/SDL3_image/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/SDL3_mixer/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/spdlog/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/tmxlite/tmxlite/CMakeLists.txt") AND 
        (EXISTS "${CMAKE_SOURCE_DIR}/CoriEngine/thirdparty/imgui/LICENSE.txt")
        ))
    message(FATAL_ERROR "CoriEngine thirdparty dependencies not found. Please run the setup script (CoriEngine/thirdparty) to download them. Make sure you run both scripts.")
endif()

    # prevent installing to system directories. 
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

project(main)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:>")

    set(CMAKE_CXX_STANDARD 20)
    
    add_compile_options(-mavx2)
    
    if(MSVC)
        message(FATAL_ERROR "Compilers with MSVC frontend are not supported, use compiler with POSIX frontend (GCC or CLANG/LLVM)") 
    endif()
    
    add_subdirectory(${PROJECT_SOURCE_DIR}/CoriEngine)
    add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/Sandbox)


#1> [CMake] -- CMAKE_SOURCE_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev
#1> [CMake] -- CMAKE_BINARY_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/out/build/x64-Clang-RelWithDebInfo
#1> [CMake] -- CMAKE_CURRENT_SOURCE_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev
#1> [CMake] -- CMAKE_CURRENT_BINARY_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/out/build/x64-Clang-RelWithDebInfo
#  ${PROJECT_SOURCE_DIR = C:/Users/salio/VSCode/VS2022/MyProject/EngineDev

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")