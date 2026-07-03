#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${NABLA_BUILD_DIR:-build}
mkdir -p "$BUILD_DIR"
CUSTOM_BUILD_DIR="$BUILD_DIR/heap_overflow"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

compile_and_expect_heap_overflow() {
    local source_file=$1
    local label=$2

    PATH=/opt/data/local/usr/bin:$PATH \
    NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 4096 "$source_file" >"$TMP_DIR/${label}.compile.out" 2>"$TMP_DIR/${label}.compile.err"

    local executable="$CUSTOM_BUILD_DIR/$(basename "$source_file" .nabla)"
    set +e
    "$executable" >"$TMP_DIR/${label}.run.out" 2>"$TMP_DIR/${label}.run.err"
    local status=$?
    set -e

    if [ "$status" -ne 255 ]; then
        echo "FAIL: dépassement heap attendu avec code 255 pour $source_file, reçu $status" >&2
        exit 1
    fi

    local expected="Nabla runtime error: heap exhausted"
    local actual
    actual=$(tr -d '\r' <"$TMP_DIR/${label}.run.err" | sed 's/[[:space:]]*$//')
    if [ "$actual" != "$expected" ]; then
        echo "FAIL: diagnostic heap overflow inattendu pour $source_file" >&2
        printf 'expected: %s\n' "$expected" >&2
        printf 'actual: %s\n' "$actual" >&2
        exit 1
    fi

    if [ -s "$TMP_DIR/${label}.run.out" ]; then
        echo "FAIL: le diagnostic heap overflow doit aller sur stderr, pas stdout ($source_file)" >&2
        exit 1
    fi
}

compile_and_expect_heap_overflow tests/test_heap_overflow_diagnostic.nabla ordinary_overflow
compile_and_expect_heap_overflow tests/test_heap_array_size_overflow.nabla array_size_overflow

echo "PASS: dépassements heap signalés code 255 et diagnostic stderr"
