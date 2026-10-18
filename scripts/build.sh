#!/usr/bin/env sh
# Quick GCC/Clang build without CMake (Linux/macOS/WSL).
set -e
cd "$(dirname "$0")/.."
mkdir -p build
c++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude \
    src/*.cpp -o build/ripple
echo "Built ./build/ripple"
