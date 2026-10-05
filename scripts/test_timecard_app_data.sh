#!/usr/bin/env bash
# Host-only consumer/backend integration; uses a temporary ordinary directory.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
runtime="$(cd "${1:?Pass exact Runtime checkout}" && pwd)"
cmp "$repo/lib/PortableTimecard/include/RiscAppDataV1.h" "$runtime/sdk/app/RiscAppDataV1.h"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
for mode in normal sanitizer;do
 flags=();if [[ "$mode" == sanitizer ]];then flags=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g);fi
 mkdir "$build/$mode"
 "${CXX:-c++}" "${flags[@]}" -std=c++17 -O1 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -I"$repo/lib/PortableTimecard/include" -I"$runtime/src" \
  "$repo/tests/timecard_runtime_files_test.cpp" "$runtime/src/runtime/storage/AppDataFiles.cpp" \
  -Wl,--wrap=write,--wrap=rename,--wrap=close -o "$build/test-$mode"
 "$build/test-$mode" "$build/$mode"
done
