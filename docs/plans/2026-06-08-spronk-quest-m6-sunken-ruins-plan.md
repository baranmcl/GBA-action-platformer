# Spronk Quest — Milestone 6 (Sunken Ruins + Dash) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Dungeon 5 (Sunken Ruins) and **Blink (Dash)** — a double-tap i-frame air-dash that smashes cracked walls and blinks through hazards — Spronk Quest's first **combo dungeon** using the whole carried kit (Featherleap + Fire + Ice + Glide + Dash).

**Architecture:** Extends shipped M5. New **pure logic**: a `DashState` struct (double-tap detection from the existing `left`/`right` HELD flags via press-edge tracking; a dash burst that overrides horizontal velocity + zeroes vy; an i-frame window; one dash per ground-touch recharged on landing; gated by `Abilities::dash`), applied in `Player::update`; a `TileKind::Spikes` damaging hazard. New **content/art**: a spike bg tile + a cracked-wall bg tile (resolving a bg-index conflict — see Phase 1), compiler symbols, and the `dungeon5` level. `scene_dungeon` is extended **additively** (set `abilities.dash`; skip damage while invincible; smash a `CrackedWall` gate on a dashing-body overlap). The combo beats are **pure authoring** — Fire/Ice/Glide/Featherleap are all shipped. Hub Door 5 unlocks after D4. Reuses all M1–M5 engine (incl. the 64×128 big-map background + camera clamp).

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

**Overall:** Not started. Branch `m6-sunken-ruins` (design spec already committed there).

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 0 — Pure logic (DashState, Spikes tile, Player dash, hazard) | ✅ Shipped | `55d95e3`,`0a6e41e`,`e56f42b` | 142/142 host, +10 tests |
| 1 — Compiler symbols + art (resolve cracked-wall bg-index conflict) | ✅ Shipped | `b0a14a4`,`b6ef019` | ROM builds; 21 compiler tests |
| 2 — scene_dungeon integration (dash wiring, i-frames, cracked-wall smash) | ✅ Shipped | `3e1872b` | ROM builds clean |
| 3 — Dungeon 5 content + hub Door 5 | ✅ Shipped | `24f666d`,`889604f` | 149 host; mGBA pending (P4) |
| 4 — Verification + docs | ⬜ Not started | — | mGBA playthrough next |

### Deviations
- _(none yet)_

### Discoveries
- **bg-index conflict (verified against source, pre-execution):** `gates.h` `GATE_TABLE` assigns BOTH `CrackedWall` and `FireWall` `bg_tile = 10`, but tile 10's art (`make_placeholder_art.py` `gen_tiles`) is the fire-wall flames. The obstacle-gate art block is tiles **7–12** and is FULL: 7=vine, 8=ice-gate, 9=water-gate, 10=fire-wall, **11=reserved for M7 `CrackedFloor`/Stone, 12=reserved for M8 `DarkVeil`/Light** (per the `GATE_TABLE` rows + the `gen_tiles` docstring). The art strip is `new_img(8 * 23, 8)` — all 23 slots (0–22) are already accounted for. So **11/12 are NOT free** — reusing them would collide with M7/M8. **Resolution:** widen the strip to `8 * 25` and add two NEW tiles — **23 = cracked-wall** (`GATE_TABLE` CrackedWall `bg_tile` 10→**23**) and **24 = spikes** (`level_view` maps collision `Spikes(9)`→**24**). This mirrors how M5 widened 20→23 for wind tiles; it is the only conflict-free choice.
- **hub Door 5 absent (verified):** `tools/levels/hub.txt`'s door row contains only doors `1 2 3 4` (no `5`); `scene_hub.cpp` `door_enterable` handles only `n==1..4`; `main.cpp` routes only `n==1..4`. Phase 3 Task 3.2 MUST add the `5` door to `hub.txt` (replacing a `.`, keeping the row width 48), add the `door_enterable` + `main.cpp` clauses, and recompile `hub.h`.
- **`DashState::charged` defaults false (test-correctness):** an air dash needs a prior grounded frame to charge; the air-dash host tests prepend a grounded `tick` before double-tapping (see Task 0.2). In-game the player spawns grounded, so this is purely a test-setup concern.

---

## Standard Task Protocol (binds EVERY task)

Per `/writing-plans-enhanced` Step 3. Stated once; treat as pasted into each task.

**BEFORE starting any task:** (1) invoke `superpowers:test-driven-development`; (2) read `docs/pitfalls/testing-pitfalls.md` + `docs/pitfalls/implementation-pitfalls.md`; (3) TDD: failing host test → run red → minimal impl → run green.

**BEFORE marking complete:** review tests vs `docs/pitfalls/testing-pitfalls.md`; run `bash tools/host_test.sh` green; commit with a message stating what happened to assertions (add/strengthen/preserve).

**Project conventions (from M1–M5 — do NOT relearn the hard way):**
- `src/logic/**` + `include/logic/**`: **pure C++17, NO `bn::` tokens (even in comments)** — host-tested via `bash tools/host_test.sh`; `check_logic_purity.py` fails on `bn::` (IMPL-1).
- `int32_t` is **`long` on arm-none-eabi**: assign a `logic::Fixed::to_int()` to an `int` local before passing it to a Butano API.
- `logic::Fixed` defaults to 0; **`Fixed` has NO `operator*(int)`** (the M5 wind code hit this — use sign branches: `if(dir>0) v=X; else v=-X;`). No `float`/`double` (IMPL-2). Fixed-point tests assert exact raw ints (TEST-2); logic tests use explicit ticks (TEST-1).
- ROM build: `bash tools/build_rom.sh` (regenerates level headers + art via Windows Python, builds via devkitPro MSYS2). Butano forbids heap. **Close mGBA before rebuilding** — a running mGBA locks `SpronkQuest.gba` and the build fails with objcopy "Invalid argument".
- Scenes own their fades; dungeons use the live fade-in. The dungeon BG is a **64×128 big map** (large static buffers live in EWRAM via `BN_DATA_EWRAM_BSS`).

**Assertion rigor:** logic here is deterministic (explicit ticks) — if a test seems to flake, the bug is real; fix the logic, never weaken the assertion. STOP and escalate if you cannot.

**After each Phase:** 3+ review rounds (ambiguity / context gaps / drift / cross-task conflicts / pitfalls); continue until a round is clean.

---

## File Structure

**New pure logic (`include/logic/`):**
- `tilemap.h` (MODIFY) — `TileKind::Spikes=9` (after `WindRight=8`); non-solid; `is_spikes()`.
- `hazard.h` (MODIFY) — `water_overlap`-pattern `spikes_overlap`; add Spikes to `hazard_overlap` (lava ∪ water ∪ spikes).
- `player.h` (MODIFY) — `Abilities::dash` (bool); a `DashState dash;` member on `Player` (or include `dash.h`).
- `dash.h` (CREATE) — `struct DashState` (double-tap detection + dash timer + i-frames + recharge-on-land).
- `player.cpp` (MODIFY) — tick the dash; apply the dash (override horizontal vel + zero vy while active, AFTER the wind kit); recharge on landing.
- `gates.h` (MODIFY) — change `CrackedWall` `bg_tile` 10 → **23** (resolve the conflict with FireWall's tile-10 flame art; 11/12 are reserved for M7/M8).

**New engine / art:**
- `make_placeholder_art.py` (MODIFY) — widen the strip from `8*23` to `8*25`; add cracked-wall art at tile **23**, spike art at tile **24**.
- `level_view.cpp` (MODIFY) — kind→bg map: `Spikes(9) → 24`.

**Scene (additive):** `scene_dungeon.cpp` — `abilities.dash` wiring; skip hazard/contact damage while `player.dash.invincible()`; smash a `CrackedWall` gate when `player.dash.active()` + the player body overlaps the gate body.

**New content:** `tools/build_level.py` (spike tile + CrackedWall gate symbols), `tools/levels/dungeon5.{txt,json}`, `src/main.cpp` (route n==5), `scene_hub.cpp` (Door 5).

**Tests (`test/`):** `test_dash.cpp` + `test_dungeon5_level.cpp` (new); extend `test_tilemap.cpp`, `test_hazard.cpp`, `test_player.cpp`, `test_build_level.py`.

---

## Phase 0 — Pure logic: Dash + Spikes

**Execution Status:** ✅ SHIPPED on 2026-06-08 (branch `m6-sunken-ruins`): `55d95e3` (Spikes+hazard), `0a6e41e` (DashState), `e56f42b` (Player dash). 142/142 host tests, purity clean (+10 tests: tilemap 1, hazard 1, dash 6, player 2).

### Task 0.1 — `TileKind::Spikes` + hazard

**Files:** Modify `include/logic/tilemap.h`, `include/logic/hazard.h`; Test `test/test_tilemap.cpp`, `test/test_hazard.cpp`.

- [ ] **Step 1 — failing tests.** In `test_tilemap.cpp`:
```cpp
TEST(spikes_kind){
  uint8_t c[]={0,(uint8_t)TileKind::Spikes}; Tilemap m{2,1,c};
  CHECK(m.is_spikes(1,0)); CHECK(!m.is_solid(1,0)); CHECK(!m.is_spikes(0,0)); }
```
In `test_hazard.cpp`:
```cpp
TEST(body_over_spikes_detected){
  uint8_t c[]={0,0,(uint8_t)TileKind::Spikes,(uint8_t)TileKind::Spikes}; Tilemap m{2,2,c};
  Body b{}; b.half_w=Fixed::from_int(3); b.half_h=Fixed::from_int(3); b.pos={Fixed::from_int(2),Fixed::from_int(8)};
  CHECK(spikes_overlap(b,m)); CHECK(hazard_overlap(b,m)); }
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement.** `tilemap.h`: `enum class TileKind : uint8_t { ..., Updraft=6, WindLeft=7, WindRight=8, Spikes=9 };` + `bool is_spikes(int tx,int ty) const { return at(tx,ty)==TileKind::Spikes; }`. **Do NOT** make Spikes solid. `hazard.h`: clone `water_overlap` as `spikes_overlap` (`is_spikes`); add it to `hazard_overlap` (`return lava_overlap || water_overlap || spikes_overlap`).
- [ ] **Step 4 — run green. Step 5 — commit** `feat(logic): TileKind::Spikes (dash-through hazard) + spikes_overlap`.

### Task 0.2 — `DashState` (double-tap detection + i-frames)

**Files:** Create `include/logic/dash.h`; Test `test/test_dash.cpp` (create).

DashState is fed `(left_held, right_held, on_ground, has_dash)` once per frame and triggers a dash on a **double-tap** of a direction (press → release → press, detected via press-edges of the held flags).

- [ ] **Step 1 — failing tests (`test_dash.cpp`):**
```cpp
#include "test_framework.h"
#include "logic/dash.h"
using namespace logic;
// helper: run the 3-frame double-tap (press, release, press) for `right`
static void dtap_right(DashState& d, bool on_ground, bool has){
  d.tick(false,true,on_ground,has);   // press
  d.tick(false,false,on_ground,has);  // release
  d.tick(false,true,on_ground,has);   // press again -> dash
}
TEST(dash_on_double_tap_when_owned){
  DashState d; dtap_right(d,true,true);
  CHECK(d.active()); CHECK_EQ(d.dir(),1); CHECK(d.invincible()); }
TEST(no_dash_without_ability){
  DashState d; dtap_right(d,true,false); CHECK(!d.active()); }
TEST(single_tap_does_not_dash){
  DashState d; d.tick(false,true,true,true); CHECK(!d.active()); }
TEST(too_slow_double_tap_does_not_dash){
  DashState d;                                  // second tap arrives after the WINDOW expires -> re-arm, no dash
  d.tick(false,true,true,true);                 // press (arm, timer=WINDOW)
  for(int i=0;i<DashState::WINDOW;++i) d.tick(false,false,true,true); // hold release past the window
  d.tick(false,true,true,true);                 // press again, but timer already 0
  CHECK(!d.active()); }
TEST(air_dash_consumes_charge_until_landing){
  DashState d;
  d.tick(false,false,true,true);   // grounded once -> charged (charged defaults FALSE, so this is required)
  // double-tap in the AIR -> dash, charge spent
  d.tick(false,true,false,true); d.tick(false,false,false,true); d.tick(false,true,false,true);
  CHECK(d.active());
  for(int i=0;i<DashState::DASH_FRAMES;++i) d.tick(false,false,false,true);  // dash ends, still airborne
  CHECK(!d.active());
  // second air double-tap with NO landing -> no dash (charge not refreshed)
  d.tick(false,true,false,true); d.tick(false,false,false,true); d.tick(false,true,false,true);
  CHECK(!d.active()); }
TEST(dash_recharges_on_ground){
  DashState d;
  d.tick(false,false,true,true);   // grounded once -> charged
  d.tick(false,true,false,true); d.tick(false,false,false,true); d.tick(false,true,false,true); // air dash
  for(int i=0;i<DashState::DASH_FRAMES;++i) d.tick(false,false,true,true);   // land (on_ground) -> recharge
  d.tick(false,true,true,true); d.tick(false,false,true,true); d.tick(false,true,true,true);     // dash again
  CHECK(d.active()); }
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement (`dash.h`):**
```cpp
#pragma once
namespace logic {
struct DashState {
    static constexpr int WINDOW = 12;     // frames to complete a double-tap (feel-tunable)
    static constexpr int DASH_FRAMES = 7; // dash duration (~5 tiles at DASH_VX) (feel-tunable)
    int dash_frames = 0, dash_dir = 0;
    bool charged = false;             // one dash per airtime; refreshed while grounded
    int tap_dir = 0, tap_timer = 0;   // pending first-tap direction + window
    bool pl = false, pr = false;      // previous left/right held

    void tick(bool left, bool right, bool on_ground, bool has_dash){
        if(on_ground) charged = true;             // recharge while grounded (ground dashes repeatable; one air-dash/airtime)
        if(dash_frames > 0) --dash_frames;
        if(tap_timer > 0) --tap_timer;
        bool press_l = left && !pl, press_r = right && !pr;
        if(press_l) arm_or_dash(-1, has_dash);
        if(press_r) arm_or_dash(1, has_dash);
        pl = left; pr = right;
    }
    bool active() const { return dash_frames > 0; }
    bool invincible() const { return dash_frames > 0; }
    int dir() const { return dash_dir; }
private:
    void arm_or_dash(int d, bool has_dash){
        if(tap_dir == d && tap_timer > 0 && has_dash && charged && dash_frames == 0){
            dash_frames = DASH_FRAMES; dash_dir = d; charged = false; tap_timer = 0;
        } else { tap_dir = d; tap_timer = WINDOW; }
    }
};
}
```
- [ ] **Step 4 — run green. Step 5 — commit** `feat(logic): DashState (double-tap dash, i-frames, recharge-on-airtime)`.

### Task 0.3 — Apply the dash in `Player::update`

**Files:** Modify `include/logic/player.h` (`Abilities::dash` + `DashState dash;`), `src/logic/player.cpp`; Test `test/test_player.cpp`.

Context (`player.cpp`): `update` does accel/friction(+slippery-ice)+`RUN_MAX` clamp, jump, `apply_gravity`, the wind kit (glide/updraft/`wind_dir`), `move_and_collide`, then the air-jump refresh. The dash overrides BOTH axes while active — and must be applied AFTER the wind kit so a gust can't add to the dash velocity (the wind kit does `body.vel.x = body.vel.x + WIND_ACCEL`).

- [ ] **Step 1 — failing test (`test_player.cpp`).** NOTE: `DASH_VX` and `RUN_MAX` are file-local `static const` in `player.cpp` — NOT visible to the test. Assert the observable effect instead: vel.x exceeds the run cap (`RUN_MAX` is `Fixed::from_int(2)`), vy is zeroed, and the dash is active.
```cpp
TEST(dash_sets_horizontal_blink){
  static uint8_t c[20*3]; for(int i=0;i<20*3;++i) c[i]=0; for(int x=0;x<20;++x) c[2*20+x]=1; // floor row 2
  Tilemap m{20,3,c};
  Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
  p.body.pos={Fixed::from_int(8),Fixed::from_int(8)}; p.abilities.dash=true;
  InputFrame settle{}; for(int i=0;i<30;++i) p.update(settle,m);    // settle to the floor (>>enough frames to ground regardless of gravity tuning) -> grounds + charges the dash
  InputFrame R{}; R.right=true; InputFrame none{};
  p.update(R,m); p.update(none,m); p.update(R,m);                   // double-tap right -> dash
  CHECK(p.dash.active());
  CHECK(p.body.vel.x > Fixed::from_int(2));        // exceeds the run cap (RUN_MAX=2) -> dash override happened
  CHECK(p.body.vel.y == Fixed::from_int(0)); }     // horizontal blink: vy zeroed
// NOTE: deliberately do NOT pin the exact DASH_VX value — Phase 4 tunes it in mGBA; assert the
// observable contract (exceeds run cap + vy zeroed + active), which survives retuning.
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement.** `player.h`: `struct Abilities { bool featherleap=false; bool glide=false; bool dash=false; };`; add `#include "logic/dash.h"` and a `DashState dash;` member to `Player`. In `player.cpp` add `static const Fixed DASH_VX = Fixed::from_int(6);`. Apply the dash in ONE block AFTER the wind kit, immediately before `move_and_collide` (do NOT split it across gravity/wind — that lets a gust add to the dash):
```cpp
// ... existing accel/friction/RUN_MAX clamp, jump block, apply_gravity, wind kit (unchanged) ...
    // --- M6 dash: a horizontal i-frame blink; overrides accel/friction/gravity/wind while active ---
    dash.tick(in.left, in.right, body.on_ground, abilities.dash);
    if(dash.active()){
        body.vel.x = (dash.dir() > 0) ? DASH_VX : Fixed::from_raw(-DASH_VX.raw); // no Fixed::operator*(int): sign-branch
        body.vel.y = Fixed::from_int(0);
    }
    move_and_collide(body, map);
// ... existing air-jump refresh (unchanged) ...
```
**Do NOT** change the accel/friction/jump/wind/glide logic. `Fixed` has no `operator*` for ints — use the sign branch shown. The recharge is handled inside `dash.tick` (`if(on_ground) charged=true`); do NOT add a separate recharge line.
- [ ] **Step 4 — run green. Step 5 — commit** `feat(logic): apply DashState in Player::update (horizontal blink + i-frame seam)`.

**After Phase 0:** review rounds; `check_logic_purity.py` clean; suite green (expect ~+9 tests: tilemap 1, hazard 1, dash 6, player 1).

---

## Phase 1 — Compiler symbols + art (resolve the bg-index conflict)

**Execution Status:** ✅ SHIPPED on 2026-06-09 (branch `m6-sunken-ruins`): `b0a14a4` (art: strip→25, cracked-wall 23, spikes 24, CrackedWall bg 10→23, level_view 9→24), `b6ef019` (compiler symbols s/K). ROM builds clean (`ROM fixed!`); 21 compiler tests pass.

### Task 1.1 — Fix the CrackedWall bg-tile conflict + add art

**Files:** Modify `include/logic/gates.h`, `tools/make_placeholder_art.py`, `src/engine/level_view.cpp`; Test `test/test_gates.cpp` (optional bg-tile assertion).

**PINNED bg indices:** CrackedWall gate → tile **23**; Spikes tile → bg **24**. (Tiles 7–12 are FULL — 11/12 are reserved for M7 `CrackedFloor` / M8 `DarkVeil`; do NOT reuse them. The art strip widens from 23 to 25 tiles, mirroring M5's 20→23 widen for wind.)

- [ ] **Step 1 — gates.h:** change the `CrackedWall` row in `GATE_TABLE` from `{ Ability::Dash, false, 10 }` to `{ Ability::Dash, false, 23 }` (tile 10 is FireWall's flame art; 11/12 are reserved for M7/M8; CrackedWall gets a fresh slot 23). Update the tile-index comment in gates.h (add: `23 cracked-wall(M6), 24 spikes(M6)`).
- [ ] **Step 2 — art (`make_placeholder_art.py` `gen_tiles`):** change `im = new_img(8 * 23, 8)` to `im = new_img(8 * 25, 8)`. Draw **tile 23 = cracked wall** (grey stone `12` fill + dark `1` crack lines, e.g. an `ox = 8 * 23` block) and **tile 24 = spikes** (dark `1` base row + red/grey `13`/`12` upward triangles, `ox = 8 * 24`). Update the `gen_tiles` docstring (it currently ends at "22 wind-right"; add 23/24).
- [ ] **Step 3 — level_view kind→bg:** in `build_level_view`, extend the ternary chain to map Spikes: add `: (kind == 9) ? 24` before the final `: kind`. (Current chain ends `...(kind == 8) ? 22 : kind`.)
- [ ] **Step 4 — regen + build smoke-test** `python tools/make_placeholder_art.py && bash tools/build_rom.sh -j8` → `ROM fixed!` (close mGBA first).
- [ ] **Step 5 — commit** `feat(assets): cracked-wall (tile 23) + spike (tile 24) art; fix CrackedWall bg-index conflict (widen strip to 25)`.

### Task 1.2 — Compiler symbols: spike tile + CrackedWall gate

**Files:** Modify `tools/build_level.py`; Test `test/test_build_level.py`.

- [ ] **Step 1 — failing test (`test_build_level.py`, mirror `test_water_gate` + `test_wind_tiles`).** Borders MUST be solid `#` (the compiler enforces a solid border), so place the spike/gate in the interior:
```python
def test_spike_is_tile_9(self):
    txt = "#####\n#@s.#\n#####\n"
    lvl = compile_str(txt, {})
    self.assertEqual(lvl['tiles'][lvl['w'] * 1 + 2], 9)  # 's' at (2,1) -> spikes

def test_cracked_wall_gate(self):
    txt = "#####\n#@K.#\n#####\n"
    lvl = compile_str(txt, {})
    self.assertEqual(lvl['gates'], [(2, 1, 'CrackedWall')])
```
- [ ] **Step 2 — run red.**
- [ ] **Step 3 — implement.** In `tools/build_level.py`: add `'s': 9` to the `TILE` dict (next to `'w': 4`); add `'K'` to `CONTENT` (`set('@CEoG12345678VIWXFBK=?*')`) and a dedicated elif mirroring `V`/`I`/`W`/`X`: `elif c == 'K': gates.append((x, y, 'CrackedWall'))`. (`'s'` and `'K'` are both confirmed unused vs the existing `TILE` keys + `CONTENT`. A `G`+JSON `{"type":"cracked_wall"}` already works via `GATE_ENUM`, but a dedicated `K` symbol avoids the JSON scan-order pitfall and matches the V/I/W/X precedent.) Update the grid-symbols docstring (add `s=spikes(9)` and `K=cracked-wall gate`). Spike `'s'` emits collision **TileKind value 9**; `level_view` maps that to bg **24** (Task 1.1).
- [ ] **Step 4 — run green** (`python -m pytest tools/test_build_level.py` or the repo's runner). **Step 5 — commit** `feat(tools): level symbols s (spikes) + K (cracked-wall gate)`.

**After Phase 1:** review; confirm CrackedWall renders as cracked stone (bg 23), spikes as spikes (bg 24), no fire-wall confusion.

---

## Phase 2 — scene_dungeon integration (additive)

**Execution Status:** ✅ SHIPPED on 2026-06-09 (branch `m6-sunken-ruins`): `3e1872b` (dash wiring + i-frame skip on both damage sites + cracked-wall smash in the gate loop). ROM builds clean.

**Files:** Modify `src/game/scene_dungeon.cpp` only. Additive; no host test (Butano-bound; verified by build + mGBA).

- [ ] **Step 1 — wire the dash ability.** Right after the existing `player.abilities.featherleap = world.has(...)` / `player.abilities.glide = world.has(...)` lines and BEFORE `player.update(in, lvl.map);`, add: `player.abilities.dash = world.has(logic::Ability::Dash);`.
- [ ] **Step 2 — i-frame damage skip.** Two damage sites; both currently gate on `invuln == 0`. Add `&& !player.dash.invincible()` to each:
  - Enemy contact (in the enemy loop): `else if(invuln == 0 && logic::aabb_overlap(player.body, inst.e.body)){ health.damage(20); invuln = 45; }` → add `&& !player.dash.invincible()` after `invuln == 0`.
  - Hazard: `if(invuln == 0 && logic::hazard_overlap(player.body, lvl.map)){ health.damage(20); invuln = 45; }` → add `&& !player.dash.invincible()` after `invuln == 0`.
  Do NOT remove the existing `invuln == 0` check (i-frames still apply post-hit).
- [ ] **Step 3 — cracked-wall smash.** The gate loop currently reads `for(GateInst& gi : gates){ logic::SpellId clears = logic::gate_cleared_by(gi.spawn.type); if(!gi.open && clears != logic::SpellId::None && spells.consume_hit(gi.body, clears)){ gi.open = true; open_column(lvl.view, gi.spawn.tx, level.h); } }`. INSIDE that same loop, after the spell-clear `if`, add the dash-break (CrackedWall returns `None` from `gate_cleared_by`, so the spell branch never fires for it — this is its only opener):
```cpp
// M6: cracked walls aren't spell-cleared; a dashing body smashes them on contact.
if(!gi.open && gi.spawn.type == logic::GateType::CrackedWall
   && player.dash.active() && logic::aabb_overlap(player.body, gi.body)){
    gi.open = true;
    open_column(lvl.view, gi.spawn.tx, level.h);
}
```
  Field names are verified against the real scene: `GateInst{ logic::GateSpawn spawn; logic::Body body; bool open; }`; `gi.spawn.type`, `gi.body`, `gi.spawn.tx`, `open_column(lvl.view, tx, level.h)`, `logic::aabb_overlap` are all already in use in this file. The CrackedWall gate is set up as a filled full-height column at init (it is non-geometry → `fill_column`), so the dashing player's AABB overlaps `gi.body` when stopped against it; `open_column` then clears the 2-wide column and the dash carries through.
- [ ] **Step 4 — build smoke-test** `bash tools/build_rom.sh -j8` → `ROM fixed!` (close mGBA first). Commit: `feat(game): dash ability wiring + i-frame damage skip + cracked-wall smash (M6 Phase 2)`.

**After Phase 2:** review; confirm the CrackedWall smash only triggers while dashing + overlapping, and the i-frame skip doesn't break normal damage.

---

## Phase 3 — Dungeon 5 content + hub Door 5

**Execution Status:** ✅ SHIPPED on 2026-06-09 (branch `m6-sunken-ruins`): `24f666d` (D5 64×24 authored as data, +7 host tests → 149), `889604f` (route n==5 + hub Door 5). ROM builds clean. **mGBA playthrough pending (Phase 4).**

### Task 3.1 — Author Dungeon 5 (combo flooded temple)

**Files:** Create `tools/levels/dungeon5.{txt,json}` → `include/game/levels/dungeon5.h`; Test `test/test_dungeon5_level.cpp`.

- [ ] **Step 1 — failing level test (`test_dungeon5_level.cpp`, mirror `test_dungeon3_level.cpp`'s style — `using`-free `Ability::`/`GateType::`/`TileKind::`, `CHECK_EQ`/`CHECK`):** pin the **exact authored `w` and `h`** (e.g. `CHECK_EQ(DUNGEON5_DATA.w, 64); CHECK_EQ(DUNGEON5_DATA.h, 28);` — `w` MUST be ≤64, `h` ≤128, the big-map limits); solid top/bottom border (loop `tiles[x]==1` and `tiles[(h-1)*w+x]==1` like d3); `pickup_count==1` and `pickups[0].ability==Ability::Dash`; ≥1 tile `==(uint8_t)TileKind::Spikes`; ≥1 `gates[i].type==GateType::CrackedWall`; ≥1 carried-power obstacle (scan `gates[i].type` for `GateType::Vine`/`Ice`/`Water`/`FireWall`, OR `brazier_count>=1`, OR a `tiles[i]` of Updraft(6)/WindLeft(7)/WindRight(8)) proving the combo theme; `has_cage && has_exit`; `enemy_count>=1`.
- [ ] **Step 2 — author** the horizontal flooded temple (≤64×≤128): first half uses the **carried kit** (a Featherleap climb, a Fire vine/brazier, an Ice freeze, a Glide gap/updraft) → **Dash shrine** (`F`, dash) → second half **dash beats** (spike corridors `s` ≤5 tiles wide, cracked-wall gates `K`, air-dash gaps) + ≥1 **combo beat** (freeze+dash / air-dash+glide / burn+smash / updraft+dash) → spronk → exit. JSON wires enemy patrols, `pickups:[{"ability":"dash"}]`, gate/trigger targets. **Honor soft-lock guards:** Dash shrine mandatory before any dash-only obstacle; a magic-source enemy before each Fire/Ice beat; spike corridors ≤ one dash (~5 tiles); no dash-gate or spike corridor before the shrine; the cage sits 2 rows above its platform (M5 spronk-placement lesson); **each cracked wall is approachable from a ground runway** (the dash recharges on the ground and ground-dashes are repeatable, so a grounded approach is always re-attemptable — do NOT gate a cracked wall behind a spot the player can only reach mid-air with a spent air-dash).
- [ ] **Step 3 — compile + green + build** `python tools/build_level.py tools/levels/dungeon5.txt include/game/levels/dungeon5.h`, `bash tools/host_test.sh`, `bash tools/build_rom.sh -j8` → `ROM fixed!`.
- [ ] **Step 4 — commit** `feat(content): Dungeon 5 (Sunken Ruins) combo temple authored as data`.

### Task 3.2 — Route D5 + hub Door 5

**Files:** Modify `src/main.cpp`, `src/game/scene_hub.cpp`, `tools/levels/hub.txt` → recompile `include/game/levels/hub.h`.

**Confirmed pre-state:** `hub.txt`'s door row is `#...1...................G.....2...3...4........#` — doors `1 2 3 4` only, NO `5`. `main.cpp` routes `n==1..4`. `door_enterable` handles `n==1..4`. All three need a D5 entry.

- [ ] **Step 1 — add the hub door.** In `tools/levels/hub.txt`, on the door row, REPLACE one `.` (in the run of dots after the `4`) with `5`, keeping the row **exactly 48 chars wide** and leaving ≥2 dots of spacing around the `5` so the player can stand on it. E.g. `#...1...................G.....2...3...4...5....#` (verify the width is still 48 — replace, never insert).
- [ ] **Step 2 — recompile the hub header** `python tools/build_level.py tools/levels/hub.txt include/game/levels/hub.h` (the door becomes a `DoorSpawn{tx,ty,5}` in `HUB_DOORS`).
- [ ] **Step 3 — route + unlock.** `main.cpp`: add `#include "game/levels/dungeon5.h"` (next to the other dungeon includes) and `else if(n == 5) lvl = &DUNGEON5_DATA;` (after the `n == 4` branch). `scene_hub.cpp` `door_enterable`: add `|| (n == 5 && w.spronk_freed(4))` to the return chain. (The existing chain is `(n == 2 && w.spronk_freed(1)) || (n == 3 && w.spronk_freed(2)) || (n == 4 && w.spronk_freed(3))` — i.e. door N opens when dungeon N−1's spronk is freed, and `scene_dungeon` frees spronk index = `world.current_dungeon`. So door 5 follows the pattern: `(n == 5 && w.spronk_freed(4))`.)
- [ ] **Step 4 — build smoke-test** `bash tools/build_rom.sh -j8` → `ROM fixed!`. Commit: `feat(game): route Dungeon 5 + hub Door 5 unlock after D4`.

**After Phase 3:** review; confirm D5 reachable only after D4 and the level test pins the dash + spikes + cracked-wall + a carried-power obstacle.

---

## Phase 4 — Verification + docs

**Execution Status:** ⬜ NOT STARTED

- [ ] **Task 4.1 — mGBA playthrough.** Reach D5 via a save with D4 cleared (or play through). Verify: **double-tap dashes** (~5 tiles, air + ground, one air-dash per airtime); **i-frames blink through** a spike corridor unharmed; **cracked walls smash** on a dash; **air-dash gaps** cross; the **combo beat(s)** work; the **first half** uses the carried kit; Dash shrine grants Dash; spronk→exit clears; **Door 5 was locked until D4 cleared**; D5 saves on clear. Tune `DASH_VX`/`DASH_FRAMES`/`WINDOW` + corridor widths for feel; log fixes as Discoveries.
- [ ] **Task 4.2 — `docs/acceptance-m6.md`.** Map each design §10 criterion → status + evidence, mirroring `docs/acceptance-m5.md`. Note the host-test count + deviations/discoveries.
- [ ] **Task 4.3 — README + plan close-out.** README: add Dash to "What's playable" + controls (double-tap to dash) + the `s`/`K` symbols + new test count. Mark all phase banners ✅ with SHAs; fill the Execution Status table.
- [ ] **Task 4.4 — finish the branch.** Per the M1–M5 pattern: merge `m6-sunken-ruins` → `main`, push to origin (CONFIRM with the user first — publishing is outward-facing).

---

## Self-Review (author, before committing the plan)

- **Spec coverage:** Dash earned (3.1 shrine + 2.1 wiring); double-tap blink (0.2+0.3); air+ground+recharge (0.2 tests); i-frames blink-through (0.2 + 2.2); cracked-wall smash (2.3); spike hazard (0.1); combo beat (3.1); spronk/exit/Door-5 (3.1/3.2); purity+host tests (Phase 0, protocol). ✅
- **Type consistency:** `TileKind::Spikes=9` (0.1) used by `hazard.h`/`level_view`/compiler; `DashState` (0.2) used in `Player` (0.3) + scene (2.x); `Abilities::dash` (0.3) wired (2.1); CrackedWall bg 10→**23** + Spikes bg **24** pinned (1.1), strip widened to 25; `DASH_VX` sign-branch (no `Fixed::operator*`); scene field names (`gi.spawn.type`/`gi.body`/`gi.spawn.tx`/`open_column`/`aabb_overlap`) verified against the real `scene_dungeon.cpp`. ✅
- **Placeholder scan:** bg indices pinned (23/24); dash constants have starting values; level dims chosen in 3.1; hub Door 5 add is concrete (replace a `.` with `5`, recompile). No TBDs.
- **Pitfalls:** IMPL-1 (purity each phase), IMPL-2 (fixed-point only, `Fixed` no `operator*`), TEST-1/2 (deterministic ticks, raw-int asserts), the mGBA-file-lock + the bg-index conflict all called out.
- **Cross-task conflicts:** `scene_dungeon.cpp` touched only in Phase 2; `gates.h` only in 1.1; `player.cpp` only in 0.3. No parallel-edit hazards.
