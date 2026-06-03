# Spronk Quest — Project Guidance

## Build

- **ROM:** `make` (requires devkitPro + Butano installed; see Butano docs for setup)
- **Host tests:** `make -C test` (needs g++ + Python).
  - **On this Windows machine** the default git-bash picks the wrong (cygwin) g++ with broken include paths. Use the **mingw64** g++ by prepending it to PATH:
    ```bash
    PATH="/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH" make -C test
    ```

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
