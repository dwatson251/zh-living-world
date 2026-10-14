#!/bin/bash
# Prepare everything the build needs, from nothing: EA's released source (patched), the llvm-mingw cross compiler, zlib, cmake/ninja, and the generated build inputs.
# Runs on Linux (also WSL). Nothing outside this folder (and DEPS) is touched. Safe to re-run.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
deps="${DEPS:-$here/deps}"; mkdir -p "$deps"
ea_sha="$(tr -d '[:space:]' < "$here/EA_SOURCE_SHA")"
llvm_tag="20261006"; llvm_name="llvm-mingw-${llvm_tag}-msvcrt-ubuntu-22.04-x86_64"
zlib_ver="1.3.1"

echo "== EA source ($ea_sha)"
if [ ! -d "$deps/ea/.git" ]; then
  rm -rf "$deps/ea"; git init -q "$deps/ea"
  git -C "$deps/ea" remote add origin https://github.com/electronicarts/CnC_Generals_Zero_Hour.git
fi
git -C "$deps/ea" fetch -q --depth 1 origin "$ea_sha"
git -C "$deps/ea" checkout -q -f FETCH_HEAD
if [ ! -f "$here/src-zh/.patched" ]; then
  rm -rf "$here/src-zh"; mkdir -p "$here/src-zh"
  cp -r "$deps/ea/GeneralsMD/Code" "$here/src-zh/Code"
  patch -p1 -s -d "$here/src-zh/Code" < "$here/patches/zh-living-world.patch"
  touch "$here/src-zh/.patched"
fi

echo "== cross compiler ($llvm_name)"
if [ ! -x "$deps/$llvm_name/bin/i686-w64-mingw32-clang++" ]; then
  curl -fsSL "https://github.com/mstorsjo/llvm-mingw/releases/download/${llvm_tag}/${llvm_name}.tar.xz" -o "$deps/llvm.tar.xz"
  tar -xJf "$deps/llvm.tar.xz" -C "$deps"; rm -f "$deps/llvm.tar.xz"
fi
export LLVM_MINGW="$deps/$llvm_name"

echo "== zlib $zlib_ver"
if [ ! -f "$here/deps/zlib-$zlib_ver/zlib.h" ]; then
  curl -fsSL "https://github.com/madler/zlib/releases/download/v${zlib_ver}/zlib-${zlib_ver}.tar.gz" -o "$deps/zlib.tar.gz"
  mkdir -p "$here/deps"; tar -xzf "$deps/zlib.tar.gz" -C "$here/deps"; rm -f "$deps/zlib.tar.gz"
fi

echo "== cmake and ninja"
if ! command -v cmake >/dev/null || ! command -v ninja >/dev/null; then
  python3 -m pip install --user --quiet cmake ninja; export PATH="$HOME/.local/bin:$PATH"
fi

echo "== generated build inputs"
mkdir -p "$here/build"
# the shim headers carry @ROOT@ / @LLVM@ placeholders; the working copy gets the real paths
rm -rf "$here/build/shim"; cp -r "$here/shim" "$here/build/shim"
find "$here/build/shim" -type f \( -name '*.h' -o -name '*.H' -o -name '*.inl' -o -name 'hash_map' -o -name 'vector.h' \) -print0 | xargs -0 sed -i "s|@ROOT@|$here|g; s|@LLVM@|$LLVM_MINGW|g"
for l in $(cd "$here/build/shim" && find . -name '*.link'); do
  target="$(sed "s|@LLVM@|$LLVM_MINGW|" "$here/build/shim/$l" | tr -d '\n')"; rm "$here/build/shim/$l"; ln -sf "$target" "$here/build/shim/${l%.link}"
done
python3 "$here/tools/gensources.py"
python3 "$here/tools/genvfs.py"
echo "LLVM_MINGW=$LLVM_MINGW" > "$here/build/env.sh"
echo "setup done"
