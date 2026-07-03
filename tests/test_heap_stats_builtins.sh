#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${NABLA_BUILD_DIR:-build}
CUSTOM_BUILD_DIR="$BUILD_DIR/heap_stats"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

cat >"$TMP_DIR/heap_stats_exact.nabla" <<'NABLA'
def main(): Int = {
    val before = heapUsed()
    val xs = new IntArray(3)
    xs.set(0, 7)
    val after = heapUsed()
    if heapCapacity() == 4096 && before == 0 && after > before && xs.get(0) == 7 {
        0
    } else {
        1
    }
}
NABLA

PATH=/opt/data/local/usr/bin:$PATH \
NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 4096 "$TMP_DIR/heap_stats_exact.nabla" \
    >"$TMP_DIR/compile.out" 2>"$TMP_DIR/compile.err"

set +e
"$CUSTOM_BUILD_DIR/heap_stats_exact" >"$TMP_DIR/run.out" 2>"$TMP_DIR/run.err"
status=$?
set -e
if [ "$status" -ne 0 ]; then
    echo "FAIL: heap stats program exited with $status" >&2
    echo "--- stdout ---" >&2
    cat "$TMP_DIR/run.out" >&2
    echo "--- stderr ---" >&2
    cat "$TMP_DIR/run.err" >&2
    exit 1
fi

if [ -s "$TMP_DIR/run.out" ] || [ -s "$TMP_DIR/run.err" ]; then
    echo "FAIL: heap stats exact test should not write output" >&2
    echo "--- stdout ---" >&2
    cat "$TMP_DIR/run.out" >&2
    echo "--- stderr ---" >&2
    cat "$TMP_DIR/run.err" >&2
    exit 1
fi

echo "PASS: heap stats builtins report used bytes and configured capacity"
