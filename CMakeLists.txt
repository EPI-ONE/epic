cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(UTIL_DIR src/utils)
set(CRYPTO src/crypto)
set(COMPAT src/compat)
set(SCRYPT src/script)
set(CORE src)
set(MAIN src/main.cpp)

aux_source_directory(${UTIL_DIR} UTIL_SRCS)
include_directories(src)

set(CODES
    ${UTIL_SRCS}
    ${CRYPTO}
    ${COMPAT}
    ${SCRYPT}
    ${CORE}
    ${MAIN}
    )

add_executable(epic ${CODES})
