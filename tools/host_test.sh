#!/usr/bin/env bash
# Reliable host-test runner for this Windows machine.
#
# Why this exists (see docs/plans Discoveries): invoking the host tests via the
# default git-bash is fragile — `g++` resolves to msys64's cygwin compiler whose
# cc1plus can't find its DLLs unless mingw64/bin is on PATH, and `make`'s recipe
# shell strips TMP/TEMP so the assembler tries to write to C:\WINDOWS (denied).
# This script fixes both: it puts mingw64 on PATH and exports a writable temp dir,
# then compiles directly (bypassing make's env-stripping). Use this instead of a
# bare `make -C test` on this machine.
set -euo pipefail

export PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH"
export TMP="C:/Users/baranmcl/AppData/Local/Temp"
export TEMP="$TMP"
export TMPDIR="$TMP"

# repo root = parent of this script's tools/ dir
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "== logic purity =="
python tools/check_logic_purity.py

echo "== level compiler unit tests =="
( cd tools && python -m unittest test_build_level.py )

echo "== regenerate level headers (so test_*_level.cpp use fresh data) =="
shopt -s nullglob
for f in tools/levels/*.txt; do
    python tools/build_level.py "$f" "include/game/levels/$(basename "${f%.txt}").h"
done

echo "== compile + run host tests =="
cd test
TEST_SRC=$(ls test_*.cpp)
LOGIC_SRC=$(ls ../src/logic/*.cpp 2>/dev/null || true)
g++ -std=c++17 -O0 -g -Wall -Wextra -I../include -I. $TEST_SRC $LOGIC_SRC -o run_tests
./run_tests
