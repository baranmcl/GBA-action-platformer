# Spronk Quest — Project Guidance

## Build

- **ROM:** `make` (requires devkitPro + Butano installed; see Butano docs for setup)
- **Host tests:** run `bash tools/host_test.sh` (preferred on this machine).
  - Plain `make -C test` is **fragile here**: git-bash resolves `g++` to msys64's cygwin compiler (whose cc1plus can't find its DLLs without mingw64 on PATH), and `make`'s recipe shell strips `TMP`/`TEMP` so the assembler tries to write to `C:\WINDOWS` (Permission denied). `tools/host_test.sh` fixes both (puts mingw64 on PATH, exports a writable temp dir) and compiles directly. A green run ends with `N/N tests passed, 0 checks failed`.

## Architecture: Three-Layer Rule

```
src/logic/   + include/logic/   — pure C++17, NO bn:: types (host-unit-testable)
src/engine/                     — Butano glue (bn:: allowed here only)
src/game/                       — scenes, entry point
```

**Never** put `bn::` types under `include/logic/` or `src/logic/`. This breaks host tests.

## Before Committing

Run the purity guard to catch accidental Butano leakage into the logic layer:

```
python tools/check_logic_purity.py
```

## References

- `docs/pitfalls/implementation-pitfalls.md` — common implementation traps
- `docs/pitfalls/testing-pitfalls.md` — common testing traps
