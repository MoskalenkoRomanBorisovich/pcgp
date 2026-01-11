cmake_minimum_required(VERSION 3.10)
project(pcgp_project C)


# if (MSVC)
#     add_compile_options(/W4 /permissive-)
#     add_compile_options("$<$<CONFIG:Release>:/Ox>")
# else()
#     add_compile_options(-Wall -Wextra -Wpedantic)
#     add_compile_options("$<$<CONFIG:Release>:-Ofast>")
# endif()

if (MSVC)
    # Remove default /W3 before adding /W4
    string(REPLACE "/W3" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/W3" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    add_compile_options(
        /W4
        /permissive-
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/fp:fast>
    )
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic
        "$<$<CONFIG:Release>:-Ofast>"
    )
endif()

add_subdirectory("source")


# first we can indicate the documentation build as an option and set it to ON by default
option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)