# Milestone 4 — Frost Hollow + Ice Spell Acceptance

Maps each criterion from the M4 design (§9) to its verification. Status as of 2026-06-08
(branch `m4-frost-hollow`). Gameplay items marked **✅ mGBA** were playtested interactively.

| # | Criterion (design) | Status | Evidence |
|---|---|---|---|
| 1 | Ice spell earned from a shrine; cast with R | ✅ mGBA | D3 Ice `F` shrine; `logic/spell.h` `SpellCast`; `test_spell.cpp` |
| 2 | `L` cycles Fire↔Ice; HUD shows the selected spell | ✅ mGBA | `SpellState::cycle` rotates owned spells; HUD icon `set_item` per selection; `cycle_rotates_owned_spells` test |
| 3 | Ice freezes water into a standable platform | ✅ mGBA | `frost.h` `ice_freezes`; scene flood-freezes the whole water run into a bridge |
| 4 | Fire melts an ice platform back to water (reversible) | ✅ mGBA | `frost.h` `fire_melts`; scene melt loop; re-freezable |
| 5 | A water gap is impassable without freezing | ✅ mGBA | wide freeze gaps with a **ceiling** (no jump-over); the ice bridge is the crossing (wading is a damage-punished shortcut, intentional — see Deviations) |
| 6 | An Ice-cleared gate opens only with Ice | ✅ mGBA | **fire-wall** gate (`GateType::FireWall`, Ability::Ice) — Ice extinguishes it |
| 7 | Dual-spell climax requires both spells | ✅ mGBA | fire wall (Ice) + freeze gap (Ice) + ice wall (Fire melts) + vine gate (Fire) |
| 8 | Water is a damaging hazard | ✅ mGBA | `TileKind::Water`; `hazard_overlap` (lava ∪ water); 20 dmg + i-frames |
| 9 | Dungeon 3 authored as data; Zelda-style arc | ✅ mGBA | `tools/levels/dungeon3.txt`+`.json` → `DUNGEON3_DATA`; `test_dungeon3_level.cpp` |
| 10 | Hub Door 3 enterable after D2 cleared | ✅ mGBA | `scene_hub` `door_enterable` (D3 when `spronk_freed(2)`) |
| 11 | New art (ice projectile, water, ice-platform, fire-wall) | ✅ mGBA | `make_placeholder_art.py`: `ice_proj` + tiles 16/19/10 |
| 12 | Three-layer purity preserved; host-tested | ✅ | `check_logic_purity.py` clean; 117/117 host |
| — | Real-hardware verification (flashcart) | ⏳ pending | inherited from M1–M3 |

## The spell generalization (the load-bearing refactor)
M3 had one spell (`FireCast` + a fire pool, single effect). M4 generalized to **typed projectiles**:
`FireCast`→`SpellCast`, `SpellId::Ice` added, `FirePool`→`SpellPool` with each `Shot` tagged by
`SpellId`; the scene resolves hits by type (`consume_hit(target, kind)`) and freezes/melts tiles
via a column-scan `consume_tile_hit`. `SpellState::cycle` finally rotates owned spells. All host-
tested before the ROM built.

## Notable fixes during M4 (see plan Discoveries)
- **Spell-vs-floor-tile height (the M3 brazier bug, caught in plan review):** spells fly at chest
  height but water/ice sit at floor level — `consume_tile_hit` scans DOWN the shot's column so an
  Ice shot actually reaches the water it flies over. Verified in mGBA.
- **Flood-freeze:** one Ice cast freezes the whole contiguous water run into a bridge (not one box).
- **Ceilings** over the freeze gaps so the player can't double-jump over them.
- **Fire-wall gate** replaced an Ice-cleared *waterfall* (which read backwards) — Ice now
  extinguishes a wall of fire, mirroring Fire melting the ice wall.

## Deviations from plan
- **Water gate → fire-wall gate:** the spec's Ice-cleared waterfall was incoherent (freezing
  solidifies water, it shouldn't *open* a path). Replaced with an Ice-extinguished fire wall.
  `GateType::Water` remains scaffolded but unused.
- **Wading is an intentional damage-punished shortcut:** the freeze bridge is the clean intended
  path and the ceiling blocks the jump-over, but flush water lets a determined player wade across
  taking contact damage. Left as-is by design decision (not hard-sealed).
- **D3 size:** a dense 64×22 gauntlet (same GBA BG-cap reality as D2), not "~8–12 screens".

## Out of scope (deferred)
- Audio, boss/Nightmare King, slippery-ice momentum physics, mid-level checkpoints, final art,
  real-hardware flash.

## Host test suite
`bash tools/host_test.sh` → **117/117 C++ tests + Python compiler tests, 0 failures**
(adds water/ice tiles + hazard, typed-spell cast/cycle, freeze/melt predicates, `gate_cleared_by`,
the fire-wall gate, and the `dungeon3` level on top of the M3 suite).
