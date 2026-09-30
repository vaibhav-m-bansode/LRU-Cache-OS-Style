cmake_minimum_required(VERSION 3.16)
project(lru_cache_engine VERSION 1.0.0 LANGUAGES CXX)

option(LRU_BUILD_TESTS "Build the self-contained unit tests" ON)
option(LRU_BUILD_BENCHMARK "Build the benchmark executable" ON)

add_library(lru_cache INTERFACE)
target_include_directories(lru_cache INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(lru_cache INTERFACE cxx_std_17)

add_executable(lru-cli src/main.cpp)
target_link_libraries(lru-cli PRIVATE lru_cache)

if(LRU_BUILD_TESTS)
  enable_testing()
  add_executable(lru-tests tests/lru_cache_test.cpp)
  target_link_libraries(lru-tests PRIVATE lru_cache)
  add_test(NAME lru-cache-tests COMMAND lru-tests)
endif()

if(LRU_BUILD_BENCHMARK)
  add_executable(lru-benchmark benchmarks/lru_benchmark.cpp)
  target_link_libraries(lru-benchmark PRIVATE lru_cache)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  foreach(target lru-cli lru-tests lru-benchmark)
    if(TARGET ${target})
      target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    endif()
  endforeach()
endif()
