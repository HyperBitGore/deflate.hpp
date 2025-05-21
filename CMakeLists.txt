cmake_minimum_required(VERSION 4.0.0)


project(deflate VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

# Detect if we're in a Debug build
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug build detected: enabling AddressSanitizer")

    # Add AddressSanitizer flags for Debug
    set(ASAN_FLAGS "-fsanitize=address")

    # Apply to CXX and C compilers
    add_compile_options(${ASAN_FLAGS})
    add_link_options(${ASAN_FLAGS})
endif()

add_executable(deflate include/deflate.hpp include/common.hpp include/inflate.hpp test/example.cpp)

project(libdeflate_test VERSION 0.0.1 LANGUAGES C CXX)
include(ExternalProject)
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
ExternalProject_Add(
    libdeflate
    GIT_REPOSITORY https://github.com/ebiggers/libdeflate.git
    GIT_TAG origin/master
    GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)



add_executable(libdeflate_test test/libdeflate.cpp)

if (UNIX)
target_link_libraries(libdeflate_test ${CMAKE_BINARY_DIR}/external/lib/libdeflate.a)
endif(UNIX)
if (WIN32)
target_link_libraries(libdeflate_test ${CMAKE_BINARY_DIR}/external/lib/deflatestatic.lib)
endif(WIN32)