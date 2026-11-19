#!/usr/bin/env bash
# Mountain - build with coverage, run unit tests and game tests, report and
# enforce the line coverage gate over the engine sources (third_party excluded).
# Usage: tools/coverage.sh [build-dir] [min-percent]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build/coverage}"
MIN="${2:-95}"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMTN_COVERAGE=ON ${CMAKE_EXTRA:-} > /dev/null
cmake --build "$BUILD"
find "$BUILD" -name '*.gcda' -delete
(cd "$BUILD" && ctest --output-on-failure ${CTEST_EXTRA:-})
mkdir -p "$BUILD/report"
gcovr --root "$ROOT" --object-directory "$BUILD" --filter "$ROOT/engine/" \
      --exclude-unreachable-branches --print-summary \
      --html-details "$BUILD/report/index.html" --txt "$BUILD/report/coverage.txt" \
      --fail-under-line "$MIN" "$BUILD"
