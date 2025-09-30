#!/bin/zsh
set -e

BUILD_DIR=build
TEST_TARGET=looperTest
TEST_TARGET_DIR="${BUILD_DIR}/test/${TEST_TARGET}"
PROFILE_FILE="${PWD}/${BUILD_DIR}/coverage-%p.profraw"
MERGED_FILE="${BUILD_DIR}/coverage.profdata"
HTML_DIR="${BUILD_DIR}/coverage_html"

cmake -S . -B "$BUILD_DIR" -DENABLE_COVERAGE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" -j8

export LLVM_PROFILE_FILE="$PROFILE_FILE"
cd "$BUILD_DIR"
ctest --output-on-failure
cd ..

rm -rf ${MERGED_FILE}
llvm-profdata merge -sparse "$BUILD_DIR"/coverage-*.profraw -o "$MERGED_FILE"
rm -rf ${BUILD_DIR}/*.profraw

llvm-cov show "$TEST_TARGET_DIR" \
    -instr-profile="$MERGED_FILE" \
    -format=html \
    -output-dir="$HTML_DIR" \
    --show-branches=percent \
    $(find "$PWD/plugin/source" -name '*.cpp' -o -name '*.h')

echo "Coverage report generated at $HTML_DIR/index.html"
