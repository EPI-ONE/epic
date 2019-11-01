find_package(Git)
if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
    set(GIT_COMMIT "git unknown")
endif()
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
configure_file(${SRC} ${DST} @ONLY)
