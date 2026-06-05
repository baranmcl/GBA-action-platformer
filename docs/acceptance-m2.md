# Milestone 2 — World Framework Acceptance

Maps each Section from the M2 design to its verification. Status as of 2026-06-04
(branch `m2-world-framework`).

| # | Criterion (design) | Status | Evidence |
|---|---|---|---|
| 1 | Level-data pipeline (ASCII `.txt` + JSON → compiler → C++ header) | ✅ | `tools/build_level.py` + 10 Python unit tests; `host_test.sh` regenerates headers |
| 2 | Dungeon 1 re-authored as data, plays identically to M1 | ✅ mGBA | `tools/levels/dungeon1.txt` → `DUNGEON1_DATA`; data-driven `scene_dungeon` verified |
| 3 | Central plaza hub with branches + dungeon doors | ✅ mGBA | `tools/levels/hub.txt`; `scene_hub` renders doors (D1 open, others locked) |
| 4 | Thematic typed-gate framework (`gates.h` table + `can_pass`) | ✅ | host tests `test_gates.cpp`; gap gate instantiated in the hub |
| 5 | Gating demo: clear D1 → Featherleap → gap-gate opens → D2 revealed | ✅ mGBA | full loop verified: gate blocks before, opens after |
| 6 | Plaza↔dungeon transitions + fades | ✅ mGBA | live fade-in (interactive), fade-out on exit; door entry on Up |
| 7 | Expanded save (v2 bitmasks) + **v1→v2 migration** | ✅ mGBA | `test_world_state_v2.cpp` (incl. `v1_save_migrates`); real-SRAM test: planted v1 save → M2 ROM boots CONTINUE + gate open |
| 8 | Reuse M1 engine; three-layer purity preserved | ✅ | level_view/avatar/bolts/hud/input/save reused; `check_logic_purity.py` clean; 76/76 host |
| — | Real-hardware verification (flashcart) | ⏳ pending | inherited from M1 — flash + confirm hub/dungeon/SRAM on real GBA |

## Save migration (verified on real SRAM)
A hand-crafted **M1 v1 save** (magic+version=1+flags 0x07, v1 checksum) planted as a 32KB
SRAM dump, loaded by the M2 ROM → title shows CONTINUE and the hub gate is already open
(Featherleap migrated from the v1 flag). Confirms `bn::sram::read` + `load_save`'s v1 path
end-to-end. Existing M1 saves survive the upgrade.

## Notable fixes during M2 (see plan Discoveries)
- `int32_t` == `long` on arm-none-eabi → assign to `int` locals before passing to `bn::fixed` APIs.
- `logic::Fixed` now defaults to 0 → `Vec2`/`Body` never start with indeterminate velocity (fixed phantom drift on restart).
- Live fade-in (gameplay runs as it fades) → no frozen-then-snap on held input.
- Camera centred on player before fade → no flash to level centre.

## Deferred (later milestones)
- **Door art** reads as a doorway by size but the tile art could look more door-like → art-polish milestone.
- Obstacle-gate rendering + clear animations (ice melt, vine burn) → arrive with their abilities (M3+).
- Per-dungeon reward field in LevelData/JSON → when D2+ bring new abilities (M3+).
- Real-hardware flash → carry the M1 discipline.

## Host test suite
`bash tools/host_test.sh` → **76/76 C++ tests + 10 Python compiler tests, 0 failures**
(fixed-point incl. default-zero, vec2, tilemap, physics, collision, player, meters, bolt,
world-state v2 + migration, gates, level-data, dungeon1 rescue, enemy, dungeon1/hub level data).
