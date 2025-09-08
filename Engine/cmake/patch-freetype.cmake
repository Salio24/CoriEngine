set(STAMP_FILE "${PATCH_BUILD_DIR}/.gpos_patch_applied")

if(EXISTS "${STAMP_FILE}")
    message(STATUS "freetype-gpos.patch already applied. Skipping.")
    return()
endif()

message(STATUS "Applying freetype-gpos.patch ...")
execute_process(
        COMMAND git apply --ignore-space-change --ignore-whitespace "${PATCH_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "Failed to apply freetype-gpos.patch:\n${output}\n${error}")
else()
    file(TOUCH "${STAMP_FILE}")
endif()