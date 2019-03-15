cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 11)


set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "")
set(EVENT__DISABLE_TESTS ON CACHE BOOL "")
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "")
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "")
add_subdirectory(libevent)

set(NET_DIR net)
set(UTIL_DIR utils)
set(MAIN main.cpp)


aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
include_directories(.)
link_directories(``````````````````````````````````````````````````````````````````````````````````````````````````````````````
    ${CMAKE_BINARY_DIR}/src/libevent/lib
)


set(CODES
    ${NET_SRCS}
    ${UTIL_SRCS}
    ${MAIN}
)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g")
set(CMAKE_VERBOSE_MAKEFILE ON)

add_executable(epic ${CODES})
add_dependencies(epic event_core event_extra)
if (WIN32)
    target_link_libraries(epic event_core.lib event_extra.lib)
else()
    target_link_libraries(epic libevent_core.a libevent_extra.a)
endif ()


