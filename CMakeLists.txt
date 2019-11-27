cmake_minimum_required(VERSION 3.14)
project(epic)
set(EPIC_MAJOR_VERSION 0)
set(EPIC_MINOR_VERSION 1)
set(EPIC_PATCH_VERSION 2)

set(CMAKE_CXX_STANDARD 17)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin/)
set(CMAKE_CXX_COMPILER "clang++")
add_definitions(-D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
add_compile_options(-Wall -Wthread-safety)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
option(ADDRESS_DETECT "sanitizer flag" ON)
if (ADDRESS-DETECT)
    STRING(APPEND CMAKE_CXX_FLAGS_DEBUG "-fsanitize=address -fno-omit-frame-pointer")
endif ()
set(CMAKE_CXX_FLAGS "-pthread -march=native $ENV{CXXFLAGS}")
set(CMAKE_VERBOSE_MAKEFILE OFF)

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
#include <nmmintrin.h>
int main() {
    _mm_crc32_u64(0,0);
    _mm_crc32_u32(0,0);
    _mm_crc32_u16(0,0);
    _mm_crc32_u8(0,0);
    return 0;
}
" HAVE_MM_CRC32)
if(HAVE_MM_CRC32)
    add_definitions(-DHAVE_MM_CRC32)
endif()

check_cxx_source_compiles("
#include <immintrin.h>
#include <wmmintrin.h>
int main() {
    uint64_t a = 0;
    uint64_t b = 0;
    _mm_clmulepi64_si128(_mm_loadu_si64(&a), _mm_loadu_si64(&b), 0x0);
    return 0;
}
" HAVE_MM_CLMULEPI)
if(HAVE_MM_CLMULEPI)
    add_definitions(-DHAVE_MM_CLMULEPI)
endif()

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

# openssl
find_package(OpenSSL REQUIRED)
target_link_libraries(OpenSSL::Crypto)
include_directories(${OPENSSL_INCLUDE_DIR})

# gtest
find_package(GTest 1.10.0 REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

#libevent
find_package(Libevent 2.1.11 EXACT REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})

# rocksdb
find_package(rocksdb 6.1.2 REQUIRED)
include_directories(${ROCKSDB_INCLUDE_DIRS})

# secp256k1
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
find_package(Secp256k1 REQUIRED)
find_library(GMP_LIBRARY NAMES gmp libgmp)
if (GMP_LIBRARY)
    MESSAGE(STATUS "Found libgmp")
    set(GMP_FOUND true)
else ()
    MESSAGE(STATUS "Not found libgmp")
endif ()
include_directories(${Secp256k1_INCLUDE_DIR})

# Protobuf and gRPC
find_package(Protobuf 3.10.0 REQUIRED)
find_package(GRPC REQUIRED)
set(RPC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc)
set(RPC_SERVICE_DIR ${RPC_DIR}/service)
set(PROTOS proto/rpc.proto)
set(PROTO_SRC_DIR ${RPC_DIR}/proto-gen)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

# Protobuf and gRPC autogeneration
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})

# generate version info
add_custom_target(version ALL
        ${CMAKE_COMMAND}
        -DSRC=${CMAKE_SOURCE_DIR}/cmake/version_config.h.in
        -DDST=${CMAKE_SOURCE_DIR}/src/config/version_config.h
        -DEPIC_VERSION="${EPIC_MAJOR_VERSION}.${EPIC_MINOR_VERSION}.${EPIC_PATCH_VERSION}"
        -P ${CMAKE_SOURCE_DIR}/cmake/version.cmake
)
include_directories(${CMAKE_SOURCE_DIR}/src/config)

# src codes
set(NET_DIR src/net)
set(PEER_DIR src/peer)
set(UTIL_DIR src/utils)
set(HASH_DIR src/hash)
set(CRYPTO_DIR src/crypto)
set(COMPAT_DIR src/compat)
set(MEMORY_DIR src/utils/memory)
set(TASM_DIR src/tasm)
set(SPDLOG_DIR src/utils/spdlog)
set(WALLET_DIR src/wallet)
set(ROOT_DIR src)
set(TOOLS_DIR src/tools)
set(CUCKAROO_DIR src/pow/cuckaroo)
set(CUDA_DIR ${CUCKAROO_DIR}/cuda)
set(CONSENSUS_DIR src/consensus)
set(MESSAGES_DIR src/messages)
set(POW_DIR src/pow)
set(STORAGE_DIR src/storage)
set(REMOTE_SOLVER_DIR src/remote_solver)

aux_source_directory(${RPC_DIR} RPC_SRCS)
aux_source_directory(${RPC_SERVICE_DIR} RPC_SERVICE_SRCS)
aux_source_directory(${NET_DIR} NET_SRCS)
aux_source_directory(${PEER_DIR} PEER_SRCS)
aux_source_directory(${UTIL_DIR} UTIL_SRCS)
aux_source_directory(${TASM_DIR} TASM_SRCS)
aux_source_directory(${HASH_DIR} HASH_SRCS)
aux_source_directory(${CRYPTO_DIR} CRYPTO_SRCS)
aux_source_directory(${ROOT_DIR} ROOT_SRCS)
aux_source_directory(${MEMORY_DIR} MEMORY_SRCS)
aux_source_directory(${TOOLS_DIR} TOOLS_SRCS)
aux_source_directory(${WALLET_DIR} WALLET_SRCS)
aux_source_directory(${CUCKAROO_DIR} CUCKAROO_SRCS)
aux_source_directory(${CONSENSUS_DIR} CONSENSUS_SRCS)
aux_source_directory(${MESSAGES_DIR} MESSAGES_SRCS)
aux_source_directory(${POW_DIR} POW_SRCS)
aux_source_directory(${STORAGE_DIR} STORAGE_SRCS)
aux_source_directory(${REMOTE_SOLVER_DIR} REMOTE_SOLVER_SRCS)

list(REMOVE_ITEM ROOT_SRCS src/epic.cpp)
list(REMOVE_ITEM ROOT_SRCS src/epic-cli.cpp)

include_directories(src
        ${RPC_DIR}
        ${RPC_SERVICE_DIR}
        ${PROTO_SRC_DIR}
        ${NET_DIR}
        ${PEER_DIR}
        ${UTIL_DIR}
        ${HASH_DIR}
        ${CRYPTO_DIR}
        ${COMPAT_DIR}
        ${ROOT_DIR}
        ${TASM_DIR}
        ${SPDLOG_DIR}
        ${MEMORY_DIR}
        ${INTERFACE_DIR}
        ${WALLET_DIR}
        ${CUCKAROO_DIR}
        ${CUDA_DIR}
        ${CONSENSUS_DIR}
        ${MESSAGES_DIR}
        ${POW_DIR}
        ${STORAGE_DIR}
        ${TOOLS_DIR}
        ${REMOTE_SOLVER_DIR}
        )

set(SRC_CODE
        ${RPC_SRCS}
        ${RPC_SERVICE_SRCS}
        ${NET_SRCS}
        ${PEER_SRCS}
        ${UTIL_SRCS}
        ${HASH_SRCS}
        ${CRYPTO_SRCS}
        ${ROOT_SRCS}
        ${TASM_SRCS}
        ${MEMORY_SRCS}
        ${INTERFACE_SRCS}
        ${WALLET_SRCS}
        ${CONSENSUS_SRCS}
        ${MESSAGES_SRCS}
        ${POW_SRCS}
        ${STORAGE_SRCS}
        )

add_library(cucakroo STATIC ${CUCKAROO_SRCS})
add_library(epic_grpc STATIC ${PROTO_SRCS} ${GRPC_SRCS})
add_library(epiccore STATIC ${SRC_CODE})

# link epiccore with external libs
target_link_libraries(epiccore epic_grpc)
target_link_libraries(epiccore cucakroo)
target_link_libraries(epiccore ${OPENSSL_CRYPTO_LIBRARY})
target_link_libraries(epiccore ${LIBEVENT_SHARED_LIBRARIES})
target_link_libraries(epiccore ${ROCKSDB_LIBRARIES})
target_link_libraries(epiccore ${Secp256k1_LIBRARY})
target_link_libraries(epiccore protobuf::libprotobuf)
target_link_libraries(epiccore gRPC::grpc++_reflection)
if (GMP_FOUND)
    target_link_libraries(epiccore ${GMP_LIBRARY})
endif ()
if (NOT CMAKE_HOST_APPLE)
    target_link_libraries(epiccore atomic)
    target_link_libraries(epiccore stdc++fs)
endif ()


# daemon
set(SRC_MAIN src/epic.cpp)
add_executable(epic ${SRC_MAIN})
target_link_libraries(epic epiccore)
add_dependencies(epic epiccore)
add_dependencies(epic version)

# commandline
set(SRC_CLIENT_MAIN src/epic-cli.cpp)
add_executable(epic-cli ${SRC_CLIENT_MAIN}
        ${RPC_DIR}/rpc_client.cpp
        ${RPC_SERVICE_DIR}/return_code.cpp
        ${PROTO_SRCS}
        ${GRPC_SRCS}
        src/version.cpp
        )
target_link_libraries(epic-cli protobuf::libprotobuf)
target_link_libraries(epic-cli gRPC::grpc++_reflection)
target_include_directories(epic-cli PRIVATE src/cli)
add_dependencies(epic-cli version)

# test codes
set(TEST_METHODS_DIR test/test-methods)

aux_source_directory(test/rpc TEST_RPC_SRCS)
aux_source_directory(test/net TEST_NET_SRCS)
aux_source_directory(test/utils TEST_UTILS_SRCS)
aux_source_directory(test/tasm TEST_TASM_SRCS)
aux_source_directory(test/crypto TEST_CRYPTO_SRCS)
aux_source_directory(test/consensus TEST_CONSENSUS_SRCS)
aux_source_directory(test/peer TEST_PEER_SRCS)
aux_source_directory(test/storage TEST_STORAGE_SRCS)
aux_source_directory(test/pow TEST_POW_SRCS)
aux_source_directory(${TEST_METHODS_DIR} TEST_METHODS_SRCS)
aux_source_directory(test/wallet TEST_WALLET_SRCS)

include_directories(test
        ${TEST_METHODS_DIR}
        )

set(TEST_CODE
        ${TEST_RPC_SRCS}
        ${TEST_NET_SRCS}
        ${TEST_UTILS_SRCS}
        ${TEST_TASM_SRCS}
        ${TEST_CRYPTO_SRCS}
        ${TEST_CONSENSUS_SRCS}
        ${TEST_PEER_SRCS}
        ${TEST_STORAGE_SRCS}
        ${TEST_METHODS_SRCS}
        ${TEST_POW_SRCS}
        ${TEST_WALLET_SRCS}
        )
set(TEST_MAIN test/main.cpp)

# test executable
add_executable(epictest ${TEST_MAIN} ${TEST_CODE})
target_link_libraries(epictest ${GTEST_BOTH_LIBRARIES})
target_link_libraries(epictest epiccore)
add_dependencies(epictest epiccore)

# tools executables
add_executable(parseBlocks src/tools/blockParser.cpp)
target_link_libraries(parseBlocks epiccore)
add_dependencies(parseBlocks epiccore)

add_executable(mineGenesis src/tools/mineGenesis.cpp ${TEST_METHODS_SRCS})
target_link_libraries(mineGenesis epiccore)
add_dependencies(mineGenesis epiccore)

# solver based on CUDA
option(EPIC_ENABLE_CUDA "Enable GPU mining when possible" ON)
find_package(CUDA)
if (CUDA_FOUND AND EPIC_ENABLE_CUDA)
    enable_language(CUDA)
    add_compile_definitions(__CUDA_ENABLED__)

    # cmake flags
    set(CUDA_PROPAGATE_HOST_FLAGS OFF)

    # nvcc flags
    set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-use_fast_math)
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_35,code=sm_35) # default
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_52,code=sm_52) # maxwell
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_60,code=sm_60) # pascal
    list(APPEND CUDA_NVCC_FLAGS -gencode arch=compute_70,code=sm_70) # volta

    # link
    file(GLOB sources "${CUDA_DIR}/*.cu")
    file(GLOB headers "${CUDA_DIR}/*.cuh")
    cuda_add_library(epiccuda STATIC ${sources} ${headers})
    set_target_properties(epiccuda PROPERTIES CUDA_SEPARABLE_COMPILATION ON)

    # utils, hash and grpc codes used in solver
    set(SOLVER_UTILS_SRCS
            src/utils/threadpool.cpp
            src/utils/arith_uint256.cpp
            src/utils/big_uint.cpp
            src/utils/utilstrencodings.cpp
            src/utils/memory/cleanse.cpp
            )

    set(SOLVER_HASH_SRCS
            src/hash/hash.cpp
            src/hash/sha256.cpp
            src/hash/blake2b.cpp
            )

    set(SOLVER_RPC
            src/rpc/basic_rpc_server.cpp)

    list(REMOVE_ITEM REMOTE_SOLVER_SRCS src/remote_solver/epic_solver.cpp)

    set(SRC_SOLVER ${REMOTE_SOLVER_SRCS} ${SOLVER_RPC} ${SOLVER_UTILS_SRCS} ${SOLVER_HASH_SRCS})
    add_library(solver_lib STATIC ${SRC_SOLVER})

    target_link_libraries(solver_lib cucakroo)
    target_link_libraries(solver_lib epiccuda)
    target_link_libraries(solver_lib epic_grpc)
    target_link_libraries(solver_lib protobuf::libprotobuf)
    target_link_libraries(solver_lib gRPC::grpc++_reflection)

    add_executable(solver src/remote_solver/epic_solver.cpp)
    target_link_libraries(solver solver_lib)

    target_link_libraries(epiccore solver_lib)
    add_dependencies(epiccore solver_lib)

else ()
    message(STATUS "CUDA disabled. Using CPU miner.")
endif ()

option(UNITTEST_COVERAGE "coverage compile flag" OFF)
if (UNITTEST_COVERAGE)
    target_compile_options(epictest PUBLIC --coverage)
    target_compile_options(epiccore PUBLIC --coverage)
    target_link_options(epictest PUBLIC --coverage)
    target_link_options(epiccore PUBLIC --coverage)
endif ()

# add test
enable_testing()
gtest_add_tests(
        TARGET epictest
)

# install
install(TARGETS epic DESTINATION bin)

# display cmake options
MESSAGE(STATUS "Build type: " ${CMAKE_BUILD_TYPE})
MESSAGE(STATUS "Compiler flags:" ${CMAKE_CXX_COMPILE_FLAGS})
MESSAGE(STATUS "Compiler cxx debug flags:" ${CMAKE_CXX_FLAGS_DEBUG})
MESSAGE(STATUS "Compiler cxx release flags:" ${CMAKE_CXX_FLAGS_RELEASE})
MESSAGE(STATUS "Compiler cxx flags:" ${CMAKE_CXX_FLAGS})
