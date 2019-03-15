cmake_minimum_required(VERSION 3.11.1)
project(epictest)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ../bin)

set(MAIN main.cpp)

find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

set(CODES
    ${MAIN}
)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g")
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(epictest ${CODES})
target_link_libraries(epictest ${GTEST_BOTH_LIBRARIES})


