#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

cat >"$TMP_DIR/ld" <<'SH'
#!/usr/bin/env bash
exit 0
SH
chmod +x "$TMP_DIR/ld"

set +e
PATH="$TMP_DIR" NABLA_BUILD_DIR=build build/nablac tests/test_arithmetic.nabla >"$TMP_DIR/stdout" 2>"$TMP_DIR/stderr"
status=$?
set -e

if [ "$status" -eq 0 ]; then
    echo "expected compilation failure when nasm is missing" >&2
    exit 1
fi

if ! grep -q "commande externe introuvable: nasm" "$TMP_DIR/stderr"; then
    echo "missing clear nasm diagnostic" >&2
    echo "--- stderr ---" >&2
    cat "$TMP_DIR/stderr" >&2
    exit 1
fi
