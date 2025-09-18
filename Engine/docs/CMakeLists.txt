find_package(Doxygen)
IF(DOXYGEN_FOUND)
    message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE} -- ${DOXYGEN_VERSION}")

    include(FetchContent)
    FetchContent_Declare(
            doxygen-awesome-css
            URL https://github.com/jothepro/doxygen-awesome-css/archive/refs/heads/main.zip
    )
    FetchContent_MakeAvailable(doxygen-awesome-css)

    FetchContent_GetProperties(doxygen-awesome-css SOURCE_DIR AWESOME_CSS_DIR)

    # Set Doxygen input and output files.
    set(DOXY_CSS_DIRECTORY ${doxygen-awesome-css_INCLUDE_DIR})
    set(DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR}/src)
    #set(DOXYGEN_ROOT_DIR ${PROJECT_SOURCE_DIR}/docs/temp)
    set(DOXYGEN_ROOT_DIR ${PROJECT_SOURCE_DIR}/docs)
    set(DOXYGEN_INDEX_FILE ${PROJECT_SOURCE_DIR}/docs/xml/index.xml)
    set(DOXYFILE_IN ${PROJECT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    # Generate DoxyFile from the input file.
    configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
    # Create Output directory.
    file(MAKE_DIRECTORY ${DOXYGEN_ROOT_DIR})
    # Command for generating doc from Doxygen config file.
    add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
            MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
            COMMENT "Generating Doxygen documentation for CoriEngine"
            VERBATIM)
    # Create CMake Target for generating doc.
    add_custom_target(CoriEngine-docs ALL DEPENDS ${DOXYGEN_INDEX_FILE})
ENDIF()

