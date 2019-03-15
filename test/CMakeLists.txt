cmake_minimum_required(VERSION 3.11.1)

set(CMAKE_CXX_STANDARD 11)

set(MAIN main.cpp)

find_package(gtest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(CODES
    ${MAIN}
)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g")
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(epictest ${CODES})

target_link_libraries(epictest gtest)


