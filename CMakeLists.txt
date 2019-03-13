cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(NET_DIR src/net)

aux_source_directory(${NET_DIR} NET_SRCS)
include_directories(${NET_DIR})


set(MAIN src/main.cpp)
set(CODES
        ${NET_SRCS}
        ${MAIN}
        )
add_executable(epic ${CODES})
