# Room-to-Room Dungeons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make dungeons longer by composing them from multiple rooms linked by doors (hard-cut transitions, room-graph model, SRAM-latched progress gating) — sidestepping the 64×128 map-size ceiling so length comes from room count, not map width.

**Architecture:** A dungeon becomes a `DungeonData` (array of `LevelData*` rooms + a start index). The existing `run_dungeon` splits into a per-room `play_room(...)` (everything it does today) and an outer `run_dungeon(DungeonData)` loop that fades between rooms on door triggers. Progress-gating events (shortcut gates, puzzle brazier-groups) carry a `latch_id`; a 32-bit `latches` field on `World` is saved to SRAM (save format → v3) so opened shortcuts persist. Each room still builds via the existing `load_level` into the existing 64×128 background — no engine map-size change.

**Tech Stack:** C++17 (three-layer: pure `logic/`, Butano `engine/`, scenes `game/`), Butano 21.6.0 (GBA), Python 3 level compiler (`tools/build_level.py`), host unit tests (`tools/host_test.sh`).

**Design spec:** `docs/superpowers/specs/2026-06-11-room-to-room-dungeons-design.md`

---

## Living Document Contract

This plan is a living document. Every executing agent MUST update it as
execution progresses, not only at completion.

- **On phase claim:** the executor MUST flip the banner to 🚧 IN PROGRESS
  with a claim timestamp (ISO 8601 UTC) and the active branch name. The
  banner MUST NOT include an expected-completion estimate — agents cannot
  reliably estimate their own wall-clock, and a fabricated duration
  becomes a stale anchor that misleads future readers. Followers
  encountering a 🚧 banner determine liveness by observable signals (PR
  existence, recent branch commits), not by arithmetic on expected times.
  See Step 5's stale-claim reclaim protocol.
- **On phase ship:** the executor MUST update that phase's **Execution
  Status** banner with the shipped commit SHA(s) and date. If a PR is
  open, the PR number and URL MUST appear in the top-of-plan Execution
  Status table.
- **On phase defer:** the executor MUST update the banner with ⏸ status
  AND a prose description of the unblock condition + a link to the
  likely-unblocker artifact (plan page, task, or PR whose own Execution
  Status banner will signal completion). Prose + link is durable across
  paraphrases and scope edits; exact-string coordination between agents
  is not.
- **On PR merge:** the executor MUST record the merge SHA in the banner
  + the top-of-plan Execution Status table.
- **On deviation from the written plan** (scope edits, structural
  refactors, dropped tasks, reordered phases): the executor MUST
  inline-document the deviation in the affected task AND summarize it
  in the top-of-plan Execution Status as a "Deviations" subsection.
  Deviation state MUST NOT live only in PR notes or status reports.
- **On discovery** (pre-existing drift surfaced during execution, new
  bugs found, architectural issues noted): the executor MUST add a
  "Discoveries" subsection at the top of the plan with pointers to the
  files/lines affected. Follow-up dispatches read this subsection to
  avoid duplicate discovery work.

The plan SHOULD reflect reality at the end of every session that touches
it. Anything worth putting in a status report to the user is worth
putting in the plan.

Rationale: `/writing-plans-enhanced` Step 5. Writing at ship time is
cheap; reconstruction by downstream readers is expensive, compounds
across dispatches, and fails silently when state is split across PR
notes and commit messages.

---

## Execution Status

**Overall:** 5/6 phases shipped (162/162 host tests + 29 Python compiler tests green; ROM builds clean). Phase 6 in progress — automatable build done by subagent, interactive emulator QA handed to the user.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Logic data model | ✅ Shipped | `5a87e0c`, `8fb7e3d`, `f8d6fde` | spec+quality reviewed |
| 2 — Room-graph pure helpers | ✅ Shipped | `95edcbf`, `ec637a7` | spec+quality reviewed |
| 3 — Save format v3 (latches) | ✅ Shipped | `bbabbf9`, `6a51676` | spec+quality reviewed; see Deviations |
| 4 — Level compiler authoring | ✅ Shipped | `103eb72`, `9427e10` | spec+quality reviewed |
| 5 — Engine/game room loop | ✅ Shipped | `262575f`, `90989e8`, `47ed5bb`, `dac7a46`, `8c80062` | spec+quality reviewed; ROM builds clean; Restart deviation below |
| 6 — Vertical slice + manual QA | 🚧 In progress | — | claimed 2026-06-11; ROM-build part automatable, 7-step emulator QA → user (mGBA at `C:/Program Files/mGBA/mGBA.exe`) |

### Deviations
- **Task 5.3 (Restart handling moved into `run_dungeon`):** As designed in the plan, START (`DungeonResult::Restart`) is now consumed inside `run_dungeon` (refills vitals + re-enters the *current* room) rather than being returned to `main.cpp`'s old `do/while`. With multi-room dungeons, START resets the room you're stuck in, not the dungeon start. `DungeonResult::Restart` remains defined but is now internal-only (never returned). `main.cpp` simplified to a single `run_dungeon` call.
- **Task 3.1 (SaveData layout):** The implementer kept `magic`+`version` at offsets 0–5 (matching v1/v2) and placed `uint32_t latches` at offset 12, instead of the plan's prescribed offset-4 placement. Rationale: version detection stays at a stable offset across all save versions and the v2 field offsets stay layout-compatible (so the v2 migration branch reads via struct members safely). `sizeof(SaveData)==16` still holds (latches@12 is 4-byte aligned); checksum is non-contiguous `[0..10]+[12..15]` skipping the checksum byte at [11]. Verified correct by spec + quality review. Also fixed a latent bug in the plan's migration test (`0x0004` commented "Fire" → `0x0002`, since `Ability::Fire`=bit 1).

### Discoveries
- **Pre-existing:** A level compiler already exists (`tools/build_level.py`, tested by `tools/test_build_level.py`). The spec listed "authoring tool" as out of scope; this plan extends the existing compiler with two new symbols (Phase 4). The larger Tiled/CSV importer remains out of scope.
- **ROM toolchain env (this Windows machine):** devkitPro is installed at `C:/devkitPro`, but the shell's `DEVKITPRO` env points to the non-existent Linux path `/opt/devkitpro` and `arm-none-eabi-g++` is not on PATH. A plain `make` will fail. Build the ROM with the corrected env: `DEVKITPRO=/c/devkitPro DEVKITARM=/c/devkitPro/devkitARM PATH="/c/devkitPro/devkitARM/bin:/c/devkitPro/tools/bin:$PATH" make`. (`arm-none-eabi-g++.exe` confirmed at `C:/devkitPro/devkitARM/bin`; `make` at `C:/devkitPro/msys2/usr/bin/make`.) Phase 5/6 build steps must use this. A full end-to-end Butano build has not yet been run — the Phase 5.1 implementer performs the first one and captures any further fixups here.

---

## Conventions every task must follow

- **Three-layer rule** (`CLAUDE.md`): nothing under `include/logic/` or `src/logic/` may include or name a `bn::` type. Phases 1–4 are pure/Python and stay host-testable. Phase 5 is the only `bn::` work.
- **Host tests** run via `bash tools/host_test.sh` (NOT bare `make -C test` on this machine). A green run ends with `N/N tests passed, 0 checks failed`. The script also runs `tools/check_logic_purity.py` and the Python compiler tests, and regenerates level headers from `tools/levels/*.txt` before compiling.
- **Purity guard** before any commit touching logic: `python tools/check_logic_purity.py`.
- **Pitfalls:** read `docs/pitfalls/implementation-pitfalls.md` and `docs/pitfalls/testing-pitfalls.md` before each phase. Relevant traps are called out per-task.

---

## Phase 1 — Logic data model

**Execution Status:** ✅ SHIPPED at `5a87e0c`, `8fb7e3d`, `f8d6fde` on 2026-06-11 (spec + code-quality reviewed; 153/153 host tests green)

Adds the pure-logic types. Uses **default member initializers on trailing fields** so every existing generated `LevelData` literal keeps compiling unchanged.

**BEFORE starting this phase:**
1. Invoke `superpowers:test-driven-development`.
2. Read `docs/pitfalls/testing-pitfalls.md` (esp. TEST-2: assert exact integer values).
Follow TDD: write failing test → implement → verify green.

### Task 1.1: Add `EntranceSpawn`, `RoomDoorSpawn`, and `DungeonData`; extend `LevelData`

**Files:**
- Modify: `include/logic/level_data.h`
- Test: `test/test_room_data.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `test/test_room_data.cpp`:

```cpp
#include "test_framework.h"
#include "logic/level_data.h"
using namespace logic;

// A minimal room: 1 entrance, 1 room-door, no other content.
static constexpr unsigned char R_TILES[] = { 1 };
static constexpr EntranceSpawn  R_ENTR[]  = { {0, 3, 5, 1}, {1, 10, 5, -1} };
static constexpr RoomDoorSpawn  R_DOORS[] = { {12, 5, 1, 0} };  // at (12,5) -> room 1, entrance 0

TEST(leveldata_defaults_room_fields_absent){
    // A LevelData built the OLD way (no entrance/room-door fields) defaults them empty.
    LevelData lv{ R_TILES, 1, 1, 0, 0, false, 0, 0, false, 0, 0,
                  nullptr,0, nullptr,0, nullptr,0, nullptr,0, nullptr,0,
                  nullptr,0, nullptr,0, nullptr,0, nullptr,0 };
    CHECK(lv.entrances == nullptr);
    CHECK_EQ(lv.entrance_count, 0);
    CHECK(lv.room_doors == nullptr);
    CHECK_EQ(lv.room_door_count, 0);
}

TEST(entrance_and_roomdoor_fields){
    EntranceSpawn e = R_ENTR[1];
    CHECK_EQ(e.id, 1); CHECK_EQ(e.tx, 10); CHECK_EQ(e.ty, 5); CHECK_EQ(e.facing, -1);
    RoomDoorSpawn d = R_DOORS[0];
    CHECK_EQ(d.tx, 12); CHECK_EQ(d.ty, 5); CHECK_EQ(d.target_room, 1); CHECK_EQ(d.target_entrance, 0);
}

TEST(dungeondata_indexes_rooms){
    static constexpr LevelData ROOMA{ R_TILES,1,1,0,0,false,0,0,false,0,0,
        nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0 };
    static constexpr LevelData ROOMB = ROOMA;
    static constexpr const LevelData* ROOMS[] = { &ROOMA, &ROOMB };
    DungeonData dg{ ROOMS, 2, 0 };
    CHECK_EQ(dg.room_count, 2);
    CHECK_EQ(dg.start_room, 0);
    CHECK(dg.rooms[1] == &ROOMB);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `bash tools/host_test.sh`
Expected: FAIL to compile (`EntranceSpawn`/`RoomDoorSpawn`/`DungeonData` undefined; `LevelData` has no `entrances`).

- [ ] **Step 3: Implement in `include/logic/level_data.h`**

Add the two spawn structs near the other spawn structs (after `BrazierGroupSpawn`):

```cpp
struct EntranceSpawn { int id, tx, ty, facing; };     // facing: -1 left, +1 right. id 0 = default/start.
struct RoomDoorSpawn { int tx, ty, target_room, target_entrance; };
```

Add **trailing** fields to `LevelData` (after `brazier_groups`/`brazier_group_count`), each with a default so existing literals still compile:

```cpp
    const EntranceSpawn* entrances  = nullptr; int entrance_count  = 0;
    const RoomDoorSpawn* room_doors = nullptr; int room_door_count = 0;
```

After the `LevelData` struct, add:

```cpp
struct DungeonData {
    const LevelData* const* rooms;   // rooms[0..room_count-1]
    int room_count;
    int start_room;
};
```

> Do NOT reorder or insert fields in the MIDDLE of `LevelData` — the generated headers in `include/game/levels/` use positional aggregate init; only trailing defaulted fields are append-safe.

- [ ] **Step 4: Register the new test file in the host build**

The host runner globs `test_*.cpp` automatically (`tools/host_test.sh`: `TEST_SRC=$(ls test_*.cpp)`), so no Makefile edit is needed. Confirm by running the suite.

- [ ] **Step 5: Run to verify it passes**

Run: `bash tools/host_test.sh`
Expected: PASS — ends with `N/N tests passed, 0 checks failed`. All pre-existing `test_*_level.cpp` (which construct `LevelData` positionally) still compile because the new fields are trailing + defaulted.

- [ ] **Step 6: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/level_data.h test/test_room_data.cpp
git commit -m "feat(logic): add EntranceSpawn, RoomDoorSpawn, DungeonData; extend LevelData"
```

### Task 1.2: Add `latch_id` to latchable spawns

**Files:**
- Modify: `include/logic/level_data.h` — both `GateSpawn` and `BrazierGroupSpawn` are declared here (`level_data.h:7` and `:13`). No other file changes.
- Test: `test/test_room_data.cpp` (extend)

- [ ] **Step 1: Write the failing test** (append to `test/test_room_data.cpp`)

```cpp
TEST(latch_id_defaults_to_minus_one){
    GateSpawn g{ 4, 5, GateType::Gap };           // old 3-field literal still compiles
    CHECK_EQ(g.latch_id, -1);
    BrazierGroupSpawn bg{ 3, 8, 9 };               // old 3-field literal still compiles
    CHECK_EQ(bg.latch_id, -1);
    GateSpawn g2{ 4, 5, GateType::Vine, 7 };       // explicit latch id
    CHECK_EQ(g2.latch_id, 7);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `bash tools/host_test.sh`
Expected: FAIL to compile (`GateSpawn` has no `latch_id`).

- [ ] **Step 3: Implement** — add a trailing defaulted field to each:

```cpp
struct GateSpawn   { int tx, ty; GateType type; int latch_id = -1; };
struct BrazierGroupSpawn { int total, target_tx, target_ty; int latch_id = -1; };
```

> Trailing + defaulted again: the generated `*_GATES[]` (`{0,0,logic::GateType::Gap}`) and `*_BRAZIER_GROUPS[]` (`{0,0,0}`) literals keep compiling, with `latch_id` defaulting to `-1` (= not latched).

- [ ] **Step 4: Run to verify it passes**

Run: `bash tools/host_test.sh`
Expected: PASS.

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/level_data.h test/test_room_data.cpp
git commit -m "feat(logic): add optional latch_id to GateSpawn and BrazierGroupSpawn"
```

**BEFORE marking Phase 1 complete:**
1. Review tests against `docs/pitfalls/testing-pitfalls.md` (TEST-2: exact integer asserts — done).
2. Verify edge cases: old positional literals still compile (covered by the unchanged `test_*_level.cpp` suite compiling green).
3. Run `bash tools/host_test.sh`, confirm green.

**After completing Phase 1:** Review the batch from multiple perspectives. Minimum 3 review rounds. If round 3 still finds issues, keep going until clean.

---

## Phase 2 — Room-graph pure helpers

**Execution Status:** ✅ SHIPPED at `95edcbf`, `ec637a7` on 2026-06-11 (spec + code-quality reviewed; 157/157 host tests green)

Two pure functions the scene will call: resolve an entrance to a spawn position, and find a room-door overlapping the player. Both `bn::`-free and host-tested. This keeps the testable decision logic out of the `bn::`-heavy scene.

**BEFORE starting this phase:**
1. Invoke `superpowers:test-driven-development`.
2. Read `docs/pitfalls/testing-pitfalls.md`.

### Task 2.1: `find_entrance` and `room_door_at`

**Files:**
- Create: `include/logic/room_graph.h`
- Test: `test/test_room_graph.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `test/test_room_graph.cpp`:

```cpp
#include "test_framework.h"
#include "logic/room_graph.h"
#include "logic/level_data.h"
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/fixed.h"
using namespace logic;

static constexpr unsigned char T[] = { 1 };
static constexpr EntranceSpawn ENTR[] = { {0, 3, 5, 1}, {2, 20, 7, -1} };
static constexpr RoomDoorSpawn DOORS[] = { {12, 5, 1, 0}, {40, 7, 2, 2} };
static constexpr LevelData ROOM{ T, 50, 12, 9, 9, false,0,0, false,0,0,
    nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,
    ENTR, 2, DOORS, 2 };

TEST(find_entrance_by_id){
    EntranceSpawn e = find_entrance(ROOM, 2);
    CHECK_EQ(e.id, 2); CHECK_EQ(e.tx, 20); CHECK_EQ(e.ty, 7); CHECK_EQ(e.facing, -1);
}
TEST(find_entrance_fallback_to_spawn){
    // id not present -> fall back to the room's default spawn tile, facing +1.
    EntranceSpawn e = find_entrance(ROOM, 99);
    CHECK_EQ(e.tx, 9); CHECK_EQ(e.ty, 9); CHECK_EQ(e.facing, 1);
}
TEST(room_door_at_overlap){
    // Player AABB centred on door 0's tile (12,5) -> px (96..). Body pos is top-left in px.
    Body p{}; p.half_w = Fixed::from_int(8); p.half_h = Fixed::from_int(16);
    p.pos = { Fixed::from_int(12*8), Fixed::from_int(5*8) };
    const RoomDoorSpawn* d = room_door_at(ROOM, p);
    CHECK(d != nullptr);
    CHECK_EQ(d->target_room, 1);
}
TEST(room_door_at_none){
    Body p{}; p.half_w = Fixed::from_int(8); p.half_h = Fixed::from_int(16);
    p.pos = { Fixed::from_int(2*8), Fixed::from_int(2*8) };   // far from any door
    CHECK(room_door_at(ROOM, p) == nullptr);
}
```

> Includes are verified correct as written: `Body` is in `logic/physics.h` (reached via `logic/collision.h`), `aabb_overlap` is in `logic/collision.h`, `Fixed::from_int` is in `logic/fixed.h`, and `Vec2` (for `Body::pos`) in `logic/vec2.h` (reached transitively). `aabb_overlap` treats `Body::pos` as TOP-LEFT with the box spanning `[pos, pos + 2*half]` (`collision.cpp:84`), which is why the player at px `(96,40)` half `(8,16)` overlaps door 0's `(96,40)` half `(8,8)` body.

- [ ] **Step 2: Run to verify it fails**

Run: `bash tools/host_test.sh`
Expected: FAIL to compile (`room_graph.h` missing).

- [ ] **Step 3: Implement `include/logic/room_graph.h`**

```cpp
#pragma once
#include "logic/level_data.h"
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/fixed.h"
namespace logic {

// Resolve an entrance id to its spawn point. If the room defines an entrance with that id
// (rooms MAY define id 0), return it. Otherwise fall back to the room's default spawn tile
// (spawn_tx/ty) facing right (+1) — the dungeon-start behavior, used when entering at id 0 a
// room that has only an '@' spawn and no matching 'N'.
inline EntranceSpawn find_entrance(const LevelData& room, int id){
    for(int i = 0; i < room.entrance_count; ++i)
        if(room.entrances[i].id == id) return room.entrances[i];
    return EntranceSpawn{ 0, room.spawn_tx, room.spawn_ty, 1 };
}

// Return the first room-door whose 1-tile (16x16) trigger body overlaps the player, else null.
inline const RoomDoorSpawn* room_door_at(const LevelData& room, const Body& player){
    for(int i = 0; i < room.room_door_count; ++i){
        const RoomDoorSpawn& d = room.room_doors[i];
        Body db{}; db.half_w = Fixed::from_int(8); db.half_h = Fixed::from_int(8);
        db.pos = { Fixed::from_int(d.tx * 8), Fixed::from_int(d.ty * 8) };
        if(aabb_overlap(player, db)) return &room.room_doors[i];
    }
    return nullptr;
}
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `bash tools/host_test.sh`
Expected: PASS.

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/room_graph.h test/test_room_graph.cpp
git commit -m "feat(logic): room_graph helpers find_entrance + room_door_at"
```

**BEFORE marking Phase 2 complete:**
1. Review tests vs `docs/pitfalls/testing-pitfalls.md`. Add a tunneling-style edge case only if relevant (door overlap is positional, not swept — TEST-3 N/A here; note that in the commit).
2. Confirm fallback path (unknown id) and no-overlap path are both covered.
3. Run `bash tools/host_test.sh`, confirm green.

**After completing Phase 2:** Minimum 3 review rounds.

---

## Phase 3 — Save format v3 (latches)

**Execution Status:** ✅ SHIPPED at `bbabbf9`, `6a51676` on 2026-06-11 (spec + code-quality reviewed; 162/162 host tests green; layout deviation recorded in Execution Status › Deviations)

Adds `World.latches` and bumps the SRAM save to v3, reusing the existing pad bytes so size stays 16. Migrates v1/v2 with `latches = 0`.

**BEFORE starting this phase:**
1. Invoke `superpowers:test-driven-development`.
2. Read `docs/pitfalls/testing-pitfalls.md` (TEST-4: cover empty/corrupt SRAM).

### Task 3.1: `World.latches` + helpers

**Files:**
- Modify: `include/logic/world_state.h`
- Test: `test/test_world_state_v3.cpp` (create)

- [ ] **Step 1: Write the failing test**

Create `test/test_world_state_v3.cpp`:

```cpp
#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

TEST(latches_default_zero){ World w; CHECK_EQ((int)w.latches, 0); CHECK(!w.latched(0)); }
TEST(set_and_test_latch){ World w; w.set_latch(5); CHECK(w.latched(5)); CHECK(!w.latched(6)); }

TEST(save_v3_roundtrip_latches){
    World w; w.grant(Ability::Fire); w.set_latch(0); w.set_latch(31); w.current_dungeon = 3;
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 3);
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.has(Ability::Fire));
    CHECK(w2.latched(0)); CHECK(w2.latched(31)); CHECK(!w2.latched(1));
    CHECK_EQ((int)w2.current_dungeon, 3);
}
TEST(save_v3_bad_checksum_rejected){ World w; SaveData s = make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s, w2) == false); }
TEST(save_v3_empty_rejected){ SaveData s{}; World w2; CHECK(load_save(s, w2) == false); }

TEST(v2_save_migrates_latches_zero){
    // Build a v2-layout 16-byte buffer by hand: magic(4) ver=2(2) spronks(2) abilities(2)
    // current_dungeon(1) checksum@11(1) pad[4]. Latches must come back 0.
    uint8_t buf[sizeof(SaveData)]; std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 2; std::memcpy(buf + 4, &ver, 2);
    uint16_t spr = 0x0001; std::memcpy(buf + 6, &spr, 2);
    uint16_t abil = 0x0004; std::memcpy(buf + 8, &abil, 2);  // Fire
    buf[10] = 0;                                              // current_dungeon
    uint32_t sum = 0; for(int i = 0; i < 11; ++i) sum += buf[i]; buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(SaveData));
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.spronk_freed(1)); CHECK(w2.has(Ability::Fire));
    CHECK_EQ((int)w2.latches, 0);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `bash tools/host_test.sh`
Expected: FAIL to compile (`World` has no `latches`/`set_latch`/`latched`).

- [ ] **Step 3: Implement in `include/logic/world_state.h`**

Add to `World`:

```cpp
    uint32_t latches = 0;        // bit i = global latchable event i triggered (0..31)
    bool latched(int i) const { return (latches >> i) & 1u; }
    void set_latch(int i){ latches |= (uint32_t)(1u << i); }
```

Rework `SaveData` to spend the pad bytes on `latches`, size stays 16. **Order fields so `uint32_t latches` is 4-byte aligned with NO padding** — placing it at offset 4 (right after `magic`) keeps `sizeof(SaveData)==16`. (Putting it after `current_dungeon` at offset 10 forces alignment padding → `sizeof==20`, which fails the assert. Do not use that order.)

```cpp
struct SaveData {
    uint32_t magic;          // [0..3]   'SPRK'
    uint32_t latches;        // [4..7]   World.latches  (4-byte aligned -> no padding)
    uint16_t version;        // [8..9]   3
    uint16_t spronks;        // [10..11] World.spronks_freed
    uint16_t abilities;      // [12..13] World.abilities
    uint8_t  current_dungeon;// [14]
    uint8_t  checksum;       // [15]     sum of bytes [0..14] & 0xFF
};
static_assert(std::is_trivially_copyable<SaveData>::value, "SaveData must be trivially copyable (SRAM)");
static_assert(sizeof(SaveData) == 16, "SaveData v3 layout changed — bump version + migrate");

static constexpr uint16_t SAVE_VERSION = 3;
```

> IMPL-4: SaveData is persisted only through `engine::write_world` / `engine::read_world`, which route through `bn::sram` — never via raw pointers. This task changes only the struct + pack/unpack in `world_state.h` (pure logic, host-tested); `engine/save.cpp` memcpys the struct unchanged.

Update `make_save` to fill `latches` and checksum over the new range:

```cpp
inline SaveData make_save(const World& w){
    SaveData s{};
    s.magic = SAVE_MAGIC; s.version = SAVE_VERSION;
    s.spronks = w.spronks_freed; s.abilities = w.abilities; s.current_dungeon = w.current_dungeon;
    s.latches = w.latches;
    s.checksum = checksum_bytes(reinterpret_cast<const uint8_t*>(&s), 15); // [0..14]
    return s;
}
```

Update `load_save`: keep the v1 and v2 branches (both set `latches = 0` via `World{}`), add a v3 branch:

```cpp
    if(version == 2){
        if(checksum_bytes(b, 11) != b[11]) return false;   // v2 checksum at offset 11
        out = World{};
        __builtin_memcpy(&out.spronks_freed, b + 6, 2);
        __builtin_memcpy(&out.abilities,     b + 8, 2);
        out.current_dungeon = b[10];
        // latches stays 0 (new in v3)
        return true;
    }
    if(version == 3){
        if(checksum_bytes(b, 15) != b[15]) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon; out.latches = s.latches;
        return true;
    }
```

> The current v2 branch reads `s.spronks`/`s.abilities` via struct members. After the struct changes, the v2 byte offsets (checksum at 11, pad after) no longer match the new member layout, so the v2 path MUST read by raw byte offset (as shown) rather than via `s.<member>`. This is why the v2 test builds a raw buffer.

- [ ] **Step 4: Run — expect exactly one pre-existing test to fail**

Run: `bash tools/host_test.sh`
Expected: the new `test_world_state_v3.cpp` passes, but the pre-existing `save_v2_is_version_2` in `test/test_world_state_v2.cpp` **FAILS** — it asserts `make_save` emits `version == 2`, which is now `3`. Tracing the other v2 tests: `save_v2_roundtrip` still PASSES (it asserts only fields, which roundtrip through the new v3 branch — just misnamed now), and `empty_sram_rejected` / `bad_checksum_rejected` / `v1_save_migrates` are version-agnostic and still PASS. So exactly one failure.

- [ ] **Step 5: Delete the obsolete version-pinning test**

In `test/test_world_state_v2.cpp`, delete ONLY `save_v2_is_version_2` (its intent — "the writer emits v2" — is obsolete; current-writer coverage now lives in `test_world_state_v3.cpp`). Add a one-line comment at the top: `// Current-writer (v3) coverage lives in test_world_state_v3.cpp; this file keeps v1->migration + corruption-rejection cases.` Leave `save_v2_roundtrip` and the others as-is (they still pass).

Run: `bash tools/host_test.sh`
Expected: PASS — `N/N tests passed, 0 checks failed`.

Run: `bash tools/host_test.sh`
Expected: PASS — `N/N tests passed, 0 checks failed`.

- [ ] **Step 6: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/world_state.h test/test_world_state_v3.cpp test/test_world_state_v2.cpp
git commit -m "feat(logic): SRAM save v3 adds World.latches; migrate v1/v2 with latches=0"
```

**BEFORE marking Phase 3 complete:**
1. Review vs `docs/pitfalls/testing-pitfalls.md` (TEST-4 covered: empty + bad checksum + v2 migration).
2. Confirm `sizeof(SaveData)==16` static_assert passes on the host compiler AND will hold on the GBA ARM toolchain (no surprise padding) — note in commit that layout is checksum-range-explicit.
3. Run `bash tools/host_test.sh`, confirm green.

**After completing Phase 3:** Minimum 3 review rounds.

---

## Phase 4 — Level compiler authoring (entrances + room-doors + brazier latch)

**Execution Status:** ✅ SHIPPED at `103eb72`, `9427e10` on 2026-06-11 (spec + code-quality reviewed; 29 Python compiler tests + 162/162 host tests green)

Extends `tools/build_level.py` so rooms can be authored in `.txt`/JSON. Adds the `N` (entrance) and `D` (room-door) grid symbols, an optional `latch_id` on brazier groups, and emits the new `LevelData` trailing fields. The per-dungeon **room table** (`DungeonData`) is hand-written in a small header for v1 (few rooms) — a descriptor generator is deferred (YAGNI).

**BEFORE starting this phase:**
1. Invoke `superpowers:test-driven-development`.
2. Read `docs/pitfalls/implementation-pitfalls.md`.
Compiler tests live in `tools/test_build_level.py` (run via `python -m unittest test_build_level.py` from `tools/`, or the whole `bash tools/host_test.sh`).

### Task 4.1: Parse `N` entrance and `D` room-door symbols

**Files:**
- Modify: `tools/build_level.py`
- Test: `tools/test_build_level.py` (extend)

- [ ] **Step 1: Write the failing tests** (append to `tools/test_build_level.py`)

```python
    def test_entrance_symbol(self):
        txt = "######\n#@N..#\n######\n"
        lvl = compile_str(txt, {"entrances": [{"id": 1, "facing": -1}]})
        self.assertEqual(lvl['entrances'], [(1, 2, 1, -1)])  # (id, tx, ty, facing)

    def test_entrance_defaults(self):
        txt = "######\n#@N..#\n######\n"
        lvl = compile_str(txt, {})  # no entrance metadata -> id by order, facing +1
        self.assertEqual(lvl['entrances'], [(0, 2, 1, 1)])

    def test_room_door_symbol(self):
        txt = "######\n#@.D.#\n######\n"
        lvl = compile_str(txt, {"room_doors": [{"target_room": 2, "target_entrance": 1}]})
        self.assertEqual(lvl['room_doors'], [(3, 1, 2, 1)])  # (tx, ty, target_room, target_entrance)

    def test_room_door_requires_target(self):
        txt = "######\n#@.D.#\n######\n"
        with self.assertRaises(build_level.LevelError):
            compile_str(txt, {})  # room-door with no target metadata

    def test_brazier_group_latch_id(self):
        txt = "########\n#@*....#\n########\n"
        lvl = compile_str(txt, {
            "braziers": [{"group": 0}],
            "brazier_groups": [{"total": 1, "target": [5, 5], "latch_id": 4}],
        })
        self.assertEqual(lvl['brazier_groups'], [(1, 5, 5, 4)])

    def test_brazier_group_latch_default(self):
        txt = "########\n#@*....#\n########\n"
        lvl = compile_str(txt, {
            "braziers": [{"group": 0}],
            "brazier_groups": [{"total": 1, "target": [5, 5]}],
        })
        self.assertEqual(lvl['brazier_groups'], [(1, 5, 5, -1)])  # default not-latched

    def test_emit_header_has_room_fields(self):
        txt = "######\n#@ND.#\n######\n"
        lvl = compile_str(txt, {"entrances": [{"id": 0, "facing": 1}],
                                "room_doors": [{"target_room": 1, "target_entrance": 0}]})
        hdr = build_level.emit_header(lvl, "TESTRM")
        self.assertIn("TESTRM_ENTRANCES", hdr)
        self.assertIn("TESTRM_ROOM_DOORS", hdr)
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd tools && python -m unittest test_build_level.py` (or `bash tools/host_test.sh`)
Expected: FAIL — unknown symbol `N`/`D`; `lvl` has no `entrances`/`room_doors`.

- [ ] **Step 3: Implement in `tools/build_level.py`**

In the docstring's "Grid symbols" block, document: `N=entrance  D=room-door`. Add `N` and `D` to `CONTENT`:

```python
CONTENT = set('@CEoG12345678VIWXFBK=?*ND')
```

Add the collectors near the others (after `braziers = []`):

```python
    entrances = []      # (id, tx, ty, facing)
    room_doors = []     # (tx, ty, target_room, target_entrance)
    n_idx = rd_idx = 0
```

Read JSON sidecars at the top with the others:

```python
    j_entrances = meta.get('entrances', [])
    j_room_doors = meta.get('room_doors', [])
```

In the row/col scan, add two branches:

```python
            elif c == 'N':
                je = j_entrances[n_idx] if n_idx < len(j_entrances) else {}
                eid = je.get('id', n_idx)
                facing = je.get('facing', 1)
                entrances.append((eid, x, y, facing))
                n_idx += 1
            elif c == 'D':
                if rd_idx >= len(j_room_doors):
                    raise LevelError(f"room-door 'D' at ({x},{y}) but JSON has no 'room_doors' entry #{rd_idx}")
                t = j_room_doors[rd_idx]
                room_doors.append((x, y, t['target_room'], t['target_entrance']))
                rd_idx += 1
```

Update `brazier_groups` parsing to read optional `latch_id`:

```python
    brazier_groups = [(g['total'], g['target'][0], g['target'][1], g.get('latch_id', -1))
                      for g in j_brazier_groups]
```

Add `entrances`/`room_doors` to the returned dict:

```python
        'entrances': entrances, 'room_doors': room_doors,
```

In `emit_header`, **immediately after the existing `BRAZIER_GROUPS` `emit_array` call** (so the variables are in scope for the `LevelData` line), emit the two new arrays, and change the `BRAZIER_GROUPS` emit to carry `latch_id`:

```python
    line, encount = emit_array('logic::EntranceSpawn', 'ENTRANCES',
                               [f'{{{eid},{tx},{ty},{fac}}}' for (eid, tx, ty, fac) in level['entrances']],
                               '{0,0,0,1}'); L.append(line)
    line, rdcount = emit_array('logic::RoomDoorSpawn', 'ROOM_DOORS',
                               [f'{{{tx},{ty},{tr},{te}}}' for (tx, ty, tr, te) in level['room_doors']],
                               '{0,0,0,0}'); L.append(line)
```

Change the `BRAZIER_GROUPS` emit to include latch_id (now a 4-tuple):

```python
    line, bgcount = emit_array('logic::BrazierGroupSpawn', 'BRAZIER_GROUPS',
                               [f'{{{tot},{ttx},{tty},{lat}}}' for (tot, ttx, tty, lat) in level['brazier_groups']],
                               '{0,0,0,-1}'); L.append(line)
```

Append the two new fields to the `LevelData` aggregate emission (after `..._BRAZIER_GROUPS, {bgcount}`):

```python
        f'{name}_BRAZIER_GROUPS, {bgcount}, '
        f'{name}_ENTRANCES, {encount}, {name}_ROOM_DOORS, {rdcount} }};'
```

> The existing single-room dungeons regenerate with `entrance_count = 0`, `room_door_count = 0` — identical runtime behavior. Keep the field ORDER identical to the `LevelData` struct (Phase 1.1): braziers/brazier_groups, then entrances, then room_doors.

- [ ] **Step 4: Fix the pre-existing brazier-group test for the 4-tuple**

Making `brazier_groups` 4-tuples breaks the existing `test_block_plate_button_brazier` assertion in `tools/test_build_level.py:148`. Update it:

```python
        self.assertEqual(lvl['brazier_groups'], [(1, 5, 5, -1)])  # latch_id defaults to -1
```

- [ ] **Step 5: Run to verify it passes**

Run: `cd tools && python -m unittest test_build_level.py`
Expected: OK (all tests pass, including the updated `test_block_plate_button_brazier`).

- [ ] **Step 6: Regenerate existing headers + run full host suite**

Run: `bash tools/host_test.sh`
Expected: the script regenerates `include/game/levels/*.h` from `tools/levels/*.txt` (now with the new trailing fields), recompiles, and ends `N/N tests passed, 0 checks failed`. The regenerated dungeon headers now carry empty entrance/room-door arrays.

- [ ] **Step 7: Commit (regenerated headers included)**

```bash
git add tools/build_level.py tools/test_build_level.py include/game/levels/*.h
git commit -m "feat(tools): compile N (entrance) and D (room-door) symbols + brazier latch_id"
```

> Commit subject touches no test assertions in the C++ suite; the regenerated headers are data, not assertion changes.

**BEFORE marking Phase 4 complete:**
1. Review tests vs `docs/pitfalls/testing-pitfalls.md`.
2. Confirm the missing-target error path (`test_room_door_requires_target`) and both latch default/explicit paths are covered.
3. Run `bash tools/host_test.sh`, confirm green AND that regenerated headers compile.

**After completing Phase 4:** Minimum 3 review rounds.

---

## Phase 5 — Engine/game room loop

**Execution Status:** ✅ SHIPPED at `262575f`, `90989e8`, `47ed5bb`, `dac7a46`, `8c80062` on 2026-06-11 (spec + code-quality reviewed; ROM builds clean; 162/162 host tests green). Runtime behavior verified in Phase 6 manual QA.

The keystone, and the **only `bn::` phase**. Splits `run_dungeon` into a per-room `play_room` and an outer room-loop `run_dungeon(DungeonData)`, applies latches on room build, spawns at entrances, and transitions on room-door overlap+UP. This phase is **not host-testable** (Butano types); it is verified by (a) the Phase 2 pure helpers it calls, (b) a clean ROM compile, and (c) the Phase 6 manual playtest. Do NOT weaken or invent host tests that pull `bn::` into the logic layer to "cover" this — the three-layer rule forbids it; rely on the pure helpers + manual QA instead.

**BEFORE starting this phase:**
1. Invoke `superpowers:test-driven-development` (applies to the helper-level seams; the scene body is a behavior-preserving mechanical extraction).
2. Read `docs/pitfalls/implementation-pitfalls.md` AND `docs/pitfalls/testing-pitfalls.md`.
3. Re-read `src/game/scene_dungeon.cpp` in full — Phase 5 restructures it.

### Task 5.1: Make `run_dungeon` take `DungeonData` (behavior-preserving for 1 room)

**Files:**
- Modify: `include/game/scene_dungeon.h`, `src/game/scene_dungeon.cpp`, `src/main.cpp`
- Modify: `include/game/levels/dungeon1.h`..`dungeon5.h` consumers via new 1-room wrappers (see below)

- [ ] **Step 1: Change the signature + wrap existing dungeons**

In `include/game/scene_dungeon.h`, change:

```cpp
DungeonResult run_dungeon(const logic::DungeonData& dungeon, logic::World& world, logic::PlayerState& ps);
```

Add a tiny wrapper header `include/game/levels/dungeons.h` that turns each existing single-room `*_DATA` into a 1-room `DungeonData`:

```cpp
#pragma once
#include "logic/level_data.h"
#include "game/levels/dungeon1.h"
#include "game/levels/dungeon2.h"
#include "game/levels/dungeon3.h"
#include "game/levels/dungeon4.h"
#include "game/levels/dungeon5.h"
inline constexpr const logic::LevelData* DUNGEON1_ROOMS[] = { &DUNGEON1_DATA };
inline constexpr logic::DungeonData DUNGEON1_DUNGEON{ DUNGEON1_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON2_ROOMS[] = { &DUNGEON2_DATA };
inline constexpr logic::DungeonData DUNGEON2_DUNGEON{ DUNGEON2_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON3_ROOMS[] = { &DUNGEON3_DATA };
inline constexpr logic::DungeonData DUNGEON3_DUNGEON{ DUNGEON3_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON4_ROOMS[] = { &DUNGEON4_DATA };
inline constexpr logic::DungeonData DUNGEON4_DUNGEON{ DUNGEON4_ROOMS, 1, 0 };
inline constexpr const logic::LevelData* DUNGEON5_ROOMS[] = { &DUNGEON5_DATA };
inline constexpr logic::DungeonData DUNGEON5_DUNGEON{ DUNGEON5_ROOMS, 1, 0 };
```

In `src/main.cpp`, include `game/levels/dungeons.h`, and map dungeon number → `DungeonData*`:

```cpp
const logic::DungeonData* lvl = nullptr;
if(n == 1) lvl = &DUNGEON1_DUNGEON;
else if(n == 2) lvl = &DUNGEON2_DUNGEON;
else if(n == 3) lvl = &DUNGEON3_DUNGEON;
else if(n == 4) lvl = &DUNGEON4_DUNGEON;
else if(n == 5) lvl = &DUNGEON5_DUNGEON;
else continue;
...
dr = game::run_dungeon(*lvl, world, ps);
```

- [ ] **Step 2: In `src/game/scene_dungeon.cpp`, wrap the existing body**

Rename the current `run_dungeon(const LevelData& level, ...)` body to a static helper that plays one room, and add the new `run_dungeon(const DungeonData&, ...)` that calls it on the start room. **Keep the helper's parameter named `level`** — the moved body references `level` throughout, so the rename is signature-only. For 5.1 the helper still `return`s the same `DungeonResult` (no room transitions yet), so behavior is identical for 1-room dungeons:

```cpp
// (temporary shape for 5.1 — becomes play_room in 5.2)
static DungeonResult play_room_single(const logic::LevelData& level,
                                       logic::World& world, logic::PlayerState& ps){
    /* ...the entire existing run_dungeon body, verbatim... */
}

DungeonResult run_dungeon(const logic::DungeonData& dungeon, logic::World& world, logic::PlayerState& ps){
    return play_room_single(*dungeon.rooms[dungeon.start_room], world, ps);
}
```

- [ ] **Step 3: Build the ROM**

Run: `make` (requires devkitPro + Butano).
Expected: compiles clean. If devkitPro is unavailable in this environment, STOP and hand back to the dispatcher — Phase 5 requires a ROM toolchain; do NOT fake the build.

- [ ] **Step 4: Commit**

```bash
git add include/game/scene_dungeon.h src/game/scene_dungeon.cpp src/main.cpp include/game/levels/dungeons.h
git commit -m "refactor(game): run_dungeon takes DungeonData; 1-room wrappers (behavior-preserving)"
```

### Task 5.2: Extract `play_room` returning `RoomOutcome`; entrance spawn; latch apply; room-door trigger

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

- [ ] **Step 1: Define `RoomOutcome`** (top of the `game` anonymous namespace in `scene_dungeon.cpp`):

```cpp
struct RoomOutcome {
    enum Kind { ExitDungeon, Quit, Restart, GoToRoom } kind;
    int target_room = 0;
    int target_entrance = 0;
};
```

- [ ] **Step 2: Add the new includes** to `src/game/scene_dungeon.cpp` (the existing include block at `scene_dungeon.cpp:31-39` has neither):

```cpp
#include "logic/room_graph.h"   // find_entrance, room_door_at
#include "engine/save.h"        // write_world (persist latches)
```

(`bn_keypad.h` is already included at `scene_dungeon.cpp:8`, so `bn::keypad::up_pressed()` needs no new include.)

- [ ] **Step 3: Convert `play_room_single` → `play_room`** with the new signature and behavior:

```cpp
static RoomOutcome play_room(const logic::LevelData& level, int entrance_id,
                             logic::World& world, logic::PlayerState& ps);
```

Inside, make these specific edits to the extracted body:

1. **Spawn at the entrance** — replace the fixed `spawn_pos` derivation (currently `level.spawn_tx/ty`, `scene_dungeon.cpp:101`) with:

```cpp
logic::EntranceSpawn ent = logic::find_entrance(level, entrance_id);   // #include "logic/room_graph.h"
const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };
player.facing = ent.facing;   // face inward at the entrance
```

(Keep `spawn_pos` used by the death-respawn at `scene_dungeon.cpp:383` — death returns the player to the room's entrance, which is correct.)

2. **Apply latches on build** — where gates are instantiated (`scene_dungeon.cpp:160-172`), after computing `passable`, also open if latched:

```cpp
bool latched_open = (g.latch_id >= 0) && world.latched(g.latch_id);
if(passable || latched_open){ gi.open = true; }
else { fill_column(lvl.view, g.tx, level.h, info.bg_tile); }
```

For brazier-group triggers (`scene_dungeon.cpp:221-225` / the trigger-apply at `:371-375`), when a group has a latched id already set, pre-open its target column at build time and mark the trigger applied:

```cpp
// at trigger construction for Braziers:
bool pre = (bg.latch_id >= 0) && world.latched(bg.latch_id);
triggers.push_back(TriggerInst{ t, 0, 0, g, pre });
if(pre) open_column(lvl.view, bg.target_tx, level.h);
```

3. **Latch on trigger fire** — in the `Braziers` trigger case (`scene_dungeon.cpp:371-375`), when it first becomes active, set + persist the latch:

```cpp
case logic::TriggerKind::Braziers: {
    int n = 0; for(BrazierInst& bi : braziers) if(bi.group == ti.group && bi.lit) ++n;
    ti.trig.lit = n;
    if(!ti.applied && ti.trig.active()){
        ti.applied = true;
        open_column(lvl.view, ti.trig.target_tx, level.h);
        const logic::BrazierGroupSpawn& bgs = level.brazier_groups[ti.group];
        if(bgs.latch_id >= 0 && !world.latched(bgs.latch_id)){
            world.set_latch(bgs.latch_id);
            engine::write_world(world);   // #include "engine/save.h"
        }
    }
    break; }
```

(For a latched gate cleared by a spell/dash at `:257-268`, set+persist its `latch_id` the same way when `gi.open` flips true and `gi.spawn.latch_id >= 0`.)

4. **Room-door trigger** — in the main loop, after the SELECT/START checks (`scene_dungeon.cpp:238-239`), add overlap+UP detection. `logic::InputFrame` has **no** `up` field (it is only `{left, right, jump_pressed, fire_pressed, glide_held}`, see `player.h:5`), so read the keypad directly — exactly as the loop already reads SELECT/START, and matching the hub's door gesture `bn::keypad::up_pressed()` (`scene_hub.cpp:95`, edge-triggered so holding UP while walking through doesn't double-fire):

```cpp
if(bn::keypad::up_pressed()){   // matches hub door gesture; edge-triggered
    if(const logic::RoomDoorSpawn* d = logic::room_door_at(level, player.body)){
        return RoomOutcome{ RoomOutcome::GoToRoom, d->target_room, d->target_entrance };
    }
}
```

(Uses `logic::room_door_at` from the Step 2 include. No fade here — the outer loop fades on every room exit, see 5.3.)

5. **Map the old returns** — the existing `result = Quit/Restart/Cleared; break;` sites (`scene_dungeon.cpp:238,239,420`) become `return RoomOutcome{RoomOutcome::Quit}` / `{RoomOutcome::Restart}` / `{RoomOutcome::ExitDungeon}`. **Delete the trailing `engine::fade_out(16); return result;` block at `scene_dungeon.cpp:431-432`** — `play_room` no longer fades; the outer `run_dungeon` loop owns the single fade-out per room exit (5.3). The fade-IN at room start (`scene_dungeon.cpp:231-232`, `engine::set_fade(16)`) stays inside `play_room`.

- [ ] **Step 4: Build the ROM**

Run: `make`
Expected: compiles clean.

- [ ] **Step 5: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): play_room — entrance spawn, latch apply/persist, room-door trigger"
```

### Task 5.3: Outer `run_dungeon` room loop

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

- [ ] **Step 1: Replace the thin `run_dungeon` with the loop**

`play_room` no longer fades; this loop fades out exactly once per room exit, then dispatches. The next `play_room` fades back in (its `set_fade(16)` at start). `Restart` keeps `cur_room`/`cur_entrance` and re-enters the SAME room, refilling vitals (the anti-soft-lock reset that `src/main.cpp:43` did before — moved here since `Restart` is now consumed inside `run_dungeon` and never returned):

```cpp
DungeonResult run_dungeon(const logic::DungeonData& dungeon, logic::World& world, logic::PlayerState& ps){
    int cur_room = dungeon.start_room;
    int cur_entrance = 0;
    while(true){
        RoomOutcome out = play_room(*dungeon.rooms[cur_room], cur_entrance, world, ps);
        engine::fade_out(16);   // one fade-out per room exit; next play_room fades in
        switch(out.kind){
            case RoomOutcome::ExitDungeon: return DungeonResult::Cleared;
            case RoomOutcome::Quit:        return DungeonResult::Quit;
            case RoomOutcome::Restart:
                ps.health.cur = ps.health.max;   // anti-soft-lock: refill vitals, replay same room
                ps.magic.cur  = ps.magic.max;
                break;
            case RoomOutcome::GoToRoom:
                cur_room = out.target_room;
                cur_entrance = out.target_entrance;
                break;
        }
    }
}
```

And simplify `src/main.cpp` to call `run_dungeon` once (no `do/while` on Restart, since Restart no longer escapes):

```cpp
world.current_dungeon = n;
game::DungeonResult dr = game::run_dungeon(*lvl, world, ps);
world.current_dungeon = 0;
if(dr == game::DungeonResult::Cleared) engine::write_world(world);
```

> This is a **deviation** from the spec's silence on Restart scope — record it in the Execution Status "Deviations" subsection: Restart handling moved from `main.cpp` into `run_dungeon` so it re-enters the current room (not the dungeon start). Rationale: with multi-room dungeons, START should reset the room you're stuck in, not teleport you to room 0.

- [ ] **Step 2: Build the ROM**

Run: `make`
Expected: compiles clean.

- [ ] **Step 3: Commit**

```bash
git add src/game/scene_dungeon.cpp src/main.cpp
git commit -m "feat(game): run_dungeon room loop with hard-cut fades; Restart re-enters current room"
```

**BEFORE marking Phase 5 complete:**
1. Re-read the full `play_room` body and confirm NO behavior changed for a 1-room, 0-room-door, 0-entrance dungeon (the existing 5 dungeons): entrance id 0 falls back to `spawn_tx/ty`, no latches set, no room-doors → identical play.
2. Confirm no `bn::` type leaked into `include/logic/` (`python tools/check_logic_purity.py`).
3. `make` compiles clean. Host suite still green (`bash tools/host_test.sh`) — Phases 1–4 unaffected.

**After completing Phase 5:** Minimum 3 review rounds, focusing on the extraction (did any `break` that should be `return` get missed? does every outcome path fade exactly once?).

---

## Phase 6 — Vertical slice + manual QA

**Execution Status:** 🚧 IN PROGRESS — claimed 2026-06-11 (branch `feat/room-to-room-dungeons`). Build/author steps automatable; the 7-step interactive emulator QA (Step 6) is handed to the user (mGBA).

A real 2-room test dungeon proving the whole path end-to-end: walk to a door, press UP, hard-cut to room 2 at the linked entrance; solve a latching brazier puzzle, leave and re-enter, confirm the shortcut stays open after a save/reload.

**BEFORE starting this phase:** read `docs/pitfalls/implementation-pitfalls.md`.

### Task 6.1: Author a 2-room test dungeon

**Files:**
- Create: `tools/levels/d6_room0.txt` + `.json`, `tools/levels/d6_room1.txt` + `.json`
- Create: regenerated `include/game/levels/d6_room0.h`, `d6_room1.h`
- Modify: `include/game/levels/dungeons.h` (add a `DUNGEON6_DUNGEON` 2-room table), `src/main.cpp` (temporary test remap — see Step 4).

> **Reaching the test dungeon:** do NOT add a hub door 6 — that requires editing `tools/levels/hub.txt` (the hub only authors doors 1–5) and `scene_hub.cpp:26 door_enterable`, which is out of scope for a throwaway test. Instead, temporarily point the always-enterable **Dungeon 1 door** at `DUNGEON6_DUNGEON` for the manual playtest, then revert before the final commit (Step 7).

Entrance model for the slice: **entrances are populated only by `N` symbols** (in scan order, mapped to the JSON `entrances` array); the `@` spawn is separate and is reached as entrance id 0 via `find_entrance`'s fallback when no `N` has id 0. So:
- Room 0: `@` = dungeon start (entrance 0, fallback). One `N` = the **return** landing, id 1 (room 1's door targets it). A Fire shrine `F` grants Fire (needed to light room 1's brazier). A door `D` → room 1 entrance 0.
- Room 1: one `N` = landing from room 0, id 0. A door `D` → room 0 entrance 1.

- [ ] **Step 1: Author room 0** — `d6_room0.txt` (exactly one `@`, solid border):

```
################################
#@.....F......................#?
#.............................D#
#..............................#
#.............N................#
################################
################################
```

> Fix the row width: every row MUST be exactly 32 chars and the border MUST be solid (`build_level.py:59,160`). The stray `?` above is a width marker — replace row 1 with a clean 32-char row, e.g. `#@.....F.....................#` padded to 32 (count carefully; the compiler errors on non-rectangular or non-solid-border input). `F` (Fire shrine) sits at an open floor tile reachable from spawn; `D` at `(30,2)`; `N` at `(13,4)`.

`d6_room0.json`:

```json
{ "tileset":"tiles",
  "entrances":[{"id":1,"facing":-1}],
  "pickups":[{"ability":"fire"}],
  "room_doors":[{"target_room":1,"target_entrance":0}] }
```

(One `N` → one `entrances` entry with id 1, facing left — the player returns here facing back into room 0. One `F` → one `pickups` entry granting Fire. One `D` → room 1 entrance 0.)

> The compiler requires exactly one `@` and a solid border (`build_level.py:152,160`). `D` and `N` must sit on interior tiles, never on the border column/row.

- [ ] **Step 2: Author room 1** with: entrance `N` id 0 (where room 0's door lands the player), a latching brazier puzzle (`*` lit by Fire opening a shortcut column), a return room-door `D` back to room 0 entrance 1, and the dungeon exit `E` + cage `C` so it is completable. The spawn `@` is the dungeon-start fallback (unused here since room 0 is the start, but the compiler requires exactly one `@`).

**Functional requirements** (the executor finalizes exact tile positions in the emulator; the compiler enforces rectangular rows + solid border, and Step 6 manual QA validates playability):
- One `@` (dungeon-start fallback, unused as the entry since room 0 is start) and one `N` (id 0) where room 0's `D` lands the player.
- A brazier `*` (group 0) the player lights with **Fire** (granted by room 0's shrine).
- A **2-wide solid wall** (`#` columns, e.g. `target_tx` and `target_tx`+1, full interior height) separating the entrance side from the cage/exit side, so the wall must open to finish. The brazier group's `target` is that wall's left column; lighting it calls `open_column(target_tx)` clearing both columns.
- Cage `C` and exit `E` on the far side of the wall (a completable dungeon).
- A return door `D` → room 0 entrance 1.

Sketch (executor adjusts widths to exactly match the chosen `w`, keeps the wall columns aligned with the JSON `target`):

```
##############################
#@..N......*........##....E...#
#..................##.........#
#.........C........##........D#
##############################
```

`d6_room1.json` (the `target` MUST equal the authored wall's left column — keep them in sync if you move the wall):

```json
{ "tileset":"tiles",
  "entrances":[{"id":0,"facing":1}],
  "room_doors":[{"target_room":0,"target_entrance":1}],
  "braziers":[{"group":0}],
  "brazier_groups":[{"total":1,"target":[19,1],"latch_id":0}] }
```

> `open_column`/`fill_column` operate on rows `1 .. h-3` and columns `tx, tx+1` (`scene_dungeon.cpp:62-73`), so author the wall solid across exactly those rows at columns `target_tx`/`target_tx`+1; rows outside that range would leave a gap the 2-tile-tall player (half-extents 8×16 px, `scene_dungeon.cpp:104`) could slip through. `latch_id:0` is the global latch persisted to SRAM on first lighting.

- [ ] **Step 3: Compile the rooms**

Run:
```bash
python tools/build_level.py tools/levels/d6_room0.txt include/game/levels/d6_room0.h
python tools/build_level.py tools/levels/d6_room1.txt include/game/levels/d6_room1.h
```
Expected: prints `compiled ... (WxH, ...)` for each.

- [ ] **Step 4: Wire the dungeon** — add the 2-room table to `include/game/levels/dungeons.h`:

```cpp
#include "game/levels/d6_room0.h"
#include "game/levels/d6_room1.h"
inline constexpr const logic::LevelData* DUNGEON6_ROOMS[] = { &D6_ROOM0_DATA, &D6_ROOM1_DATA };
inline constexpr logic::DungeonData DUNGEON6_DUNGEON{ DUNGEON6_ROOMS, 2, 0 };
```

For the playtest, **temporarily** remap the always-enterable Dungeon 1 door to it in `src/main.cpp` (the `if(n == 1) lvl = &DUNGEON1_DUNGEON;` line):

```cpp
if(n == 1) lvl = &DUNGEON6_DUNGEON;   // TEMP: route door 1 to the 2-room test dungeon — REVERT in Step 7
```

> This avoids editing `hub.txt`/`door_enterable`. `world.current_dungeon` becomes 1, so room 1's cage frees spronk 1 — fine for a test. Revert this one line before the final commit (Step 7); ship `DUNGEON6_DUNGEON` defined-but-unreferenced (or delete it if the slice is throwaway — decide in the Phase 6 completion check).

- [ ] **Step 5: Build + host suite**

Run: `bash tools/host_test.sh` then `make`
Expected: host suite green (the new headers compile via the `_level` glob if you add a `test/test_d6_rooms_level.cpp`, optional); ROM compiles.

- [ ] **Step 6: Manual QA on emulator** (REQUIRED — this is the only real verification of Phase 5)

Use `/run` or load the built `.gba` in an emulator (mGBA). Verify, with screenshots/notes recorded in this task:
1. Enter dungeon 6 → spawn at room 0 entrance 0 (left), facing right.
2. Walk to the `D` door, press UP → hard-cut fade → room 1 at entrance 0.
3. Light the brazier group → shortcut column opens.
4. Take room 1's return `D` → back in room 0 at entrance 1 (facing left).
5. Re-enter room 1 → the shortcut is STILL open (RAM latch).
6. Clear the dungeon (or SELECT to hub), then re-enter dungeon 6 → shortcut STILL open (SRAM latch survived `write_world`). Power-cycle the emulator (reset, keep SRAM) → still open.
7. Press START mid-room → the CURRENT room resets, vitals refill, you are NOT teleported to room 0.

Record results inline here. If any step fails, file it under "Discoveries" and fix before closing the phase.

- [ ] **Step 7: Revert the temp remap, then commit**

First revert the Step 4 TEMP line in `src/main.cpp` back to `if(n == 1) lvl = &DUNGEON1_DUNGEON;` so door 1 ships pointing at the real Dungeon 1. Then:

```bash
git add tools/levels/d6_room0.* tools/levels/d6_room1.* include/game/levels/d6_room0.h include/game/levels/d6_room1.h include/game/levels/dungeons.h
git commit -m "test(content): 2-room dungeon 6 vertical slice — door transition + latched shortcut"
```

> `src/main.cpp` should have NO diff after the revert (the temp line is undone), so it is intentionally absent from `git add`. Confirm with `git diff src/main.cpp` showing nothing before committing.

**BEFORE marking Phase 6 complete:**
1. All 7 manual QA steps pass with evidence recorded in Task 6.1 Step 6.
2. `bash tools/host_test.sh` green; `make` clean.
3. Decide whether dungeon 6 ships as content or stays a throwaway test (note the decision in Deviations). If throwaway, leave hub door 6 disabled in the final commit.

**After completing Phase 6:** Minimum 3 review rounds.

---

## Final self-review (run before declaring the plan done)

- [ ] **Spec coverage:** every spec section maps to a task — data model (P1), latches+save (P1.2/P3), room loop/Approach 1 (P5), entrance markers (P1/P2/P4), backward compat (P1 defaults + P5.1 wrappers), testing (host tests P1–P4, manual P6). ✅
- [ ] **Three-layer rule:** P1–P4 stay pure/Python; P5 is the only `bn::` work; P5 explicitly forbids leaking `bn::` into logic to "test" the scene. ✅
- [ ] **No second save migration:** save bumped to v3 once (P3), before any content authoring. ✅
- [ ] **Type consistency:** `find_entrance`, `room_door_at`, `RoomOutcome{ExitDungeon,Quit,Restart,GoToRoom}`, `DungeonData{rooms,room_count,start_room}`, `set_latch`/`latched`, `latch_id` — names used identically across P2/P3/P5. ✅
- [ ] **Open question carried from spec:** which concrete events get `latch_id`s — resolved per-content in P6; framework is content-agnostic. ✅
```
