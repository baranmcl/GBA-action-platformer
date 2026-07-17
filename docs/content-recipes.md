# Content Recipes

Checklists for adding a dungeon / boss / enemy without re-deriving the pipeline from
milestone plans. Every step names an exact file. Derived from the code as of M14;
re-verify file paths if they've moved.

Grid symbols + JSON sidecar schema are **not** duplicated here — see the docstring at
the top of `tools/build_level.py`.

## 1. Add a dungeon

1. Author room files: `tools/levels/dungeonN_room0.txt` (+ `room1`, `room2`, ... as needed)
   and each one's `.json` sidecar, using the grid symbols documented in
   `tools/build_level.py`'s module docstring.
2. Register the rooms in `tools/levels/manifest.json` (exists after Task 1.2 of the
   remediation plan; until then there is no manifest step).
3. Hand-add the generated includes + a `DUNGEONN_ROOMS[]` array + a
   `DUNGEONN_DUNGEON` `logic::DungeonData` to `include/game/levels/dungeons.h`
   (follow the `DUNGEON3_ROOMS` / `DUNGEON3_DUNGEON` pattern already there).
4. Extend the dungeon-select table in `src/main.cpp` (the `if(n == 1) ... else if(n == 2) ...`
   chain, currently ending at `else if(n == 8) lvl = &DUNGEON8_DUNGEON;`) with your new `n`.
5. Wire the hub door:
   - Hub door: all nine `1`-`9` door glyphs in `tools/levels/hub.txt` are already allocated (D1–D8 + finale) — adding a 10th dungeon door requires the door-identity rework described in the remediation plan's Appendix A ('The 9-dungeon door-glyph ceiling', deliberately deferred). For a dungeon that REPLACES or reuses an existing slot, reuse its digit.
   - Add its gating clause to `door_enterable()` in `src/game/scene_hub.cpp` (currently a
     chain of `n == K && w.spronk_freed(K-1)` clauses — follow that pattern).
6. Allocate registry ids (see [Global registries](#5-global-registries) below):
   - A latch id in `[0..23]`, globally unique across all dungeons' JSON (only if the
     dungeon uses a latch — braziers/cracked-floor shortcuts).
   - A heart-container id in `[0..7]`, globally unique (only if the dungeon has a heart
     container).
   - After Task 1.2 lands, `tools/validate_dungeons.py` enforces both uniqueness rules;
     today there is no automated check — grep `tools/levels/*.json` for `latch_id`/
     `heart_containers` before picking a number.
7. Write dungeon-specific tests:
   - **Today:** copy an existing `test/test_dungeonN_level.cpp` (e.g.
     `test_dungeon8_level.cpp`) and adapt it to the new rooms/geometry.
   - **After Phase 2** of the remediation plan: build on the shared harness,
     `test/level_harness.h`, instead of copy-adapting.
8. Build + verify:
   - `bash tools/host_test.sh` — must end `N/N tests passed, 0 checks failed`.
   - `bash tools/build_rom.sh` then run the ROM in mGBA — walk the new dungeon
     end-to-end (spawn -> content -> spronk/exit or boss victory -> hub return).

## 2. Add a boss

1. Define the boss's data in `include/logic/boss.h`:
   - A `BossPhaseDef[]` phase table + a `BossDef` (follow `D3_PHASES` / `D3_DEF` as the
     current model — dual-element `SpellExpose`, `Locomotion::Stationary`).
   - **After Phase 5** of the remediation plan: per-phase `proj_speed` and `rock_count`
     move into the phase/def struct too (today those are hardcoded in
     `scene_dungeon.cpp`'s attack-emission code, not data-driven).
   - Give it a `BossId`-equivalent identity — today that's just "the def's address"
     (pointer identity; see step 4). A real `BossId` enum arrives with Phase 5.
2. Add a `"dN": 'logic::DN_DEF'` entry to the `BOSS_SYMBOL` map in `tools/build_level.py`
   (an unknown boss name is a build-time (tools/build_level.py, Python) `LevelError`, not silently ignored).
3. Add placeholder sprite art in `tools/make_placeholder_art.py`:
   - A `draw_<boss>_frame(...)` function (model: `draw_coldforge_frame`, line ~625).
   - A `gen_<boss>()` function that composes the frames and calls `write(...)`
     (model: `gen_coldforge`, line ~700).
   - Register the `gen_<boss>()` call in the `__main__` block (model: `gen_coldforge()`
     call near the bottom, alongside `gen_guardian()` / `gen_slagshell()`).
   - Regenerate: this produces `graphics/<boss>.bmp` + `graphics/<boss>.json`.
4. Add an id-to-sprite row in `boss_sprite_for()` in `src/game/scene_dungeon.cpp`
   (~line 151): today this is a **pointer-compare branch** —
   `if(def == &logic::D3_DEF) return bn::sprite_items::coldforge;` — because `BossDef` is
   pure logic and can't name `bn::` sprite items directly. Add your `if(def == &logic::DN_DEF) ...`
   line before the final `return bn::sprite_items::guardian;` (D1 default fallback).
5. Set `"boss": "dN"` in the arena room's JSON sidecar (resolved via `BOSS_SYMBOL` above).
6. Respect the [arena authoring constraints](#3-arena-authoring-constraints-i36) below —
   the arena room is just a normal room whose JSON has a `"boss"` key.
7. Def-invariant tests: TODAY you must hand-write per-def structural checks in `test/test_boss.cpp` (copy the pattern of `bossdef_d3_coldforge_fields` — telegraph >= SWITCH_BUDGET per phase, end_hp descending to 0) AND add your def to `switch_budget_holds_for_all_defs`. (After the remediation plan's Phase 5 lands, one all-defs invariant loop covers every registered def automatically.) Still write a dungeon-level test per step 1.7 above for the arena room's integration.

## 3. Add an enemy type

*(This recipe applies after Task 6.6 of the remediation plan lands an `EnemyType` enum;
today there is only one enemy type ('o'/`EntitySpawn`, patrol + fire-immune flag) and no
per-type table.)*

1. Add the new value to the `EnemyType` enum and its row in the per-type behavior table.
2. Add its sprite assets (placeholder art via `tools/make_placeholder_art.py`, same
   draw/gen/register pattern as bosses above).
3. Add a `"type"` key to the room JSON's `enemies[]` entries to select it (defaulting to
   the existing patrol-walker type for omitted keys, to avoid a breaking change to
   existing rooms).

## 4. Arena authoring constraints (I36)

Any room whose JSON sets `"boss"` must satisfy, or the fight is unwinnable/broken:

- **Flat solid floor at row `h-2`**, spanning at least the boss's center column
  (`w/2`) and its immediate neighbors (`w/2 - 1`, `w/2 + 1`). The boss is placed
  centered on `w/2` with its feet resting on `(h-2)*8` px
  (`scene_dungeon.cpp` `run_room_boss`, ~lines 196-206) — a gap or hazard tile there
  drops the boss through the floor.
- **1-tile solid walls at columns 0 and `w-1`.** A `Locomotion::Pacing` boss reverses at
  `pace_min_cx`/`pace_max_cx`, computed as "just inside column 1" / "just inside column
  `w-2`" (`scene_dungeon.cpp` ~lines 213-214) — these bounds assume the outer columns are
  solid walls, same as every other room.
- **Exactly one magic crystal is honored.** Only `level.magic_crystals[0]` is read
  (`scene_dungeon.cpp` ~lines 244-246); additional `'$'` symbols in the room compile but
  are inert. Author exactly one.
- **Entrance authored safe**: the entrance the player lands at (via the room's `'N'` /
  default spawn) must not put them inside the boss's attack range or off the solid floor
  before they get a frame to react.

## 5. Global registries

- **Latch ids:** bits `[0..23]` of `World::latches`, globally unique across every
  dungeon's JSON (`brazier_groups[].latch_id`, `cracked_floors[].latch_id`). Current
  allocations: `0` = D6 (brazier-group shortcut), `1` = D7 (cracked-floor shortcut),
  `2` = D8 (cracked-floor shortcut). Next free id: `3`.
- **Heart-container ids:** `[0..7]`, one bit each at
  `World::HEART_CONTAINER_LATCH_BASE + id` (bits `[24..31]`,
  `include/logic/world_state.h` lines 25-27), globally unique
  (`heart_containers[].id` in room JSON). Current allocations: `0` = D6, `1` = D7,
  `2` = D8. Next free id: `3`.
- **Tile-index map:** the collision/bg tile numbering (0 blank, 1 ground, 2 one-way, ...,
  25 grapple-anchor) is documented as a comment block in `include/logic/gates.h`
  (lines 30-35) today. After Task 4.1 of the remediation plan it moves to a dedicated
  `include/logic/tile_ids.h`.
- **Entity caps** (per room; enforced by `tools/build_level.py`'s validator after Task 1.1
  lands — today these are the design caps, not yet machine-checked):

| entity | cap | | entity | cap |
|---|---|---|---|---|
| enemies | 8 | | magic crystals | 8 |
| non-CrackedFloor gates | 24 | | room_doors | 8 |
| CrackedFloor gates | 16 | | braziers | 16 |
| shrines (pickups) | 4 | | plates | 16 |
| heart containers | 4 | | buttons | 16 |
| blocks | 8 | | **plates + buttons + brazier_groups (shared trigger vector)** | **16** |
| boulders | 8 | | level width | ≤ 64 |
| loose platforms | 8 (len ≤ 8) | | level height | ≤ 128 |
| hidden platforms | 8 (len ≤ 8) | | w*h | ≤ 8192 |
