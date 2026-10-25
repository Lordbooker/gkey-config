#!/bin/sh
# SPDX-FileCopyrightText: 2026 Lordbooker
# SPDX-License-Identifier: GPL-2.0-only
#
# Runs clang-tidy on all sources of the project (src/ and tests/) with the
# settings in .clang-tidy and tests/.clang-tidy. Exits with an error if
# clang-tidy reports anything.
#
#   scripts/clang-tidy.sh [build directory]    (default: build)
#
# The build directory needs compile_commands.json (always written by the
# CMake setup) and must have been built once, so the generated moc files exist.
# CLANG_TIDY selects the program (default: clang-tidy), JOBS the number of
# parallel runs (default: number of CPUs).
set -eu

build=${1:-build}
tidy=${CLANG_TIDY:-clang-tidy}
jobs=${JOBS:-$(nproc)}
root=$(cd "$(dirname "$0")/.." && pwd)

if [ ! -f "$build/compile_commands.json" ]; then
    echo "$build/compile_commands.json not found - configure and build first" >&2
    exit 2
fi

"$tidy" --version | head -n 2
python3 -c '
import json, sys
root = sys.argv[2]
files = sorted({e["file"] for e in json.load(open(sys.argv[1]))
                if e["file"].startswith((root + "/src/", root + "/tests/"))})
print("\n".join(files))
' "$build/compile_commands.json" "$root" |
    xargs -P "$jobs" -n 1 "$tidy" -p "$build" --quiet
