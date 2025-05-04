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

    # Define the source asset directory from the engine project
# NOTE: Adjust the folder name "assets" if yours is different.
# This relies on CoriEngine being added via add_subdirectory in a parent scope.
    set(ENGINE_ASSETS_SOURCE_DIR "${CMAKE_SOURCE_DIR}/CoriEngine/assets")

# Define the destination directory within the Sandbox binary output path
# NOTE: Adjust the folder name "assets" if you want a different name in the destination.
set(ENGINE_ASSETS_DEST_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/engine")

# --- Safety Check: Ensure the source directory exists ---
if(EXISTS "${ENGINE_ASSETS_SOURCE_DIR}")

    # --- Get a list of all files in the source directory ---
    # This list acts as the dependency trigger.
    # CONFIGURE_DEPENDS ensures CMake re-runs if files are added/removed.
    file(GLOB_RECURSE ENGINE_ASSET_FILES
            FOLLOW_SYMLINKS       # Optional: follow symbolic links
            CONFIGURE_DEPENDS     # Re-run configure if file list changes
            "${ENGINE_ASSETS_SOURCE_DIR}/*" # Glob pattern for all files/dirs
    )

    # --- Create a custom target to perform the copy ---
    # The target is named 'copy_engine_assets_for_sandbox' (choose any unique name)
    # 'ALL' means this target is part of the default build process.
    add_custom_target(copy_engine_assets_for_sandbox ALL
        # The command to execute.
        # Use CMake's cross-platform copy_directory_if_different for efficiency.
        # It only copies if source is newer or destination doesn't match.
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${ENGINE_ASSETS_SOURCE_DIR}"  # Source
                "${ENGINE_ASSETS_DEST_DIR}"    # Destination

        # DEPENDS lists the files that trigger the command.
        # If any file in ENGINE_ASSET_FILES changes, the COMMAND runs.
        DEPENDS ${ENGINE_ASSET_FILES}

        # A message printed during the build
        COMMENT "Copying engine assets from ${ENGINE_ASSETS_SOURCE_DIR} to ${ENGINE_ASSETS_DEST_DIR} if changed."
        VERBATIM # Ensures the command is executed literally
    )

    # --- Make the main executable depend on the copy target ---
    # This ensures the assets are copied *before* Sandbox might need them
    # (e.g., before running it after build).
    add_dependencies(${EXECUTABLE_NAME} copy_engine_assets_for_sandbox)

else()
    message(WARNING "Engine asset source directory not found, skipping copy: ${ENGINE_ASSETS_SOURCE_DIR}")
endif()
