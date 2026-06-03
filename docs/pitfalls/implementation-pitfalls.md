# Implementation Pitfalls

This file collects implementation traps for the Spronk Quest GBA project to help avoid common mistakes during development.

- **IMPL-1:** Never put `bn::` types under `include/logic/` or `src/logic/` — it breaks host tests. The logic layer must be pure C++.
- **IMPL-2:** The GBA has no FPU — use integer fixed-point math, never `float`/`double` in gameplay/hot paths.
- **IMPL-3:** VRAM writes during active display cause tearing; mutate sprites/backgrounds only via Butano APIs (which defer to VBlank).
- **IMPL-4:** SRAM reads/writes must go through `bn::sram`; raw pointer access is unreliable across carts.
