cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/)
set(CMAKE_CXX_COMPILER "clang++")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(EVENT__DISABLE_BENCHMARK ON CACHE BOOL "")
set(EVENT__DISABLE_TESTS ON CACHE BOOL "")
set(EVENT__DISABLE_REGRESS ON CACHE BOOL "")
set(EVENT__DISABLE_SAMPLES ON CACHE BOOL "")

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG  "-O0 -g -fstandalone-debug -fsanitize=address")
set(CMAKE_CXX_FLAGS "-pthread")
set(CMAKE_VERBOSE_MAKEFILE OFF)

# default bulid type
set(DEFAULT_BUILD_TYPE "Debug")

# gtest
find_package(gtest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# src codes
set(UTIL_DIR src/utils)
set(CRYPTO_DIR src/crypto)
set(COMPAT_DIR src/compat)
set(SCRYPT_DIR src/script)
set(CORE_DIR src)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
aux_source_directory(${CRYPTO_DIR} CRYPTO_SRCS)
aux_source_directory(${CORE_DIR} CORE_SRCS)
list(REMOVE_ITEM CORE_SRCS src/main.cpp)

include_directories(src
    ${UTIL_DIR}
    ${CRYPTO_DIR}
    ${COMPAT_DIR}
    ${SCRYPT_DIR}
    ${CORE_DIR}
    )

set(SRC_CODE
        ${UTIL_SRCS}
        ${CRYPTO_SRCS}
        ${CORE_SRCS}
        )
set(SRC_MAIN src/main.cpp)

# test codes
aux_source_directory(test/crypto TEST_CRYPTO_SRCS)
set(TEST_CODE
        ${TEST_DEMO_SRCS}
        ${TEST_CRYPTO_SRCS}
        )
set(TEST_MAIN test/main.cpp)

# generate executable
add_executable(epic ${SRC_CODE} ${SRC_MAIN} )
add_executable(epictest ${TEST_MAIN} ${SRC_CODE} ${TEST_CODE} )
target_link_libraries(epictest ${GTEST_BOTH_LIBRARIES})

# add test
gtest_add_tests(
        TARGET epictest
        TEST_SUFFIX .noArgs
        TEST_LIST   noArgsTests
)

# install
install(TARGETS epic DESTINATION bin)
