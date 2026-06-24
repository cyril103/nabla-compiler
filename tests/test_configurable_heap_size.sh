#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${NABLA_BUILD_DIR:-build}
mkdir -p "$BUILD_DIR"
CUSTOM_BUILD_DIR="$BUILD_DIR/configurable_heap"
TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"

expect_invalid_heap_size() {
    local value=$1
    local label=$2
    local stderr_file="$TMP_DIR/${label}.err"
    if NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size "$value" tests/test_configurable_heap_size.nabla >"$TMP_DIR/${label}.out" 2>"$stderr_file"; then
        echo "FAIL: --heap-size $value aurait dû échouer" >&2
        exit 1
    fi
    if ! grep -q "Argument invalide pour --heap-size" "$stderr_file"; then
        echo "FAIL: diagnostic manquant pour --heap-size $value" >&2
        exit 1
    fi
}

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 1048576 --keep-temp tests/test_configurable_heap_size.nabla >"$TMP_DIR/compile.out" 2>"$TMP_DIR/compile.err"

ASM_FILE="$CUSTOM_BUILD_DIR/test_configurable_heap_size_tmp.asm"
if [ ! -f "$ASM_FILE" ]; then
    echo "FAIL: fichier assembleur attendu absent: $ASM_FILE" >&2
    exit 1
fi

if ! grep -q "heap_capacity: dq 1048576" "$ASM_FILE"; then
    echo "FAIL: --heap-size n'a pas été propagé dans l'assembleur" >&2
    exit 1
fi

set +e
"$CUSTOM_BUILD_DIR/test_configurable_heap_size" >/dev/null 2>&1
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "FAIL: exécutable compilé avec heap personnalisé a retourné $status au lieu de 42" >&2
    exit 1
fi

NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size 4096 --keep-temp tests/test_configurable_heap_size.nabla >"$TMP_DIR/minimum.out" 2>"$TMP_DIR/minimum.err"
if ! grep -q "heap_capacity: dq 4096" "$ASM_FILE"; then
    echo "FAIL: --heap-size 4096 aurait dû être accepté et propagé" >&2
    exit 1
fi

expect_invalid_heap_size 0 zero
expect_invalid_heap_size 4095 below_minimum
expect_invalid_heap_size -1 negative
expect_invalid_heap_size +4096 plus_sign
expect_invalid_heap_size abc non_integer

if NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size tests/test_configurable_heap_size.nabla >"$TMP_DIR/missing.out" 2>"$TMP_DIR/missing.err"; then
    echo "FAIL: --heap-size sans valeur numérique aurait dû échouer" >&2
    exit 1
fi
if ! grep -q "Argument invalide pour --heap-size" "$TMP_DIR/missing.err"; then
    echo "FAIL: diagnostic manquant pour --heap-size sans valeur numérique" >&2
    exit 1
fi

echo "PASS: --heap-size configure la capacité du heap et valide les entrées"
