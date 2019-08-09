cmake_minimum_required(VERSION 3.11.1)
project(epic)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/)
set(CMAKE_CXX_COMPILER "clang++")
add_definitions(-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
add_compile_options(-Wall -Wthread-safety)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(DEFAULT_DEBUG_FLAGS "-O0 -g -fstandalone-debug -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_DEBUG ${DEFAULT_DEBUG_FLAGS})
set(CMAKE_CXX_FLAGS "-pthread -march=native $ENV{CXXFLAGS}")
set(CMAKE_VERBOSE_MAKEFILE OFF)

# if the architecture in question supports the extensions
# below it makes sense to uncomment the respective lines
# as a speedup will be realized

#set(CMAKE_CXX_FLAGS "-DENABLE_SSE42 ${CMAKE_CXX_FLAGS}")
#set(CMAKE_CXX_FLAGS "-DENABLE_PCLMULQDQ ${CMAKE_CXX_FLAGS}")

# default bulid type
set(DEFAULT_BUILD_TYPE "Debug")

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
    set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
    # Set the possible values of build type for cmake-gui
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release")
endif ()

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

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
find_library(GMP_LIBRARY NAMES gmp libgmp)
if (GMP_LIBRARY)
    MESSAGE(STATUS "GMP FOUND")
    set(GMP_FOUND true)
else()
    MESSAGE(STATUS "GMP NOT FOUND")
endif()
include_directories(${Secp256k1_INCLUDE_DIR})

# Protobuf and gRPC
find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)
set(RPC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc)
set(PROTOS proto/rpc.proto)
set(PROTO_SRC_DIR ${RPC_DIR}/proto-gen)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

# Protobuf and gRPC autogeneration
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})

# src codes
set(NET_DIR src/net)
set(UTIL_DIR src/utils)
set(CRYPTO_DIR src/crypto)
set(COMPAT_DIR src/compat)
set(SUPPORT_DIR src/support)
set(TASM_DIR src/tasm)
set(SPDLOG_DIR src/spdlog)
set(INTERFACE_DIR src/interfaces)
set(WALLET_DIR src/wallet)
set(CORE_DIR src)
set(TOOLS_DIR src/tools)
set(CUCKAROO_DIR src/utils/cuckaroo)
set(CUDA_DIR ${CUCKAROO_DIR}/cuda)
aux_source_directory(${RPC_DIR} RPC_SRCS)
aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
aux_source_directory(${TASM_DIR} TASM_SRCS)
aux_source_directory(${CRYPTO_DIR} CRYPTO_SRCS)
aux_source_directory(${CORE_DIR} CORE_SRCS)
aux_source_directory(${SUPPORT_DIR} SUPPORT_SRCS)
aux_source_directory(${TOOLS_DIR} TOOLS_SRCS)
aux_source_directory(${INTERFACE_DIR} INTERFACE_SRCS)
aux_source_directory(${WALLET_DIR} WALLET_SRCS)
aux_source_directory(${CUCKAROO_DIR} CUCKAROO_SRCS)

list(REMOVE_ITEM CORE_SRCS src/main.cpp)

include_directories(src
        ${RPC_DIR}
        ${PROTO_SRC_DIR}
        ${NET_DIR}
        ${UTIL_DIR}
        ${CRYPTO_DIR}
        ${COMPAT_DIR}
        ${CORE_DIR}
        ${TASM_DIR}
        ${SPDLOG_DIR}
        ${SUPPORT_DIR}
        ${INTERFACE_DIR}
        ${WALLET_DIR}
        ${CUCKAROO_DIR}
        ${CUDA_DIR}
        )

set(SRC_CODE
        ${RPC_SRCS}
        ${PROTO_SRCS}
        ${GRPC_SRCS}
        ${NET_SRCS}
        ${UTIL_SRCS}
        ${CRYPTO_SRCS}
        ${CORE_SRCS}
        ${TASM_SRCS}
        ${SUPPORT_SRCS}
        ${INTERFACE_SRCS}
        ${WALLET_SRCS}
        ${CUCKAROO_SRCS}
        )

set(SRC_MAIN src/main.cpp)
set(SRC_CLIENT_MAIN src/epicc/main.cpp)

# test codes
set(TEST_METHODS_DIR test/test-methods)

aux_source_directory(test/rpc TEST_RPC_SRCS)
aux_source_directory(test/net TEST_NET_SRCS)
aux_source_directory(test/utils TEST_UTILS_SRCS)
aux_source_directory(test/tasm TEST_TASM_SRCS)
aux_source_directory(test/crypto TEST_CRYPTO_SRCS)
aux_source_directory(test/core TEST_CORE_SRCS)
aux_source_directory(test/support TEST_SUPPORT_SRCS)
aux_source_directory(${TEST_METHODS_DIR} TEST_METHODS_SRCS)

include_directories(test
    ${TEST_METHODS_DIR}
    )

set(TEST_CODE
        ${TEST_RPC_SRCS}
        ${TEST_NET_SRCS}
        ${TEST_UTILS_SRCS}
        ${TEST_TASM_SRCS}
        ${TEST_CRYPTO_SRCS}
        ${TEST_CORE_SRCS}
        ${TEST_SUPPORT_SRCS}
        ${TEST_METHODS_SRCS}
        )
set(TEST_MAIN test/main.cpp)

add_library(epiccore STATIC ${SRC_CODE})

# Cuda
option(EPIC_ENABLE_CUDA "Enable GPU mining when possible" ON)
find_package(CUDA)
if (CUDA_FOUND AND EPIC_ENABLE_CUDA)
    enable_language(CUDA)
    add_compile_definitions(__CUDA_ENABLED__)

    # cmake flags
    set(CUDA_PROPAGATE_HOST_FLAGS OFF)
    set(EXTRA_CUDA_DEBUG_FLAGS " -asan-force-dynamic-shadow=true")
    string(APPEND CMAKE_CXX_FLAGS_DEBUG ${EXTRA_CUDA_DEBUG_FLAGS})

    # nvcc flags
    set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-use_fast_math)
    list(APPEND CUDA_NVCC_FLAGS_RELEASE -O3)
    list(APPEND CUDA_NVCC_FLAGS_DEBUG -G)
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_35,code=sm_35) # default
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_52,code=sm_52) # maxwell
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_60,code=sm_60) # pascal
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_70,code=sm_70) # volta

    # link
    file(GLOB sources "${CUDA_DIR}/*.cu")
    file(GLOB headers "${CUDA_DIR}/*.cuh")
    cuda_add_library(epiccuda STATIC ${sources} ${headers})
    set_target_properties(epiccuda PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
    target_link_libraries(epiccore epiccuda)

    set(CMAKE_CXX_FLAGS_DEBUG ${DEFAULT_DEBUG_FLAGS})
else()
    message(STATUS "No CUDA support. Using CPU miner.")
endif()

add_executable(epic ${SRC_MAIN})
add_executable(epicc ${SRC_CLIENT_MAIN}
    ${RPC_DIR}/rpc_client.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
    )
target_link_libraries(epicc protobuf::libprotobuf)
target_link_libraries(epicc gRPC::grpc++_reflection)

add_executable(epictest ${TEST_MAIN} ${TEST_CODE})

target_link_libraries(epiccore ${LIBEVENT_LIBRARIES})
target_link_libraries(epiccore ${ROCKSDB_LIBRARIES})
target_link_libraries(epiccore ${Secp256k1_LIBRARY})
if (GMP_FOUND)
    target_link_libraries(epiccore ${GMP_LIBRARY})
endif()
target_link_libraries(epiccore protobuf::libprotobuf)
target_link_libraries(epiccore gRPC::grpc++_reflection)

target_link_libraries(epic epiccore)
add_dependencies(epic epiccore)

target_link_libraries(epictest ${GTEST_BOTH_LIBRARIES})
target_link_libraries(epictest epiccore)
add_dependencies(epictest epiccore)

add_executable(parseBlocks ${TOOLS_SRCS})
target_link_libraries(parseBlocks epiccore)
add_dependencies(parseBlocks epiccore)

option(UNITTEST_COVERAGE "coverage compile flag" OFF)
if (UNITTEST_COVERAGE)
    target_compile_options(epictest PUBLIC --coverage)
    target_compile_options(epiccore PUBLIC --coverage)
    target_link_options(epictest PUBLIC --coverage)
    target_link_options(epiccore PUBLIC --coverage)
endif()

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
