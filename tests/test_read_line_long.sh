#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cat >"$TMP_DIR/read_line_long.nabla" <<'NABLA'
def main(): Int = {
    val first = readLine()
    val second = readLine()
    if first.length() == 1500 && second == "b" {
        42
    } else {
        1
    }
}
NABLA

export PATH="/opt/data/local/usr/bin:$PATH"
NABLA_BUILD_DIR="$TMP_DIR" build/nablac "$TMP_DIR/read_line_long.nabla" >/dev/null

set +e
{
    head -c 1500 /dev/zero | tr '\0' a
    printf '\nb\n'
} | "$TMP_DIR/read_line_long"
status=$?
set -e

if [ "$status" -ne 42 ]; then
    echo "expected long readLine input to preserve the following line, got status $status" >&2
    exit 1
fi

echo "PASS: readLine grows beyond 1024 bytes without consuming the next line"
