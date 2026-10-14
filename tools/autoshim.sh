#!/bin/bash
# Resolve "file not found" includes: case-insensitive match in tree/mingw sysroot -> forwarding shim, else empty stub.
R=/home/dnlwtsn/Code/zh-living-world; SYS="$HOME/tools/llvm-mingw-20261006/generic-w64-mingw32/include $HOME/tools/llvm-mingw-20261006/i686-w64-mingw32/include"
for h in $(grep -h "fatal error" $R/build/syn/*.log | grep -o "'[^']*' file not found" | sed "s/'\(.*\)' file not found/\1/" | sort -u); do
  b=$(basename "$h"); [ -e "$R/shim/$h" ] && continue
  hit=$(find $R/shim $R/src-zh/Code/GameEngine $R/src-zh/Code/Libraries $R/src-zh/Code/GameEngineDevice $SYS -iname "$b" 2>/dev/null | head -1)
  mkdir -p "$R/shim/$(dirname "$h")"
  if [ -n "$hit" ] && [[ "$hit" != *"/Libraries/Source/STLport"* ]] && [[ "$hit" != *"/c++/"* ]]; then echo "#include \"$hit\"" > "$R/shim/$h"; echo "map  $h -> $hit"
  else echo "/* shim: stub for unavailable $h */" > "$R/shim/$h"; echo "stub $h"; fi
done
