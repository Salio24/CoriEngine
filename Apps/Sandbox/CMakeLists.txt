cmake_minimum_required(VERSION 3.28)

project(Sandbox)
    # project's binary setup

    set(EXECUTABLE_NAME ${PROJECT_NAME})

    # build binary output dir
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin/$<CONFIG>)

    # source files
    file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g)
        #add_compile_options(-fsanitize=address)
    endif()

    add_executable(${EXECUTABLE_NAME} ${MY_SOURCES})


    target_compile_options(${EXECUTABLE_NAME} PRIVATE -Wall -Wextra)

    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE
        $<$<CONFIG:Debug>:DEBUG_BUILD>
        $<$<CONFIG:RelWithDebInfo>:DEBUG_BUILD>
        $<$<CONFIG:Release>:RELEASE_BUILD>
    )

    target_link_libraries(${EXECUTABLE_NAME} PRIVATE CoriEngine_static)

    set(ENGINE_ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/CoriEngine/assets")

    set(ENGINE_ASSETS_DEST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/engine")


    # copy asset files from engine dir
    if(EXISTS "${ENGINE_ASSETS_SOURCE_DIR}")
        file(GLOB_RECURSE ENGINE_ASSET_FILES
                FOLLOW_SYMLINKS
                CONFIGURE_DEPENDS
                "${ENGINE_ASSETS_SOURCE_DIR}/*"
        )
    
        add_custom_target(copy_engine_assets_for_sandbox ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                    "${ENGINE_ASSETS_SOURCE_DIR}"
                    "${ENGINE_ASSETS_DEST_DIR}"
            DEPENDS ${ENGINE_ASSET_FILES}
    
            COMMENT "Copying engine assets from ${ENGINE_ASSETS_SOURCE_DIR} to ${ENGINE_ASSETS_DEST_DIR} if changed."
            VERBATIM
        )
    
        add_dependencies(${EXECUTABLE_NAME} copy_engine_assets_for_sandbox)
    
    else()
        message(WARNING "Engine asset source directory not found, skipping copy: ${ENGINE_ASSETS_SOURCE_DIR}")
    endif()
