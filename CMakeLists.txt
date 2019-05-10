cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/)
set(CMAKE_CXX_COMPILER "clang++")
add_definitions(-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
add_compile_options(-Wthread-safety)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fstandalone-debug -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS "-pthread $ENV{CXXFLAGS} -march=native")
set(CMAKE_VERBOSE_MAKEFILE OFF)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# default bulid type
set(DEFAULT_BUILD_TYPE "Debug")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()


# gtest
find_package(GTest 1.8.1 EXACT REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

#libevent
find_package(Libevent 2.1.8 EXACT REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})

# rocksdb
find_package(rocksdb REQUIRED)
include_directories(${ROCKSDB_INCLUDE_DIRS})

# secp256k1
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
find_package(Secp256k1 REQUIRED)
find_package(GMP REQUIRED)
include_directories(${Secp256k1_INCLUDE_DIR})

# src codes
set(NET_DIR src/net)
set(UTIL_DIR src/utils)
set(SUPPORT_DIR src/support)
set(CRYPTO_DIR src/crypto)
set(COMPAT_DIR src/compat)
set(SUPPORT_DIR src/support)
set(TASM_DIR src/tasm)
set(SPDLOG_DIR src/spdlog)
set(CORE_DIR src)
aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
aux_source_directory(${TASM_DIR} TASM_SRCS)
aux_source_directory(${CRYPTO_DIR} CRYPTO_SRCS)
aux_source_directory(${CORE_DIR} CORE_SRCS)
aux_source_directory(${SUPPORT_DIR} SUPPORT_DIR)
list(REMOVE_ITEM CORE_SRCS src/main.cpp)

include_directories(src
        ${NET_DIR}
        ${UTIL_DIR}
        ${CRYPTO_DIR}
        ${COMPAT_DIR}
        ${CORE_DIR}
        ${TASM_DIR}
        ${SPDLOG_DIR}
        ${SUPPORT_DIR}
        )

set(SRC_CODE
        ${NET_SRCS}
        ${UTIL_SRCS}
        ${CRYPTO_SRCS}
        ${CORE_SRCS}
        ${TASM_SRCS}
        ${SUPPORT_DIR}
        )

set(SRC_MAIN src/main.cpp)


# test codes
aux_source_directory(test/net TEST_NET_SRCS)
aux_source_directory(test/utils TEST_UTILS_SRCS)
aux_source_directory(test/tasm TEST_TASM_SRCS)
aux_source_directory(test/crypto TEST_CRYPTO_SRCS)
aux_source_directory(test/core TEST_CORE_SRCS)
aux_source_directory(test/support TEST_SUPPORT_SRCS)
set(TEST_CODE
        ${TEST_NET_SRCS}
        ${TEST_UTILS_SRCS}
        ${TEST_TASM_SRCS}
        ${TEST_CRYPTO_SRCS}
        ${TEST_CORE_SRCS}
        ${TEST_SUPPORT_SRCS}
        )
set(TEST_MAIN test/main.cpp)

# generate executable
add_executable(epic ${SRC_CODE} ${SRC_MAIN})
add_executable(epictest ${TEST_MAIN} ${SRC_CODE} ${TEST_CODE})

target_link_libraries(epic ${LIBEVENT_LIBRARIES})
target_link_libraries(epic ${ROCKSDB_LIBRARIES})
target_link_libraries(epic ${Secp256k1_LIBRARY})
target_link_libraries(epic ${GMP_LIBRARY})

target_link_libraries(epictest ${LIBEVENT_LIBRARIES})
target_link_libraries(epictest ${GTEST_BOTH_LIBRARIES})
target_link_libraries(epictest ${ROCKSDB_LIBRARIES})
target_link_libraries(epictest ${Secp256k1_LIBRARY})
target_link_libraries(epictest ${GMP_LIBRARY})

option(UNITTEST_COVERAGE "coverage compile flag" OFF)
if (UNITTEST_COVERAGE)
    target_compile_options(epictest PUBLIC --coverage)
    target_link_options(epictest PUBLIC --coverage)
endif ()

# add test
gtest_add_tests(
        TARGET epictest
        TEST_SUFFIX .noArgs
        TEST_LIST noArgsTests
)

# install
install(TARGETS epic DESTINATION bin)

# display cmake options
MESSAGE(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
MESSAGE(STATUS "Compiler flags:" ${CMAKE_CXX_COMPILE_FLAGS})
MESSAGE(STATUS "Compiler cxx debug flags:" ${CMAKE_CXX_FLAGS_DEBUG})
MESSAGE(STATUS "Compiler cxx release flags:" ${CMAKE_CXX_FLAGS_RELEASE})
MESSAGE(STATUS "Compiler cxx flags:" ${CMAKE_CXX_FLAGS})
