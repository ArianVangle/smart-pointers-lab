#!/bin/sh
set -e
cd "$(dirname "$0")"

SOURCES="src/main.cpp src/Tests.cpp src/Benchmark.cpp src/Demo.cpp src/Ui.cpp src/Output.cpp src/AllocCounter.cpp"
COMMON="-std=c++20 -Wall -Wextra -Wpedantic -Iinclude -Isrc"

mkdir -p build

${CXX:-g++} $COMMON -O2 -DNDEBUG $SOURCES -o build/lab1
echo "Собрано: ./build/lab1   (--test | --bench [maxN] | --demo | --plot)"

if [ "$1" = "--sanitize" ]; then
  ${CXX:-g++} $COMMON -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    $SOURCES -o build/lab1_asan
  echo "Собрано: ./build/lab1_asan (ASan + UBSan + LeakSanitizer)"
fi
