#!/usr/bin/env python3
"""crashsym.py [um_client_crash.bin] [exe]: symbolize the record the client's crash capture wrote (code, frame-pointer chain, registers)."""
import struct, subprocess, sys
import os; G = os.environ.get("UM_CRASHFILE", "/mnt/e/SteamLibrary/steamapps/common/Command & Conquer Generals - Zero Hour/um_client_crash.bin")
path = sys.argv[1] if len(sys.argv) > 1 else G
exe = sys.argv[2] if len(sys.argv) > 2 else "/home/dnlwtsn/Code/zh-living-world/build/cl/zh_client.exe"
d = open(path, "rb").read(); v = struct.unpack("<%dI" % (len(d) // 4), d); base = v[3]
print("code=%#x addr=%#x base=%#x access=%d at %#x; fault module %#x" % (v[0], v[1], base, v[4], v[5], v[6 + 1024]))
chain = []
for x in [v[2]] + [x for x in v[6 + 1032:6 + 1032 + 64] if x]:
    if not chain or chain[-1] != x or len(chain) < 3: chain.append(x)
args = ["%#x" % (c - base + 0x400000) for c in chain[:40] if base <= c < base + 0x3000000]
r = subprocess.run(["/home/dnlwtsn/tools/llvm-mingw-20261006/bin/llvm-symbolizer", "--obj=" + exe, "--demangle"] + args, capture_output=True, text=True)
out = [l for l in r.stdout.replace("/home/dnlwtsn/Code/zh-living-world/src-zh/Code/", "").split("\n") if l.strip()]
for i in range(0, len(out), 2): print(" ", out[i], "  ", out[i + 1] if i + 1 < len(out) else "")
