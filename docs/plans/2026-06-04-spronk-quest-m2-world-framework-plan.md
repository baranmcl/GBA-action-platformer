# Spronk Quest — Milestone 2 (World Framework) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the reusable world framework — a data-driven level pipeline (ASCII+JSON→compiler), a central-plaza hub with thematic ability-gates, plaza↔dungeon transitions, and an expanded save — then prove it by re-authoring Dungeon 1 as data and running Title→Hub→D1→clear→back-to-Hub-with-Featherleap→a double-jump gate opens revealing the (locked) D2 door→progress saved.

**Architecture:** Extends the shipped M1 three-layer codebase. New **pure logic** (host-tested): expanded `World`/`SaveData` v2 + v1→v2 migration, a `GateType` table + `can_pass`, and a `LevelData` view. New **Python**: `tools/build_level.py` compiles ASCII `.txt` + JSON sidecar → C++ headers (same shape the engine consumes). New **engine**: a level loader (LevelData→bg+collision+entities, reusing `level_view`) and a fade helper. New **game**: `scene_hub` (plaza) and a generalized `scene_dungeon` (M1's `scene_play` parameterized by `LevelData`). Reuses M1 `avatar`/`bolts`/`hud`/`input`/`save`/collision/player unchanged.

**Tech Stack:** devkitARM + Butano (C++17), Python 3 + Pillow, g++ host tests (`tools/host_test.sh`), ROM build (`tools/build_rom.sh`), mGBA.

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

**Overall:** ✅ **M2 feature-complete, mGBA-verified** (2026-06-04). All phases shipped on `m2-world-framework`; real-hardware flash pending. 76/76 host + 10 Python compiler tests.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 0 — Save v2 + gate logic (pure) | ✅ Shipped | Task 0.1, 0.2 | 60/60 host tests. |
| 1 — Level-data pipeline | ✅ Shipped | Task 1.1–1.3 | compiler + Dungeon 1 as data. 70/70 host. |
| 2 — Engine: loader + fade + scene_dungeon | ✅ Shipped | Phase 2 + polish | data-driven D1 == M1, mGBA-verified. Fixes: Fixed zero-init, live fade-in, camera/spawn, int32_t==long on ARM. |
| 3 — Hub scene + gate integration | ✅ Shipped | Task 3.1, 3.2 | plaza + door tiles + 2×4 door archways. Gate blocks/opens from save. |
| 4 — Integration loop (title→hub→dungeon→hub) | ✅ Shipped | main.cpp | full demo verified: enter D1 → clear → gate opens → D2 revealed → save. |
| 5 — Verification + docs | ✅ Shipped | `docs/acceptance-m2.md` | v1→v2 migration verified on real SRAM. Real-hardware flash pending. |

### Deviations
- _(none yet)_

### Discoveries
- **[Phase 2] `int32_t` is `long int` on arm-none-eabi** (both 32-bit). `logic::Fixed::to_int()` returns `int32_t`==`long`, so passing `to_int()` expressions directly to Butano APIs taking `bn::fixed` is **ambiguous** (host g++ where `int32_t`==`int` compiles fine, ARM does not). Assign to an `int` local first. Surfaced in `scene_dungeon` camera `set_position`.
- **[Phase 2] `logic::Fixed` had no default member initializer** → `Vec2`/`Body` started with indeterminate values; a default-constructed `Player` reused stale stack velocity, drifting on level restart. Fixed by `int32_t raw = 0;` in `fixed.h` (+ `fixed_default_is_zero` test). M1 only "worked" by luck of zeroed stack.
- **[Phase 2] Blocking fades freeze gameplay** → held input snaps in when control resumes. Dungeon uses a LIVE fade-in (ramp `engine::set_fade` inside the loop); static scenes (title) can keep the blocking `fade_in`.

---

## Standard Task Protocol (binds EVERY task)

Per `/writing-plans-enhanced` Step 3. Stated once; treat as pasted into each task.

**BEFORE starting any task:** (1) invoke `superpowers:test-driven-development`; (2) read `docs/pitfalls/testing-pitfalls.md` + `docs/pitfalls/implementation-pitfalls.md`; (3) TDD: failing test → run red → minimal impl → run green.

**BEFORE marking complete:** review tests vs `docs/pitfalls/testing-pitfalls.md`; run the test command green; commit with a message that states what happened to assertions (never an obscuring "fix").

**Logic-vs-glue convention (from M1):**
- `src/logic/**` + `include/logic/**` are **pure C++17, NO `bn::` types** — host-tested via **`bash tools/host_test.sh`** (NOT bare `make -C test`; see `CLAUDE.md`). The `check_logic_purity.py` guard fails the build on any `bn::` under logic — **including in comments** (M1 discovery: reword comments to avoid the literal `bn::` token; write "Butano SRAM API" not "bn::sram").
- `src/engine/**` + `src/game/**` touch Butano; build the ROM via **`bash tools/build_rom.sh`** and verify by **mGBA observation** against the explicit acceptance in the task. Extract any decidable logic into `src/logic/` and host-test it there.

**Assertion rigor:** if a test races/flakes, fix with deterministic logic, never by weakening the assertion; if you can't, STOP and escalate.

**After each Phase:** 3+ review rounds; if round 3 still finds issues, continue until clean.

---

## File Structure (decomposition locked here)

```
include/logic/                 (pure, host-tested — NO bn::)
  world_state.h    (MODIFY)   -- World bitmasks + Ability enum + SaveData v2 + v1->v2 migration
  gates.h          (CREATE)   -- GateType, GATE_TABLE, gate_info(), can_pass()
  level_data.h     (CREATE)   -- EntitySpawn/GateSpawn/DoorSpawn + LevelData view + invariants
include/engine/
  level_loader.h   (CREATE)   -- LevelData -> Tilemap + bg (reuses level_view) + entity placement helpers
  fade.h           (CREATE)   -- fade_out()/fade_in() transition
include/game/
  scene_hub.h      (CREATE)   -- plaza
  scene_dungeon.h  (CREATE)   -- generalized play scene (was scene_play)
  scene_title.h    (MODIFY)   -- returns to Hub
  levels/
    dungeon1.h     (GENERATED), hub.h (GENERATED) -- by tools/build_level.py
src/logic/         -- (header-only units above need no .cpp unless noted)
src/engine/        -- level_loader.cpp, fade.cpp
src/game/          -- scene_hub.cpp, scene_dungeon.cpp (from scene_play.cpp), scene_title.cpp (mod), main.cpp (mod)
tools/
  build_level.py   (CREATE)   -- ASCII+JSON -> C++ header
  test_build_level.py (CREATE)-- Python unittest for the compiler
  levels/
    dungeon1.txt, dungeon1.json, hub.txt, hub.json   (CREATE)
test/              -- test_world_state_v2.cpp, test_gates.cpp, test_level_data.cpp (host)
```

> **Retire `scene_play.{h,cpp}`:** Phase 2 renames/generalizes it to `scene_dungeon`. The current `src/game/scene_play.cpp` (hardcoded Dungeon 1) is replaced by `scene_dungeon.cpp` (data-driven). Keep its proven combat/loop logic.

---

## Phase 0 — Save schema v2 + gate logic (pure, host-tested)

**Execution Status:** ✅ SHIPPED on 2026-06-04 (branch `m2-world-framework`). Tasks 0.1 (World bitmasks + SaveData v2 + v1→v2 migration + dungeon1.h ripple) and 0.2 (gate-type table + can_pass) done. 60/60 host tests green. NOTE: ROM build is intentionally broken from here until Phase 2 retires the old-World references in `scene_play`/`scene_title`.

Pure foundations everything else builds on. Strict TDD via `bash tools/host_test.sh`.

### Task 0.1: Expand World + SaveData v2 + v1→v2 migration

**Files:** Modify `include/logic/world_state.h`; Create `test/test_world_state_v2.cpp`.

Context: M1 `World` is three bools; `SaveData` v1 is `{magic, version=1, flags(2), checksum(1), pad(3)}` (12 bytes), checksum = sum of first 8 bytes & 0xFF, v1 flags bit0=spronk1, bit1=featherleap, bit2=gate1. **Migration robustness:** `bn::sram::read` fills a 16-byte v2 struct, but an M1 cart only wrote 12 v1 bytes (bytes 12–15 are uninitialized SRAM). The v1 migration path reads ONLY bytes [0..8] (magic, version, flags, v1-checksum), so it is correct regardless of the trailing 4 bytes — verified by the `v1_save_migrates` test which zero-fills them. Existing M1 tests (`test_world_state.cpp`) reference the OLD API — **this task replaces `world_state.h` AND updates `test_world_state.cpp`** (the M1 file) to the new API, plus adds the v2/migration tests. Existing callers of `make_save`/`load_save` (`src/engine/save.cpp`) keep the same signatures.

- [ ] **Step 1: Write failing tests** `test/test_world_state_v2.cpp`:
```cpp
#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;
TEST(world_fresh_defaults){ World w; CHECK_EQ((int)w.spronks_freed,0); CHECK_EQ((int)w.abilities,0); CHECK(!w.has(Ability::Featherleap)); }
TEST(world_free_and_grant){ World w; w.free_spronk(1); w.grant(Ability::Featherleap);
  CHECK(w.spronk_freed(1)); CHECK(!w.spronk_freed(2)); CHECK(w.has(Ability::Featherleap)); CHECK(!w.has(Ability::Fire)); }
TEST(save_v2_roundtrip){ World w; w.free_spronk(1); w.grant(Ability::Featherleap); w.grant(Ability::Fire); w.current_dungeon=2;
  SaveData s = make_save(w); World w2; CHECK(load_save(s,w2)==true);
  CHECK(w2.spronk_freed(1)); CHECK(w2.has(Ability::Featherleap)); CHECK(w2.has(Ability::Fire)); CHECK_EQ((int)w2.current_dungeon,2); }
TEST(save_v2_is_version_2){ World w; SaveData s = make_save(w); CHECK_EQ((int)s.version, 2); }
TEST(empty_sram_rejected){ SaveData s{}; World w2; CHECK(load_save(s,w2)==false); }
TEST(bad_checksum_rejected){ World w; SaveData s=make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s,w2)==false); }
TEST(v1_save_migrates){ // build a v1-layout 16-byte buffer (v1 was 12 bytes; rest zero)
  uint8_t buf[sizeof(SaveData)]; std::memset(buf,0,sizeof(buf));
  uint32_t magic=0x5350524B; std::memcpy(buf,&magic,4);
  uint16_t ver=1; std::memcpy(buf+4,&ver,2);
  uint16_t flags=0x0007; std::memcpy(buf+6,&flags,2);   // v1: spronk1|featherleap|gate1
  uint32_t sum=0; for(int i=0;i<8;++i) sum+=buf[i]; buf[8]=(uint8_t)(sum&0xFF); // v1 checksum at offset 8
  SaveData s; std::memcpy(&s,buf,sizeof(SaveData));
  World w2; CHECK(load_save(s,w2)==true);
  CHECK(w2.spronk_freed(1)); CHECK(w2.has(Ability::Featherleap)); }
```
- [ ] **Step 2: Run red** — `bash tools/host_test.sh` (compile error / failures).
- [ ] **Step 3: Implement** `include/logic/world_state.h` (replace whole file):
```cpp
#pragma once
#include <cstdint>
#include <type_traits>
namespace logic {
enum class Ability : uint8_t { Featherleap=0, Fire, Ice, Glide, Dash, Grapple, Stone, Light };

struct World {
    uint16_t spronks_freed = 0;  // bit (d-1) = dungeon d's spronk (d in 1..8)
    uint16_t abilities = 0;      // bit (int)Ability
    uint8_t  current_dungeon = 0; // 0 = hub
    // PRECONDITION: d in 1..8 (bit d-1). Callers MUST pass a real dungeon number;
    // never call with current_dungeon==0 (the hub) — (1u << -1) is undefined behavior.
    bool spronk_freed(int d) const { return (spronks_freed >> (d-1)) & 1u; }
    void free_spronk(int d){ spronks_freed |= (uint16_t)(1u << (d-1)); }
    bool has(Ability a) const { return (abilities >> (int)a) & 1u; }
    void grant(Ability a){ abilities |= (uint16_t)(1u << (int)a); }
};

struct SaveData {
    uint32_t magic;          // 'SPRK'
    uint16_t version;        // 2
    uint16_t spronks;        // World.spronks_freed
    uint16_t abilities;      // World.abilities
    uint8_t  current_dungeon;
    uint8_t  checksum;       // sum of bytes [0..10] & 0xFF (magic..current_dungeon)
    uint8_t  _pad[4];
};
static_assert(std::is_trivially_copyable<SaveData>::value, "SaveData must be trivially copyable (SRAM)");
static_assert(sizeof(SaveData) == 16, "SaveData v2 layout changed — bump version + migrate");

static constexpr uint32_t SAVE_MAGIC = 0x5350524B; // "SPRK"
static constexpr uint16_t SAVE_VERSION = 2;

inline uint8_t checksum_bytes(const uint8_t* p, int n){ uint32_t s=0; for(int i=0;i<n;++i) s+=p[i]; return (uint8_t)(s&0xFF); }

inline SaveData make_save(const World& w){
    SaveData s{};
    s.magic=SAVE_MAGIC; s.version=SAVE_VERSION;
    s.spronks=w.spronks_freed; s.abilities=w.abilities; s.current_dungeon=w.current_dungeon;
    s.checksum = checksum_bytes(reinterpret_cast<const uint8_t*>(&s), 11); // magic(4)+ver(2)+spronks(2)+abil(2)+cur(1)
    return s;
}
inline bool load_save(const SaveData& s, World& out){
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&s);
    uint32_t magic; uint16_t version;
    __builtin_memcpy(&magic, b, 4); __builtin_memcpy(&version, b+4, 2);
    if(magic != SAVE_MAGIC) return false;
    if(version == 1){ // migrate: v1 layout flags@6(2), checksum@8(1), checksum=sum(b[0..7])
        if(checksum_bytes(b, 8) != b[8]) return false;
        uint16_t flags; __builtin_memcpy(&flags, b+6, 2);
        out = World{};
        if(flags & 1) out.free_spronk(1);
        if(flags & 2) out.grant(Ability::Featherleap);
        return true;
    }
    if(version == 2){
        if(checksum_bytes(b, 11) != s.checksum) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities; out.current_dungeon = s.current_dungeon;
        return true;
    }
    return false;
}
}
```
- [ ] **Step 4:** Delete `test/test_world_state.cpp` (its old-API tests no longer compile; `test_world_state_v2.cpp` supersedes it).
- [ ] **Step 4b: Fix the ripple — `dungeon1.h` rescue helper.** The old `include/logic/dungeon1.h` `try_free_spronk` references the now-deleted `w.spronk1_freed/featherleap/gate1_open` fields, so the World change **breaks the host build** until it's updated. Rewrite it to the new API, taking the dungeon number and setting only the spronk bit (the ability grant becomes `run_dungeon`'s job — Task 2.3):
```cpp
#pragma once
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/world_state.h" // World
namespace logic {
// Overlap the cage -> free dungeon d's spronk. Idempotent. Returns true if freed (now or before).
inline bool try_free_spronk(const Body& player, const Body& cage, World& w, int d){
    if(aabb_overlap(player, cage)){ w.free_spronk(d); return true; }
    return w.spronk_freed(d);
}
}
```
  Update `test/test_dungeon1.cpp` to the new signature: pass a dungeon number (e.g. `1`) and assert `w.spronk_freed(1)` instead of the old bools; drop the featherleap/gate assertions (no longer this helper's concern). Keep the overlap / no-overlap / idempotent cases.
- [ ] **Step 5: Run green** — `bash tools/host_test.sh`. All `world_*`/`save_*`/`v1_save_migrates` pass. (Pitfall TEST-4: corrupt/empty SRAM covered.)
- [ ] **Step 6:** `src/engine/save.cpp` uses unchanged `make_save`/`load_save` signatures — no change needed. **NOTE — expected broken ROM build window:** the World change breaks the Butano-side files that read the old bool fields (`scene_play.cpp`'s gate logic, `scene_title.cpp`'s `spronk1_freed` CONTINUE check), so **`bash tools/build_rom.sh` is EXPECTED to fail from this task until Task 2.3 / 4.1 retire those references.** Host tests (`bash tools/host_test.sh`) MUST stay green throughout (they don't compile the Butano scene files). Do NOT try to "fix" the ROM build during Phase 0/1 — it's intentionally mid-refactor.
- [ ] **Step 7: Commit** — `git add include/logic/world_state.h include/logic/dungeon1.h test/test_world_state_v2.cpp test/test_dungeon1.cpp && git rm test/test_world_state.cpp && git commit -m "feat(logic): expand World to bitmasks + SaveData v2 + v1->v2 migration (update dungeon1 rescue helper)"`

### Task 0.2: Gate-type table + can_pass

**Files:** Create `include/logic/gates.h`, `test/test_gates.cpp`.

- [ ] **Step 1: Write failing test** `test/test_gates.cpp`:
```cpp
#include "test_framework.h"
#include "logic/gates.h"
#include "logic/world_state.h"
using namespace logic;
static uint16_t bit(Ability a){ return (uint16_t)(1u << (int)a); }
TEST(gap_needs_featherleap){ CHECK(!can_pass(GateType::Gap, 0)); CHECK(can_pass(GateType::Gap, bit(Ability::Featherleap))); }
TEST(ice_needs_fire){ CHECK(!can_pass(GateType::Ice, 0)); CHECK(can_pass(GateType::Ice, bit(Ability::Fire))); CHECK(!can_pass(GateType::Ice, bit(Ability::Ice))); }
TEST(water_needs_ice){ CHECK(can_pass(GateType::Water, bit(Ability::Ice))); CHECK(!can_pass(GateType::Water, bit(Ability::Fire))); }
TEST(gate_geometry_flags){ CHECK(gate_info(GateType::Gap).is_geometry); CHECK(!gate_info(GateType::Ice).is_geometry); }
```
- [ ] **Step 2: Run red.**
- [ ] **Step 3: Implement** `include/logic/gates.h`:
```cpp
#pragma once
#include <cstdint>
#include "logic/world_state.h" // Ability
namespace logic {
enum class GateType : uint8_t {
    Gap=0,        // geometry: double-jump (Featherleap)
    GrapplePoint, // geometry: Grapple
    Vine,         // obstacle: Fire
    Ice,          // obstacle: Fire (melts)
    Water,        // obstacle: Ice (freezes to platform)
    CrackedWall,  // obstacle: Dash
    CrackedFloor, // obstacle: Stone (ground-pound)
    DarkVeil,     // obstacle: Light
    Count
};
struct GateInfo { Ability required; bool is_geometry; uint8_t bg_tile; }; // bg_tile = the CLOSED-state visual tile index (a wall the player sees while gated)
constexpr GateInfo GATE_TABLE[(int)GateType::Count] = {
    /*Gap*/          { Ability::Featherleap, true,  3 },  // closed = gate tile (3); only one instantiated in M2
    /*GrapplePoint*/ { Ability::Grapple,     true,  3 },
    /*Vine*/         { Ability::Fire,         false, 7 },  // tiles 7-12 are M3+ obstacle-gate art (not in tiles.bmp yet)
    /*Ice*/          { Ability::Fire,         false, 8 },
    /*Water*/        { Ability::Ice,          false, 9 },
    /*CrackedWall*/  { Ability::Dash,         false, 10 },
    /*CrackedFloor*/ { Ability::Stone,        false, 11 },
    /*DarkVeil*/     { Ability::Light,        false, 12 },
};
// Tile-index map (graphics/tiles.bmp): 0 blank, 1 ground, 2 one-way, 3 gate(closed wall),
// 4 cage, 5 door-open, 6 door-locked, 7-12 reserved for M3+ obstacle-gate art.
inline const GateInfo& gate_info(GateType t){ return GATE_TABLE[(int)t]; }
inline bool can_pass(GateType t, uint16_t abilities){ return (abilities >> (int)gate_info(t).required) & 1u; }
}
```
  > NOTE: M2 only instantiates `Gap`, whose closed visual is the existing gate tile (index 3, already in `graphics/tiles.bmp` from M1). `bg_tile` indices 5–10 are placeholders for future obstacle-gate art; when obstacle gates first appear (M3+), add those tiles to `graphics/tiles.bmp` and reconcile the indices.
- [ ] **Step 4: Run green** — `bash tools/host_test.sh`.
- [ ] **Step 5: Commit** — `git add include/logic/gates.h test/test_gates.cpp && git commit -m "feat(logic): add thematic gate-type table + can_pass"`

**After Phase 0:** 3-round review. Confirm host suite green and purity guard clean (watch for `bn::` in comments).

---

## Phase 1 — Level-data pipeline

**Execution Status:** ⬜ NOT STARTED

ASCII `.txt` + JSON sidecar → `tools/build_level.py` → C++ header consumed via a pure `LevelData`.

### Task 1.1: LevelData pure model

**Files:** Create `include/logic/level_data.h`, `test/test_level_data.cpp`.

Defines the in-memory shape the compiled headers populate and the engine consumes. Pure; tiles are `TileKind`-compatible `uint8_t` (0 empty,1 solid,2 one-way — same as M1).

- [ ] **Step 1: Write failing test** `test/test_level_data.cpp`:
```cpp
#include "test_framework.h"
#include "logic/level_data.h"
using namespace logic;
static const uint8_t TILES[] = { 1,1,1, 1,0,1, 1,1,1 };
static const EntitySpawn ENEMIES[] = { {1,1,0,0} };
static const GateSpawn GATES[] = { {2,2,GateType::Gap} };
static const DoorSpawn DOORS[] = { {1,1,2} };
static constexpr LevelData L = { TILES,3,3, 1,1, true,1,1, true,2,2, ENEMIES,1, GATES,1, DOORS,1 };
TEST(level_dims){ CHECK_EQ(L.w,3); CHECK_EQ(L.h,3); }
TEST(level_spawn){ CHECK_EQ(L.spawn_tx,1); CHECK_EQ(L.spawn_ty,1); }
TEST(level_counts){ CHECK_EQ(L.enemy_count,1); CHECK_EQ(L.gate_count,1); CHECK_EQ(L.door_count,1); }
TEST(level_tile_at){ CHECK_EQ((int)L.tiles[L.spawn_ty*L.w + L.spawn_tx], 0); } // spawn on empty
TEST(level_door_dungeon){ CHECK_EQ(L.doors[0].dungeon, 2); }
```
- [ ] **Step 2: Run red.**
- [ ] **Step 3: Implement** `include/logic/level_data.h`:
```cpp
#pragma once
#include <cstdint>
#include "logic/gates.h"
namespace logic {
struct EntitySpawn { int tx, ty, param0, param1; };   // enemy: param0/1 = patrol left/right tile
struct GateSpawn   { int tx, ty; GateType type; };
struct DoorSpawn   { int tx, ty, dungeon; };          // dungeon 1..8
struct LevelData {
    const uint8_t* tiles; int w, h;
    int spawn_tx, spawn_ty;
    bool has_cage; int cage_tx, cage_ty;
    bool has_exit; int exit_tx, exit_ty;
    const EntitySpawn* enemies; int enemy_count;
    const GateSpawn*   gates;   int gate_count;
    const DoorSpawn*   doors;   int door_count;
};
}
```
- [ ] **Step 4: Run green.**
- [ ] **Step 5: Commit** — `git add include/logic/level_data.h test/test_level_data.cpp && git commit -m "feat(logic): add LevelData view consumed by the level loader"`

### Task 1.2: `tools/build_level.py` compiler + Python tests

**Files:** Create `tools/build_level.py`, `tools/test_build_level.py`.

Compiles `<name>.txt` (grid) + `<name>.json` (metadata) → `include/game/levels/<name>.h` defining `constexpr` arrays + a `constexpr logic::LevelData <NAME>_DATA`.

- [ ] **Step 1: Write the compiler** `tools/build_level.py`. Behavior:
  - Read `<base>.txt` (lines = rows; all rows equal width else error) and `<base>.json`.
  - Tile symbols → tile ints: `#`→1, `.`→0, `^`→2. Content symbols (`@ C E o G 1..8`) occupy a tile that is **empty (0)** for collision (entities sit in open space) — EXCEPT they may be drawn over a floor; the symbol itself sets tile 0 unless the JSON says otherwise. (Keep simple: all non-`#^` symbols → tile 0.)
  - Collect by scanning the grid **row-major (top→bottom, left→right)**: exactly one `@` (spawn) — else error. Optional `C`(cage), `E`(exit). The Nth `o` in scan order maps to the Nth JSON `enemies[]` entry (patrol bounds; default ±2 tiles if absent); likewise the Nth `G` → the Nth JSON `gates[]` entry (its `type`). Each digit `1`-`8`→door(dungeon=digit). **The emitted `LevelData` aggregate initializer MUST list fields in the exact order they appear in the `logic::LevelData` struct (Task 1.1):** `tiles, w, h, spawn_tx, spawn_ty, has_cage, cage_tx, cage_ty, has_exit, exit_tx, exit_ty, enemies, enemy_count, gates, gate_count, doors, door_count`.
  - **Validate the solid-border invariant** (row 0, row H-1, col 0, col W-1 all `#`) — else error (mirrors M1 collision requirement).
  - Emit a header with include-guard + `#pragma once`, `#include "logic/level_data.h"`, then the arrays as **`inline constexpr`** (NOT `static constexpr`): `inline constexpr uint8_t <NAME>_TILES[] = {...};`, `inline constexpr logic::EntitySpawn <NAME>_ENEMIES[] = {...};`, etc., and `inline constexpr logic::LevelData <NAME>_DATA = { ... };`. **Use `inline` (external linkage, C++17) so the single `LevelData` points at a single deduplicated array across translation units** — `static constexpr` would give each TU its own internal-linkage copy, and an `inline` LevelData referencing an internal-linkage array is an ODR hazard. Empty entity lists: emit a 1-element dummy array (`{{0,0,0,0}}`) with `count = 0` to avoid non-standard zero-size arrays; the runtime keys off the count, never reading the dummy. **All generated symbols (`<NAME>_TILES`, `<NAME>_DATA`, …) live at GLOBAL namespace scope** (uppercase-prefixed names avoid collisions), so consumers reference `DUNGEON1_DATA` / `HUB_DATA` unqualified (the `logic::LevelData` *type* is namespaced, the *instances* are global).
  - CLI: `python tools/build_level.py <in.txt> <out.h>` (reads sibling `.json`).
- [ ] **Step 2: Write Python tests** `tools/test_build_level.py` (unittest): valid level compiles + emits expected symbols/counts; missing `@` errors; two `@` errors; non-rectangular errors; non-solid border errors; unknown symbol errors. Use temp files.
- [ ] **Step 3: Run** `cd tools && python -m unittest test_build_level.py` → all pass.
- [ ] **Step 4: Update `tools/host_test.sh`** to, BEFORE compiling the C++ host tests: (a) run the compiler's Python unittests (`python -m unittest` in `tools/`), failing the script on error; AND (b) **regenerate every level header** so `test_*_level.cpp` always compiles against fresh data, not a stale committed copy: `for f in tools/levels/*.txt; do python tools/build_level.py "$f" "include/game/levels/$(basename "${f%.txt}").h"; done`. Use the Windows-python path the script already configures. (At this task no `tools/levels/*.txt` exists yet — the loop is a no-op until Task 1.3; that's fine.)
- [ ] **Step 5: Commit** — `git add tools/build_level.py tools/test_build_level.py tools/host_test.sh && git commit -m "feat(tools): add ASCII+JSON level compiler with Python tests"`

### Task 1.3: Author Dungeon 1 as data + verify generated header

**Files:** Create `tools/levels/dungeon1.txt`, `tools/levels/dungeon1.json`; generated `include/game/levels/dungeon1.h`; `test/test_dungeon1_level.cpp`.

- [ ] **Step 1:** Author `tools/levels/dungeon1.txt` reproducing the M1 Dungeon-1 layout (48×20): solid border + 2-row floor; a cage platform with `C` (spronk) reachable by single jump; one enemy `o` patrolling the floor; a one-way `^` platform; a high exit platform with `E` reachable only after Featherleap (granted on freeing the spronk). Spawn `@` on the left. (No in-dungeon gate entity — Dungeon 1's exit is a *geometry* gate: the high platform. The Featherleap-gated plaza demo is the hub's job.)
- [ ] **Step 2:** `tools/levels/dungeon1.json`: `{ "tileset":"tiles", "enemies":[{"patrol":[14,22]}], "gates":[], "doors":{} }`.
- [ ] **Step 3:** Compile: `python tools/build_level.py tools/levels/dungeon1.txt include/game/levels/dungeon1.h`. **Wire codegen into `tools/build_rom.sh`** (NOT Butano's Makefile — that's devkitPro-owned and fragile to extend): before invoking `make`, add a loop that compiles every `tools/levels/*.txt` → `include/game/levels/*.h`, failing the script on any compiler error. This mirrors how the build already runs Python (placeholder art / Windows-python-on-PATH). Generated headers are committed (so a clean checkout builds without first running codegen), and regenerated on each build.
- [ ] **Step 4:** Host test `test/test_dungeon1_level.cpp`: `#include "game/levels/dungeon1.h"`; assert `DUNGEON1_DATA.w==48 && .h==20`, spawn in-bounds + on empty tile, `has_cage`, `has_exit`, `enemy_count==1`, border solid. `bash tools/host_test.sh` green.
- [ ] **Step 5: Commit** — `git add tools/levels/dungeon1.* include/game/levels/dungeon1.h Makefile test/test_dungeon1_level.cpp && git commit -m "feat(content): author Dungeon 1 as data + compile to header"`

**After Phase 1:** 3-round review. Confirm Python + host tests green; generated header compiles.

---

## Phase 2 — Engine: level loader + fade + scene_dungeon

**Execution Status:** ⬜ NOT STARTED

Bridge LevelData → live scene; generalize M1's play loop. Verified by mGBA (Dungeon 1 plays identically to M1).

### Task 2.1: Level loader

**Files:** Create `include/engine/level_loader.h`, `src/engine/level_loader.cpp`.

- [ ] **Step 1:** Implement `engine/level_loader` turning a `logic::LevelData` into runtime objects:
  - **Mutable tile buffer (CRITICAL — M1 lesson "const-vs-mutable tile data"):** the generated `LevelData.tiles` is `inline constexpr` (read-only `.rodata`). Gates open at runtime by flipping a collision tile, which **cannot write `.rodata`**. So `level_loader` MUST `memcpy` `level.tiles` into a scene-owned **mutable** `static uint8_t s_grid[64*32]` and build the `logic::Tilemap` over `s_grid` (NOT over `level.tiles`). This mirrors M1's `scene_play` mutable-grid copy. A single shared file-static buffer is fine (scenes are sequential).
  - Build the bg via `engine::build_level_view` over that Tilemap (reuse M1).
  - Provide pixel-coord accessors for spawn/cage/exit and per-entity (`enemy[i]`, `gate[i]`, `door[i]`) positions (tile→pixel = `×8`).
  - Interface (concrete): `struct LoadedLevel { logic::Tilemap map; engine::LevelView view; };` + `LoadedLevel load_level(const logic::LevelData&);` + a mutation helper `void set_collision_tile(int tx, int ty, uint8_t v);` (writes the shared mutable `s_grid` behind `LoadedLevel.map`). To open a gate: `engine::set_collision_tile(tx,ty,0)` (collision) AND `engine::set_level_tile(view, tx,ty, openTile)` (visual). `run_dungeon`/`run_hub` consume this — they do NOT each rebuild the tilemap/bg (single source).
  - **Size constraint:** `level_view`'s bg is 64×32 tiles (512×256px). M2 levels (dungeon1 48×20, hub) MUST fit within **64×32**. If the plaza needs more vertical room, either lay it out within 32 tiles tall or bump `level_view` COLS/ROWS to 64×64 (and `s_grid` to match) — note which you did.
  - Keep it thin — adapters, not logic. Host-test any nontrivial pure coord conversion in `logic::LevelData`.
- [ ] **Step 2:** mGBA verify deferred to Task 2.3. Confirm `bash tools/build_rom.sh` compiles the new files.
- [ ] **Step 3: Commit** — `git commit -am "feat(engine): add level loader (LevelData -> tilemap/bg/entities)"`

### Task 2.2: Fade transition helper

**Files:** Create `include/engine/fade.h`, `src/engine/fade.cpp`.

- [ ] **Step 1:** Implement `engine::fade_out(int frames)` / `fade_in(int frames)` using Butano's brightness/blend (`bn::bg_palettes::set_fade` + `bn::sprite_palettes::set_fade`, ramping intensity 0→1 for out, 1→0 for in; each runs its own `bn::core::update()` loop for `frames` frames). Reference `butano/examples/blending` for the fade API.
  > **Ownership convention (load-bearing — prevents fading an empty screen):** a scene MUST call `fade_in()` as the LAST thing in its setup (right after building its visuals, before the main loop) and `fade_out()` as the LAST thing before it returns (while its sprites/bg are STILL ALIVE). The caller (`main`) does NOT fade — if it did, the scene's objects would already be destroyed (RAII at function return) and the fade would animate a blank screen. So: scene builds → `fade_in` → loop → on exit `fade_out` → return (screen already black) → next scene builds black → `fade_in`.
- [ ] **Step 2:** mGBA verify in 2.3/Phase 4 (visible on scene switch). Confirm ROM builds.
- [ ] **Step 3: Commit** — `git commit -am "feat(engine): add screen fade transition helper"`

### Task 2.3: Generalize scene_play → scene_dungeon(LevelData)

**Files:** Create `include/game/scene_dungeon.h`, `src/game/scene_dungeon.cpp`; remove `src/game/scene_play.{h,cpp}`.

- [ ] **Step 1:** Port M1's `scene_play.cpp` loop into `game::run_dungeon(const logic::LevelData& level, logic::World& world)`. Replace the hardcoded `build_dungeon1`/constants with values **read from `level`** via `engine::load_level(level)` (Task 2.1 — gives the mutable Tilemap + bg): spawn the player at `level.spawn_*×8`; build cage/exit collision `Body`s from tile coords with fixed half-extents (cage `half_w=8, half_h=12`; exit `half_w=12, half_h=12`; both centered on the tile) when `has_cage`/`has_exit`; spawn an enemy per `level.enemies[i]` (loop `i` in `[0, level.enemy_count)`) into a **fixed-size pool** (e.g. `bn::vector<...>` / array of `max 8`, no heap) — each at `(tx×8,ty×8)` with `left_bound=param0×8`, `right_bound=param1×8` and the M1 enemy half-extents 8/8, each with its own sprite. Per frame, update + render every active enemy and run bolt-kill / contact-damage against EACH (M1 handled exactly one — generalize to the loop). Dungeon 1 has 1 enemy, but the loop MUST support `enemy_count` > 1. Keep the M1 combat / HUD / respawn / i-frame logic otherwise verbatim.
  - **DROP the M1 in-dungeon gate-pillar logic** (`GATE_X`, the `set_level_tile` gate-open on rescue). Dungeon 1's data has **no gate entity** — its only "gate" is the geometry high-exit platform, reachable once Featherleap is owned. (`level.gates` is empty for Dungeon 1; obstacle gates inside dungeons arrive in M3+.)
  - **Rescue timing (CRITICAL — matches M1):** when the player overlaps the cage, call `logic::try_free_spronk(player.body, cage, world, world.current_dungeon)` AND **immediately `world.grant(Ability::Featherleap)`** — the grant happens at *free-spronk* time, NOT at exit time, so Featherleap is usable for the double-jump up to the exit (otherwise the dungeon is uncompletable). Hide the spronk sprite on free.
    > M2 hardcodes the reward to Featherleap for Dungeon 1. Add a per-dungeon `reward` field to LevelData/JSON in a later milestone when D2+ bring new abilities — noted as a deliberate M2 scope limit.
  - **Sync ability:** each frame set `player.abilities.featherleap = world.has(Ability::Featherleap)` (so a continued game with Featherleap already owned starts with the double-jump; and freshly granting it mid-dungeon enables it immediately).
  - **Result:** `enum class DungeonResult { Cleared, Quit };` — return `Cleared` when the player reaches the exit zone *after* the spronk is freed; return `Quit` on SELECT. Signature: `DungeonResult run_dungeon(const logic::LevelData&, logic::World&);`
  - **Fades:** per the Task 2.2 ownership convention, call `engine::fade_in(16)` right after building the scene's visuals, and `engine::fade_out(16)` just before returning (Cleared or Quit), while sprites/bg are still alive.
  > Signature: `enum class DungeonResult { Cleared, Quit }; DungeonResult run_dungeon(const logic::LevelData&, logic::World&);`
  > The reward ability for M2 is hardcoded to Featherleap for Dungeon 1; add a `reward` field to LevelData/JSON in a later milestone when D2+ bring new abilities (note this as a deliberate M2 scope limit).
- [ ] **Step 2:** Update `main.cpp` temporarily to a testable harness: `World world; if(!read_world(world)) world=World{}; world.current_dungeon=1; run_title(world); run_dungeon(DUNGEON1_DATA, world);` — **`current_dungeon` MUST be set to 1 before `run_dungeon`** because the clear logic calls `world.free_spronk(world.current_dungeon)`; passing 0 (hub) would shift by −1 (UB). This temp main is replaced by the real scene loop in Task 4.1.
- [ ] **Step 3:** mGBA verify: **Dungeon 1 plays identically to M1** — run/jump, free spronk → double-jump → high exit → cleared. This is the pipeline correctness check (data-driven == hardcoded).
- [ ] **Step 4: Commit** — `git add -A && git commit -m "feat(game): generalize scene_play into data-driven scene_dungeon"`

**After Phase 2:** 3-round review + mGBA confirms data-driven Dungeon 1 matches M1.

---

## Phase 3 — Hub scene + gate integration

**Execution Status:** ⬜ NOT STARTED

### Task 3.1: Author the plaza level

**Files:** Create `tools/levels/hub.txt`, `tools/levels/hub.json`; generated `include/game/levels/hub.h`; `test/test_hub_level.cpp`.

- [ ] **Step 0: Extend the tileset with door tiles.** In `tools/make_placeholder_art.py` `gen_tiles()`, add **tile 5 = door-open** (an archway / open doorway) and **tile 6 = door-locked** (a barred/locked door), so the bg strip is ≥7 tiles (0–6). Regenerate `graphics/tiles.bmp` (`python tools/make_placeholder_art.py`) and rebuild so grit re-imports it. (Indices 7–12 are reserved for M3+ obstacle-gate art; not added now.)
- [ ] **Step 1:** Author `tools/levels/hub.txt` (central plaza, branching): spawn `@` in the center; door `1` reachable from the start; a **`G` gate** (type `gap`) placed so that beyond it sits door `2`; doors `3`-`8` placed on branches as locked markers. Solid border + floors per the platformer.
- [ ] **Step 2:** `tools/levels/hub.json`: `{ "tileset":"tiles", "gates":[{"type":"gap"}], "enemies":[], "doors":{} }`.
- [ ] **Step 3:** Compile `python tools/build_level.py tools/levels/hub.txt include/game/levels/hub.h` (the `build_rom.sh` codegen loop from Task 1.3 picks it up automatically — no new wiring). Host test `test/test_hub_level.cpp`: dims>0, border solid, `door_count>=2`, `gate_count==1`, the gap gate present (`HUB_DATA.gates[0].type == logic::GateType::Gap`). `bash tools/host_test.sh` green.
- [ ] **Step 4: Commit** — `git add tools/levels/hub.* include/game/levels/hub.h test/test_hub_level.cpp && git commit -m "feat(content): author plaza hub level with gap gate + dungeon doors"`

### Task 3.2: scene_hub (plaza)

**Files:** Create `include/game/scene_hub.h`, `src/game/scene_hub.cpp`.

- [ ] **Step 1:** Implement `game::HubResult run_hub(logic::World& world)` where `struct HubResult { int enter_dungeon; }; // 1..8 = enter that dungeon`. The hub loops internally until the player enters a door, then returns. Build the plaza from `HUB_DATA` via `engine::load_level(HUB_DATA)` — which gives the **mutable** Tilemap copy required to flip gate tiles open (Task 2.1). Laurel walks the plaza (reuse `avatar`+`input`+player physics).
  - **Gate handling (M2 model — IMPORTANT, removes the design's geometry-vs-terrain ambiguity):** every gate (including `gap`) is implemented as an **ability-keyed solid tile** — exactly M1's gate mechanic, now data-driven. For each `GateSpawn`: at scene start, if `logic::can_pass(gate.type, world.abilities)` is true, the gate tile is **removed** (collision tile→0 AND `engine::set_level_tile`→0, i.e. open); otherwise it stays **solid** (a wall blocking the path beyond). The `is_geometry` flag in `gates.h` is recorded for FUTURE art/animation (obstacle gates get a melt/burn anim; geometry gates just vanish) but **M2 does not branch on it** — all gates render as a removable solid tile via the existing `set_level_tile` path. So the `gap` gate behaves as "a wall that opens once you own Featherleap."
  - **Doors:** for each `DoorSpawn`, set its bg tile (via `engine::set_level_tile`) to **5 (door-open)** for door 1 (enterable) or **6 (door-locked)** for `dungeon>1`. The door tile's COLLISION stays empty (0) — the compiler already emitted door symbols as empty tiles — so Laurel can stand in front of a door to enter it.
  - **Door entry:** when Laurel's body overlaps a door tile and **D-pad Up** is pressed, set `result.enter_dungeon = door.dungeon`, `fade_out(16)`, and return — **only for door 1** in M2 (locked doors ignore the press, optionally flashing a "locked" cue).
  - **Fades:** call `engine::fade_in(16)` after building the plaza visuals; `fade_out(16)` on door-entry before returning (per the Task 2.2 convention).
  > For M2: only door 1 is enterable. The `gap` gate walls off the path to door 2's location; once Featherleap is owned (after clearing D1) the gate opens, revealing door 2 rendered "coming soon" (locked).
- [ ] **Step 2:** mGBA verify (Phase 4 wires title→hub; for now temp-call from main): plaza renders; door 1 visible; the gap gate is solid before Featherleap. 
- [ ] **Step 3: Commit** — `git commit -am "feat(game): add plaza hub scene with gate/door state from save"`

**After Phase 3:** 3-round review.

---

## Phase 4 — Integration loop (Title → Hub → Dungeon → Hub)

**Execution Status:** ⬜ NOT STARTED

### Task 4.1: Wire the scene loop

**Files:** Modify `src/game/scene_title.cpp` (CONTINUE condition + flows to Hub), `src/main.cpp`. (No `scene.h`/`SceneId` enum — the main loop calls `run_title`/`run_hub`/`run_dungeon` directly.)

- [ ] **Step 1:** Concrete `main.cpp`:
```cpp
int main(){
    bn::core::init();
    logic::World world;
    if(!engine::read_world(world)) world = logic::World{};
    game::run_title(world);                 // START -> enter the hub
    while(true){
        game::HubResult hr = game::run_hub(world);   // returns when a door is entered
        int n = hr.enter_dungeon;                     // 1..8; M2 only ever 1 (locked doors)
        if(n != 1) continue;                          // guard: only Dungeon 1 has data in M2
        world.current_dungeon = n;
        game::DungeonResult dr = game::run_dungeon(DUNGEON1_DATA, world); // scene owns fade in/out
        world.current_dungeon = 0;            // back in the hub BEFORE saving (so the save records "in hub")
        if(dr == game::DungeonResult::Cleared) engine::write_world(world); // persist earned spronk/ability + hub location
    }
}
```
  **`scene_title.cpp` change:** its CONTINUE check currently reads the deleted `world.spronk1_freed` field — update it to `world.spronks_freed != 0` (any spronk freed = a game in progress). Text is otherwise unchanged ("PRESS START" / "CONTINUE - PRESS START"); START now flows into the hub (not a dungeon). The hub re-renders gate/door state from `world` each time it's re-entered, so after clearing D1 the gap gate is open. (No `level_for(n)` helper needed in M2 — only `DUNGEON1_DATA` exists; the `n != 1` guard makes the locked-door invariant explicit.)
- [ ] **Step 2:** mGBA verify the **full demo**: Title → START → Hub → walk to door 1 → Up → fade → Dungeon 1 → clear it → fade → back in Hub → **now the gap gate is open** (Featherleap owned) → cross it → **door 2 visible (locked "coming soon")** → progress saved.
- [ ] **Step 3: Commit** — `git commit -am "feat(game): title->hub->dungeon->hub loop with fade + save"`

**After Phase 4:** 3-round review + full demo verified in mGBA.

---

## Phase 5 — Verification + docs

**Execution Status:** ⬜ NOT STARTED

### Task 5.1: Acceptance + v1→v2 save migration on hardware-accurate emulation

- [ ] **Step 1:** In mGBA: run the full demo; confirm the gap gate stays closed before clearing D1 and opens after; door 2 reveals locked. Power-cycle (Ctrl+R) → CONTINUE → Hub reflects saved state (Featherleap, gap open).
- [ ] **Step 2:** **v1→v2 migration on real save data:** take an actual M1 `.sav` (or the M1 ROM's save), load it with the M2 ROM, confirm it boots as a continued game (spronk1 + Featherleap) without corruption. Document in `docs/acceptance-m2.md`.
- [ ] **Step 3: Commit** — `git add docs/acceptance-m2.md && git commit -m "docs: M2 acceptance incl. v1->v2 save migration"`

### Task 5.2: Docs + milestone close

- [ ] **Step 1:** Update `README.md` (M2 status: hub + data-driven levels), and this plan's Execution Status to ✅. Note the deferred items (obstacle-gate rendering, per-dungeon reward field, real-hardware flash).
- [ ] **Step 2: Commit** — `git commit -am "docs: M2 README + plan close-out"`

**After Phase 5:** Final 3-round review. M2 done when the full demo + save migration verify in mGBA and host tests are green.

---

## Self-Review (against M2 design)

- **Level-data pipeline (§2)** → Tasks 1.1–1.3. ✅
- **Hub plaza + branches (§3)** → Tasks 3.1–3.2. ✅
- **Thematic typed gates (§3)** → Task 0.2 (table+can_pass) + 3.2 (instantiated gap gate). ✅ (obstacle-gate rendering deferred — no instances in M2, noted.)
- **Featherleap live demo (§3)** → Tasks 3.2 + 4.1. ✅
- **Scene/transition framework (§4)** → Tasks 2.2, 2.3, 3.2, 4.1. ✅
- **Expanded save v2 + migration (§5)** → Task 0.1 + 5.1. ✅
- **scene_dungeon generalization (§4/§6)** → Task 2.3. ✅
- **Reuse M1 engine (§6)** → Tasks 2.1/2.3 reuse level_view/avatar/bolts/hud/input/save. ✅
- **Three-layer purity (§6)** → all logic tasks host-tested; guard applies. ✅

Type names consistent across tasks: `World`/`Ability`/`SaveData`/`make_save`/`load_save`, `GateType`/`gate_info`/`can_pass`, `LevelData`/`EntitySpawn`/`GateSpawn`/`DoorSpawn`, `run_dungeon`/`DungeonResult`, `run_hub`, `level_loader`, `fade_out`/`fade_in`. No spec requirement left without a task (obstacle-gate rendering explicitly deferred with rationale).
