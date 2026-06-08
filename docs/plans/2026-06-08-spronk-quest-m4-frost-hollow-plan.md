# Spronk Quest — Milestone 4 (Frost Hollow + Ice Spell) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Dungeon 3 (Frost Hollow) and the Ice spell — Spronk Quest's first **two-spell** dungeon, introducing reversible **water↔ice** terrain (Ice freezes water into platforms; Fire melts them back) and making `L` cycle Fire↔Ice.

**Architecture:** Extends shipped M3. The load-bearing change is **generalizing the single-spell module to two typed spells**: `FirePool`→`SpellPool` with each projectile tagged by `logic::SpellId`, the scene resolving hits by type. New **pure logic** (host-tested): `TileKind::Water` (damaging hazard) + `TileKind::IcePlatform` (meltable solid), reversible freeze/melt tile predicates, a `gate_cleared_by(GateType)→SpellId` generalization, and `SpellState::cycle` rotating owned spells. New **content**: compiler symbols (`w` water, `W` Water gate) + the `dungeon3` level. New **engine**: typed projectile pool, ice-projectile art, water/ice-platform tiles, a spell HUD icon that reflects the selected spell. `scene_dungeon` is extended **additively** (freeze/melt grid mutation, water hazard, dual-spell resolution, Ice shrine — the shrine/spronk/exit machinery already exists). The hub's Door 3 unlocks once D2's spronk is freed. Reuses all M1/M2/M3 engine.

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

**Overall:** ✅ **M4 COMPLETE** — all phases shipped and mGBA-verified (Frost Hollow end-to-end: Ice spell, cycle, freeze-bridges, fire-wall gate, dual-spell, hazard). Branch `m4-frost-hollow`, 117/117 host. Acceptance: `docs/acceptance-m4.md`. Remaining: real-hardware flash (inherited, deferred) + finish/merge the branch.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 0 — Pure logic (spell generalization, water/ice tiles, freeze/melt, gate) | ✅ Shipped | `105148f` | 111/111 host (+7); purity clean |
| 1 — Compiler symbols + art | ✅ Shipped | `3229fb1` | w/W symbols (+2 compiler tests); art (tiles 9/16/19 + ice_proj). ROM build deferred to Ph3 (see Deviations) |
| 2 — Engine: typed spell pool + tile render | ✅ Shipped | `5d7721e` | SpellPool (typed + tile-hit), level_view tiles. (HUD icon moved to Ph3.) |
| 3 — scene_dungeon integration (additive) | ✅ Shipped | `fa6e3ad` | water hazard, freeze/melt, dual-spell gates, HUD icon. First green ROM; 111/111 host |
| 4 — Dungeon 3 content + hub unlock | ✅ Shipped | `98c05ae`, `e212e83`, `cbc8756` | D3 gauntlet + Door 3; playtest fixes (flood-freeze, ceilings, fire-wall gate) folded in. 117/117 host |
| 5 — Verification + docs | ✅ Shipped | (this commit) | mGBA-verified end-to-end; `docs/acceptance-m4.md`, README, plan close-out |

### Deviations
- **Phase 1.2 ROM smoke-test deferred:** Phase 0's `FireCast`→`SpellCast` rename immediately breaks `engine/fire_pool.{h,cpp}` (it references `logic::FireCast`), so NO ROM build is green between Phase 0 and the Phase 2 `SpellPool` rename. Phase 1's art was verified by successful asset generation (`tiles.bmp`=160px/20 tiles, `ice_proj.bmp` written) instead of a ROM build; the first green ROM is at end of Phase 3 (as already noted for Phases 2–3).

### Discoveries
- **Water gate → fire-wall gate (playtest, user design call):** "freeze a waterfall → it vanishes" was incoherent (freezing should make water MORE solid). Replaced D3's Ice-cleared *waterfall* with an Ice-cleared **wall of fire** (Ice extinguishes it) — a clean elemental mirror of the Fire-melts-ice-wall gate. Added `GateType::FireWall` (Ability::Ice, bg tile 10 flames) + compiler symbol `X` + art; `gate_cleared_by` handles it generically. `GateType::Water` stays in the table (scaffolded) but is unused. Also removed D3's 2-tile teaser water (too narrow to sink into → walkover with no damage; a buggy no-op).
- **Freeze must bridge the whole run + gaps must not be jumpable (playtest):** the column-scan freeze worked (verified in mGBA), but freezing one tile per cast made a poor bridge AND the narrow gaps were double-jumpable, so the freeze wasn't a real gate. Fixed: (1) scene flood-freezes the contiguous horizontal water run on one Ice hit (`scene_dungeon.cpp` freeze loop); (2) D3 gaps widened (5/4 tiles) with a solid **ceiling** (row 15) over each so the player can't jump over — the ice bridge is the only way across.

---

## Standard Task Protocol (binds EVERY task)

Per `/writing-plans-enhanced` Step 3. Stated once; treat as pasted into each task.

**BEFORE starting any task:** (1) invoke `superpowers:test-driven-development`; (2) read `docs/pitfalls/testing-pitfalls.md` + `docs/pitfalls/implementation-pitfalls.md`; (3) TDD: write the failing host test → run red → minimal impl → run green.

**BEFORE marking complete:** review tests vs `docs/pitfalls/testing-pitfalls.md`; run `bash tools/host_test.sh` green; commit with a message stating what happened to assertions (add/strengthen/preserve).

**Project conventions (from M1–M3 — do NOT relearn the hard way):**
- `src/logic/**` + `include/logic/**`: **pure C++17, NO `bn::` tokens (even in comments)** — host-tested via **`bash tools/host_test.sh`**. `check_logic_purity.py` fails on `bn::` (IMPL-1).
- `int32_t` is **`long` on arm-none-eabi**: never pass a `logic::Fixed::to_int()` expression straight into a Butano API taking `bn::fixed` — assign to an `int` local first.
- `logic::Fixed` defaults to 0; `Vec2`/`Body` members start zeroed. Rely on it, but DO explicitly set meaningful fields.
- No `float`/`double` in gameplay (IMPL-2). Fixed-point tests assert exact raw ints (TEST-2). Collision tests cover tunneling (TEST-3).
- ROM build: **`bash tools/build_rom.sh`** (regenerates level headers + art via Windows Python, builds via devkitPro MSYS2). Butano forbids heap — fixed pools only.
- Scenes own their fades; dungeons use the **live fade-in** (ramp `engine::set_fade` in the loop).

**Assertion rigor:** if a test races/flakes (none expected here — logic is deterministic with explicit ticks per TEST-1), fix with deterministic logic, never by weakening assertions; if you can't, STOP and escalate.

**After each Phase:** 3+ review rounds across ambiguity / context gaps / interpretation drift / cross-task conflicts / pitfalls; if round 3 still finds issues, continue until clean.

---

## File Structure (what each new/changed file is responsible for)

**New pure logic (`include/logic/`):**
- `tilemap.h` (MODIFY) — add `TileKind::Water=4`, `TileKind::IcePlatform=5`; `is_water()`; `is_solid()` now true for `Solid` OR `IcePlatform`.
- `hazard.h` (MODIFY) — add `water_overlap()` (clone of `lava_overlap`); add a combined `hazard_overlap()` = lava OR water.
- `spell.h` (MODIFY) — `SpellId::Ice`; generalize the cast to a spell-agnostic `SpellCast` (rename of `FireCast`) emitting a projectile that the pool tags by `SpellState::selected`; `SpellState` gains Ice, and `cycle()` rotates the **owned** spells.
- `frost.h` (CREATE) — pure reversible-tile predicates: `ice_freezes(TileKind)→TileKind` (Water→IcePlatform else unchanged) and `fire_melts(TileKind)→TileKind` (IcePlatform→Water else unchanged). Pure, tiny, host-tested.
- `gates.h` (MODIFY) — add `SpellId spell_for_ability(Ability)` + `gate_cleared_by(GateType)→SpellId` (Fire for Vine/Ice gates, Ice for Water gate, None for geometry/other). Keep `fire_clears_gate` or replace its callers.

**New engine (`include/engine/` + `src/engine/`):**
- `spell_pool.{h,cpp}` (RENAME from `fire_pool.{h,cpp}`) — `SpellPool`; each `Shot` carries `logic::SpellId kind`; casts whatever spell is selected; `consume_hit(target, kind)` filters by kind; new `consume_tile_hit(map, target_kind, spell_kind, out_tx, out_ty)` for freeze/melt.
- `level_view.cpp` (MODIFY) — collision-kind→bg map gains Water and IcePlatform indices.

**New content + art:**
- `tools/build_level.py` (MODIFY) — `w`→Water tile, `W`→Water gate. (Ice shrine `F`+`"ability":"ice"` already works.)
- `tools/make_placeholder_art.py` (MODIFY) — `ice_proj` sprite (cyan), water bg tile, ice-platform bg tile, Water-gate waterfall bg tile (index 9, already reserved in the gate table).
- `tools/levels/dungeon3.txt` + `dungeon3.json` (CREATE) → `include/game/levels/dungeon3.h`.

**Scenes:**
- `src/game/scene_dungeon.cpp` (MODIFY) — additive: water hazard, freeze/melt resolution, generalized gate clearing, spell-icon swap. **No structural rewrite.**
- `src/game/scene_hub.cpp` (MODIFY) — `door_enterable` adds D3 when `spronk_freed(2)`.
- `src/main.cpp` (MODIFY) — route `n==3` to `DUNGEON3_DATA`.

**Tests (`test/`):** `test_frost.cpp`, `test_dungeon3_level.cpp` (new); extend `test_spell.cpp`, `test_hazard*`/`test_tilemap*`, `test_gates.cpp`, `test_build_level.py`.

---

## Phase 0 — Pure logic systems

**Execution Status:** ✅ SHIPPED at `105148f` on 2026-06-08 (111/111 host, purity clean)

Goal: every new rule is pure C++ and host-tested before any Butano code exists. Order matters: tiles → spell generalization → freeze/melt → gate generalization.

### Task 0.1 — Water + IcePlatform tile kinds & hazard

**Files:** Modify `include/logic/tilemap.h`, `include/logic/hazard.h`; Tests: add the tile-kind test to the existing tilemap test (grep `test/` for `Tilemap` / `is_lava` to find it), and add the hazard test alongside the existing `lava_overlap` test (grep `test/` for `lava_overlap`; if there is no dedicated hazard test, create `test/test_hazard.cpp` and ensure the harness picks it up the same way other `test/test_*.cpp` are compiled by `host_test.sh`).

- [ ] **Step 1 — failing test.** In the tilemap test, add:
```cpp
TEST(water_and_ice_kinds){
  uint8_t cells[4] = { (uint8_t)TileKind::Empty, (uint8_t)TileKind::Solid,
                       (uint8_t)TileKind::Water, (uint8_t)TileKind::IcePlatform };
  Tilemap m{4,1,cells};
  CHECK(m.is_water(2,0));
  CHECK(!m.is_solid(2,0));          // water is NOT solid
  CHECK(m.is_solid(3,0));           // ice platform IS solid (stand on it)
  CHECK(m.is_solid(1,0));           // regular solid still solid
  CHECK(!m.is_water(3,0));
}
```
And in the hazard test:
```cpp
TEST(water_overlap_damages){
  uint8_t cells[2] = { (uint8_t)TileKind::Empty, (uint8_t)TileKind::Water };
  Tilemap m{2,1,cells};
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(8),Fixed::from_int(0)};   // over tile (1,0)
  CHECK(water_overlap(b,m));
  CHECK(hazard_overlap(b,m));        // combined helper sees water too
}
```

- [ ] **Step 2 — run red.** `bash tools/host_test.sh` → fails (`Water`/`IcePlatform`/`is_water`/`water_overlap` undefined).

- [ ] **Step 3 — implement.** In `tilemap.h`:
```cpp
enum class TileKind : uint8_t { Empty=0, Solid=1, OneWay=2, Lava=3, Water=4, IcePlatform=5 };
// ...
bool is_solid(int tx,int ty) const { TileKind k=at(tx,ty); return k==TileKind::Solid || k==TileKind::IcePlatform; }
bool is_water(int tx,int ty) const { return at(tx,ty)==TileKind::Water; }
// keep is_oneway / is_lava as-is
```
In `hazard.h`, clone `lava_overlap` as `water_overlap` (same AABB scan, `is_water`), then:
```cpp
inline bool hazard_overlap(const Body& b, const Tilemap& map){ return lava_overlap(b,map) || water_overlap(b,map); }
```
**Do NOT** change `is_lava` or existing lava behavior. **Do NOT** make `IcePlatform` a hazard.

- [ ] **Step 4 — run green.** `bash tools/host_test.sh`.
- [ ] **Step 5 — commit.** `feat(logic): TileKind::Water (hazard) + IcePlatform (meltable solid) + water_overlap`.

> **Why IcePlatform is its own kind (context, do NOT collapse into Solid):** a frozen platform must be standable (solid) yet meltable by Fire, while regular walls must NOT melt. `is_solid` covers both; only `IcePlatform` is meltable (Task 0.3). Collapsing them would let Fire melt walls.

### Task 0.2 — Generalize the spell module to two typed spells

**Files:** Modify `include/logic/spell.h`; Test `test/test_spell.cpp`.

Current state (`include/logic/spell.h`): `enum class SpellId{None,Fire}`; `struct FireCast{cost=10;cooldown_ticks=20;cd;tick();try_cast(pressed,Meter&,Vec2,facing,BoltSpawn&)}`; `struct SpellState{selected; has_any(World); refresh(World); cycle(World)}` where `cycle` is a no-op alias for `refresh`. The fire pool reads `spell.selected==Fire`.

Desired: a spell-agnostic caster (rename `FireCast`→`SpellCast`, identical behavior — the projectile's *type* is decided by the pool from `SpellState::selected`, not by the caster), `SpellId::Ice`, and `cycle()` that rotates the **owned** spells.

- [ ] **Step 1 — failing test.** Extend `test_spell.cpp`:
```cpp
TEST(cycle_rotates_owned_spells){
  SpellState s; World w;
  w.grant(Ability::Fire); s.refresh(w);
  CHECK(s.selected==SpellId::Fire);
  s.cycle(w); CHECK(s.selected==SpellId::Fire);     // only one owned -> stays
  w.grant(Ability::Ice); s.cycle(w);                // now two owned
  CHECK(s.selected==SpellId::Ice);
  s.cycle(w); CHECK(s.selected==SpellId::Fire);     // wraps
}
TEST(refresh_prefers_an_owned_spell){
  SpellState s; World w; w.grant(Ability::Ice); s.refresh(w);
  CHECK(s.selected==SpellId::Ice);                  // owns Ice but not Fire
}
```
Also update the THREE existing `test_spell.cpp` tests that name the old type — `fire_needs_magic`, `fire_spends_and_spawns`, `fire_cooldown` — changing `FireCast fc;` → `SpellCast fc;` in each (behavior is identical: cost 10, cooldown 20, spends magic, vel from facing). Add nothing to the cast's signature. The `spellstate_availability` test stays valid (Fire still selectable).

- [ ] **Step 2 — run red.**

- [ ] **Step 3 — implement.** In `spell.h`:
```cpp
enum class SpellId : uint8_t { None=0, Fire, Ice };   // D3 adds Ice; D5+ extend

struct SpellCast {                 // was FireCast — spell-agnostic; pool decides projectile type
    int cost = 10;
    int cooldown_ticks = 20;
    int cd = 0;
    void tick(){ if(cd>0) --cd; }
    bool try_cast(bool pressed, Meter& magic, Vec2 origin, int facing, BoltSpawn& out){
        if(!pressed || cd>0) return false;
        if(!magic.spend(cost)) return false;
        out.pos = origin;
        out.vel = Vec2{ Fixed::from_int(2*facing), Fixed::from_int(0) };
        cd = cooldown_ticks;
        return true;
    }
};

struct SpellState {
    SpellId selected = SpellId::None;
    bool owns(const World& w, SpellId s) const {
        return (s==SpellId::Fire && w.has(Ability::Fire)) ||
               (s==SpellId::Ice  && w.has(Ability::Ice));
    }
    bool has_any(const World& w) const { return owns(w,SpellId::Fire) || owns(w,SpellId::Ice); }
    void refresh(const World& w){                    // pick first owned (Fire, then Ice)
        if(owns(w,SpellId::Fire))      selected = SpellId::Fire;
        else if(owns(w,SpellId::Ice))  selected = SpellId::Ice;
        else                           selected = SpellId::None;
    }
    void cycle(const World& w){                      // rotate Fire->Ice->Fire among owned
        if(!has_any(w)){ selected = SpellId::None; return; }
        SpellId order[2] = { SpellId::Fire, SpellId::Ice };
        int start = (selected==SpellId::Ice) ? 1 : 0;
        for(int i=1;i<=2;++i){ SpellId c=order[(start+i)%2]; if(owns(w,c)){ selected=c; return; } }
    }
};
```
**Do NOT** rename `BoltSpawn` or change `SpellCast::try_cast`'s parameters. The only call site of `FireCast`/`SpellState::selected==Fire` outside logic is the pool (Phase 2) — leave it for that task.

- [ ] **Step 4 — run green.**
- [ ] **Step 5 — commit.** `feat(logic): generalize spell module to typed spells + cycle owned (Fire/Ice)`.

### Task 0.3 — Reversible freeze/melt predicates

**Files:** Create `include/logic/frost.h`; Test `test/test_frost.cpp` (create).

- [ ] **Step 1 — failing test (`test_frost.cpp`):**
```cpp
#include "test_framework.h"
#include "logic/frost.h"
using namespace logic;
TEST(ice_freezes_only_water){
  CHECK(ice_freezes(TileKind::Water)==TileKind::IcePlatform);
  CHECK(ice_freezes(TileKind::Empty)==TileKind::Empty);   // unchanged
  CHECK(ice_freezes(TileKind::Solid)==TileKind::Solid);   // never affects walls
}
TEST(fire_melts_only_ice_platform){
  CHECK(fire_melts(TileKind::IcePlatform)==TileKind::Water);
  CHECK(fire_melts(TileKind::Solid)==TileKind::Solid);    // never melts walls
  CHECK(fire_melts(TileKind::Water)==TileKind::Water);    // unchanged
}
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement (`frost.h`):**
```cpp
#pragma once
#include "logic/tilemap.h"
namespace logic {
// Ice spell freezes a water tile into a standable platform; leaves everything else alone.
inline TileKind ice_freezes(TileKind k){ return k==TileKind::Water ? TileKind::IcePlatform : k; }
// Fire spell melts a frozen platform back to water; NEVER affects regular Solid walls.
inline TileKind fire_melts(TileKind k){ return k==TileKind::IcePlatform ? TileKind::Water : k; }
}
```
- [ ] **Step 4 — run green.**
- [ ] **Step 5 — commit.** `feat(logic): reversible freeze/melt tile predicates (frost.h)`.

### Task 0.4 — Generalize gate clearing to a spell type

**Files:** Modify `include/logic/gates.h`; Test `test/test_gates.cpp`.

Context: `scene_dungeon.cpp:232` clears a gate with `fire_clears_gate(type) && fire.consume_hit(body)`. With two spells, the scene must know WHICH spell clears each gate (Fire→Vine/Ice gates, Ice→Water gate).

- [ ] **Step 1 — failing test (extend `test_gates.cpp`):**
```cpp
TEST(gate_cleared_by_spell){
  CHECK(gate_cleared_by(GateType::Vine)  == SpellId::Fire);
  CHECK(gate_cleared_by(GateType::Ice)   == SpellId::Fire);
  CHECK(gate_cleared_by(GateType::Water) == SpellId::Ice);
  CHECK(gate_cleared_by(GateType::Gap)   == SpellId::None);  // geometry gate, not spell-cleared
}
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement (`gates.h`, needs `#include "logic/spell.h"` for `SpellId` — verify no include cycle; `spell.h` includes `world_state.h`+`bolt.h`+`meters.h`, none include `gates.h`, so it's safe):**
```cpp
inline SpellId spell_for_ability(Ability a){
    if(a==Ability::Fire) return SpellId::Fire;
    if(a==Ability::Ice)  return SpellId::Ice;
    return SpellId::None;
}
// Which spell projectile clears this gate (None if geometry/ability-only, not spell-cleared).
inline SpellId gate_cleared_by(GateType t){
    const GateInfo& gi = gate_info(t);
    return gi.is_geometry ? SpellId::None : spell_for_ability(gi.required);
}
```
**Leave `include/logic/fire_effect.h`'s `fire_clears_gate` and its test untouched** — `gate_cleared_by` is a new, more general helper in `gates.h`; Phase 3.1 switches the scene's gate loop from `fire_clears_gate` to `gate_cleared_by`, after which `fire_clears_gate` is simply unused (harmless, keeps its passing test). **Do NOT** delete `fire_effect.h`, alter `GATE_TABLE`, or change `can_pass`.
- [ ] **Step 4 — run green.**
- [ ] **Step 5 — commit.** `feat(logic): gate_cleared_by(GateType)->SpellId (Fire/Ice/None)`.

**After Phase 0:** run the 3+ review rounds. Confirm `check_logic_purity.py` clean and host suite green (expect ~+8 tests).

---

## Phase 1 — Level compiler symbols + placeholder art

**Execution Status:** ✅ SHIPPED at `3229fb1` on 2026-06-08 (art via asset-gen; ROM build deferred to Ph3)

### Task 1.1 — Compiler: water tile `w` + Water gate `W`

**Files:** Modify `tools/build_level.py`; Test `test/test_build_level.py`.

Context: `build_level.py` has `TILE={'#':1,'.':0,'^':2,'~':3}` (lava=3) and a gate parser (`V`→Vine, `I`→Ice). Add water tile (kind 4) and a Water gate.

- [ ] **Step 1 — failing test (`test_build_level.py`):** assert a grid with `w` emits tile value `4` at that cell, and a `W` emits a `GateSpawn` of type `Water`. Mirror the existing vine/ice gate assertions.
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement.** Add `'w':4` to `TILE`. In the content parser, map `W`→`GateType.Water` (mirror how `V`/`I` build gates). Update the symbol legend/docstring. **Do NOT** repurpose `~` (lava) or existing letters; `w`/`W` are unused (verify against `#.^~@CEoGVIFB=?*1-8`).
- [ ] **Step 4 — run green** (`bash tools/host_test.sh` runs the Python suite too).
- [ ] **Step 5 — commit.** `feat(tools): level symbols w (water) + W (Water gate)`.

### Task 1.2 — Placeholder art: ice projectile, water, ice-platform, waterfall

**Files:** Modify `tools/make_placeholder_art.py`.

Context: `gen_tiles()` builds the 19-tile `tiles.bmp`; bg indices: 13 lava, 8 ice-gate, gate table reserves 9 for Water gate. `gen_ember_sprites()` builds `fire_proj` etc. Palette indices: 0 transparent,1 dark,2 rose,3 skin,4 green,5 dark-green,6 gold,7 rose-light,8 cyan,9 white,10 brown,11 dark-brown,12 stone,13 red,14 shadow,15 white.

**AUTHORITATIVE bg-tile indices (PINNED here — Phases 2.2 and 3.2 consume these exact numbers; do NOT choose different ones):**
- **Water bg tile = `16`** (currently a free/blank slot, 0–18 strip). Visual for `TileKind::Water`.
- **Ice-platform bg tile = `19`** (NEW slot — widen the strip from 19→20 tiles). Visual for `TileKind::IcePlatform`. Must look distinct from the ice-*gate* tile (8).
- **Water-gate waterfall = `9`** (already reserved in `gates.h` `GATE_TABLE` for `GateType::Water`'s `bg_tile`).

- [ ] **Step 1 — implement (no host test; visual — verified in mGBA Phase 5).** Add to the generators:
  - **`ice_proj`** 8×8 sprite — cyan orb (mirror `fire_proj` shape; palette cyan(8)/white(9) instead of gold/red). MUST be written to `graphics/ice_proj.bmp` (+`.json`) exactly like `fire_proj`, so Butano generates `bn_sprite_items_ice_proj.h` (consumed by Task 2.1).
  - **Water bg tile at index 16** — static art that reads as water (cyan(8) fill + white(9) glints). Not literally animated (a single bg tile has no frames).
  - **Ice-platform bg tile at index 19** — frozen white/cyan slab; widen the tile strip to 20 tiles (`8*20`).
  - **Water-gate waterfall at index 9** — vertical blue streaks.
- [ ] **Step 2 — keep indices consistent.** Update `gates.h`'s tile-index comment to record water=16, ice-platform=19. These are the SAME numbers Phase 2.2 (`level_view.cpp`) and Phase 3.2 (`scene_dungeon.cpp` `WATER_BG`/`ICE_PLATFORM_BG`) use — they MUST NOT diverge.
- [ ] **Step 3 — regenerate + build smoke-test.** `python tools/make_placeholder_art.py` then `bash tools/build_rom.sh -j8` → expect `ROM fixed!` (art imports clean).
- [ ] **Step 4 — commit.** `feat(assets): ice projectile + water/ice-platform/waterfall tiles`.

**After Phase 1:** review rounds; confirm the bg-index numbers are written down in ONE authoritative place and referenced (not duplicated divergently).

---

## Phase 2 — Engine: typed spell pool + tile render

**Execution Status:** ✅ SHIPPED at `5d7721e` on 2026-06-08 (compiles with Phase 3)

> **⚠ Phases 2 and 3 are co-dependent — land them together.** Task 2.1 changes `SpellPool::consume_hit` to take a `kind` argument, which makes `scene_dungeon.cpp`'s existing M3 call sites (`fire.consume_hit(gi.body)`, etc.) stop compiling. The first GREEN ROM build is at the **end of Phase 3**, not Phase 2. Do NOT expect `ROM fixed!` after Phase 2 alone. If using subagent-driven execution, dispatch Phase 2 + Phase 3 to the same agent (or sequence them with no intermediate ROM gate). Both touch the engine/scene boundary; `scene_dungeon.cpp` is edited in Phase 3 only (the HUD-icon change moved there to keep Phase 2 engine-only).

### Task 2.1 — `FirePool`→`SpellPool` (typed projectiles + tile-hit)

**Files:** Rename `include/engine/fire_pool.h`→`spell_pool.h`, `src/engine/fire_pool.cpp`→`spell_pool.cpp`; update the `#include` in `scene_dungeon.cpp` (only consumer). No host test (Butano layer); behavior verified by the scene + mGBA.

Current `FirePool` (see `src/engine/fire_pool.cpp`): casts only when `spell.selected==Fire`, all shots use `fire_proj`, `consume_hit(target)` despawns any overlapping shot, `despawn_on_solid` kills shots on solid tiles.

- [ ] **Step 1 — rename + retype.** Rename the class to `SpellPool` and both files via `git mv fire_pool.h spell_pool.h` + `git mv fire_pool.cpp spell_pool.cpp` (the old files MUST be gone — Butano globs all `src/**/*.cpp`, so a leftover `fire_pool.cpp` would cause duplicate definitions). No Makefile edit needed. Add `#include "bn_sprite_items_ice_proj.h"` alongside the existing `bn_sprite_items_fire_proj.h` (the ice art from Task 1.2 generates this header). Update the `#include "engine/fire_pool.h"`→`"engine/spell_pool.h"` in `scene_dungeon.cpp` (its only consumer) and the member type `engine::FirePool fire`→`engine::SpellPool spells` (rename the variable too; Phase 3 references `spells`). Give `Shot` a `logic::SpellId kind = logic::SpellId::None;`. Change the member `logic::FireCast _caster;`→`logic::SpellCast _caster;` (renamed type from Task 0.2).
- [ ] **Step 2 — cast the selected spell (any type).** Replace ONLY the cast block (the `if(spell.selected == logic::SpellId::Fire){...}` block); **keep the rest of `update_and_cast` unchanged** — the projectile-advance loop, the OFF-LEVEL despawn, and the `set_position` call all stay as-is. New cast block:
```cpp
if(spell.selected != logic::SpellId::None){
    logic::BoltSpawn spawn;
    if(_caster.try_cast(cast_pressed, magic, muzzle, facing, spawn)){
        for(Shot& s : _shots){ if(!s.active){
            s.active=true; s.kind=spell.selected;
            s.body.pos=spawn.pos; s.body.half_w=logic::Fixed::from_int(3); s.body.half_h=logic::Fixed::from_int(3);
            s.vel=spawn.vel;
            s.sprite = (spell.selected==logic::SpellId::Ice)
                     ? bn::sprite_items::ice_proj.create_sprite(0,0)
                     : bn::sprite_items::fire_proj.create_sprite(0,0);
            s.sprite->set_camera(_camera); break; } }
    }
}
```
- [ ] **Step 3 — type-filtered hit:** `bool consume_hit(const logic::Body& target, logic::SpellId kind)` — only despawn+return true for an active shot whose `s.kind==kind` overlapping `target`.
- [ ] **Step 4 — tile hit (freeze/melt), with a COLUMN-DOWN scan.** ⚠ **This is the M3 brazier-height bug applied to terrain:** spells travel horizontally at the player's chest height (~row 18 from `muzzle`), but water and ice-platform tiles sit at FLOOR level (rows ~19–20). A tile-exact check at the shot's own tile would fly OVER the water and never freeze it (this soft-locked D2's braziers until we gave them tall hit-bodies). So scan **down the shot's column** for the first `target_kind` tile beneath the shot:
```cpp
// Despawn the first active shot of `spell_kind` whose column contains `target_kind` below it
// (chest-height shot vs floor-level tile). Report the matched tile. Stops at a wall so we never
// freeze water sealed under a floor.
bool consume_tile_hit(const logic::Tilemap& map, logic::TileKind target_kind,
                      logic::SpellId spell_kind, int& out_tx, int& out_ty){
    for(Shot& s : _shots){
        if(!s.active || s.kind != spell_kind) continue;
        int tx = logic::Tilemap::px_to_tile(s.body.pos.x + s.body.half_w);
        int ty = logic::Tilemap::px_to_tile(s.body.pos.y + s.body.half_h);
        for(int y = ty; y < map.h; ++y){
            if(map.at(tx, y) == target_kind){ s.active=false; s.sprite.reset(); out_tx=tx; out_ty=y; return true; }
            // IcePlatform is solid, but we test target_kind FIRST above, so a melt still matches it;
            // a Solid wall (or an IcePlatform when freezing) stops the scan.
            if(map.is_solid(tx, y)) break;
        }
    }
    return false;
}
```
(For MELT, `target_kind==IcePlatform`: the `at==target_kind` test matches before the `is_solid` break, so the ice platform is found. For FREEZE, `target_kind==Water`: the scan stops at the first solid tile, so water under a floor is never frozen.)
- [ ] **Step 5 — keep `despawn_on_solid`** unchanged (it now also despawns Fire/Ice shots that hit an IcePlatform/wall AFTER the scene's freeze/melt+gate resolution runs — ordering enforced in Phase 3).
- [ ] **Step 6 — commit** (no ROM build yet — see the Phase 2/3 coupling note; `scene_dungeon.cpp` won't compile until Phase 3.1 updates its `consume_hit` call sites). Commit: `refactor(engine): FirePool->SpellPool with typed projectiles + tile-hit`.

> **Pitfall (ordering):** `IcePlatform` is solid, so `despawn_on_solid` would kill a Fire shot before it can melt. The scene MUST resolve `consume_tile_hit(melt)` and gates BEFORE `despawn_on_solid` (Phase 3 fixes the order, mirroring how M3 already orders gates before despawn).

### Task 2.2 — `level_view` water/ice-platform → bg index

**Files:** Modify `src/engine/level_view.cpp`.

Context: `build_level_view` maps collision kind→bg: `int kind=...; tile_index = (kind==3)?13:kind;` (lava→13).

- [ ] **Step 1 — extend the map** to send `TileKind::Water` (4) → bg `16` and `TileKind::IcePlatform` (5) → bg `19` (the indices pinned in Task 1.2). Add a comment citing `gates.h`'s tile map. The existing lava case (`kind==3 → 13`) stays.
- [ ] **Step 2 — commit** `feat(engine): level_view maps Water(16)/IcePlatform(19) bg tiles` (no ROM build — see the Phase 2/3 coupling note; build happens at end of Phase 3).

**After Phase 2:** review rounds; confirm the renamed pool's header/source are internally consistent and the bg indices equal Phase 1's pinned numbers. The renamed-API break in `scene_dungeon.cpp` is EXPECTED and resolved in Phase 3 — do not "fix" it by reverting the signature.

---

## Phase 3 — scene_dungeon integration (additive)

**Execution Status:** ✅ SHIPPED at `fa6e3ad` on 2026-06-08 (first green ROM; 111/111 host)

**Files:** Modify `src/game/scene_dungeon.cpp` only. No new structure — extend the existing fire-resolution block (`:226`–`:262`) and the lava-hazard line (`:262`). Verified by build + mGBA (Phase 5); no host test (scene is Butano-bound).

Current main-loop order (M3): read spell intent → `if(cycle) spell.cycle` → `fire.update_and_cast` → gates (Fire) → braziers → enemies → `despawn_on_solid` → lava damage. The pool is now `SpellPool` (renamed include).

- [ ] **Step 1 — generalized gate clearing.** Replace the gate loop (`:231`–`:234`):
```cpp
for(GateInst& gi : gates){
    logic::SpellId s = logic::gate_cleared_by(gi.spawn.type);
    if(!gi.open && s != logic::SpellId::None && spells.consume_hit(gi.body, s)){
        gi.open = true; open_column(lvl.view, gi.spawn.tx, level.h);
    }
}
```
(braziers stay Fire-only: `spells.consume_hit(bi.body, SpellId::Fire)`; the fire-immune enemy check stays `consume_hit(inst.e.body, SpellId::Fire)`.)

- [ ] **Step 2 — freeze + melt resolution (BEFORE `despawn_on_solid`).** After the enemy loop, add:
```cpp
int tx, ty;
// Ice freezes water it flies into.
while(spells.consume_tile_hit(lvl.map, logic::TileKind::Water, logic::SpellId::Ice, tx, ty)){
    engine::set_collision_tile(tx, ty, (int)logic::TileKind::IcePlatform);
    engine::set_level_tile(lvl.view, tx, ty, ICE_PLATFORM_BG);   // index from Phase 1.2
}
// Fire melts ice platforms it hits (must precede despawn_on_solid; IcePlatform is solid).
while(spells.consume_tile_hit(lvl.map, logic::TileKind::IcePlatform, logic::SpellId::Fire, tx, ty)){
    engine::set_collision_tile(tx, ty, (int)logic::TileKind::Water);
    engine::set_level_tile(lvl.view, tx, ty, WATER_BG);          // index from Phase 1.2
}
spells.despawn_on_solid(lvl.map);
```
Use `while` so multiple shots resolve in one frame. Define `WATER_BG=16` and `ICE_PLATFORM_BG=19` as `constexpr int` locals near this block (the pinned Phase 1.2 indices) — do NOT inline bare numbers. **Critical M3 distinction:** `set_collision_tile(tx,ty,v)` takes a **TileKind collision value** (so `(int)TileKind::IcePlatform`=5 / `(int)TileKind::Water`=4), while `set_level_tile(view,tx,ty,idx)` takes a **bg tile INDEX** (so `ICE_PLATFORM_BG`=19 / `WATER_BG`=16). These two numbers are different for the same tile — passing a bg index to `set_collision_tile` (or vice-versa) is the classic M3 bug. Because `lvl.map.cells` points at the mutable grid that `set_collision_tile` writes, `consume_tile_hit` sees the freeze immediately (an already-frozen tile is `IcePlatform`, not `Water`, so it won't re-freeze).

- [ ] **Step 3 — water hazard.** Change the lava line (`:262`) to use the combined helper:
```cpp
if(invuln == 0 && logic::hazard_overlap(player.body, lvl.map)){ health.damage(20); invuln = 45; }
```
(20 dmg + 45 i-frames, identical to lava — honors soft-lock guard #1: damage, not instakill.)

- [ ] **Step 4 — spell HUD icon reflects the selected spell.** Today `spell_icon` (`:106`–`:107`, `:336`) is always `fire_proj`, shown only when `selected==Fire`. Change it to show whenever `spell.selected != SpellId::None`, and to display the selected spell's art. Use Butano's `spell_icon.set_item(bn::sprite_items::ice_proj)` / `set_item(bn::sprite_items::fire_proj)` to swap the image WITHOUT recreating the sprite, and only call `set_item` when `selected` changed (track a `logic::SpellId last_icon` across frames). Confirm `spell.refresh(world)` already runs after the Ice-shrine grant (`:332`) so the icon updates on pickup, and `spell.cycle(world)` on `L` (`:227`) updates it.
- [ ] **Step 5 — build smoke-test** `bash tools/build_rom.sh -j8` → `ROM fixed!` (this is the FIRST green ROM since the Phase 2 API change; it proves Phases 2+3 compile together). Commit: `feat(game): integrate water hazard + reversible freeze/melt + dual-spell gates + spell HUD icon (M4 Phase 3)`.

> **Cross-task note:** `ICE_PLATFORM_BG` / `WATER_BG` are the exact indices from Task 1.2 — define them as named constants near the top of the freeze/melt block (or a small `constexpr`), do NOT inline magic numbers that could drift from `level_view.cpp`.

**After Phase 3:** review rounds focusing on resolution ORDER (freeze/melt before `despawn_on_solid`; gates before melt so a Fire shot clears an Ice-wall gate rather than melting a platform behind it — they're different tiles so order among them is safe, but melt MUST precede `despawn_on_solid`).

---

## Phase 4 — Dungeon 3 content + hub unlock

**Execution Status:** ✅ SHIPPED at `98c05ae` (+ playtest fixes `e212e83`, `cbc8756`) on 2026-06-08

### Task 4.1 — Author Dungeon 3 (`dungeon3.txt` + `.json`)

**Files:** Create `tools/levels/dungeon3.txt` + `dungeon3.json` → compile to `include/game/levels/dungeon3.h`; Test `test/test_dungeon3_level.cpp`.

- [ ] **Step 1 — failing level test (`test_dungeon3_level.cpp`, mirror `test_dungeon2_level.cpp`):** concrete assertions —
  - `DUNGEON3_DATA.w==64 && h==22`; border all `Solid` (tile value 1).
  - `pickup_count==1` and `pickups[0].ability==Ability::Ice` (the Ice shrine).
  - at least one tile equals `(uint8_t)TileKind::Water` (4) — scan `tiles[]`.
  - at least one `gates[i].type==GateType::Water` (the Ice-cleared Water gate).
  - at least one Fire obstacle proving dual-spell: `gates[i].type==GateType::Vine` OR `brazier_count>=1`.
  - `has_cage && has_exit`; `enemy_count>=1` (a magic source).
- [ ] **Step 2 — author the level** as the spec's beat sequence (64×22 gauntlet): Fire-first half (enemy → vine gate → brazier → Ice shrine ~col 30), frozen second half (wide water gap requiring a mid-gap freeze → Water gate → dual-spell climax → spronk ~col 60 → exit ~col 61). JSON wires enemy patrols, `pickups:[{"ability":"ice"}]`, gate/trigger targets. **Honor soft-lock guards:** a magic-source enemy before each mandatory cast; water gaps escapable; never a melt-and-strand. **INVARIANT — D3 assumes the player owns Fire:** the Fire-first half (vine gate, brazier) requires Fire, which is guaranteed because Door 3 is only enterable after D2 is cleared (Task 4.3) and D2 grants Fire. Do NOT place a Fire shrine in D3 (only the Ice shrine). Do NOT design a Fire obstacle that a Fire-less player could reach.

**AUTHORING for the column-scan freeze (Task 2.1 Step 4):** water gaps must be **open from above** — the water surface must have empty tiles above it along the player's casting path, so the Ice shot's column scan reaches the water (do NOT roof a freezable water tile with a solid tile). A frozen single tile (8px) is standable by the 16px player (foot-AABB overlap supports them), but author gaps so a couple of frozen stepping-stones cross them; final widths/stepping-stone count are tuned in Task 5.1.
- [ ] **Step 3 — compile + run green.** `python tools/build_level.py tools/levels/dungeon3.txt include/game/levels/dungeon3.h` then `bash tools/host_test.sh`.
- [ ] **Step 4 — commit.** `feat(content): Dungeon 3 (Frost Hollow) authored as data`.

### Task 4.2 — Route D3 in `main.cpp`

**Files:** Modify `src/main.cpp`.

- [ ] **Step 1 — implement.** `#include "game/levels/dungeon3.h"`; in the dungeon dispatch add `else if(n==3) lvl=&DUNGEON3_DATA;`. Pattern matches the existing `n==1/n==2` block.
- [ ] **Step 2 — build smoke-test** (folded into 4.3).

### Task 4.3 — Hub Door 3 enterable after D2 cleared

**Files:** Modify `src/game/scene_hub.cpp` (`door_enterable`, ~`:25`).

- [ ] **Step 1 — implement.** Extend:
```cpp
bool door_enterable(int n, const logic::World& w){
    return n == 1
        || (n == 2 && w.spronk_freed(1))
        || (n == 3 && w.spronk_freed(2));
}
```
- [ ] **Step 2 — build smoke-test** `bash tools/build_rom.sh -j8` → `ROM fixed!`. Commit: `feat(game): route Dungeon 3 + hub Door 3 unlock after D2`.

**After Phase 4:** review rounds; confirm D3 is reachable only after D2 and that the level test pins all the required systems.

---

## Phase 5 — Verification + docs

**Execution Status:** ✅ SHIPPED on 2026-06-08 (mGBA-verified; acceptance-m4 + README + close-out)

- [ ] **Task 5.1 — mGBA playthrough.** Because D3's Fire-first half needs Fire, reach D3 by playing through (D1→D2→D3) OR boot a save that already owns Fire + has D2 cleared (so Door 3 is open). On a fresh save, that means clearing D1 (Featherleap) and D2 (Fire) first. Verify end-to-end: reach the Ice shrine using Fire/bolt; earn Ice; HUD icon swaps; `L` cycles Fire↔Ice; **Ice freezes a wide water gap into a crossable platform**; **Fire melts an ice platform back to water** (and you can re-freeze it); the **Water gate** opens only with Ice; the dual-spell climax requires cycling; water damages (not instakill) and is escapable; spronk → exit clears; **Door 3 was locked until D2 cleared**; D3 saves on clear. Tune gap widths so the freeze is mandatory (wider than a double-jump). Log any fix as a Discovery.
- [ ] **Task 5.2 — `docs/acceptance-m4.md`.** Map each design criterion (§9 of the spec) → status + evidence, mirroring `docs/acceptance-m3.md`. Note the host-test count and any deviations/discoveries.
- [ ] **Task 5.3 — README + plan close-out.** README: add the Ice spell to "What's playable" + the level-symbol legend (`w`, `W`) + the new test count. Mark all phase banners ✅ with SHAs; fill the Execution Status table.
- [ ] **Task 5.4 — finish the branch.** Per the user's M1–M3 pattern: merge `m4-frost-hollow` → `main`, push to origin (CONFIRM with the user first — publishing is outward-facing).

---

## Self-Review (run by the author before committing the plan)

- **Spec coverage:** every §9 success criterion maps to a task — Ice earned (4.1 shrine), cycle (0.2+2.3), freeze→platform (0.3+3.2), Fire melts back (0.3+3.2), wide gap impassable without freeze (4.1 authoring + 5.1 tuning), Water gate Ice-only (0.4+3.1), dual-spell climax (4.1), spronk/exit/Door-3 (4.1/4.3), purity+host tests (Phase 0, protocol). ✅
- **Type consistency:** `SpellCast` (was `FireCast`) used in pool 2.1; `SpellId::Ice` defined 0.2 before use; `gate_cleared_by` defined 0.4 used 3.1; `consume_hit(target,kind)` signature defined 2.1 used 3.1; `consume_tile_hit` defined 2.1 used 3.2; `ICE_PLATFORM_BG`/`WATER_BG` sourced from 1.2 used 2.2+3.2. ✅
- **Placeholder scan:** bg indices intentionally deferred to 1.2 with an authoritative-location rule (not a vague TBD). ✅
- **Pitfalls:** IMPL-1 (purity guard each phase), IMPL-2 (no float — all fixed-point), TEST-1/2 (deterministic ticks, raw-int asserts) called out in the protocol; the freeze-vs-despawn ORDER pitfall is flagged in 2.1 + 3.2.
