# Implementation Pitfalls

This file collects implementation traps for the Spronk Quest GBA project to help avoid common mistakes during development.

- **IMPL-1:** Never put `bn::` types under `include/logic/` or `src/logic/` — it breaks host tests. The logic layer must be pure C++.
- **IMPL-2:** The GBA has no FPU — use integer fixed-point math, never `float`/`double` in gameplay/hot paths.
- **IMPL-3:** VRAM writes during active display cause tearing; mutate sprites/backgrounds only via Butano APIs (which defer to VBlank).
- **IMPL-4:** SRAM reads/writes must go through `bn::sram`; raw pointer access is unreliable across carts.
- **IMPL-5:** Ground a room-authored visual (brazier hit-body, heart-container/cage/spronk sprite, room-door archway) by scanning down from the authored tile for the first solid/one-way row (`floor_row_below`); never hardcode a row offset. A fixed offset assumes a uniform floor depth and silently sinks the sprite into the floor (or floats it) the first time a room authors a ledge or a non-standard floor row (hit twice: the M3 brazier hit-body and the M7 heart-container sprite).
- **IMPL-6:** In the per-frame spell resolution, freeze/melt tile-hit consumption (Ice turning water into `IcePlatform`, Fire melting it back) must run BEFORE `despawn_on_solid`. `IcePlatform` is solid, so if despawn ran first it would remove a melt/freeze shot the instant it touches the now-solid tile, before the freeze/melt logic gets to act on it.
- **IMPL-7:** A pound-impact handler needs two distinct rows, not one: the body's lowest OCCUPIED tile (for non-solid markers like plates, which the body stands ON) and the SOLID tile directly under the feet (for cracked floors/boulders, which the body lands ON TOP of). The collision resolver rests the body just above the floor, so these two rows differ by one tile — conflating them mis-targets the pound at the wrong tile kind.
- **IMPL-8:** Any post-respawn invulnerability window must outlast the on-hit invulnerability window (`RESPAWN_IFRAMES > HIT_IFRAMES`, enforced by a `static_assert`), so a respawn into an authored-unsafe or hazardous spot still yields real control frames instead of an unbreakable every-frame death loop.
- **IMPL-9:** Always clamp a scrolling room/level camera to the authored level bounds. An unclamped camera can scroll past the edge of a finite level into the blank/wrapping region of a fixed-size background (the BG tilemap repeats), showing the far edge of the level wrapped onto the near edge.
