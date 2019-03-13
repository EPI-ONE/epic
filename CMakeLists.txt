cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(NET_DIR src/net)
set(UTIL_DIR src/utils)
set(LOG_DIR src/spdlog)


aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)

include_directories(${NET_DIR} ${UTIL_DIR} ${LOG_DIR})



set(MAIN src/main.cpp)
set(CODES
        ${NET_SRCS}
        ${UTIL_SRCS}
        ${MAIN}
        )


add_executable(epic ${CODES})

