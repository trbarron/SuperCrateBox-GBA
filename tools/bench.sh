#!/bin/bash
# Build an auto-running benchmark ROM, run it in an emulator, and print the
# frame-time figures it leaves in cartridge SRAM.
#
# The ROM runs a fixed thirty second scripted game with the player invincible,
# the monster pool kept full and the weapon rotated through all eleven, then
# writes its results to SRAM offset 64 over and over, so whenever the emulator
# next flushes its battery file the finished figures are in it.
#
# Usage: tools/bench.sh [label]
set -e

LABEL="${1:-run}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${BENCH_OUT:-/tmp/scb-bench}"
EMU="${MGBA:-$HOME/.local/opt/mGBA.app}"
RUN_SECONDS="${BENCH_SECONDS:-70}"

mkdir -p "$OUT"
cd "$HERE"

echo "building benchmark ROM..."
cp -f main.gba "$OUT/main.gba.keep" 2>/dev/null || true
rm -f ./*.o
make CFLAGS="-g -O3 -Wall -DBENCH_AUTORUN" >/dev/null
cp -f main.gba "$OUT/bench.gba"

echo "restoring the normal ROM..."
rm -f ./*.o
make >/dev/null

rm -f "$OUT/bench.sav"
echo "running for ${RUN_SECONDS}s..."
open -a "$EMU" --args "$OUT/bench.gba"
sleep "$RUN_SECONDS"
kill -INT "$(pgrep -f 'mGBA.app/Contents/MacOS/mGBA' | head -1)" 2>/dev/null || true
sleep 2

python3 "$HERE/tools/benchreport.py" "$OUT/bench.sav" "$LABEL"
