#!/usr/bin/env python3
import pathlib, re, sys
roots = [pathlib.Path("include/logic"), pathlib.Path("src/logic")]
pat = re.compile(r"\bbn::")
bad = []
for root in roots:
    if not root.exists(): continue
    for f in root.rglob("*"):
        if f.suffix in (".h", ".cpp"):
            for i, line in enumerate(f.read_text().splitlines(), 1):
                if pat.search(line): bad.append(f"{f}:{i}: {line.strip()}")
if bad:
    print("LOGIC PURITY VIOLATION — bn:: found under logic layer:")
    print("\n".join(bad)); sys.exit(1)
print("logic purity OK"); sys.exit(0)
