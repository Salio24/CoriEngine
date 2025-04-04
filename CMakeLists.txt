cmake_minimum_required(VERSION 3.13)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:>")

# prevent installing to system directories. 
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

set(CMAKE_CXX_STANDARD 20)
add_compile_options(/arch:AVX2)

if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND MSVC))
    message(FATAL_ERROR "Unsupported compiler is NOT SUPPORTED, use clang-cl (Clang with MSVC ABI) instead")
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/CoriEngine)
add_subdirectory(${PROJECT_SOURCE_DIR}/Apps/Sandbox)


#1> [CMake] -- CMAKE_SOURCE_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev
#1> [CMake] -- CMAKE_BINARY_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/out/build/x64-Clang-RelWithDebInfo
#1> [CMake] -- CMAKE_CURRENT_SOURCE_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev
#1> [CMake] -- CMAKE_CURRENT_BINARY_DIR: C:/Users/salio/VSCode/VS2022/MyProject/EngineDev/out/build/x64-Clang-RelWithDebInfo
#  ${PROJECT_SOURCE_DIR = C:/Users/salio/VSCode/VS2022/MyProject/EngineDev

#add ${PROJECT_SOURCE_DIR} where possible
# TODO: configure compiler in this cmake file for different configurations 

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")