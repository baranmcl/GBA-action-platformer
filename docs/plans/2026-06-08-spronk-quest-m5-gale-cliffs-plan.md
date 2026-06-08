# Spronk Quest — Milestone 5 (Gale Cliffs + Glide) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Dungeon 4 (Gale Cliffs) and the Glide (Wind Cloak) ability — Spronk Quest's first **vertical-ascent** dungeon, with a tile-based **wind kit** (glide, updraft shafts, gust zones).

**Architecture:** Extends shipped M4. Glide is a **traversal** ability (no magic, no projectile), the sibling of Featherleap — its rules live in pure `Player::update`. New **pure logic**: `Abilities::glide` + `InputFrame.glide_held`; three non-solid `TileKind`s (`Updraft`, `WindLeft`, `WindRight`); a `wind.h` helper module (`updraft_overlap`, `wind_dir`); and the glide/updraft/gust rules in `Player::update`. New **engine**: read `bn::keypad::a_held()` into `glide_held`; a kind→bg map + placeholder art for the wind tiles. **`scene_dungeon` change is one line** (`player.abilities.glide = world.has(Glide)`) — the player rules do the rest. New **content**: a vertically-oriented `dungeon4` level (≤32 tiles tall — the BG already supports it) + compiler symbols. Hub Door 4 unlocks after D3.

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

**Overall:** Not started. Branch `m5-gale-cliffs` (design spec already committed there).

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 0 — Vertical-scroll confirmation spike | ✅ Shipped | `1fa452d` | scroll verified; camera clamp added (wrap fix) + hub 20-tall. 117/117 host |
| 1 — Pure logic (glide/updraft/wind + tiles) | ✅ Shipped | `4a077e6` | 3 tiles, wind.h, Player::update rules. 124/124 host (+7) |
| 2 — Engine: glide input + wind/updraft tiles & art | ⬜ Not started | — | — |
| 3 — Dungeon 4 content + scene wiring + hub Door 4 | ⬜ Not started | — | — |
| 4 — Verification + docs | ⬜ Not started | — | — |

### Deviations
- **Camera clamp WAS needed (Phase 0 Step 4 fallback invoked):** the fixed 64×32 BG wraps past the authored level, so a 30-tall stub showed its top wrapped onto the screen bottom. Added `set_clamped_cam` in `scene_dungeon.cpp` AND `scene_hub.cpp` (keeps the 240×160 view inside level bounds; centers sub-screen levels). Also extended the **hub 18→20 rows** so its floor fills the screen height (user-requested; the open-plaza sky on the sides is intentional, left as-is). SHA `1fa452d`.

### Discoveries
- **Vertical scroll is already supported (pre-execution finding):** `src/engine/level_view.cpp` allocates a fixed **64×32** BG (`COLS=64, ROWS=32`) and the camera transform uses the full BG size, not the level's `h`. Existing levels are 64×22 (already taller than the 20-tile screen), so vertical scrolling already works — a taller D4 (≤32 rows) just fills more of the BG. Phase 0 confirms this visually rather than building new scroll support.

---

## Standard Task Protocol (binds EVERY task)

Per `/writing-plans-enhanced` Step 3. Stated once; treat as pasted into each task.

**BEFORE starting any task:** (1) invoke `superpowers:test-driven-development`; (2) read `docs/pitfalls/testing-pitfalls.md` + `docs/pitfalls/implementation-pitfalls.md`; (3) TDD: failing host test → run red → minimal impl → run green.

**BEFORE marking complete:** review tests vs `docs/pitfalls/testing-pitfalls.md`; run `bash tools/host_test.sh` green; commit with a message stating what happened to assertions (add/strengthen/preserve).

**Project conventions (from M1–M4 — do NOT relearn the hard way):**
- `src/logic/**` + `include/logic/**`: **pure C++17, NO `bn::` tokens (even in comments)** — host-tested via `bash tools/host_test.sh`; `check_logic_purity.py` fails on `bn::` (IMPL-1).
- `int32_t` is **`long` on arm-none-eabi**: assign a `logic::Fixed::to_int()` to an `int` local before passing it to a Butano API.
- `logic::Fixed` defaults to 0. No `float`/`double` in gameplay (IMPL-2); fixed-point tests assert exact raw ints (TEST-2); logic tests use explicit ticks, no frame timing (TEST-1).
- ROM build: `bash tools/build_rom.sh` (regenerates level headers + art via Windows Python, builds via devkitPro MSYS2). Butano forbids heap — fixed pools/arrays only.
- Scenes own their fades; dungeons use the live fade-in.

**Assertion rigor:** logic here is deterministic (explicit ticks) — if a test seems to flake, the bug is real; fix the logic, never weaken the assertion. STOP and escalate if you cannot.

**After each Phase:** 3+ review rounds (ambiguity / context gaps / drift / cross-task conflicts / pitfalls); continue until a round is clean.

---

## File Structure

**New pure logic (`include/logic/`):**
- `tilemap.h` (MODIFY) — add `TileKind::Updraft=6`, `WindLeft=7`, `WindRight=8` (all non-solid; `is_solid` unchanged).
- `player.h` (MODIFY) — `Abilities::glide` (bool); `InputFrame.glide_held` (bool).
- `player.cpp` (MODIFY) — glide/updraft/gust rules in `Player::update`; tuning constants.
- `wind.h` (CREATE) — `updraft_overlap(body, map)→bool` and `wind_dir(body, map)→int` (−1/0/+1), AABB-scan helpers (the `hazard.h` pattern).

**New engine:**
- `src/engine/input.cpp` (MODIFY) — fill `glide_held = bn::keypad::a_held()`.
- `src/engine/level_view.cpp` (MODIFY) — kind→bg map for Updraft/WindLeft/WindRight.
- `tools/make_placeholder_art.py` (MODIFY) — updraft + wind bg tiles.

**Scene:**
- `src/game/scene_dungeon.cpp` (MODIFY) — ONE line: `player.abilities.glide = world.has(logic::Ability::Glide);` next to the existing featherleap line. (Optional MAY: tint the avatar while gliding.)
- `src/game/scene_hub.cpp` (MODIFY) — `door_enterable` adds D4 when `spronk_freed(3)`.
- `src/main.cpp` (MODIFY) — route `n==4` to `DUNGEON4_DATA`.

**Content + tests:** `tools/build_level.py` (updraft/wind symbols), `tools/levels/dungeon4.{txt,json}`, `test/test_wind.cpp` + extend `test_player.cpp`, `test_tilemap.cpp`, `test_build_level.py`, `test/test_dungeon4_level.cpp`.

---

## Phase 0 — Vertical-scroll confirmation spike

**Execution Status:** ✅ SHIPPED at `1fa452d` on 2026-06-08 (scroll verified; camera clamp + hub 20-tall)

Goal: confirm a level taller than one screen scrolls cleanly BEFORE authoring D4. Per the Discoveries note, the 64×32 BG already supports this; this phase is a fast visual check + a fallback.

### Task 0.1 — Lay the D4 plumbing with a tall STUB level + verify vertical scroll in mGBA

This task establishes the permanent D4 plumbing (file, route, hub door) ONCE, seeded with a tall
throwaway stub so the scroll can be verified. Phase 3.3 later replaces ONLY the stub's `.txt`
content with the real level — it does NOT re-touch `main.cpp`/`scene_hub.cpp` (done here).

**Files:** Create `tools/levels/dungeon4.txt` + `dungeon4.json` → `include/game/levels/dungeon4.h`; Modify `src/main.cpp`, `src/game/scene_hub.cpp`.

- [ ] **Step 1 — tall STUB level.** Author a **24 wide × 30 tall** stub: solid border; a vertical stack of one-way ledges (`^`, two tiles wide) every **≤4 rows** from bottom to top (so the player climbs with the **double-jump** they already own — Glide is NOT implemented yet, so the stub MUST be climbable by Featherleap alone); spawn `@` near the bottom; a `C` cage + `E` exit near the top. JSON: `{ "tileset":"tiles", "enemies":[] }`. Compile: `python tools/build_level.py tools/levels/dungeon4.txt include/game/levels/dungeon4.h`.
- [ ] **Step 2 — route + unlock (permanent).** `main.cpp`: add `#include "game/levels/dungeon4.h"` and `else if(n==4) lvl=&DUNGEON4_DATA;` to the dungeon dispatch. `scene_hub.cpp` `door_enterable`: add `|| (n==4 && w.spronk_freed(3))`. Confirm `HUB_DOORS` in `include/game/levels/hub.h` already has a `dungeon==4` entry (M4 left `{...,4}` present; if absent, add `4` to `hub.txt` and recompile).
- [ ] **Step 3 — build + verify scroll.** `bash tools/build_rom.sh -j8` → `ROM fixed!`. In mGBA, reach D4 (use a save with D1–D3 cleared — Door 4 needs `spronk_freed(3)` — or play through) and climb. **Confirm:** the camera scrolls UP smoothly as the player climbs; content stays visible; no garbage/torn rows at the top/bottom extremes.
- [ ] **Step 4 — IF scrolling is broken or shows ugly blank/garbage at the extremes (fallback ONLY):** add **camera clamping** in `scene_dungeon.cpp` at the per-frame `cam.set_position(cx - hw, cy - hh)` call (search for it) — clamp so the camera never scrolls past the level's authored bounds (`level.w*8`/`level.h*8` vs the 240×160 screen, half 120/80). Record as a Deviation. If scrolling is fine, NO code change — just record the confirmation.
- [ ] **Step 5 — commit** `feat(m5): D4 plumbing (route + Door 4) with a tall stub; vertical scroll verified`.

> **Why first:** the whole D4 shape is vertical. If scroll needed real work, every later phase's assumptions change. Confirming now (cheap — the BG is already 64×32, and existing 64×22 levels already scroll vertically a little) de-risks the milestone. The stub climbs with Featherleap only, so this phase has NO dependency on the Phase 1 glide work.

**After Phase 0:** review; record the confirmed max usable height (≤32) for Phase 3 authoring.

---

## Phase 1 — Pure logic: glide, updraft, wind + tiles

**Execution Status:** ✅ SHIPPED at `4a077e6` on 2026-06-08 (124/124 host, +7)

### Task 1.1 — Three non-solid wind `TileKind`s

**Files:** Modify `include/logic/tilemap.h`; Test `test/test_tilemap.cpp`.

- [ ] **Step 1 — failing test:**
```cpp
TEST(wind_tiles_are_not_solid){
  uint8_t c[] = { (uint8_t)TileKind::Updraft, (uint8_t)TileKind::WindLeft, (uint8_t)TileKind::WindRight };
  Tilemap m{3,1,c};
  CHECK(m.at(0,0)==TileKind::Updraft);
  CHECK(m.at(1,0)==TileKind::WindLeft);
  CHECK(m.at(2,0)==TileKind::WindRight);
  CHECK(!m.is_solid(0,0)); CHECK(!m.is_solid(1,0)); CHECK(!m.is_solid(2,0)); }   // all passable
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement:** extend the enum (keep existing values):
```cpp
enum class TileKind : uint8_t { Empty=0, Solid=1, OneWay=2, Lava=3, Water=4, IcePlatform=5,
                                Updraft=6, WindLeft=7, WindRight=8 };
```
Do NOT change `is_solid`/`is_water` (the new tiles are non-solid forces). 
- [ ] **Step 4 — run green. Step 5 — commit** `feat(logic): non-solid Updraft/WindLeft/WindRight tile kinds`.

### Task 1.2 — `wind.h` detection helpers

**Files:** Create `include/logic/wind.h`; Test `test/test_wind.cpp` (create).

- [ ] **Step 1 — failing test (`test_wind.cpp`):**
```cpp
#include "test_framework.h"
#include "logic/wind.h"
using namespace logic;
static Body at(int x,int y){ Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3);
  b.pos={Fixed::from_int(x),Fixed::from_int(y)}; return b; }
TEST(updraft_detected){
  uint8_t c[]={0,(uint8_t)TileKind::Updraft}; Tilemap m{2,1,c};
  CHECK(updraft_overlap(at(8,0),m)); CHECK(!updraft_overlap(at(0,0),m)); }
TEST(wind_direction_signed){
  uint8_t c[]={(uint8_t)TileKind::WindLeft,0,(uint8_t)TileKind::WindRight}; Tilemap m{3,1,c};
  CHECK_EQ(wind_dir(at(0,0),m), -1);   // over WindLeft -> push left
  CHECK_EQ(wind_dir(at(8,0),m),  0);   // empty
  CHECK_EQ(wind_dir(at(16,0),m), 1); } // over WindRight -> push right
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement (`wind.h`, AABB scan like `hazard.h`):**
```cpp
#pragma once
#include "logic/physics.h"
#include "logic/tilemap.h"
namespace logic {
inline bool updraft_overlap(const Body& b, const Tilemap& map){
    int l=Tilemap::px_to_tile(b.pos.x), r=Tilemap::px_to_tile(b.pos.x+b.half_w+b.half_w-Fixed::from_int(1));
    int t=Tilemap::px_to_tile(b.pos.y), bm=Tilemap::px_to_tile(b.pos.y+b.half_h+b.half_h-Fixed::from_int(1));
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx) if(map.at(tx,ty)==TileKind::Updraft) return true;
    return false;
}
// -1 if any overlapped tile is WindLeft, +1 if WindRight, 0 if neither (or both cancel).
inline int wind_dir(const Body& b, const Tilemap& map){
    int l=Tilemap::px_to_tile(b.pos.x), r=Tilemap::px_to_tile(b.pos.x+b.half_w+b.half_w-Fixed::from_int(1));
    int t=Tilemap::px_to_tile(b.pos.y), bm=Tilemap::px_to_tile(b.pos.y+b.half_h+b.half_h-Fixed::from_int(1));
    int d=0;
    for(int ty=t;ty<=bm;++ty) for(int tx=l;tx<=r;++tx){
        TileKind k=map.at(tx,ty);
        if(k==TileKind::WindLeft) d-=1; else if(k==TileKind::WindRight) d+=1;
    }
    return d<0 ? -1 : d>0 ? 1 : 0;
}
}
```
- [ ] **Step 4 — run green. Step 5 — commit** `feat(logic): wind.h updraft_overlap + wind_dir`.

### Task 1.3 — Glide / updraft / gust rules in `Player::update`

**Files:** Modify `include/logic/player.h` (Abilities + InputFrame), `src/logic/player.cpp`; Test `test/test_player.cpp`.

Context (`src/logic/player.cpp`): `update` does horizontal accel/friction (clamped to `RUN_MAX`), jump/double-jump, then `apply_gravity(body, PH)`, then `move_and_collide`, then refreshes `air_jumps_left`. Featherleap is an `Abilities` flag read in the jump block (`:25`) and the refresh (`:32`).

- [ ] **Step 1 — failing tests (`test_player.cpp`).** Use a tall enough air column so the body is airborne; integrate a few ticks. Examples:
```cpp
TEST(glide_caps_fall_speed_when_owned_and_held){
  uint8_t cells[3*10]={0}; for(int x=0;x<3;++x) cells[9*3+x]=1;     // floor row 9
  Tilemap m{3,10,cells};
  Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
  p.body.pos={Fixed::from_int(8),Fixed::from_int(0)};                // up high, airborne
  p.abilities.glide=true;
  InputFrame in; in.glide_held=true;
  for(int i=0;i<30;++i) p.update(in,m);
  CHECK(p.body.vel.y <= GLIDE_VY);            // never exceeds the gentle glide terminal
}
TEST(no_glide_without_ability_or_hold){
  static uint8_t cells[3*40]; for(int i=0;i<3*40;++i) cells[i]=0;    // TALL empty column (no floor reached)
  Tilemap m{3,40,cells};
  Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
  p.body.pos={Fixed::from_int(8),Fixed::from_int(0)};
  InputFrame in; in.glide_held=true;                                 // held but ability NOT owned
  for(int i=0;i<12;++i) p.update(in,m);                              // still airborne after 12 ticks
  CHECK(p.body.vel.y > GLIDE_VY);             // falls fast, no glide cap (ability absent)
}
TEST(updraft_rises_only_while_gliding){
  static uint8_t cells[3*10]; for(int i=0;i<3*10;++i) cells[i]=0;
  for(int y=0;y<10;++y) cells[y*3+1]=(uint8_t)TileKind::Updraft;  // updraft column at x=1 (px 8..15)
  Tilemap m{3,10,cells};
  // GLIDING in the updraft -> rises (vel.y goes negative)
  Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
  p.body.pos={Fixed::from_int(8),Fixed::from_int(40)}; p.abilities.glide=true;
  InputFrame g; g.glide_held=true;
  for(int i=0;i<8;++i) p.update(g,m);
  CHECK(p.body.vel.y < Fixed::from_int(0));                       // rising
  // SAME spot, NOT gliding (button released) -> falls through (vel.y positive)
  Player q; q.body.half_w=Fixed::from_int(3); q.body.half_h=Fixed::from_int(3);
  q.body.pos={Fixed::from_int(8),Fixed::from_int(40)}; q.abilities.glide=true;
  InputFrame ng; ng.glide_held=false;
  for(int i=0;i<8;++i) q.update(ng,m);
  CHECK(q.body.vel.y > Fixed::from_int(0));                       // falls through the updraft
}
TEST(wind_pushes_horizontally){
  uint8_t cells[3*4]={0}; for(int i=0;i<3*4;++i) cells[i]=(uint8_t)TileKind::WindRight;
  Tilemap m{3,4,cells};
  Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
  p.body.pos={Fixed::from_int(0),Fixed::from_int(0)};
  InputFrame in; p.update(in,m);
  CHECK(p.body.vel.x > Fixed::from_int(0)); }   // pushed right
```
(Adjust the exact tilemap fixtures to keep the body airborne where needed; do NOT assert against frame timing — assert the velocity invariant after N ticks, per TEST-1.)
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement.** In `player.h`:
```cpp
struct InputFrame { bool left=false, right=false, jump_pressed=false, fire_pressed=false, glide_held=false; };
struct Abilities { bool featherleap=false; bool glide=false; };
```
In `player.cpp`, add constants near the others and the wind-kit block **AFTER `apply_gravity`, BEFORE `move_and_collide`** (include `logic/wind.h`). **ORDER IS LOAD-BEARING — do NOT move this block before `apply_gravity` (gravity must set the fall speed first so glide can cap it) or after `move_and_collide` (the velocity must be final before collision resolves it):**
```cpp
static const Fixed GLIDE_VY   = Fixed::from_int(1);   // gentle fall terminal while gliding (vs PH terminal 6)
static const Fixed UPDRAFT_VY = Fixed::from_int(-2);  // rise speed in an updraft while gliding
static const Fixed WIND_ACCEL = Fixed::from_raw(24);  // per-frame sideways push from a gust (== RUN_ACCEL)
// ...
apply_gravity(body, PH);
if(abilities.glide && in.glide_held && !body.on_ground){
    if(updraft_overlap(body, map)) body.vel.y = UPDRAFT_VY;        // updraft OVERRIDES glide OVERRIDES gravity
    else if(body.vel.y > GLIDE_VY) body.vel.y = GLIDE_VY;          // otherwise glide (cap the fall)
}
body.vel.x = body.vel.x + WIND_ACCEL * wind_dir(body, map);        // gusts push regardless of ability
move_and_collide(body, map);
```
**No separate wind speed clamp is needed** (and none was added): the `RUN_MAX` clamp at the TOP of `update` re-bounds `vel.x` every frame, so the per-frame wind add cannot accumulate or "fling" the player — its steady-state effect is a drift toward the wind direction near `RUN_MAX`. If a *stronger* shove is wanted, that is a Phase 4 tuning decision (raise `WIND_ACCEL`, or temporarily widen the clamp while in wind) — logged as a Discovery, not done speculatively here.

**Do NOT** change the existing jump/featherleap/friction/`RUN_MAX`-clamp logic. The constants are starting values — mGBA tunes feel (Phase 4).

**Tap-vs-hold interplay (intentional, do NOT "fix"):** `jump_pressed` is edge-triggered (`a_pressed`), `glide_held` is level (`a_held`, Task 2.1). So a single tap of A jumps; *holding* A after the jump glides at the apex; tapping A again mid-air double-jumps (Featherleap). Holding A continuously does NOT double-jump (no second edge) — it glides. Gliding only caps the fall (`vel.y > GLIDE_VY`), so it has no effect during the jump's rise. Walking off a ledge while holding A glides immediately — intended.
- [ ] **Step 4 — run green. Step 5 — commit** `feat(logic): glide caps fall + updraft lift (gliding only) + gust push in Player::update`.

**After Phase 1:** review rounds; confirm `check_logic_purity.py` clean and the suite green (expect ~+6 tests).

---

## Phase 2 — Engine: glide input + wind/updraft tiles & art

**Execution Status:** ⬜ NOT STARTED

### Task 2.1 — Read A-held into `glide_held`

**Files:** Modify `src/engine/input.cpp`. (No host test — Butano layer; covered by the logic tests + mGBA.)

- [ ] **Step 1 — implement.** In `read_input()`, add `f.glide_held = bn::keypad::a_held();` (keep `jump_pressed = a_pressed()` — tap A jumps, hold A glides). 
- [ ] **Step 2 — commit** (build folded into 2.3) `feat(engine): read A-held into InputFrame.glide_held`.

### Task 2.2 — Wind/updraft bg art + kind→bg map

**Files:** Modify `tools/make_placeholder_art.py`, `src/engine/level_view.cpp`.

**PINNED bg indices (consumed by `level_view.cpp`): Updraft=20, WindLeft=21, WindRight=22** (widen the tile strip from 20→23 tiles).

- [ ] **Step 1 — art.** In `gen_tiles()`: widen the strip to `8*23`; add tile 20 **updraft** (upward cyan/white chevrons on a faint blue), tile 21 **wind-left** (leftward white streaks), tile 22 **wind-right** (rightward white streaks). Update the docstring + the `gates.h` tile-index comment.
- [ ] **Step 2 — kind→bg map.** In `level_view.cpp`, extend the chain: `(kind==6)?20:(kind==7)?21:(kind==8)?22:...` alongside the existing Lava/Water/IcePlatform cases.
- [ ] **Step 3 — regen + build** `python tools/make_placeholder_art.py && bash tools/build_rom.sh -j8` → `ROM fixed!` (engine compiles; the one-line scene change in Phase 3 is what makes glide active, but the build is green now since `glide_held` defaults false and `Abilities::glide` defaults false).
- [ ] **Step 4 — commit** `feat(engine+assets): updraft/wind bg tiles (20/21/22) + kind->bg map`.

**After Phase 2:** review; confirm bg indices match between art + `level_view.cpp`.

---

## Phase 3 — Dungeon 4 content + scene wiring + hub Door 4

**Execution Status:** ⬜ NOT STARTED

### Task 3.1 — Scene wires the Glide ability onto the player

**Files:** Modify `src/game/scene_dungeon.cpp`.

- [ ] **Step 1 — implement.** Next to the existing `player.abilities.featherleap = world.has(logic::Ability::Featherleap);` (in the main loop), add `player.abilities.glide = world.has(logic::Ability::Glide);`. (Optional MAY, skip for placeholder: tint the avatar while `in.glide_held && owns glide`.) This is the ONLY scene logic change — the wind-kit rules are all in `Player::update`.
- [ ] **Step 2 — build smoke-test** folded into 3.3.

### Task 3.2 — Compiler symbols for updraft + wind tiles

**Files:** Modify `tools/build_level.py`; Test `tools/test_build_level.py`.

- [ ] **Step 1 — failing test:** a grid with `u`/`<`/`>` emits tile values 6/7/8 respectively (mirror `test_water_is_tile_4`).
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement.** Add to `TILE`: `'u':6` (updraft), `'<':7` (wind-left), `'>':8` (wind-right). (These are terrain tiles, not content symbols — add to the `TILE` dict, NOT `CONTENT`. The chars `u`/`<`/`>` are unused — verify against the existing set `#.^~w@CEoGVIWXFB=?*1-8`.) Update the symbol legend/docstring. Note: these emit collision **TileKind values** 6/7/8; `level_view` (Task 2.2) maps those to bg **indices** 20/21/22 — different numbers, same tile (the M3/M4 collision-value-vs-bg-index distinction).
- [ ] **Step 4 — run green. Step 5 — commit** `feat(tools): level symbols u (updraft) + {/} (wind left/right)`.

### Task 3.3 — Replace the D4 stub with the real vertical level

The route + Door 4 + `dungeon4.h` plumbing already exist from Phase 0 — this task ONLY rewrites
`dungeon4.txt`/`.json` content and adds the level test. Do NOT re-edit `main.cpp` or `scene_hub.cpp`.

**Files:** Overwrite `tools/levels/dungeon4.{txt,json}` → recompile `include/game/levels/dungeon4.h`; Test `test/test_dungeon4_level.cpp` (create).

- [ ] **Step 1 — failing level test (`test_dungeon4_level.cpp`, mirror `test_dungeon3_level.cpp`):** concrete assertions — `DUNGEON4_DATA.h > 22 && h <= 32` (vertical); `w` matches the authored width; border solid; `pickup_count==1` and `pickups[0].ability==Ability::Glide`; ≥1 tile `==(uint8_t)TileKind::Updraft`; ≥1 tile `==WindLeft || ==WindRight`; ≥1 `gates[i].type==GateType::Vine` (the Fire beat); `has_cage && has_exit`; `spawn_ty > exit_ty` (spawn below exit — vertical ascent).
- [ ] **Step 2 — run red** (the stub has no shrine/wind tiles → fails).
- [ ] **Step 3 — author** the real vertical level (bottom→top), height per Phase 0's confirmed max (≤32): lower **Featherleap** climb (one-way ledges ≤4 rows apart) with a **vine gate** (`V`, Fire) and an enemy or two; **Glide shrine** (`F`, glide) mid-height; **updraft shafts** (`u` columns) + **gust zones** (`<`/`>`) to ascend the upper cliffs; **spronk** + **exit** at the top. JSON: enemy patrols, `pickups:[{"ability":"glide"}]`. **Honor soft-lock guards:** no mandatory updraft before the shrine; falls land on re-climbable ledges; the vine beat needs Fire (guaranteed — Door 4 requires D3→D2→Fire); no Glide shrine duplicate.
- [ ] **Step 4 — compile + green + build** `python tools/build_level.py tools/levels/dungeon4.txt include/game/levels/dungeon4.h`, `bash tools/host_test.sh`, `bash tools/build_rom.sh -j8` → `ROM fixed!`.
- [ ] **Step 5 — commit** `feat(content): Dungeon 4 (Gale Cliffs) real vertical level (glide/updraft/gust climb)`.

**After Phase 3:** review; confirm D4 is reachable only after D3 and the level test pins all required systems.

---

## Phase 4 — Verification + docs

**Execution Status:** ⬜ NOT STARTED

- [ ] **Task 4.1 — mGBA playthrough.** Reach D4 by playing through (or a save with D3 cleared + Glide-less). Verify: **vertical scroll** climbs cleanly; **Featherleap** lower climb; **vine gate** burns with Fire; **Glide shrine** grants Glide; **hold A** glides (slow fall + air control); **updraft shafts lift only while gliding** (dive through otherwise); **gusts push sideways**; the upper cliffs are impassable without Glide; **falls land on re-climbable ledges** (no soft-lock); spronk→exit clears; **Door 4 was locked until D3 cleared**; D4 saves on clear. **Tune** GLIDE_VY / UPDRAFT_VY / WIND_ACCEL / WIND_MAX and ledge spacing for feel; log fixes as Discoveries.
- [ ] **Task 4.2 — `docs/acceptance-m4.md`… → `docs/acceptance-m5.md`.** Map each design §11 criterion → status + evidence, mirroring `docs/acceptance-m4.md`. Note the host-test count + deviations/discoveries.
- [ ] **Task 4.3 — README + plan close-out.** README: add Glide to "What's playable" + controls (A held = glide) + the `u`/`{`/`}` symbols + new test count. Mark all phase banners ✅ with SHAs; fill the Execution Status table.
- [ ] **Task 4.4 — finish the branch.** Per the M1–M4 pattern: merge `m5-gale-cliffs` → `main`, push to origin (CONFIRM with the user first — publishing is outward-facing).

---

## Self-Review (author, before committing the plan)

- **Spec coverage:** Glide earned (3.3 shrine + 3.1 wiring); hold-A slow fall (1.3); updraft lifts only while gliding (1.3 + tests); gusts push (1.3); upper cliffs need Glide (3.3 authoring + 4.1 tuning); vine beat needs Fire (3.3 + invariant); vertical scroll (Phase 0); spronk/exit/Door-4 (3.3); purity + host tests (Phases 1, protocol). ✅
- **Type consistency:** `TileKind::Updraft/WindLeft/WindRight` (1.1) used by `wind.h` (1.2), `Player::update` (1.3), `level_view` (2.2), compiler (3.2); `InputFrame.glide_held`/`Abilities::glide` (1.3) used by `input.cpp` (2.1) + `scene_dungeon` (3.1); bg indices 20/21/22 pinned (2.2) and matched in art + level_view. ✅
- **Placeholder scan:** no TBDs; the level height is bounded by Phase 0's confirmed max (≤32), tuning constants have starting values.
- **Pitfalls:** IMPL-1 (purity each phase), IMPL-2 (fixed-point only), TEST-1/2 (deterministic ticks + raw-int asserts) in the protocol; the vertical-scroll risk is front-loaded to Phase 0.
- **Cross-task conflicts:** `scene_dungeon.cpp` touched only in 3.1 (one line); `level_view.cpp` only in 2.2; `tilemap.h` only in 1.1. No parallel-edit hazards.
