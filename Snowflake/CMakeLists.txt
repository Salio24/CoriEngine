cmake_minimum_required(VERSION 3.28)

project(Snowflake)
    # project's binary setup

    set(EXECUTABLE_NAME ${PROJECT_NAME})

    # build binary output dir
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/${CORI_BUILD_NAME})

    # source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g)
        #add_compile_options(-fsanitize=address)
    endif()

    add_executable(${EXECUTABLE_NAME} ${MY_SOURCES})

    target_compile_options(${EXECUTABLE_NAME} PRIVATE -w)

    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)

    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
    )

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE CoriEngine_static)
