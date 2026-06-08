# Milestone 5 — Gale Cliffs + Glide Acceptance

Maps each criterion from the M5 design (§11) to its verification. Status as of 2026-06-08
(branch `m5-gale-cliffs`). Gameplay items marked **✅ mGBA** were playtested interactively.

| # | Criterion (design) | Status | Evidence |
|---|---|---|---|
| 1 | Glide earned from a shrine; hold-A slows the fall with air control | ✅ mGBA | D4 Glide `F` shrine; `Abilities::glide` + `InputFrame.glide_held`; `Player::update` glide rule; `test_player.cpp` |
| 2 | Updraft shafts lift **only while gliding**; dive through otherwise | ✅ mGBA | `TileKind::Updraft` + `wind.h` `updraft_overlap`; `updraft_rises_only_while_gliding` test |
| 3 | Wind gusts push sideways (first directional force on the player) | ✅ mGBA | `TileKind::WindLeft/WindRight` + `wind_dir`; D4 tailwind gust beat; `wind_pushes_horizontally` test |
| 4 | The upper cliffs are impassable without Glide | ✅ mGBA | 28-row updraft gap (>> double-jump); land-on-top one-way exit platform |
| 5 | One Fire beat (vine gate) needs a carried spell | ✅ mGBA | grounded vine corridor wall + enemy for magic; Fire-owned invariant via the D3→D2 gate |
| 6 | Vertical scrolling works (the flagged risk) | ✅ mGBA | Phase 0 spike; camera clamp keeps the view inside level bounds |
| 7 | Dungeon 4 authored as data; vertical ascent | ✅ mGBA | `tools/levels/dungeon4.{txt,json}` → `DUNGEON4_DATA` (30×56); `test_dungeon4_level.cpp` |
| 8 | Hub Door 4 enterable after D3 cleared | ✅ mGBA | `scene_hub` `door_enterable` (D4 when `spronk_freed(3)`) |
| 9 | New tiles/art (updraft, wind streaks); spell-agnostic glide input | ✅ mGBA | `make_placeholder_art.py` tiles 20/21/22; `input.cpp` reads `a_held()` |
| 10 | Three-layer purity preserved; host-tested | ✅ | `check_logic_purity.py` clean; 132/132 host |
| — | Real-hardware verification (flashcart) | ⏳ pending | inherited from M1–M4 |

## Engine enhancement: 128-tall levels (beyond original scope)
The original engine used a fixed 64×32 background — a 32-tile ceiling. To give Gale Cliffs real
vertical room, the background was raised to **64×128** (a Butano "big map", auto-streamed; the
GBA's regular-bg hardware tops out at 64×64, and Butano streams the visible window for anything
larger, up to 2048×2048). The big static cell + collision buffers were moved to **EWRAM**
(`BN_DATA_EWRAM_BSS`) to avoid overflowing the 32 KB IWRAM. Levels can now be up to 128 tiles tall;
D4 uses 56. Verified D1–D3 + hub still render correctly under the big map.

## Tuning + feel changes (this milestone)
- **Tighter global jump:** `JUMP_VY` -4 → raw -812 (single jump ~5.5 → ~3.5 tiles, double ~7). Makes
  the Featherleap climb *require* the double-jump and keeps glide/updraft gaps un-jumpable. D1–D3
  re-verified clearable.
- **Slippery ice:** standing on a frozen-water `IcePlatform` tile uses much weaker friction
  (`ICE_FRICTION`), so you slide. Pure-logic, host-tested; applies wherever ice exists (D3+).
- **Fire flood-melt:** Fire now melts the whole contiguous ice run (symmetric to Ice flood-freezing
  the whole water run).
- **D3 forced ice crossing:** replaced a walkable-ceiling bypass with a solid overhang over a 9-tile
  water channel — the ice crossing is now mandatory (freeze a bridge, walk/slide under it).

## Bug fixes surfaced during M5
- **Collision hang (latent since M1):** an entity spawned *embedded* in solid with horizontal
  velocity hung the game (the back-out loop backed out sideways forever). Bounded both back-out
  loops + regression test (`test_collision.cpp`).
- **D2 block soft-lock:** a push-block shoved into a dead corner could not be reset; now **death
  restores the pushable blocks** to their authored start.
- **Camera wrap:** the fixed 64×N background wraps past the authored level (a tall level showed its
  top on the screen bottom). Added a **camera clamp** in `scene_dungeon` + `scene_hub`; filled the
  hub to 20 rows.
- **Land-on-exit:** the exit clears only when **grounded** on the platform (no head-bump from below).

## Deviations from plan
- **D4 shape:** authored as a 30×**56** tall climb (using the new 128-row engine), not ≤32. The
  vine beat is a **grounded bottom-corridor wall** (with the enemy for magic), not a shaft-floating
  gate, after playtest showed a full-height vine in a vertical shaft is incoherent + soft-lock-prone.
- **Engine height bump + tighter jump + slippery ice** were added mid-milestone in response to
  playtest (all beyond the original Glide-only scope).

## Out of scope (deferred)
Audio; boss / Nightmare King; mid-level checkpoints; final art; real-hardware flash.

## Host test suite
`bash tools/host_test.sh` → **132/132 C++ tests + Python compiler tests, 0 failures** (adds the
wind kit — glide/updraft/gust + 3 tiles + `wind.h` — the slippery-ice test, the embedded-spawn
collision regression, and the `dungeon4` level on top of the M4 suite).
