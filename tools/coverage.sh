#!/bin/sh
# Code coverage of athrill-arm-common's headers by its tests (clang
# source-based coverage): line, branch and function coverage per header.
#   tools/coverage.sh [build dir]
set -e
cd "$(dirname "$0")/.."
build="${1:-build-coverage}"
if command -v xcrun >/dev/null 2>&1; then
  profdata="xcrun llvm-profdata"
  cov="xcrun llvm-cov"
else
  profdata="llvm-profdata"
  cov="llvm-cov"
fi
cmake -S . -B "$build" -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DATHRILL_ARM_COMMON_COVERAGE=ON >/dev/null
cmake --build "$build" >/dev/null
rm -f "$build"/*.profraw
for t in test_helpers test_pseudo test_pseudo_ls test_pseudo_misc; do
  LLVM_PROFILE_FILE="$build/$t.profraw" "$build/$t" >/dev/null
done
$profdata merge -o "$build/tests.profdata" "$build"/*.profraw
$cov report -instr-profile="$build/tests.profdata" "$build/test_pseudo" -object "$build/test_helpers" -object "$build/test_pseudo_ls" -object "$build/test_pseudo_misc" \
  include/arm_common/*.h
