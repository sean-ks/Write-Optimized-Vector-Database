cmake_minimum_required(VERSION 3.20)
project(woved VERSION 0.1.0 LANGUAGES CXX)

# Project options
option(WOVED_USE_IOURING "Enable io_uring support" ON)
option(WOVED_USE_LIBCXX "Use libc++ instead of libstdc++" OFF)
option(WOVED_ENABLE_LTO "Enable Link Time Optimization" ON)
option(WOVED_ENABLE_PGO "Enable Profile Guided Optimization" OFF)
option(WOVED_CPU_BASELINE "CPU baseline architecture" "x86-64-v3")
option(WOVED_CPU_AVX2 "Build AVX2 kernels" ON)
option(WOVED_CPU_AVX512 "Build AVX-512 kernels" ON)
option(WOVED_USE_PMEM "Enable persistent memory support" OFF)
option(WOVED_BUILD_TESTS "Build test suite" ON)
option(WOVED_BUILD_BENCH "Build benchmarks" ON)
option(WOVED_BUILD_TOOLS "Build admin tools" ON)
option(WOVED_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(WOVED_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# Include custom modules
include(CompilerOptions)
include(CPUDispatch)
include(Sanitizers)

# Find dependencies
find_package(Threads REQUIRED)
find_package(OpenMP REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(flatbuffers REQUIRED)
find_package(GTest)
find_package(benchmark)
find_package(PkgConfig REQUIRED)

# Find FAISS
find_package(faiss REQUIRED)

# Find CRoaring
find_package(Roaring REQUIRED)

# Find compression libraries
find_package(ZLIB REQUIRED)
find_package(lz4 REQUIRED)
find_package(zstd REQUIRED)

# Find prometheus-cpp
find_package(prometheus-cpp REQUIRED)

# Find spdlog package
find_package(spdlog REQUIRED)

# Link your target with spdlog
target_link_libraries(YourTargetName PRIVATE spdlog::spdlog)

# Find io_uring if enabled
if(WOVED_USE_IOURING)
    pkg_check_modules(LIBURING REQUIRED liburing>=2.0)
    add_definitions(-DWOVED_USE_IOURING)
endif()

# Find PMDK if enabled
if(WOVED_USE_PMEM)
    find_package(PMDK REQUIRED)
    add_definitions(-DWOVED_USE_PMEM)
endif()

# Find hwloc for NUMA
pkg_check_modules(HWLOC REQUIRED hwloc>=2.0)

# Configure compiler options
configure_compiler_options()

# Configure CPU dispatch
configure_cpu_dispatch()

# Configure sanitizers
if(WOVED_ENABLE_ASAN OR WOVED_ENABLE_UBSAN)
    configure_sanitizers()
endif()

# Generate version header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/woved/version.h
)

# FlatBuffers schemas
set(FBS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/storage/wal)
set(FBS_FILES ${FBS_DIR}/wal_record.fbs)

foreach(FBS_FILE ${FBS_FILES})
    get_filename_component(FBS_NAME ${FBS_FILE} NAME_WE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/${FBS_NAME}_generated.h
        COMMAND flatc --cpp -o ${CMAKE_CURRENT_BINARY_DIR}/include ${FBS_FILE}
        DEPENDS ${FBS_FILE}
        COMMENT "Generating FlatBuffers header for ${FBS_NAME}"
    )
    list(APPEND FBS_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/include/${FBS_NAME}_generated.h)
endforeach()

add_custom_target(generate_fbs ALL DEPENDS ${FBS_HEADERS})

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${HWLOC_INCLUDE_DIRS}
)

# Build distance kernels
add_library(woved_kernels_base OBJECT src/kernels/distance_base.cpp)
target_compile_options(woved_kernels_base PRIVATE -march=${WOVED_CPU_BASELINE})

if(WOVED_CPU_AVX2)
    add_library(woved_kernels_avx2 OBJECT src/kernels/distance_avx2.cpp)
    target_compile_options(woved_kernels_avx2 PRIVATE -mavx2 -mfma)
endif()

if(WOVED_CPU_AVX512)
    add_library(woved_kernels_avx512 OBJECT src/kernels/distance_avx512.cpp)
    target_compile_options(woved_kernels_avx512 PRIVATE -mavx512f -mavx512dq)
endif()

# Main library
file(GLOB_RECURSE WOVED_SOURCES
    src/api/*.cpp
    src/core/*.cpp
    src/storage/*.cpp
    src/index/*.cpp
    src/bitmaps/*.cpp
    src/io/*.cpp
    src/util/*.cpp
)

add_library(woved_core STATIC
    ${WOVED_SOURCES}
    $<TARGET_OBJECTS:woved_kernels_base>
    $<$<BOOL:${WOVED_CPU_AVX2}>:$<TARGET_OBJECTS:woved_kernels_avx2>>
    $<$<BOOL:${WOVED_CPU_AVX512}>:$<TARGET_OBJECTS:woved_kernels_avx512>>
)

add_dependencies(woved_core generate_fbs)

target_link_libraries(woved_core PUBLIC
    Threads::Threads
    OpenMP::OpenMP_CXX
    protobuf::libprotobuf
    gRPC::grpc++
    faiss
    Roaring::roaring
    ZLIB::ZLIB
    lz4::lz4
    zstd::libzstd_static
    prometheus-cpp::core
    prometheus-cpp::pull
    ${HWLOC_LIBRARIES}
)

if(WOVED_USE_IOURING)
    target_link_libraries(woved_core PUBLIC ${LIBURING_LIBRARIES})
endif()

if(WOVED_USE_PMEM)
    target_link_libraries(woved_core PUBLIC pmem pmem2)
endif()

# Main executable
add_executable(wovedd src/main.cpp)
target_link_libraries(wovedd PRIVATE woved_core)

# Proto library
add_subdirectory(proto)

# Tests
if(WOVED_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Benchmarks
if(WOVED_BUILD_BENCH)
    add_subdirectory(tests/bench)
endif()

# Tools
if(WOVED_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

# Installation
install(TARGETS wovedd
    RUNTIME DESTINATION bin
)

install(DIRECTORY configs/
    DESTINATION etc/woved
)

# CPack configuration
set(CPACK_PACKAGE_NAME "woved")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION "Write-Optimized Vector Database")
set(CPACK_PACKAGE_CONTACT "support@woved.io")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6, libgomp1")
set(CPACK_GENERATOR "DEB;TGZ")

include(CPack)
