cmake_minimum_required(VERSION 3.16)
project(SmartPointersLab1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

option(LAB1_SANITIZE "Build with AddressSanitizer + UBSan (leak detection)" OFF)

add_executable(lab1
  src/main.cpp
  src/Tests.cpp
  src/Benchmark.cpp
  src/Demo.cpp
  src/Ui.cpp
  src/Output.cpp
  src/AllocCounter.cpp
)

target_include_directories(lab1 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
  target_compile_options(lab1 PRIVATE /W4 /utf-8)
else()
  target_compile_options(lab1 PRIVATE -Wall -Wextra -Wpedantic)
  if(LAB1_SANITIZE)
    target_compile_options(lab1 PRIVATE -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(lab1 PRIVATE -fsanitize=address,undefined)
    set_target_properties(lab1 PROPERTIES OUTPUT_NAME lab1_asan)
  endif()
endif()

enable_testing()
add_test(NAME functional_tests COMMAND lab1 --test)
