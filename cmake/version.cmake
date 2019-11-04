find_package(Git)
if (GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif ()

if (NOT GIT_COMMIT)
    set(GIT_COMMIT "unknown")
endif ()

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")

MESSAGE(STATUS "Version: " ${EPIC_VERSION})
MESSAGE(STATUS "Commit: " ${GIT_COMMIT})
MESSAGE(STATUS "Build time: " ${BUILD_TIMESTAMP})

configure_file(${SRC} ${DST} @ONLY)
