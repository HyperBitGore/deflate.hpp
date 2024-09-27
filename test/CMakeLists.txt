cmake_minimum_required(VERSION 3.5.0)

project(libdeflate_test VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED On)
set(CMAKE_CXX_EXTENSIONS Off)
set(CMAKE_EXPORT_COMPILE_COMMANDS On)

add_executable(libdeflate_test libdeflate.cpp)

target_link_libraries(libdeflate_test ${CMAKE_SOURCE_DIR}/build/libdeflate.a)