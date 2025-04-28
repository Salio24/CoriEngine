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
    
    ## Assume that CORI_ENGINE_DLL is defined or computed to be the full path to the CoriEngine DLL.
    #set(CORI_ENGINE_DLL "${CMAKE_SOURCE_DIR}/CoriEngine/bin/$<CONFIG>/CoriEngine_shared.dll")
    #set(SANDBOX_BIN_DIR "${PROJECT_SOURCE_DIR}/bin/$<CONFIG>")
    #
    ## Create a dummy file stamp to track the copy operation.
    #set(DLL_COPY_STAMP "${SANDBOX_BIN_DIR}/.cori_engine_dll_copied.stamp")
    #
    ## Custom command that copies the DLL and touches a stamp file.
    #add_custom_command(
    #    OUTPUT ${DLL_COPY_STAMP}
    #    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #        ${CORI_ENGINE_DLL} ${SANDBOX_BIN_DIR}
    #    COMMAND ${CMAKE_COMMAND} -E touch ${DLL_COPY_STAMP}
    #    DEPENDS ${CORI_ENGINE_DLL}  # Re-run if the DLL changes.
    #    COMMENT "Copying CoriEngine DLL to Sandbox output directory"
    #)
    #
    ## Custom target that always runs the above command.
    #add_custom_target(copy_cori_engine_dll ALL
    #    DEPENDS ${DLL_COPY_STAMP}
    #)
    #
    ## Ensure your Sandbox target depends on the copy target.
    #add_dependencies(${EXECUTABLE_NAME} copy_cori_engine_dll)

    #add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
    #COMMAND ${CMAKE_COMMAND} -E copy_if_different
    #${CMAKE_SOURCE_DIR}/CoriEngine/bin/$<CONFIG>/CoriEngine_shared.dll
    #$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>
    #)