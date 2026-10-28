# Builds the prototype tests. Standalone:
#   cmake -S prototype -B build-proto -DCMAKE_PREFIX_PATH=<igraph 1.x install>
#   cmake --build build-proto && ctest --test-dir build-proto
cmake_minimum_required(VERSION 3.18)
project(igraph-cpp-prototype LANGUAGES CXX)

find_package(igraph 1.0 REQUIRED CONFIG)
include(CTest)

option(PROTO_SANITIZE "Build the prototype tests with ASan and UBSan" ON)

function(proto_test NAME SOURCE STD)
    add_executable(${NAME} ${SOURCE})
    target_link_libraries(${NAME} PRIVATE igraph::igraph)
    target_compile_features(${NAME} PRIVATE cxx_std_${STD})
    target_compile_options(${NAME} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>)
    if(PROTO_SANITIZE AND NOT MSVC)
        target_compile_options(${NAME} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
        target_link_options(${NAME} PRIVATE -fsanitize=address,undefined)
    endif()
    add_test(NAME ${NAME} COMMAND ${NAME})
endfunction()

proto_test(test_inplace_cxx17 inplace/test_inplace.cpp 17)
proto_test(test_inplace_cxx20 inplace/test_inplace.cpp 20)
proto_test(test_proxy_cxx20 proxy/test_proxy.cpp 20)
