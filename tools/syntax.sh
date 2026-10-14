#!/bin/bash
# usage: syntax.sh <dir under src-zh/Code> ; syntax-checks every .cpp, writes build/syn/<name>.log
R=/home/dnlwtsn/Code/zh-living-world; C=$R/src-zh/Code; CXX=~/tools/llvm-mingw-20261006/bin/i686-w64-mingw32-clang++
INC="-I$R/shim -I$C/GameEngine/Include/Precompiled -I$C/GameEngine/Include -I$C/GameEngineDevice/Include -I$C/Libraries/Include -I$C/Libraries/Source -I$C/Libraries/Source/WWVegas -I$C/Libraries/Source/WWVegas/WWLib -I$C/Libraries/Source/WWVegas/WWMath -I$C/Libraries/Source/WWVegas/WWDebug -I$C/Libraries/Source/WWVegas/WWSaveLoad -I$C/Libraries/Source/WWVegas/Wwutil -I$C/Libraries/Source/WWVegas/WW3D2 -I$C/Libraries/Source/Compression"
find "$R/build/syn" -name "*.log" -delete
find "$C/$1" -name '*.cpp' | xargs -P 20 -I{} bash -c 'f={}; n=$(echo ${f#'$C'/} | tr / _); timeout 60 '$CXX' -std=gnu++98 -fms-extensions -fms-compatibility -U_MSC_VER -U_MSC_FULL_VER -U_MSC_BUILD -D__GNUC__=4 -D__GNUC_MINOR__=2 -DWIN32 -D_WINDOWS -DNDEBUG -DZ_PREFIX -DUM_MAX_PLAYERS=128 -DIG_DEBUG_STACKTRACE -DRELEASE_DEBUG_LOGGING -Xclang -fno-wchar -fasm-blocks -include /home/dnlwtsn/Code/zh-living-world/shim/compat.h -fsyntax-only -w -fpermissive -ferror-limit=5 '"$INC"' $f > '$R'/build/syn/$n.log 2>&1; echo "$? $n"' > $R/build/syn/summary.txt
awk '{c[$1==0?"ok":"fail"]++} END{for(k in c)print k,c[k]}' $R/build/syn/summary.txt
