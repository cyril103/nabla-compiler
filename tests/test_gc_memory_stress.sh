#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

BUILD_DIR=${NABLA_BUILD_DIR:-build}
CUSTOM_BUILD_DIR="$BUILD_DIR/gc_memory_stress"

rm -rf "$CUSTOM_BUILD_DIR"
mkdir -p "$CUSTOM_BUILD_DIR"
TMP_DIR=$(mktemp -d "$CUSTOM_BUILD_DIR/src.XXXXXX")
trap 'rm -rf "$TMP_DIR"' EXIT

compile_and_run() {
    local source_file=$1
    local label=$2
    local heap_size=$3

    if ! PATH=/opt/data/local/usr/bin:$PATH \
        NABLA_BUILD_DIR="$CUSTOM_BUILD_DIR" "$BUILD_DIR/nablac" --heap-size "$heap_size" \
        "$source_file" \
        >"$TMP_DIR/${label}.compile.out" \
        2>"$TMP_DIR/${label}.compile.err"; then
        echo "FAIL: compilation $label" >&2
        cat "$TMP_DIR/${label}.compile.out" >&2
        cat "$TMP_DIR/${label}.compile.err" >&2
        exit 1
    fi

    local executable="$CUSTOM_BUILD_DIR/$(basename "$source_file" .nabla)"
    if "$executable" >"$TMP_DIR/${label}.run.out" 2>"$TMP_DIR/${label}.run.err"; then
        :
    else
        local code=$?
        echo "FAIL: $label a terminé avec le code $code" >&2
        cat "$TMP_DIR/${label}.run.out" >&2
        cat "$TMP_DIR/${label}.run.err" >&2
        exit 1
    fi

    if [ -s "$TMP_DIR/${label}.run.out" ] || [ -s "$TMP_DIR/${label}.run.err" ]; then
        echo "FAIL: $label ne doit rien écrire" >&2
        cat "$TMP_DIR/${label}.run.out" >&2
        cat "$TMP_DIR/${label}.run.err" >&2
        exit 1
    fi
}

cat >"$TMP_DIR/gc_stress_temporaries_nested.nabla" <<'NABLA'
class Cell(value: Int) {
    def get(): Int = {
        value
    }
}

class Pair(left: Cell, right: Cell) {
    def sum(): Int = {
        left.get() + right.get()
    }
}

def churn(value: Int): Int = {
    val pair = new Pair(new Cell(value), new Cell(value + 1))
    pair.sum()
}

def main(): Int = {
    val kept = new Pair(new Cell(20), new Cell(22))
    var i = 0
    var total = 0
    while i < 700 {
        total = total + churn(i)
        i = i + 1
    }
    if kept.sum() == 42 && total > 0 && gcCollections() > 0 && gcLastFreedBytes() > 0 {
        0
    } else {
        10
    }
}
NABLA

cat >"$TMP_DIR/gc_stress_strings_runtime_helpers.nabla" <<'NABLA'
def churnString(value: Int): Int = {
    val text = "item-" + value.toString() + ":" + (value + 1).toString()
    val repeated = text.repeat(2)
    repeated.length()
}

def main(): Int = {
    val kept = "anchor".repeat(3)
    var i = 0
    var total = 0
    while i < 220 {
        total = total + churnString(i)
        i = i + 1
    }
    if kept == "anchoranchoranchor" && total > 0 && gcCollections() > 0 &&
       gcLastFreedBytes() > 0 && heapFreeBytes() > 0 {
        0
    } else {
        11
    }
}
NABLA

cat >"$TMP_DIR/gc_stress_arrays_stdlib.nabla" <<'NABLA'
import collections.array

def exercise(seed: Int): Int = {
    val base = Array.range(18)
    val shifted = base.map(x => x + seed)
    val even = shifted.filter(x => x % 2 == 0)
    val reversed = even.reverse()
    reversed.size() + reversed.head() + shifted.last()
}

def main(): Int = {
    val kept = Array.range(6).append(99).prepend(7)
    var i = 0
    var total = 0
    while i < 120 {
        total = total + exercise(i)
        i = i + 1
    }
    if kept.head() == 7 && kept.last() == 99 && total > 0 &&
       gcCollections() > 0 && gcLastFreedBytes() > 0 && heapLargestFreeBlock() > 0 {
        0
    } else {
        12
    }
}
NABLA

cat >"$TMP_DIR/gc_stress_object_arrays_stdlib.nabla" <<'NABLA'
import collections.array

class Item(value: Int) {
    def get(): Int = {
        value
    }

    override def toString(): String = {
        "item" + value.toString()
    }
}

def makeItem(index: Int): Item = {
    new Item(index + 1)
}

def exercise(seed: Int): Int = {
    val values = Array.tabulate[Item](10, makeItem)
    val mapped = values.map[Int](item => item.get() + seed)
    mapped.size() + mapped.get(0) + mapped.get(9)
}

def main(): Int = {
    val kept = Array.tabulate[Item](3, makeItem)
    var i = 0
    var total = 0
    while i < 90 {
        total = total + exercise(i)
        i = i + 1
    }
    if kept.get(0).get() == 1 && kept.get(2).toString() == "item3" &&
       total > 0 && gcCollections() > 0 && gcLastFreedBytes() > 0 {
        0
    } else {
        13
    }
}
NABLA

cat >"$TMP_DIR/gc_stress_map_set_stdlib.nabla" <<'NABLA'
import collections.array
import collections.map
import collections.set

class Key(name: String, hash: Int) {
    override def hashCode(): Int = {
        hash
    }

    override def equals(other: Any): Bool = {
        this.toString() == other.toString()
    }

    override def toString(): String = {
        name
    }
}

def makeMap(seed: Int): Int = {
    val first = new Key("k" + seed.toString(), 7)
    val second = new Key("m" + seed.toString(), 7)
    val values = Map(first -> seed, second -> (seed + 1), first -> (seed + 2))
    values.size() + values.getOrElse(first, 0) + values.getOrElse(second, 0)
}

def main(): Int = {
    val keptA = new Key("a", 5)
    val keptB = new Key("b", 5)
    val kept = Set(keptA, keptB, keptA)
    val keptMap = Map(keptA -> 41, keptB -> 1)
    var i = 0
    var total = 0
    while i < 55 {
        total = total + makeMap(i)
        i = i + 1
    }
    if kept.size() == 2 && kept.contains(keptA) && kept.contains(keptB) &&
       keptMap.getOrElse(keptA, 0) == 41 && keptMap.getOrElse(keptB, 0) == 1 && total > 0 &&
       gcCollections() > 0 && gcLastFreedBytes() > 0 && heapFreeBytes() > 0 {
        0
    } else {
        14
    }
}
NABLA

compile_and_run "$TMP_DIR/gc_stress_temporaries_nested.nabla" gc_stress_temporaries_nested 8192
compile_and_run "$TMP_DIR/gc_stress_strings_runtime_helpers.nabla" gc_stress_strings_runtime_helpers 16384
compile_and_run "$TMP_DIR/gc_stress_arrays_stdlib.nabla" gc_stress_arrays_stdlib 24576
compile_and_run "$TMP_DIR/gc_stress_object_arrays_stdlib.nabla" gc_stress_object_arrays_stdlib 24576
compile_and_run "$TMP_DIR/gc_stress_map_set_stdlib.nabla" gc_stress_map_set_stdlib 49152

echo "PASS: GC memory stress suite covers temporaries, strings, arrays, objects and stdlib maps/sets"
