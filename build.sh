#!/bin/zsh

set -e

# cmake -S. -Bbuild
cmake --build build -j8 # --clean-first

pushd build && ctest --output-on-failure && popd
