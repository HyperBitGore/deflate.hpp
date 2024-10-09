cmake_minimum_required(VERSION 3.5.0)

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



add_executable(libdeflate_test libdeflate.cpp)

if (UNIX)
target_link_libraries(libdeflate_test ${CMAKE_BINARY_DIR}/external/lib/libdeflate.a)
endif(UNIX)