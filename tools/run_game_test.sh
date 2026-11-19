#!/usr/bin/env bash
# Mountain - run one automated game test headless: a virtual X display with
# software OpenGL (Mesa llvmpipe) and the dummy audio driver.
# Usage: tools/run_game_test.sh <mountain executable> <test.lua> [data dir]
set -euo pipefail
EXE="$1"
TEST="$2"
NAME="$(basename "$TEST" .lua)"
DATA="${3:-$(dirname "$EXE")/game-tests/$NAME}"
rm -rf "$DATA"
mkdir -p "$DATA"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
export LIBGL_ALWAYS_SOFTWARE=1
if [ -n "${DISPLAY:-}" ] && [ -z "${MTN_FORCE_XVFB:-}" ]; then
  exec "$EXE" --test "$TEST" --data-dir "$DATA" --frames 20000
fi
exec xvfb-run -a -s "-screen 0 1280x720x24" "$EXE" --test "$TEST" --data-dir "$DATA" --frames 20000
