#!/usr/bin/env python3
"""Checks that every Lua function registered by the engine is documented in
docs/script-api.md (run by CTest). MIT License."""
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
src = (root / "engine/src/script/lua_api.c").read_text()
docs = (root / "docs/script-api.md").read_text()
arrays = dict(re.findall(r"static const luaL_Reg (\w+)\[\] = \{(.*?)\{NULL, NULL\}\};", src, re.S))
libs = re.findall(r'\{"(\w+)", (\w+)\}', src.split("static const lib_def libs[]")[1].split("};")[0])
missing = []
total = 0
for lib, var in libs:
    for fn in re.findall(r'\{"(\w+)",', arrays[var]):
        total += 1
        if f"{lib}.{fn}" not in docs:
            missing.append(f"{lib}.{fn}")
if missing:
    print("undocumented script functions:", ", ".join(missing))
    sys.exit(1)
print(f"all {total} script functions are documented")
