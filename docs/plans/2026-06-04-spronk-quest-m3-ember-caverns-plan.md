# Spronk Quest — Milestone 3 (Ember Caverns + Fire Spell) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Dungeon 2 (Ember Caverns) and the Fire spell — a puzzle-rich content dungeon that introduces a spell system, a mid-dungeon ability pickup, and a generic trigger→target puzzle framework, proving the content template for D3–D8.

**Architecture:** Extends the shipped M2 codebase. New **pure logic** (host-tested): a spell/`FireCast` system + magic economy, pushable-block physics, a `Trigger`→`Target` puzzle resolver, an ability-pickup grant rule, a lava-damage rule, a fire-immune enemy flag, a fire-clears-gate rule, and `TileKind::Lava`. New **content**: `build_level.py` gains symbols (`F`,`B`,`=`,`?`,`*`,`~`,`V`,`I`) + the `dungeon2` level. New **engine**: spell input (R/L), a fire-projectile pool, and rendering for the new tiles/entities. `scene_dungeon` is extended **additively** (it is already `LevelData`-parameterized). Reuses all M1/M2 engine.

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

**Overall:** Not started.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 0 — Pure logic systems | ⬜ Not started | — | — |
| 1 — Level compiler symbols + art | ⬜ Not started | — | — |
| 2 — Engine: spell input + fire pool + rendering | ⬜ Not started | — | — |
| 3 — scene_dungeon integration (additive) | ⬜ Not started | — | — |
| 4 — Dungeon 2 content + hub unlock | ⬜ Not started | — | — |
| 5 — Verification + docs | ⬜ Not started | — | — |

### Deviations
- _(none yet)_

### Discoveries
- _(none yet)_

---

## Standard Task Protocol (binds EVERY task)

Per `/writing-plans-enhanced` Step 3. Stated once; treat as pasted into each task.

**BEFORE starting any task:** (1) invoke `superpowers:test-driven-development`; (2) read `docs/pitfalls/testing-pitfalls.md` + `docs/pitfalls/implementation-pitfalls.md`; (3) TDD: failing test → run red → minimal impl → run green.

**BEFORE marking complete:** review tests vs `docs/pitfalls/testing-pitfalls.md`; run the test command green; commit with a message stating what happened to assertions.

**Project conventions (from M1/M2 — do NOT relearn the hard way):**
- `src/logic/**` + `include/logic/**`: **pure C++17, NO `bn::` tokens (even in comments)** — host-tested via **`bash tools/host_test.sh`**. `check_logic_purity.py` fails the build on `bn::`.
- `int32_t` is **`long` on arm-none-eabi**: never pass a `logic::Fixed::to_int()` expression straight to a Butano API taking `bn::fixed` — assign to an `int` local first (M2 discovery).
- `logic::Fixed` defaults to 0 (M2 fix) — new logic structs with `Fixed`/`Vec2`/`Body` members start zeroed; rely on it, don't re-init redundantly, but DO explicitly set meaningful fields.
- ROM build: **`bash tools/build_rom.sh`** (regenerates level headers + art via Windows Python, builds via devkitPro MSYS2). Butano forbids heap — fixed pools only.
- Scene transitions: scenes own their fades (`engine::fade_in/out`); dungeons use the **live fade-in** (ramp `engine::set_fade` in the loop) so gameplay isn't frozen.

**Assertion rigor:** if a test races/flakes, fix with deterministic logic, never by weakening assertions; if you can't, STOP and escalate.

**After each Phase:** 3+ review rounds; if round 3 still finds issues, continue until clean.

---

## File Structure (decomposition locked here)

```
include/logic/   (pure, host-tested — NO bn::)
  player.h        (MODIFY)  -- Abilities gains nothing new; abilities live in world (see SpellState)
  spell.h         (CREATE)  -- SpellId, SpellState (selected + available-from-world), FireCast
  pushable_block.h(CREATE)  -- PushableBlock + push/slide/plate/gap physics
  puzzle.h        (CREATE)  -- Trigger (brazier-group/plate/button) -> Target resolution
  ability_pickup.h(CREATE)  -- AbilityPickup + try_take_pickup
  hazard.h        (CREATE)  -- lava_overlap(body, tilemap)
  tilemap.h       (MODIFY)  -- TileKind::Lava + is_lava()
  enemy.h         (MODIFY)  -- bool fire_immune
  gates.h         (no change -- table already maps Vine/Ice -> Fire)
include/engine/
  spell_input.h   (CREATE)  -- bn::keypad R/L -> cast/cycle intents
  fire_pool.h     (CREATE)  -- fire-projectile pool (BoltPool sibling) + level interaction hooks
  level_view.h    (MODIFY)  -- collision-TileKind -> bg-tile-index map (incl. Lava)
include/game/
  scene_dungeon.h (MODIFY)  -- additive: new entities driven from LevelData
  levels/dungeon2.h(GENERATED)
src/logic/, src/engine/, src/game/ -- impls
tools/
  build_level.py  (MODIFY)  -- new symbols F B = ? * ~ V I + JSON params
  levels/dungeon2.{txt,json} (CREATE)
  make_placeholder_art.py (MODIFY) -- ember tiles, lava, brazier, block, plate, button, shrine, fire proj, fire enemy, vine/ice gate tiles
test/ -- test_spell.cpp, test_pushable_block.cpp, test_puzzle.cpp, test_ability_pickup.cpp,
         test_hazard.cpp, test_tilemap (lava), test_enemy (fire_immune), test_dungeon2_level.cpp
```

> **Tile-value vs bg-index (READ THIS — M2 left it implicit):** the level's **collision tile array** stores `logic::TileKind` values (`0` empty, `1` solid, `2` one-way, **`3` lava** new). The **bg visual** uses tileset indices: M2 `0` blank,`1` ground,`2` one-way,`3` gap-gate,`4` cage,`5` door-open,`6` door-locked; **vine/ice reuse `gates.h`'s reserved `7`=vine, `8`=ice** (gates render via `gate_info(type).bg_tile`, which M2 already set to 7/8 — do NOT pick new indices for them); new M3 **bg tiles**: `13` lava, `14` brazier-unlit, `15` brazier-lit, `17` plate, `18` button. The **pushable block and the ability shrine are SPRITES** (they move / are pickups), not bg tiles — their collision is handled separately (block = a Solid grid cell with a blank bg under its sprite; shrine = non-solid). Indices `16`/`19` are left free. `engine::level_view` MUST map collision TileKind → bg index via a small table (M2 used identity for 0/1/2; Lava breaks identity → add the table). Gates/doors/entities continue to be overlaid via `set_level_tile`. Keep `graphics/tiles.bmp` indices in sync with `gates.h` comments + this list.

---

## Phase 0 — Pure logic systems (host-tested)

**Execution Status:** ⬜ NOT STARTED

The testable heart. Every task strict TDD via `bash tools/host_test.sh`. No `bn::`.

### Task 0.1: `TileKind::Lava` + `is_lava`

**Files:** Modify `include/logic/tilemap.h`; Test: `test/test_tilemap.cpp` (append).

- [ ] **Step 1: Write failing test** (append to `test_tilemap.cpp`):
```cpp
TEST(tile_lava_is_not_solid_but_is_lava){
  uint8_t cells[] = {0,3,1};  // 3 = lava
  Tilemap m{3,1,cells};
  CHECK(m.is_lava(1,0)); CHECK(!m.is_lava(0,0));
  CHECK(!m.is_solid(1,0));      // lava is NOT solid (you fall in)
  CHECK(!m.is_oneway(1,0)); }
```
- [ ] **Step 2: Run red** — `bash tools/host_test.sh`.
- [ ] **Step 3: Implement** — in `tilemap.h`, add `Lava=3` to `enum class TileKind` and a query:
```cpp
bool is_lava(int tx,int ty) const { return at(tx,ty)==TileKind::Lava; }
```
  (Leave `is_solid`/`is_oneway` unchanged — lava is neither, so collision treats it as empty.)
- [ ] **Step 4: Run green.** **Step 5: Commit** — `git commit -am "feat(logic): add TileKind::Lava + is_lava"`

### Task 0.2: Lava-damage rule

**Files:** Create `include/logic/hazard.h`, `test/test_hazard.cpp`.

- [ ] **Step 1: Write failing test** `test/test_hazard.cpp`:
```cpp
#include "test_framework.h"
#include "logic/hazard.h"
using namespace logic;
TEST(body_over_lava_detected){
  uint8_t cells[] = {0,0, 3,3};      // bottom row lava (row1)
  Tilemap m{2,2,cells};
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(2), Fixed::from_int(8)};   // overlaps row1 (y8..)
  CHECK(lava_overlap(b,m)==true);
  b.pos={Fixed::from_int(2), Fixed::from_int(0)};   // row0 only
  CHECK(lava_overlap(b,m)==false); }
```
- [ ] **Step 2: Run red. Step 3: Implement** `include/logic/hazard.h`:
```cpp
#pragma once
#include "logic/physics.h"
#include "logic/tilemap.h"
namespace logic {
// True if any tile the body's AABB overlaps is lava. The scene applies health damage.
inline bool lava_overlap(const Body& b, const Tilemap& map){
    int l = Tilemap::px_to_tile(b.pos.x);
    int r = Tilemap::px_to_tile(b.pos.x + b.half_w + b.half_w - Fixed::from_int(1));
    int t = Tilemap::px_to_tile(b.pos.y);
    int bm= Tilemap::px_to_tile(b.pos.y + b.half_h + b.half_h - Fixed::from_int(1));
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx) if(map.is_lava(tx,ty)) return true;
    return false;
}
}
```
- [ ] **Step 4: Run green. Step 5: Commit** — `git commit -am "feat(logic): add lava_overlap hazard rule"`

### Task 0.3: Spell state + FireCast (magic economy)

**Files:** Create `include/logic/spell.h`, `test/test_spell.cpp`.

Reuses `logic::BoltSpawn` (bolt.h) and `logic::Meter` (meters.h).

- [ ] **Step 1: Write failing test** `test/test_spell.cpp`:
```cpp
#include "test_framework.h"
#include "logic/spell.h"
#include "logic/meters.h"
#include "logic/world_state.h"
using namespace logic;
TEST(fire_needs_magic){ FireCast fc; Meter magic{10,100}; BoltSpawn out;
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==false); CHECK_EQ(magic.cur,10); }   // insufficient
TEST(fire_spends_and_spawns){ FireCast fc; Meter magic{50,100}; BoltSpawn out;
  CHECK(fc.try_cast(true, magic, Vec2{Fixed::from_int(5),Fixed::from_int(5)}, 1, out)==true);
  CHECK_EQ(magic.cur,25); CHECK(out.vel.x.raw>0); }                                    // spent 25, facing +
TEST(fire_cooldown){ FireCast fc; Meter magic{100,100}; BoltSpawn out;
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==true);
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==false);   // cooling
  for(int i=0;i<fc.cooldown_ticks;++i) fc.tick();
  CHECK(fc.try_cast(true, magic, Vec2{}, 1, out)==true); }
TEST(spellstate_availability){ SpellState s; World w;
  CHECK(!s.has_any(w));                       // no spells before any ability
  w.grant(Ability::Fire); s.refresh(w);
  CHECK(s.has_any(w)); CHECK(s.selected==SpellId::Fire); }
```
- [ ] **Step 2: Run red. Step 3: Implement** `include/logic/spell.h`:
```cpp
#pragma once
#include "logic/bolt.h"        // BoltSpawn
#include "logic/meters.h"      // Meter
#include "logic/world_state.h" // World, Ability
namespace logic {
enum class SpellId : uint8_t { None=0, Fire };   // D3+: Ice, Light, Stone...

struct FireCast {
    int cost = 25;
    int cooldown_ticks = 20;
    int cd = 0;
    void tick(){ if(cd>0) --cd; }
    // Spends magic + yields a fire projectile (slower than the free bolt's 3 px/tick).
    bool try_cast(bool pressed, Meter& magic, Vec2 origin, int facing, BoltSpawn& out){
        if(!pressed || cd>0) return false;
        if(!magic.spend(cost)) return false;
        out.pos = origin;
        out.vel = Vec2{ Fixed::from_int(2*facing), Fixed::from_int(0) };
        cd = cooldown_ticks;
        return true;
    }
};

// Selected-spell + availability (M3: only Fire). refresh() picks the first owned spell.
struct SpellState {
    SpellId selected = SpellId::None;
    bool has_any(const World& w) const { return w.has(Ability::Fire); }
    void refresh(const World& w){ selected = w.has(Ability::Fire) ? SpellId::Fire : SpellId::None; }
    void cycle(const World& w){ refresh(w); } // M3: one spell, cycle is a no-op (wiring for D3+)
};
}
```
- [ ] **Step 4: Run green. Step 5: Commit** — `git commit -am "feat(logic): add spell state + FireCast (magic-cost economy)"`

### Task 0.4: Ability pickup grant

**Files:** Create `include/logic/ability_pickup.h`, `test/test_ability_pickup.cpp`.

- [ ] **Step 1: Write failing test**:
```cpp
#include "test_framework.h"
#include "logic/ability_pickup.h"
using namespace logic;
static Body mk(int x,int y){ Body b{}; b.half_w=Fixed::from_int(6); b.half_h=Fixed::from_int(6);
  b.pos={Fixed::from_int(x),Fixed::from_int(y)}; return b; }
TEST(pickup_grants_on_overlap){ World w; Body p=mk(8,8), shrine=mk(10,10);
  CHECK(try_take_pickup(p, shrine, w, Ability::Fire)==true); CHECK(w.has(Ability::Fire)); }
TEST(pickup_idempotent){ World w; w.grant(Ability::Fire); Body p=mk(8,8), shrine=mk(10,10);
  CHECK(try_take_pickup(p, shrine, w, Ability::Fire)==false); CHECK(w.has(Ability::Fire)); } // already had it -> not "newly taken"
TEST(pickup_no_overlap){ World w; Body p=mk(0,0), shrine=mk(99,0);
  CHECK(try_take_pickup(p, shrine, w, Ability::Fire)==false); CHECK(!w.has(Ability::Fire)); }
```
- [ ] **Step 2: Run red. Step 3: Implement** `include/logic/ability_pickup.h`:
```cpp
#pragma once
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/world_state.h"
namespace logic {
struct AbilityPickup { int tx, ty; Ability ability; };
// Returns true ONLY on the frame the ability is newly taken (overlap + not already owned).
inline bool try_take_pickup(const Body& player, const Body& shrine, World& w, Ability a){
    if(aabb_overlap(player, shrine) && !w.has(a)){ w.grant(a); return true; }
    return false;
}
}
```
- [ ] **Step 4: Run green. Step 5: Commit** — `git commit -am "feat(logic): add ability-pickup grant rule"`

### Task 0.5: Fire-immune enemy + fire-clears-gate

**Files:** Modify `include/logic/enemy.h`; Create `include/logic/fire_effect.h`; Tests: `test/test_enemy.cpp` (append), `test/test_fire_effect.cpp`.

- [ ] **Step 1: Write failing tests.** Append to `test_enemy.cpp`:
```cpp
TEST(enemy_fire_immune_flag){ Enemy e; e.fire_immune=true; CHECK(e.fire_immune); Enemy d; CHECK(!d.fire_immune); }
```
New `test/test_fire_effect.cpp`:
```cpp
#include "test_framework.h"
#include "logic/fire_effect.h"
#include "logic/gates.h"
using namespace logic;
TEST(fire_clears_vine_and_ice){ CHECK(fire_clears_gate(GateType::Vine)); CHECK(fire_clears_gate(GateType::Ice)); }
TEST(fire_does_not_clear_gap){ CHECK(!fire_clears_gate(GateType::Gap)); } // gap needs Featherleap, not Fire
```
- [ ] **Step 2: Run red. Step 3: Implement.** In `enemy.h` add `bool fire_immune = false;` to `struct Enemy`. Create `include/logic/fire_effect.h`:
```cpp
#pragma once
#include "logic/gates.h"
namespace logic {
// A fire projectile clears a gate iff the gate's required ability is Fire (vine, ice).
inline bool fire_clears_gate(GateType t){ return gate_info(t).required == Ability::Fire; }
}
```
- [ ] **Step 4: Run green. Step 5: Commit** — `git commit -am "feat(logic): fire-immune enemy flag + fire-clears-gate rule"`

### Task 0.6: Pushable block physics

**Files:** Create `include/logic/pushable_block.h`, `src/logic/pushable_block.cpp`, `test/test_pushable_block.cpp`.

Grid-aligned block (one tile). `push(dir, map)` slides it one tile if the target tile is empty; blocked by solids/edges. `apply_gravity_step(map)` drops it one tile if the tile below is empty (so it falls into gaps / settles on ground).

- [ ] **Step 1: Write failing test**:
```cpp
#include "test_framework.h"
#include "logic/pushable_block.h"
using namespace logic;
// 5x3 map: floor row 2 solid, a wall at (3,1)
static uint8_t cells[] = {0,0,0,0,0, 0,0,0,1,0, 1,1,1,1,1};
static Tilemap mk(){ return Tilemap{5,3,cells}; }
TEST(block_pushes_into_empty){ PushableBlock b{1,1}; CHECK(b.push(+1, mk())==true); CHECK_EQ(b.tx,2); }
TEST(block_blocked_by_wall){ PushableBlock b{2,1}; CHECK(b.push(+1, mk())==false); CHECK_EQ(b.tx,2); } // (3,1) is solid
TEST(block_blocked_by_edge){ PushableBlock b{0,1}; CHECK(b.push(-1, mk())==false); CHECK_EQ(b.tx,0); }
TEST(block_falls_into_gap){ PushableBlock b{1,0}; b.apply_gravity_step(mk()); CHECK_EQ(b.ty,1); // falls to row1 (row2 solid stops it)
  b.apply_gravity_step(mk()); CHECK_EQ(b.ty,1); }
```
- [ ] **Step 2: Run red. Step 3: Implement** `include/logic/pushable_block.h`:
```cpp
#pragma once
#include "logic/tilemap.h"
namespace logic {
struct PushableBlock {
    int tx, ty;
    bool active = true;   // false once it has filled a gap / been consumed (future)
    // Slide one tile in dir (+1/-1) if the destination tile is non-solid; returns moved.
    bool push(int dir, const Tilemap& map){
        int nx = tx + dir;
        if(map.is_solid(nx, ty)) return false;       // wall/edge (OOB is solid)
        tx = nx; return true;
    }
    // Fall one tile if the tile below is non-solid; returns moved.
    bool apply_gravity_step(const Tilemap& map){
        if(map.is_solid(tx, ty+1)) return false;
        ++ty; return true;
    }
};
}
```
  (Header-only is fine; no `.cpp` needed — remove the `src/logic/pushable_block.cpp` from the file map.)
  > **CONTRACT (how the scene uses this — see Task 3.4):** `PushableBlock` only updates its own `tx/ty`; it does NOT touch the tilemap. The SCENE keeps the block's tile marked **Solid** in the mutable collision grid (so the player collides with it via the existing `move_and_collide` — a block is a solid the player can't pass), and on a successful `push`/`apply_gravity_step` the scene clears the old tile (→Empty) and sets the new tile (→Solid) + moves the sprite. `push()`'s `is_solid(nx,ty)` checks the tile IN FRONT (not the block's own), so a clear destination returns true. This is why the block must be a real collision tile, not a free-floating entity.
- [ ] **Step 4: Run green. Step 5: Commit** — `git commit -am "feat(logic): pushable block grid physics"`

### Task 0.7: Trigger → target puzzle resolver

**Files:** Create `include/logic/puzzle.h`, `test/test_puzzle.cpp`.

A `Trigger` is active when its condition is met. Conditions (M3): a count `lit/total` (braziers) reaching total; a `pressed` bool (plate or button). Pure resolution; the scene maps "active" → open a target tile.

- [ ] **Step 1: Write failing test**:
```cpp
#include "test_framework.h"
#include "logic/puzzle.h"
using namespace logic;
TEST(brazier_group_active_when_all_lit){ Trigger t = Trigger::braziers(3);
  t.lit=2; CHECK(!t.active()); t.lit=3; CHECK(t.active()); t.lit=4; CHECK(t.active()); }
TEST(plate_active_when_pressed){ Trigger t = Trigger::plate(); CHECK(!t.active()); t.pressed=true; CHECK(t.active()); }
TEST(button_active_when_pressed){ Trigger t = Trigger::button(); CHECK(!t.active()); t.pressed=true; CHECK(t.active()); }
```
- [ ] **Step 2: Run red. Step 3: Implement** `include/logic/puzzle.h`:
```cpp
#pragma once
#include <cstdint>
namespace logic {
enum class TriggerKind : uint8_t { Braziers, Plate, Button };
struct Trigger {
    TriggerKind kind;
    int total = 0;     // braziers: how many to light
    int lit = 0;       // braziers: how many lit
    bool pressed = false; // plate/button
    int target_tx = 0, target_ty = 0; // tile the scene opens when active()
    static Trigger braziers(int total){ Trigger t; t.kind=TriggerKind::Braziers; t.total=total; return t; }
    static Trigger plate(){ Trigger t; t.kind=TriggerKind::Plate; return t; }
    static Trigger button(){ Trigger t; t.kind=TriggerKind::Button; return t; }
    bool active() const {
        switch(kind){
            case TriggerKind::Braziers: return total>0 && lit>=total;
            case TriggerKind::Plate:    return pressed;
            case TriggerKind::Button:   return pressed;
        }
        return false;
    }
};
}
```
- [ ] **Step 4: Run green. Step 5: Commit** — `git commit -am "feat(logic): trigger->target puzzle resolver"`

**After Phase 0:** 3-round review. Confirm `bash tools/host_test.sh` fully green + purity clean (watch `bn::` in comments). This phase is independent of the ROM build (Butano-free) — ROM build not expected to change yet.

---

## Phase 1 — Level compiler symbols + placeholder art

**Execution Status:** ⬜ NOT STARTED

### Task 1.1: Extend `LevelData` + `build_level.py` with new symbols

**Files:** Modify `include/logic/level_data.h`, `tools/build_level.py`, `tools/test_build_level.py`; Test: `test/test_level_data.cpp` (append).

- [ ] **Step 1:** Extend `logic::LevelData` (host test first — append a `test_level_data.cpp` case constructing a level with the new arrays). Add arrays + counts for the new entity kinds, mirroring the existing pattern (pointer + count, 1-element dummy when empty):
  - `const AbilityPickup* pickups; int pickup_count;` (reuse `logic::AbilityPickup`)
  - `const BlockSpawn* blocks; int block_count;` where `struct BlockSpawn{int tx,ty;};`
  - `const PlateSpawn* plates; int plate_count;` `struct PlateSpawn{int tx,ty,target_tx,target_ty;};`
  - `const ButtonSpawn* buttons; int button_count;` `struct ButtonSpawn{int tx,ty,target_tx,target_ty;};`
  - `const BrazierSpawn* braziers; int brazier_count;` `struct BrazierSpawn{int tx,ty,group;};`
  - `const BrazierGroupSpawn* brazier_groups; int brazier_group_count;` `struct BrazierGroupSpawn{int total,target_tx,target_ty;};` — **a brazier GROUP (multiple braziers) shares one target**; group `g` opens `(target_tx,target_ty)` when `total` braziers with `group==g` are lit. (Plates/buttons are 1:1 with their own target, so they need no group; only braziers do.)
  - (vine/ice gates reuse the existing `GateSpawn`+`GateType`; lava is a TILE value `3`, not an entity.)
  - **Extend `EntitySpawn`** (M2: `{tx,ty,param0,param1}`, param0/1 = patrol bounds) with `int param2 = 0;` (enemy flags; **bit0 = fire_immune**). M2 enemies keep `param2=0` (unaffected). The compiler sets `param2 |= 1` when an enemy's JSON entry has `"fire_immune": true`. The scene sets `enemy.fire_immune = (spawn.param2 & 1)`. (Update the enemy aggregate emitter + the `test_level_data` EntitySpawn literal to the 5-field form.)
  - **PIN the full `LevelData` aggregate-init field order** (the compiler emitter MUST match exactly): `tiles, w, h, spawn_tx, spawn_ty, has_cage, cage_tx, cage_ty, has_exit, exit_tx, exit_ty, enemies, enemy_count, gates, gate_count, doors, door_count, pickups, pickup_count, blocks, block_count, plates, plate_count, buttons, button_count, braziers, brazier_count, brazier_groups, brazier_group_count`. (New M3 arrays appended AFTER all M2 fields.)
- [ ] **Step 2:** Extend `tools/build_level.py`: map symbols → tiles/entities. `~`→tile `3` (lava, collision+visual). `V`→GateSpawn type Vine, `I`→GateSpawn type Ice (both reuse the gate path; tile `0` collision, gate overlay). `F`→AbilityPickup; its ability comes from the JSON (per-shrine, default `"fire"`), mapped name→`logic::Ability` enum int: `featherleap`→0, `fire`→1, `ice`→2, `glide`→3, `dash`→4, `grapple`→5, `stone`→6, `light`→7 (the enum order in `world_state.h`). `B`→BlockSpawn. `=`→PlateSpawn (target from JSON). `?`→ButtonSpawn (target from JSON). `*`→BrazierSpawn (group from JSON). All content symbols → collision tile `0` except `~` (lava `3`). Per-symbol JSON params keyed by scan order (as M2). **The JSON `targets` block** wires plate/button/brazier-group → a target tile coord.
- [ ] **Step 3:** Extend `tools/test_build_level.py`: a level using every new symbol compiles + emits the right counts; unknown-symbol still errors; lava emits tile `3`; a vine `V` emits a `GateType::Vine` gate.
- [ ] **Step 4:** `cd tools && python -m unittest test_build_level.py` green; `bash tools/host_test.sh` green (level_data test).
- [ ] **Step 4b: Regenerate + re-commit the existing level headers.** The committed `include/game/levels/dungeon1.h` and `hub.h` were emitted by the M2 compiler and now lag the new `LevelData` layout (they'd compile via zero-init of the new trailing fields, but should carry the new arrays explicitly). Run `python tools/build_level.py` for each existing level (or `bash tools/host_test.sh`, which regenerates them) so `dungeon1.h`/`hub.h` are re-emitted with the new format, and commit them.
- [ ] **Step 5: Commit** — `git add tools/build_level.py tools/test_build_level.py include/logic/level_data.h test/test_level_data.cpp include/game/levels/*.h && git commit -m "feat(tools): level compiler symbols for Ember Caverns (shrine/block/plate/button/brazier/lava/vine/ice)"`

### Task 1.2: Placeholder art for the new tiles + entities

**Files:** Modify `tools/make_placeholder_art.py`; regenerated `graphics/*.bmp`.

- [ ] **Step 1:** Extend `gen_tiles()` to add **bg tiles** at the reconciled indices (match `gates.h`): `7` vine (green), `8` ice (pale blue), `13` lava (orange/red), `14` brazier-unlit, `15` brazier-lit (gold flame), `17` plate (recessed floor), `18` button (small floor switch). (Indices 9–12 reserved for D3+ obstacle gates per `gates.h`; 16/19 left free.) Extend the strip width accordingly (keep ≤16-colour palette).
- [ ] **Step 2:** Add **sprites**: `fire_proj` (8×8, orange), `fire_enemy` (16×16, red enemy variant), `block` (16×16 crate), `shrine` (16×16+ glowing pedestal). Pair each with a Butano `.json`.
- [ ] **Step 3:** Run `python tools/make_placeholder_art.py`; confirm 16-colour BMPs. (mGBA visual check happens in Phase 3.)
- [ ] **Step 4: Commit** — `git add tools/make_placeholder_art.py graphics && git commit -m "feat(assets): ember placeholder tiles + fire projectile/enemy sprites"`

**After Phase 1:** 3-round review. Python + host tests green.

---

## Phase 2 — Engine: spell input + fire pool + rendering

**Execution Status:** ⬜ NOT STARTED

Butano glue; verified by mGBA in Phase 3. Each task confirms `bash tools/build_rom.sh` compiles.

### Task 2.1: Spell input adapter

**Files:** Create `include/engine/spell_input.h`, `src/engine/spell_input.cpp`.

- [ ] **Step 1:** Map keypad to spell intents: `bool cast = bn::keypad::r_pressed();` `bool cycle = bn::keypad::l_pressed();` returned in a small `SpellIntent{cast,cycle}` struct. (B bolt / A jump unchanged in `engine/input`.)
- [ ] **Step 2:** Confirms `bash tools/build_rom.sh` builds. **Step 3: Commit** — `git commit -am "feat(engine): spell input adapter (R cast / L cycle)"`

### Task 2.2: Fire projectile pool

**Files:** Create `include/engine/fire_pool.h`, `src/engine/fire_pool.cpp`.

- [ ] **Step 1:** Sibling of `BoltPool`, using `fire_proj` sprites + `logic::FireCast`. **Split responsibilities so gate-clearing works** (a vine/ice gate IS a solid tile — if the pool auto-despawned on solid tiles, the projectile would die on the gate before the scene could clear it):
  - `update_and_cast(cast_pressed, logic::SpellState&, logic::Meter& magic, logic::Vec2 muzzle, int facing)`: ticks cooldown, casts on intent if `spell.selected==SpellId::Fire` + magic available, advances active projectiles, despawns **OFF-LEVEL only** (NOT on solid tiles).
  - `bool consume_hit(const logic::Body& target)`: if an active projectile overlaps `target`, despawn it + return true. Same signature as `BoltPool::consume_hit`. The scene calls it per enemy/gate/brazier.
  - `void despawn_on_solid(const logic::Tilemap& map)`: despawn any projectile still overlapping a solid tile (walls). Called AFTER all `consume_hit` so cleared gates (already consumed) aren't double-handled.
  - Fixed pool (no heap).
- [ ] **Step 2:** Builds. **Step 3: Commit** — `git commit -am "feat(engine): fire projectile pool (casts + level/enemy hit hooks)"`

### Task 2.3: `level_view` collision→bg-index map (incl. Lava)

**Files:** Modify `include/engine/level_view.h`, `src/engine/level_view.cpp`.

- [ ] **Step 1:** Replace the identity `tile_index = cells[...]` with a small map: `0→0,1→1,2→2,3→13(lava)`; default to the raw value for forward-compat. (Gates/doors/entities still overlaid via `set_level_tile`.) This is the only change needed for lava to render; vine/ice/brazier/etc. are drawn as entity overlays by the scene.
- [ ] **Step 2:** Builds. **Step 3: Commit** — `git commit -am "feat(engine): level_view TileKind->bg-index map (lava)"`

**After Phase 2:** 3-round review. ROM builds clean.

---

## Phase 3 — scene_dungeon integration (additive)

**Execution Status:** ⬜ NOT STARTED

`scene_dungeon` is already `LevelData`-parameterized (M2). These tasks ADD handling for the new entities; existing Dungeon 1 behaviour MUST remain identical (regression-check D1 in mGBA after each task). All new gameplay decisions call the Phase-0 pure rules.

### Task 3.1: Lava damage + spell state in the loop

- [ ] **Step 1:** In `run_dungeon`: each frame, if `invuln==0 && logic::lava_overlap(player.body, map)` → `health.damage(20); invuln=45;` (reuse the M2 i-frame/blink/respawn path). Add a `logic::SpellState spell; spell.refresh(world);` and refresh it after any ability is granted.
- [ ] **Step 2:** mGBA: stand in lava → health drops + blink + respawn-on-empty. D1 still plays identically (D1 has no lava). **Step 3: Commit.**

### Task 3.2: Fire shrine (ability pickup) + Fire enablement

- [ ] **Step 1:** Spawn a shrine sprite per `level.pickups[i]`; build its `Body`. Each frame call `logic::try_take_pickup(player.body, shrine_body, world, pickup.ability)`; on true, `spell.refresh(world)`, light the shrine sprite, (optionally flash). Fire becomes castable once owned.
- [ ] **Step 2:** mGBA: walk into the shrine → Fire becomes available (HUD indicator appears in 3.6). **Step 3: Commit.**

### Task 3.3: Gate setup + Fire casting + fire-vs-enemy/gate/brazier

- [ ] **Step 1 (gate setup — geometry vs obstacle, finally giving `is_geometry` a purpose):** at setup, for each `GateSpawn`, build a one-tile `logic::Body`, mark its tile **Solid** in the mutable grid, and render its bg tile via `gate_info(type).bg_tile` (Vine→7, Ice→8 — already set in `gates.h`). Then:
  - **Geometry gates** (`gate_info(type).is_geometry` true — gap/grapple): open PASSIVELY at setup if `logic::can_pass(type, world.abilities)` (M2 hub behavior — owning the ability opens the path); re-evaluate when an ability is granted mid-dungeon.
  - **Obstacle gates** (`is_geometry` false — vine/ice/...): stay solid until ACTIVELY cleared by a matching element hit (Fire); never auto-open.
  (A taller gate wall = a column of `V`/`I` symbols = multiple GateSpawns, one tile each — same as M2's gate column.)
- [ ] **Step 2:** Add `engine::FirePool fire(...)`. Resolve per frame in THIS ORDER (cleared gates are consumed before the wall-despawn):
  1. `fire.update_and_cast(read_spell_intent().cast, spell, magic, muzzle, facing)` (advance + cast + off-level despawn).
  2. each closed vine/ice gate: `if(logic::fire_clears_gate(gate.type) && fire.consume_hit(gate.body))` → open (collision→Empty + `set_level_tile`→blank).
  3. each unlit brazier: `if(fire.consume_hit(brazier.body))` → `lit[i]=true`, swap tile 14→15.
  4. each alive enemy: `if(fire.consume_hit(enemy.body))` → kill **unless `enemy.fire_immune`** (when immune, the projectile is consumed but the enemy survives). **A Fire kill does NOT refill magic** (only the bolt path calls `magic.heal(25)`) — otherwise Fire would pay for itself (cast 25 → kill → regain 25) and the cost would be meaningless.
  5. `fire.despawn_on_solid(map)` (remaining projectiles die on real walls).
  Keep the existing bolt resolution (B bolt still kills enemies — incl. fire-immune ones — AND refills magic 25/kill, the M2 behavior; this is how you bank magic to spend on Fire).
- [ ] **Step 3:** mGBA: cast Fire (R) → kills basic enemies, fire-immune ones survive (bolt them), burns a vine gate, melts an ice gate, lights a brazier; fire still dies on real walls. **Step 4: Commit.**

### Task 3.4: Pushable blocks + pressure plates

- [ ] **Step 1:** Spawn `logic::PushableBlock` per `level.blocks[i]` + a sprite. **At setup, mark each block's tile Solid in the mutable collision grid** (the scene already owns a mutable copy of the tile grid from M2's `scene_dungeon` — the same `s_grid` the gate-open path writes). So the player collides with blocks via the existing `move_and_collide` (no special player-vs-block code).
  - **Push detection (concrete + unambiguous):** when the player is grounded and holding left/right, compute the tile immediately in front of the player's leading edge: `front_tx = px_to_tile(player.body.pos.x [+ width if facing right]) + facing`, `front_ty` = the player's mid/feet row. If `(front_tx, front_ty)` equals some block's `(tx, ty)`, call `that_block.push(facing, map)`. On `true`: update only the COLLISION grid (old cell→Empty, new cell→Solid) and move the block SPRITE (the block is a sprite, NOT a bg tile — do NOT `set_level_tile` for it; the bg under it stays blank/ground). **Rate-limit to one push per ~8 frames** (a `push_cooldown` int) so holding a direction slides the block one tile per press, not continuously.
  - **Gravity:** each frame `block.apply_gravity_step(map)`; on `true`, move its tile down one (clear old, set new, move sprite).
  - **Plate:** a plate's `Trigger.pressed` = true iff a block's `(tx,ty)` equals the plate tile OR the player's body overlaps the plate tile. Recompute each frame (latching not required — holding it down keeps it pressed; M3 puzzles assume the block stays on the plate).
- [ ] **Step 2:** mGBA: shove a block — it slides one tile per press, stops at walls, falls into gaps, and resting it on a plate fires that plate's trigger (its target opens). Regression: D1 (no blocks) unchanged. **Step 3: Commit.**

### Task 3.5: Trigger→target wiring (braziers/plates/buttons → open targets)

- [ ] **Step 1:** Build a fixed-size array of `logic::Trigger` at setup, one per puzzle:
  - **Brazier groups:** one `Trigger::braziers(group.total)` per `level.brazier_groups[g]`, with `target_tx/ty` from the group. The scene also holds a **mutable `bool lit[]` per brazier** (BrazierSpawns start unlit); Task 3.3's fire-vs-brazier sets `lit[i]=true` (and swaps tile 14→15). Each frame, recompute each group's trigger `.lit = count of lit braziers with that group`.
  - **Plates:** one `Trigger::plate()` per `level.plates[p]` with its `target_tx/ty`; `.pressed` set in Task 3.4.
  - **Buttons (PIN the affordance — step-on latching switch):** one `Trigger::button()` per `level.buttons[b]` with its `target_tx/ty`; `.pressed` latches **true once the player's body overlaps the button tile** (a concealed floor switch you must find; once stepped on it stays pressed). A brief flash on press for feedback (no separate pressed-tile art needed in M3).
  - Each frame, for every trigger: if `trigger.active()` AND its target is still closed → open the target tile (collision→Empty + `engine::set_level_tile`→blank) + a one-frame visual flash. Track an `applied` bool per trigger so it opens once. (Audio/SFX feedback deferred to M11.)
- [ ] **Step 2:** mGBA: light all braziers in a group → its door opens; push a block onto a plate → that plate's door opens; step on the hidden button → its door opens. Regression: D1 unchanged. **Step 3: Commit.**

### Task 3.6: HUD spell indicator + fire-immune enemy spawn

- [ ] **Step 1:** Add a small HUD sprite showing the selected spell (M3: a Fire icon once `spell.selected==SpellId::Fire`; hidden before). When spawning enemies, set `enemy.fire_immune = (spawn.param2 & 1)`; fire-immune enemies use the `fire_enemy` sprite, basic enemies use the `enemy` sprite.
- [ ] **Step 2:** mGBA: Fire icon appears after the shrine; fire enemies render distinctly + survive Fire. **Step 3: Commit.**

**After Phase 3:** 3-round review. Regression-confirm Dungeon 1 still plays identically.

---

## Phase 4 — Dungeon 2 content + hub unlock

**Execution Status:** ⬜ NOT STARTED

### Task 4.1: Author Dungeon 2

**Files:** Create `tools/levels/dungeon2.txt`, `tools/levels/dungeon2.json`; generated `include/game/levels/dungeon2.h`; Test `test/test_dungeon2_level.cpp`.

- [ ] **Step 1:** Author the Zelda arc (~8–12 screens wide, e.g. 90–140 tiles): solid border + floors; **first half** (no Fire): lava pools to double-jump over, a pushable-block→plate puzzle opening a gate, a hidden-button gate; the **`F` Fire shrine**; **second half** (Fire): vine + ice gates on the critical path, a 3-brazier puzzle opening a door, a mix of basic + fire-immune enemies; the caged spronk `C`; exit `E`. Solid outer border (compiler invariant). Wire plate/button/brazier targets in the JSON.
- [ ] **Step 2:** Compile (`build_rom.sh`/`host_test.sh` codegen picks it up). Host test `test_dungeon2_level.cpp`: dims, border solid, exactly one `@`/`F`/`C`/`E`, ≥1 each of block/plate/button/brazier/vine/ice, lava tiles present.
- [ ] **Step 3: Commit** — `git add tools/levels/dungeon2.* include/game/levels/dungeon2.h test/test_dungeon2_level.cpp && git commit -m "feat(content): author Dungeon 2 (Ember Caverns) Zelda arc"`

### Task 4.2: Hub D2 unlock + main loop routing

**Files:** Modify `tools/levels/hub.txt`/`hub.json` (if needed), `src/game/scene_hub.cpp`, `src/main.cpp`.

- [ ] **Step 1:** Make the hub's Dungeon-2 door enterable once reachable. In `scene_hub`: door 2 renders "open" (tile 5) and is enterable when `world.spronk_freed(1)` (D1 cleared); otherwise it stays "locked" (tile 6, non-enterable). `main.cpp` (include `game/levels/dungeon2.h`): replace the M2 `if(n != 1) continue;` guard + single-dungeon call with:
```cpp
        const logic::LevelData* lvl = nullptr;
        if(n == 1) lvl = &DUNGEON1_DATA;
        else if(n == 2) lvl = &DUNGEON2_DATA;
        else continue;                 // doors 3-8 not built
        world.current_dungeon = n;
        game::DungeonResult dr = game::run_dungeon(*lvl, world);
        world.current_dungeon = 0;
        if(dr == game::DungeonResult::Cleared) engine::write_world(world);
```
- [ ] **Step 2:** mGBA: from the hub, after D1, door 2 is enterable → loads Dungeon 2. **Step 3: Commit.**

### Task 4.3: Reward-model cleanup (D1 Featherleap via pickup)

- [ ] **Step 1:** Decide + implement: place a Featherleap `F` ability-pickup in `dungeon1.txt` (its JSON sets that shrine's ability to `featherleap`, NOT the default `fire`) **at/just before the cage** (where the old spronk-grant effectively fired), and REMOVE the hardcoded `world.grant(Ability::Featherleap)` from `run_dungeon`'s spronk-free path. This (a) unifies D1+D2 on the pickup model (spronk = cleared-bit only) AND (b) removes the M2 bug where the hardcoded grant fired Featherleap on EVERY dungeon's spronk-free (harmless on D2 since you already own it, but wrong). Re-verify D1 plays identically: Featherleap is now granted by walking into the shrine near the cage, same beat, so the high exit is still reachable.
  > If this proves fiddly, the fallback is to KEEP D1's special-case grant and only D2 uses pickups; document the choice as a deviation. Prefer the unified pickup model.
- [ ] **Step 2:** mGBA: D1 still grants Featherleap at the right beat. `bash tools/host_test.sh` green. **Step 3: Commit.**

**After Phase 4:** 3-round review + full Dungeon 2 playthrough in mGBA.

---

## Phase 5 — Verification + docs

**Execution Status:** ⬜ NOT STARTED

### Task 5.1: Full acceptance pass

- [ ] **Step 1:** mGBA: Title→Hub→D1 (regression)→D2 full arc (lava, block puzzle, hidden button, Fire shrine, vine/ice gates, brazier puzzle, fire enemies, spronk)→back to hub with Fire + D2 cleared→save persists (power-cycle → CONTINUE reflects Fire + both dungeons). Capture into `docs/acceptance-m3.md`.
- [ ] **Step 2: Commit** — `git add docs/acceptance-m3.md && git commit -m "docs: M3 acceptance record"`

### Task 5.2: Docs + close

- [ ] **Step 1:** Update `README.md` (M3 status: Fire spell, Ember Caverns, puzzle systems, spell-cast controls R/L), and this plan's Execution Status to ✅. Note deferrals (boss, audio, final art).
- [ ] **Step 2: Commit** — `git commit -am "docs: M3 README + plan close-out"`

**After Phase 5:** Final 3-round review. M3 done when the full D2 playthrough + save verify in mGBA and host tests are green.

---

## Self-Review (against M3 design)

- **Spell system + Fire (§2)** → Tasks 0.3 (FireCast/SpellState), 2.1 (input), 2.2 (pool), 3.3 (cast), 3.6 (HUD). ✅
- **Ability pickup + reward generalization (§3)** → Tasks 0.4, 3.2, 4.3. ✅
- **Obstacle gates rendered+cleared (§4)** → Tasks 0.5 (fire_clears_gate), 1.1 (V/I symbols), 3.3 (clear). ✅
- **Lava (§4)** → Tasks 0.1 (TileKind), 0.2 (rule), 2.3 (render), 3.1 (damage). ✅
- **Fire enemy (§4)** → Tasks 0.5 (flag), 3.6 (spawn). ✅
- **Pushable block (§4)** → Tasks 0.6, 3.4. ✅
- **Trigger→target (§4)** → Tasks 0.7, 1.1 (plate/button/brazier symbols), 3.4/3.5 (wiring). ✅
- **Dungeon 2 (§5)** → Tasks 1.1/1.2 (symbols/art), 4.1 (content). ✅
- **Hub unlock (§5)** → Task 4.2. ✅
- **Three-layer purity (§6)** → all Phase-0 logic host-tested; guard applies. ✅

Type names consistent across tasks: `SpellId`/`SpellState`/`FireCast`, `AbilityPickup`/`try_take_pickup`, `PushableBlock`/`push`/`apply_gravity_step`, `Trigger`/`TriggerKind`/`active`, `lava_overlap`, `fire_clears_gate`, `TileKind::Lava`/`is_lava`, `Enemy::fire_immune`, `FirePool`. Tile-index map pinned in the file-structure note. No spec requirement left without a task.
