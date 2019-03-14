cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(NET_DIR src/net)
set(UTIL_DIR src/utils)
set(MAIN src/main.cpp)


aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
include_directories(src)

#find_package(GTest)


set(CODES
        ${NET_SRCS}
        ${UTIL_SRCS}
        ${MAIN}
        )

add_executable(epic ${CODES})
target_link_libraries(epic GTest)

