cmake_minimum_required(VERSION 3.13)

project(Sandbox)
    # project's binary setup

    set(EXECUTABLE_NAME ${PROJECT_NAME})

    # build binary output dir
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>)

    # source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)
    
    add_executable(${EXECUTABLE_NAME} ${MY_SOURCES})

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE CoriEngine_shared)

    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/CoriEngine/bin/$<CONFIG>/CoriEngine_shared.dll
    $<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
)