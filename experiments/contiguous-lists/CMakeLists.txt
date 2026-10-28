# Standalone experiment; build with: cmake -S . -B build -DCMAKE_PREFIX_PATH=<igraph 1.x prefix>
cmake_minimum_required(VERSION 3.18)
project(contiguous-lists LANGUAGES CXX)

find_package(igraph 1.0 REQUIRED)
include(CTest)

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(test_contiguous_list test_contiguous_list.cpp)
target_link_libraries(test_contiguous_list PRIVATE igraph::igraph)
add_test(NAME test_contiguous_list COMMAND test_contiguous_list)
