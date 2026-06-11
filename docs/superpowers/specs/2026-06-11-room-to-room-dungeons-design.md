# Room-to-Room Dungeons — Design

**Date:** 2026-06-11
**Status:** Approved (design); pending implementation plan
**Goal:** Make dungeons longer and bigger without fighting the GBA map-size ceiling, by composing a dungeon out of multiple screen-or-larger *rooms* connected by doors — Zelda-style.

## Motivation

A single level is rendered into one fixed 64×128-tile Butano big-map background
(`src/engine/level_view.cpp`), capping a level at ~512 px (≈2.1 GBA screens) wide.
A typical Super Mario Bros level is ~14 screens wide, so widening the map is both
expensive (EWRAM) and authoring-heavy.

Instead, **length comes from room count, not map width.** Each room stays within the
existing 64×128 box; a dungeon becomes a graph of rooms linked by doors. The map-size
ceiling becomes irrelevant.

## Locked decisions

These were settled during brainstorming and are not open for re-litigation in the plan:

1. **Transition style:** hard cut / quick fade at doors (not continuous scroll, not
   camera-snap within one map). Reuses the existing fade + per-room `load_level` flow.
2. **Room model:** a dungeon is a **room graph** (branching, backtracking, hub rooms) —
   not a linear chain, and rooms are *nested under* a dungeon (the dungeon stays a unit
   for save/exit-to-hub purposes).
3. **State persistence:** **progress-gating only.** Permanent gating events (opened
   shortcut/locked doors, lit puzzle braziers) latch; refightable enemies and minor state
   rebuild fresh from ROM on every room entry.
4. **Latch scope:** **permanent**, saved to battery-backed SRAM (like abilities/spronks).
5. **Architecture:** the room-switch loop lives **inside `run_dungeon`** (Approach 1).
6. **Door linking:** **explicit entrance markers** (supports one-way drops and multiple
   doors between the same two rooms — important for a platformer).

## Architecture: three-layer compliance

Per `CLAUDE.md`, `include/logic/` and `src/logic/` must stay free of `bn::` types.

- **Pure logic** (`include/logic/`): `EntranceSpawn`, `RoomDoorSpawn`, `DungeonData`,
  the `LevelData` additions, the `latches` field on `World`, the latch-apply rule, and
  the room-graph transition decision (a pure function).
- **Engine** (`src/engine/`): unchanged — `load_level` / `build_level_view` build each
  room exactly as today.
- **Game** (`src/game/`): `run_dungeon` / `play_room` orchestration, fade transitions,
  door-overlap+UP detection.

## Section 1 — Data model (pure logic)

New types in `include/logic/` (mirroring the existing spawn-struct + count-field pattern
in `level_data.h`):

```cpp
// A labeled landing point in a room. id 0 = the room's default spawn (dungeon start / restart).
struct EntranceSpawn { int id, tx, ty, facing; };   // facing: -1 left, +1 right

// A door that moves the player to another room in the SAME dungeon.
struct RoomDoorSpawn { int tx, ty, target_room, target_entrance; };

// A dungeon is a graph of rooms. A room IS a LevelData (unchanged).
struct DungeonData {
    const logic::LevelData* const* rooms;   // rooms[0..room_count-1]
    int room_count;
    int start_room;
};
```

`LevelData` gains two array fields (same `const T* ptr; int count;` shape as its existing
spawn arrays):

```cpp
const EntranceSpawn*  entrances;   int entrance_count;
const RoomDoorSpawn*  room_doors;  int room_door_count;
```

Unchanged and reused as-is:

- `has_exit` / `exit_tx` / `exit_ty` — the **dungeon completion** trigger (returns
  `Cleared`). One room per dungeon owns the real exit.
- `DoorSpawn { tx, ty, dungeon }` — the **hub→dungeon** hop (`scene_hub.cpp`). Not a
  room door; left untouched.

**Backward compatibility:** each existing dungeon wraps as a `DungeonData` with
`room_count = 1`, `start_room = 0`, zero entrances (entrance 0 falls back to
`spawn_tx/ty`), zero room-doors. Behavior is identical to today — M1–M6 content needs
**no rewrite** to keep working.

## Section 2 — Latch persistence (SRAM)

Add to `World` (`include/logic/world_state.h`):

```cpp
uint32_t latches = 0;   // bit i = global latchable event i is triggered (0..31)
```

A latchable event is authored with a `latch_id` (0–31; `-1` = not latched). The relevant
spawn struct(s) — at minimum `BrazierGroupSpawn`, and locked/shortcut gates — carry the
id.

- **On room build:** if `latch_id >= 0` and `(world.latches >> latch_id) & 1`, the
  gate/door spawns already-open / the brazier-group spawns already-satisfied.
- **On player trigger:** set the bit and `engine::write_world(world)` immediately — the
  same save-on-event pattern abilities already use.

**Save format:** `SaveData` → **version 3**. Reorder to spend the existing 4 pad bytes on
`uint32_t latches`; widen `checksum` coverage to include them; total size stays **16
bytes** (keeps the `static_assert(sizeof(SaveData)==16)` honest). `load_save` migrates v1
and v2 by setting `latches = 0`. The existing versioned-migration discipline in
`world_state.h` documents exactly this path.

**Latch budget:** 32 global events (≈4 per dungeon across 8 dungeons) for v1 of the
feature. Expansion path if content needs more: widen to `uint64_t` (32→64) or move to a
per-dungeon latch array — both are a version bump + migration, no architectural change.

## Section 3 — Control flow (Approach 1)

Split the current monolithic `run_dungeon` into two functions in `src/game/`:

### `play_room(const LevelData& room, int entrance_id, World&, PlayerState&) → RoomOutcome`

Everything `run_dungeon` does today — build the level via `load_level`, create
avatar/bolt/spell pools and camera, run the per-frame game loop — with two changes:

1. The player spawns at the `EntranceSpawn` whose `id == entrance_id` (id 0 falls back to
   `spawn_tx/ty`), facing per the entrance.
2. The loop additionally tests each `RoomDoorSpawn`: on **player overlap + UP press**
   (the hub-door convention from `scene_hub.cpp:94`), it returns
   `GoToRoom{target_room, target_entrance}`.

`RoomOutcome` is one of:

```
ExitDungeon | Quit | Restart | GoToRoom{ target_room, target_entrance }
```

(`ExitDungeon` = landed on the dungeon exit; `Quit` = SELECT; `Restart` = START, the
existing anti-soft-lock reset — replays the current room.)

### `run_dungeon(const DungeonData&, World&, PlayerState&) → DungeonResult`

Owns `cur_room` and `cur_entrance`:

```
cur_room = dungeon.start_room; cur_entrance = 0
loop:
    outcome = play_room(*dungeon.rooms[cur_room], cur_entrance, world, ps)
    switch outcome:
        ExitDungeon -> return Cleared
        Quit        -> return Quit
        Restart     -> (cur_entrance stays; replay same room)
        GoToRoom    -> fade_out(16); cur_room = target_room;
                       cur_entrance = target_entrance; fade_in; continue
```

Health, magic, and `world.latches` persist across rooms **for free** because they live in
this call frame / `World`. `main.cpp` changes only in that it passes a `DungeonData`
instead of a `LevelData`.

The "given a `GoToRoom` outcome, what are the next `(room, entrance)`?" decision is a
**pure function**, host-tested without `bn::`.

## Section 4 — What does NOT change

- `level_view.cpp` / `level_loader.cpp` — the 64×128 box and EWRAM budget are untouched.
- The camera clamp (`scene_dungeon.cpp`) — already derives bounds from `level.w/h`.
- Gate / brazier / enemy / spell logic — each room is a normal level.

This feature is **purely additive framework**. The map-size ceiling is sidestepped, not
raised.

## Section 5 — Testing

Host tests (`tools/host_test.sh`), all pure logic, no `bn::`:

- Room-graph transition decision: `GoToRoom` → correct next `(room, entrance)`.
- Entrance resolution: `entrance_id` → correct `(tx, ty, facing)`; id 0 falls back to
  `spawn_tx/ty`.
- Latch apply: a set latch bit pre-opens the matching gate/brazier on room build.
- Door-overlap detection via `aabb_overlap`.
- Save round-trip: v3 pack/unpack preserves `latches`; v1/v2 → `latches = 0` migration.

A green run ends with `N/N tests passed, 0 checks failed`.

Run the purity guard before committing: `python tools/check_logic_purity.py`.

## Out of scope (separate efforts)

- **Level/room authoring tool.** Hand-writing multiple room arrays per dungeon (as in
  `include/game/levels/dungeonN.h`) is tedious. A Tiled/CSV → header converter is the
  real leverage for SMB-scale content but is its own project.
- **Smooth-scroll transitions** (Link's Awakening glide) — hard cut ships first.
- **Resuming mid-dungeon at the last room.** Re-entry starts at `start_room`; latches
  keep shortcuts open. Saving `cur_room` is deliberately YAGNI for v1.

## Open question carried into the plan

- Confirm which concrete events get `latch_id`s in the first content pass (likely: D-level
  shortcut doors and any brazier-group that opens a non-trivial gate). The framework
  supports it regardless; this is a content decision, not an architecture one.
