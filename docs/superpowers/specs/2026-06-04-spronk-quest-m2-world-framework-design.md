# Spronk Quest — Milestone 2: World Framework — Design

**Platform:** Game Boy Advance (Butano / devkitARM)
**Date:** 2026-06-04
**Builds on:** M1 vertical slice (shipped). See `2026-06-03-spronk-quest-design.md` (north star)
and `2026-06-03-spronk-quest-m1-vertical-slice.md` (M1 plan).
**Status:** Design — pending implementation plan.

---

## 1. Concept & Scope

M2 builds the **reusable world machinery** that Dungeons 2–8 will plug into — **no new dungeon
content**. Four pillars:

1. **Level-data pipeline** — ASCII `.txt` maps + JSON sidecars → Python compiler → C++ headers.
   Re-author **Dungeon 1 as data** (proving the pipeline replaces M1's hardcoded tilemap).
2. **Hub overworld** — a central **plaza** (branching layout) with paths to 8 dungeon doors.
3. **Gating framework** — **thematic typed gates** keyed to abilities, demonstrated live with
   Featherleap.
4. **Scene/transition + expanded save** — plaza ↔ dungeon entry/exit; save tracks all 8 dungeons.

**Explicitly OUT of scope:** new dungeon content (D2–8), new abilities/spells, elemental-spell
combat, audio, story. Those are M3–M12.

**Success = ** Title → Hub plaza → walk into Dungeon 1's door → play the data-driven Dungeon 1 →
free the spronk → return to the plaza with Featherleap → a double-jump geometry gate in the plaza
opens, revealing the (locked "coming soon") Dungeon 2 door → progress saved. All on real hardware.

---

## 2. Level-Data Pipeline

**Authoring format (chosen): ASCII tile-maps + JSON sidecar.** Human-readable, diff-friendly,
zero external tools.

- **Source files:** `tools/levels/<name>.txt` (the tile grid) + `tools/levels/<name>.json`
  (metadata).
- **Tile/entity symbols (`.txt`):**
  | Symbol | Meaning |
  |---|---|
  | `#` | solid |
  | `.` | empty |
  | `^` | one-way platform |
  | `@` | player spawn (exactly one) |
  | `C` | caged spronk |
  | `o` | enemy (params from JSON) |
  | `E` | exit / dungeon-clear zone |
  | `G` | gate (type from JSON) |
  | `1`–`8` | dungeon door (hub only; door N → dungeon N) |

  Extensible — new symbols added as future content needs them.
- **JSON sidecar:** `{ "tileset": "...", "gates": [{"symbol":"G","type":"ice"}...],
  "enemies": [{"symbol":"o","patrol":[x0,x1]}...], "doors": {...}, "links": {...} }`. Keys map a
  grid symbol to its parameters.
- **Compiler `tools/build_level.py`:** validates (rectangular grid, **solid outer border**,
  exactly one `@`, every symbol known), then emits `include/game/levels/<name>.h` — a tile array
  (`uint8_t`, `TileKind`-compatible), `W`/`H`, and entity coordinate lists. **Same header shape the
  engine already consumes**, so the runtime is authoring-format-agnostic.
- **Make integration:** a rule compiles each `tools/levels/*.txt` (+ its `.json`) to its header,
  with the ROM build depending on them (mirrors the deferred M1 Task 3.2).
- **Tests:** Python unit tests (`tools/test_build_level.py`) for parse, each validation failure,
  and symbol→entity mapping; a C++ host test that a generated header is well-formed (dims > 0,
  spawn on empty tile, border solid).

---

## 3. Hub Overworld + Gating Framework

### Plaza (layout: central plaza with branches)
- The plaza is itself a data-driven level (`tools/levels/hub.txt`): a central area with branch
  paths leading to dungeon-door markers `1`–`8`.
- On entering the plaza, the framework reads the save and sets each gate/door to its correct state
  (open/locked) and hides freed spronks' doors as "cleared".

### Thematic typed gates
A **gate** is a data entity whose **`type`** bundles (a) the ability that clears it, (b) its visual
tile/animation, and (c) its clear interaction. Code is generic (a gate-type table); each gate looks
and behaves like the power that beats it. The systemic element interactions from the north-star
design fall out of this table for free.

| Gate type | Cleared by | Interaction | Flavor |
|---|---|---|---|
| `vine` | Fire | burns away | obstacle |
| `ice` | Fire | melts | obstacle |
| `water` | Ice | freezes into a standable platform | obstacle |
| `cracked_wall` | Dash | dash-shatters | obstacle |
| `cracked_floor` | Ground-pound | smashes through | obstacle |
| `dark_veil` | Light | dispelled | obstacle |
| `gap` | Double-jump / Glide | terrain only the traversal power crosses | **geometry** |
| `grapple_point` | Vine Grapple | reach-enabled terrain | **geometry** |

Two flavors: **obstacle gates** (a typed tile/object a spell clears, with its own art + clear
animation) and **geometry gates** (terrain only the right traversal power crosses — no object).

The runtime gate-type table lives in **`src/logic/gates.h`** (pure): `gate_type → {required_ability,
tile_index, is_geometry}`. A pure predicate `can_pass(GateType, abilities) -> bool` drives both
collision (obstacle solid until cleared) and the open animation. Host-tested.

### M2 live demonstration (with only Dungeon 1 built)
- **Dungeon 1's door** is reachable from the plaza start.
- Entering + clearing Dungeon 1 grants **Featherleap**; back in the plaza, a **`gap` geometry gate**
  (a high ledge, double-jump-only — the M1 exit pattern) is now passable, revealing the **Dungeon 2
  door rendered "locked / coming soon."**
- Dungeon doors 3–8 appear as locked markers (visual only).
- This exercises the *entire* gating loop (ability bit → gate passability → collision + visual)
  end-to-end, even though no obstacle gates (ice/vine/…) are cleared yet — those arrive with their
  abilities in M3+. The obstacle-gate *type system* is built and host-tested now so future dungeons
  drop in `{gate, type: ice}` with zero engine changes.

---

## 4. Scene & Transition Framework

- **Scene flow:** Title → **Hub (plaza)** → **Dungeon** → back to Hub.
- **Dungeon entry:** Laurel stands on a dungeon door and presses **Up** (the D-pad up; A stays
  jump) → brief fade → load that dungeon's play scene (parameterized by its level data).
- **Dungeon exit:** on clearing (reach `E` after freeing the spronk) → fade → return to plaza with
  the earned ability applied + progress saved. On death-out or **SELECT-to-quit** → return to plaza,
  dungeon not marked cleared.
- **Hub remembers state:** re-entering the plaza reflects the save (which gates open, which doors
  cleared).
- **Refactor:** generalize M1's `scene_play` into a **`scene_dungeon`** parameterized by a
  `LevelData` reference (Dungeon 1 = "scene_dungeon loaded with `dungeon1` data"), so dungeons are
  data + a shared runtime, not bespoke code. A `fade` transition helper lives in `src/engine/`.

---

## 5. Expanded Save (schema v2 + migration)

- **`World` grows** to bitmasks: `uint16_t spronks_freed` (bit N = dungeon N's spronk),
  `uint16_t abilities` (bit per power: featherleap, fire, ice, glide, dash, grapple, stone, light),
  and `uint8_t current_dungeon` (0 = hub). Replaces M1's three bools.
- **`SaveData` v2:** `version = 2`, widened payload, checksum recomputed over the new layout.
- **Migration:** `load_save` accepts **v1** saves and upgrades them to v2 in memory — a v1 save
  (Dungeon 1 cleared) becomes `spronks_freed bit0 + abilities.featherleap`. Empty/corrupt SRAM is
  still rejected as a fresh game (M1 behavior preserved).
- **Tests:** v2 round-trip, **v1→v2 migration**, empty/corrupt rejection, `static_assert`s on the
  new `SaveData` size + trivial-copyability.

This establishes the **schema-bump pattern** (version + migration) so M3–M9 add save bits with zero
breakage; each new dungeon just sets its spronk/ability bit.

---

## 6. Architecture & File Structure

```
src/logic/   (pure, host-tested)
  world_state.h     -- expanded World + SaveData v2 + v1->v2 migration
  gates.h           -- GateType table + can_pass(type, abilities)
  level_data.h      -- pure LevelData view (tiles + entities) consumed by scene_dungeon
src/engine/
  level_loader.*    -- build bg + collision tilemap + entities from a compiled level header
  fade.*            -- screen fade transition helper
src/game/
  scene_title.*     -- (updated: -> Hub instead of play)
  scene_hub.*       -- plaza: render, door/gate state from save, door-entry -> dungeon
  scene_dungeon.*   -- generalized M1 play scene, parameterized by LevelData
  levels/*.h        -- GENERATED by tools/build_level.py
tools/
  build_level.py    -- ASCII+JSON -> C++ header (+ test_build_level.py)
  levels/hub.{txt,json}, dungeon1.{txt,json}
```

- **Three-layer rule preserved**: gate/save/level *logic* is pure and host-tested; Butano touches
  only `engine/` + `game/`. The `check_logic_purity.py` guard still applies.
- **Reuses M1 engine** (`level_view`/`set_level_tile`, `avatar`, `bolts`, `hud`, `input`, `save`,
  collision/player) — `scene_dungeon` is M1's loop generalized, not rewritten.

---

## 7. Testing Strategy

- **Host (g++ via `tools/host_test.sh`):** gate `can_pass` truth table; save v2 round-trip +
  v1→v2 migration + corruption rejection; level-data model invariants; (compiler validation via
  Python `unittest`).
- **mGBA observation:** plaza renders with correct door/gate states; door-entry transition; clear →
  return-to-plaza with the Featherleap gate now open + D2 door revealed; SRAM persists the expanded
  state (incl. a v1 save migrating on first boot).
- **Real hardware:** flash + confirm hub navigation, transitions, and v2 SRAM persistence.

---

## 8. Open Questions / Deferred

- Transition style (instant cut vs fade) — default to a short fade; tune in implementation.
- Obstacle-gate clear animations — placeholder (instant tile swap) in M2; polish with the dungeons.
- Plaza music/ambience — M11.
- Whether dungeon progression is a strict chain or a branching dependency graph — the *framework*
  supports arbitrary gate→door dependencies; the concrete D2–8 dependency map is content, decided
  as each dungeon is built (M3+).
