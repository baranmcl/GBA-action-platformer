# Milestone 6 — Sunken Ruins + Dash Acceptance

Maps each criterion from the M6 design (§10) to its verification. Status as of 2026-06-09
(branch `m6-sunken-ruins`). Gameplay items marked **✅ mGBA** were playtested interactively.

| # | Criterion (design) | Status | Evidence |
|---|---|---|---|
| 1 | Dash earned from a shrine; double-tap blinks ~5 tiles, air + ground, one air-dash per airtime | ✅ mGBA | D5 Dash `F` shrine; `DashState` (double-tap press-edges, `DASH_VX`/`DASH_FRAMES`, recharge while grounded); `test_dash.cpp` (6 tests), `dash_sets_horizontal_blink` in `test_player.cpp` |
| 2 | i-frames dash **through** a spike hazard unharmed; cracked walls **smash** on a dash | ✅ mGBA | `TileKind::Spikes` + `spikes_overlap`; `scene_dungeon` skips damage while `dash.invincible()`; `CrackedWall` gate opens on `dash.active()` + body overlap |
| 3 | Second half impassable without Dash; the first half uses the carried kit | ✅ mGBA | D5: Fire (vine) + Featherleap (climb) + Ice (fire-wall) + Glide (updraft) before the shrine; spikes / cracked wall / lava gap after |
| 4 | At least one combo beat chains a carried power with the dash | ✅ mGBA | air-dash + glide across the 6-wide lava gap (jump too short; glide extends the landing) |
| 5 | Spronk freed → exit clears → Door 5 enterable only after D4 | ✅ mGBA | `try_free_spronk` + grounded exit; `door_enterable` (D5 when `spronk_freed(4)`); hub door 5 |
| 6 | All new logic host-tested; three-layer purity preserved; mGBA-verified end-to-end | ✅ | `check_logic_purity.py` clean; **149/149** host; D1→D5 playtested |
| — | Real-hardware verification (flashcart) | ⏳ pending | inherited from M1–M5 |

## bg-tile-index conflict resolved (Phase 1)
`GATE_TABLE` reused tile **10** for both `CrackedWall` and `FireWall`, but tile 10's art is the
fire-wall flames, and tiles 11/12 are reserved for M7 (`CrackedFloor`/Stone) and M8
(`DarkVeil`/Light). The placeholder art strip was widened from 23 to **25** tiles, giving the
cracked wall its own tile **23** (CrackedWall `bg_tile` 10→23) and the spikes tile **24**
(`level_view` maps collision `Spikes(9)`→24) — caught in plan review before any art was authored.

## Playtest-driven changes (this milestone)
- **Level-reset button (START):** a player who ran out of magic before a spell gate could
  soft-lock. `DungeonResult::Restart` re-runs the current dungeon fresh (rebuilt gates/enemies →
  renewable magic) and `main.cpp` refills health + magic. General safety net for **all** dungeons.
- **Red FireWall gate, not blue Water gate:** the D5 Ice-cleared gate was first authored as the
  blue waterfall (`W`); Ice-clears-blue-water reads as confusing, so it became the red flame wall
  (`X`, Ice extinguishes) — matching the M4 decision.
- **Separated dash beats:** the platform between the spike corridor and the cracked wall was
  widened 2 → **6 tiles** so one dash can't clear both — each reads as a distinct dash use.

## Dungeon 5 shape
A horizontal **64×24** flooded temple authored as data (`tools/levels/dungeon5.{txt,json}` →
`DUNGEON5_DATA`; generator `tools/levels/_gen_dungeon5.py`). Left→right: spawn + magic enemy →
**Fire** vine gate → **Featherleap** climb (one-way ledge over a barrier) → magic enemy → **Ice**
fire-wall gate → **Glide** updraft (ride up through a cap, over a barrier) → **Dash shrine** →
spike corridor (i-frame dash) → cracked wall (smash) → air-dash+glide lava gap → spronk → exit.
Carried-power invariant holds via the D5←D4←D3←D2 door gating, so Fire/Ice/Glide/Featherleap are
always owned. `test_dungeon5_level.cpp` pins the dims, border, dash shrine, a spike tile, a cracked
wall, a carried-power gate + an updraft tile, and the cage/exit/enemy.

## Out of scope (deferred)
Audio; boss / Nightmare King; mid-level checkpoints; final art; real-hardware flash; a swimmable
underwater medium (deep water is just a damaging hazard).

## Host test suite
`bash tools/host_test.sh` → **149/149 C++ tests + Python compiler tests, 0 failures** (adds the
`DashState` suite, the `Spikes` tile + hazard, the `Player::update` dash, and the `dungeon5` level
on top of the M5 suite; the compiler gains the `s`/`K` symbols).
