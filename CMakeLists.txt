cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 11)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "")
set(EVENT__DISABLE_TESTS ON CACHE BOOL "")
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "")
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "")

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g")
set(CMAKE_VERBOSE_MAKEFILE ON)


# gtest
find_package(GTest REQUIRED)
if (GTEST_FOUND)
    message("Find Gtest lib in: ${GTEST_BOTH_LIBRARIES}")
    include_directories(${GTEST_INCLUDE_DIRS})
else()
    message("Gtest not found")
endif ()


# libevent
add_subdirectory(src/libevent)
link_directories(
        ${CMAKE_BINARY_DIR}/src/libevent/lib
)


# src codes
set(NET_DIR src/net)
set(UTIL_DIR src/utils)
include_directories(src ${NET_DIR} ${UTIL_DIR} )
aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
set(SRC_CODE
        ${NET_SRCS}
        ${UTIL_SRCS}
        )
set(SRC_MAIN src/main.cpp)


# test codes
aux_source_directory(test/net TEST_NET_SRCS)
set(TEST_CODE
        ${TEST_NET_SRCS}
        )
set(TEST_MAIN test/main.cpp)


# generate executable
add_executable(epic ${SRC_CODE} ${SRC_MAIN})
add_executable(epictest ${TEST_MAIN} ${SRC_CODE} ${TEST_CODE} )


# link libraries
add_dependencies(epic event_core event_extra)
add_dependencies(epictest event_core event_extra)

if (WIN32)
    target_link_libraries(epic event_core.lib event_extra.lib)
    target_link_libraries(epictest event_core.lib event_extra.lib)
else()
    target_link_libraries(epic libevent_core.a libevent_extra.a)
    target_link_libraries(epictest libevent_core.a libevent_extra.a)
endif ()

target_link_libraries(epictest ${GTEST_BOTH_LIBRARIES})


# add test
gtest_add_tests(
        TARGET epictest
        TEST_SUFFIX .noArgs
        TEST_LIST   noArgsTests
)


# add install
install(TARGETS epic DESTINATION bin)

