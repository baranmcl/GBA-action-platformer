# M8 — Quaking Quarry (Dungeon 7) + Stone Ground-Pound Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Dungeon 7 (Quaking Quarry) and the **Stone ground-pound** — a Down+A airborne slam that smashes cracked floors to descend, trips heavy switches, crushes armored enemies, breaks boulders, and shakes loose platforms — as a vertical-descent multi-room dungeon on the room-to-room framework, and wire hub Door 7.

**Architecture:** The pound is a pure `StoneState` in `Player::update` (mirrors `DashState`/`GrappleState`, fully host-tested). The five interactions are **scene-resolved** at the pound's `just_landed` impact point against level entities — exactly how the scene already resolves Dash→CrackedWall (`scene_dungeon.cpp:424-430`), spell→gate, bolt→enemy. Stone joins the `Abilities` bitmask (bit 6, already in the enum); shortcut + optional heart container ride the existing `latches`. NO SaveData format change.

**Tech Stack:** C++17 (three-layer: pure `logic/`, Butano `engine/`, scenes `game/`), Butano 21.6.0 (GBA), Python 3 level compiler (`tools/build_level.py`), host unit tests (`bash tools/host_test.sh`), ROM build (`bash tools/build_rom.sh`).

**Design spec:** `docs/superpowers/specs/2026-06-13-spronk-quest-m8-quaking-quarry-design.md`

**Branch:** create `feat/m8-quaking-quarry` off `main` before Task 1.1.

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

**Overall:** Not started.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure logic: StoneState + input/ability/data | ⬜ Not started | — | — |
| 2 — Level compiler: cracked-floor/boulder/heavy-switch/loose-platform symbols | ✅ Shipped | 54a4554 (k gate), 612cac9 (heavy plate), a1925bd (boulder+loose-platform), 10ce2d3 (header regen) | — |
| 3 — Engine/scene wiring (pound input, impact resolution, falling terrain last, VFX) | ⬜ Not started | — | — |
| 4 — Content: Quaking Quarry 3 rooms + hub Door 7 + no-soft-lock invariants + QA | ⬜ Not started | — | — |

### Deviations
- _(none yet)_

### Discoveries
- _(none yet)_

---

## Conventions every task must follow

- **Three-layer rule** (`CLAUDE.md`): nothing under `include/logic/` or `src/logic/` may include or name a `bn::` type. Phases 1–2 stay host-testable; Phases 3–4 are the only `bn::` work.
- **Host tests:** `bash tools/host_test.sh` (NOT bare `make -C test`). A green run ends `N/N tests passed, 0 checks failed`. The script also runs `tools/check_logic_purity.py`, the Python compiler tests, and regenerates level headers before compiling.
- **ROM build (this Windows machine):** `bash tools/build_rom.sh` (plain `make` fails — `DEVKITPRO` misconfigured). A clean build prints `ROM fixed!`.
- **Purity guard** before any commit touching logic: `python tools/check_logic_purity.py`.
- **Pitfalls:** read `docs/pitfalls/implementation-pitfalls.md` and `docs/pitfalls/testing-pitfalls.md` before each phase. Relevant traps: IMPL-2 (no float — integer fixed-point only; the pound velocity MUST be `Fixed`), IMPL-1 (no `bn::` in logic), TEST-2 (assert exact raw fixed values, e.g. `CHECK_EQ(b.vel.y.raw, POUND_VY.raw)`), TEST-1 (explicit per-tick stepping, no frame timing).
- **Room/camera constraint** (room-to-room): every authored room MUST be **≥30 wide × ≥20 tall**, solid border, standard ~22-tall floor (content row 18, floor row 20, gap row 19, bottom border row 21). Two-way doors with co-located return entrances (the D6 lesson). Sprites (cage/shrine/enemy/heart) sit on the **content row 18** so they ground; tile symbols (gates, anchors, doors) may sit on ledges.
- **TDD mandate (every code task):**
  ```
  BEFORE starting work:
  1. Invoke superpowers:test-driven-development
  2. Read docs/pitfalls/testing-pitfalls.md
  Follow TDD: write failing test → implement → verify green.

  BEFORE marking this task complete:
  1. Review tests against docs/pitfalls/testing-pitfalls.md
  2. Verify coverage (error paths? edge cases?)
  3. Run tests and confirm green
  ```
- **After each phase (logical group):** review the batch from multiple perspectives, minimum 3 rounds; if round 3 still finds issues, keep going until clean.
- **Single committer:** one implementer per task; commit before the next task. Do NOT dispatch parallel implementers on overlapping files.

---

## Phase 1 — Pure logic: StoneState + input/ability/data

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `docs/pitfalls/implementation-pitfalls.md` + `docs/pitfalls/testing-pitfalls.md`. Model files: `include/logic/dash.h` (`DashState`), `include/logic/grapple.h` (`GrappleState`), `src/logic/player.cpp` (`Player::update` ordering), `test/test_dash.cpp` + `test/test_player.cpp` (test style).

### Task 1.1: Add the input/ability/player fields for Stone

**Files:** Modify `include/logic/player.h`; Test: `test/test_player.cpp`.

- Current (`player.h:6-7`):
  ```cpp
  struct InputFrame { bool left=false, right=false, jump_pressed=false, fire_pressed=false, glide_held=false, grapple_fire=false; };
  struct Abilities { bool featherleap=false; bool glide=false; bool dash=false; bool grapple=false; };
  ```
- Desired:
  ```cpp
  struct InputFrame { bool left=false, right=false, jump_pressed=false, fire_pressed=false, glide_held=false, grapple_fire=false, down=false; };
  struct Abilities { bool featherleap=false; bool glide=false; bool dash=false; bool grapple=false; bool stone=false; };
  ```
- **Do NOT** touch `read_input` here (that's engine/Phase 3).
- **Do NOT** add the `Player.stone` member here — it needs the `StoneState` type, so it is added in Task 1.2 (which creates `stone.h`). Task 1.1 is ONLY the two struct-field additions above (both default-false, preserving all existing behavior).

- [ ] **Step 1:** add the fields; build the host tests to confirm nothing breaks (defaults preserve all existing behavior).
- [ ] **Step 2:** `bash tools/host_test.sh` green; `python tools/check_logic_purity.py` clean.
- [ ] **Step 3:** commit `feat(logic): InputFrame.down + Abilities.stone + Player.stone for the ground-pound`.

### Task 1.2: Create `StoneState` (pure logic, host-tested)

**Files:** Create `include/logic/stone.h`; modify `include/logic/player.h` (add the `Player.stone` member + include — moved here from 1.1 since it needs the type); Test: create `test/test_stone.cpp`.

Mirror `DashState` EXACTLY for structure. The pound is started by the PLAYER (Task 1.3 decides Down+A precedence); `StoneState` owns the pound lifecycle + landing signal. **Include note:** `stone.h` needs NO includes (like `dash.h`) — it stores `POUND_VY_RAW` as a raw `int` constant and only `bool`s; the `Fixed::from_raw(StoneState::POUND_VY_RAW)` conversion happens in `player.cpp` (Task 1.3), not here. After creating `stone.h`, add to `Player` (in `player.h`, after the `grapple` member): `#include "logic/stone.h"` at top + `StoneState stone;` member.

Suggested shape (the implementer MAY refine names but MUST keep it pure + host-tested):
```cpp
#pragma once
namespace logic {
// Stone ground-pound (M8): a locked, fast downward plunge with i-frames, started while airborne.
// Pure logic; the Player decides WHEN to start it (Down+A), the scene resolves WHAT it hits on landing.
struct StoneState {
    static constexpr int POUND_VY_RAW = 2048;  // = Fixed::from_int(8) = 8 px/frame; > the normal fall
        // terminal (PH terminal from_int(6)=1536). move_and_collide sub-steps at <=1 tile
        // (collision.cpp:13 "no tunneling"), so this speed never tunnels through a thin floor (TEST-3).
    bool pounding = false;
    bool landed   = false;   // one-frame signal: became grounded this tick while pounding
    // Begin (or RE-ARM) a pound (caller guarantees airborne + ability + Down+A, OR the scene re-arms
    // after smashing a cracked floor so one pound plunges through STACKED cracked floors — see Task 3.2).
    void start(){ if(!pounding){ pounding = true; landed = false; } }
    // Called once per frame from Player::update AFTER gravity/glide, BEFORE move_and_collide.
    // While pounding: lock vel to straight-down POUND_VY, zero horizontal. Returns the override vel.
    // (The Player applies it; see Task 1.3.)
    bool active() const { return pounding; }
    bool invincible() const { return pounding; }   // i-frames like Dash
    bool just_landed() const { return landed; }
    // Called by Player::update with the post-move grounded state. Raises `landed` the frame the
    // pound hits ground; ends the pound. (The scene reads just_landed() that same frame.)
    void post(bool on_ground_after_move){
        landed = false;
        if(pounding && on_ground_after_move){ landed = true; pounding = false; }
    }
};
}
```
**TESTS (`test_stone.cpp`, TDD — write failing first):**
- `stone_start_sets_pounding`; `stone_not_active_by_default`.
- `stone_post_raises_landed_and_ends_on_ground`: start → `post(true)` → `just_landed()` true, `active()` false.
- `stone_post_no_landing_while_airborne`: start → `post(false)` → still `active()`, `just_landed()` false.
- `stone_invincible_while_pounding`.
- `stone_landed_is_one_frame`: after a landing, a subsequent `post(true)` does NOT re-raise `landed` (pound already ended).
- Assert exact raw fixed values per TEST-2 where velocity is involved (the velocity application is verified in Task 1.3's Player test).

- [ ] Steps: write failing tests → implement `stone.h` → green → `host_test.sh` + purity → commit `feat(logic): StoneState pound lifecycle (host-tested)`.

### Task 1.3: Integrate the pound into `Player::update` (precedence + velocity + i-frames)

**Files:** Modify `src/logic/player.cpp`; Test: `test/test_player.cpp`.

Context — current ordering (`player.cpp` `Player::update`): horizontal accel/friction → **jump/featherleap** (`if(in.jump_pressed){ if(on_ground) JUMP_VY; else if(featherleap && air_jumps_left>0) JUMP_VY, --air_jumps_left; }`) → `apply_gravity` → glide → wind → **dash** (overrides vel) → **grapple** (overrides vel) → `move_and_collide` → (grapple post) → refresh air jumps on ground.

Desired changes (integer fixed-point only — IMPL-2):
1. **Trigger + precedence:** Down+A while airborne starts the pound and MUST NOT also double-jump. In the jump block: `if(in.jump_pressed){ if(body.on_ground) body.vel.y=JUMP_VY; else if(abilities.stone && in.down) stone.start(); else if(abilities.featherleap && air_jumps_left>0){ body.vel.y=JUMP_VY; --air_jumps_left; } }`. (So Down+A in the air → pound; A alone in the air → double-jump.)
2. **Apply pound velocity:** insert the pound application immediately BEFORE the existing grapple block (which stays LAST). If `stone.active()`: `body.vel.x = Fixed::from_int(0); body.vel.y = Fixed::from_raw(StoneState::POUND_VY_RAW);` (locked straight-down; overrides accel/glide/wind/dash). Because the grapple block runs after and overrides `body.vel` when `grapple.active()`, **grapple naturally wins** — no extra guard needed.
3. **Landing:** after `move_and_collide`, call `stone.post(body.on_ground);` so `just_landed()` is available to the scene this frame.
4. **i-frames:** the scene reads `player.stone.invincible()` (Task 3.2) — no change needed here beyond exposing it.

**TESTS (`test_player.cpp`):**
- `pound_starts_on_down_plus_a_in_air`: airborne player with `abilities.stone`, `in={down:true, jump_pressed:true}` → `player.stone.active()` true; `body.vel.y.raw == StoneState::POUND_VY_RAW` after update; `body.vel.x.raw == 0`.
- `down_plus_a_overrides_double_jump`: airborne, `featherleap` AND `stone` owned, `air_jumps_left==1`, `in={down:true,jump_pressed:true}` → pounds (does NOT consume the air-jump; `air_jumps_left` unchanged) and vel.y is the pound velocity, not JUMP_VY.
- `a_alone_in_air_still_double_jumps`: airborne, `in={down:false,jump_pressed:true}`, featherleap → double-jumps (vel.y==JUMP_VY, air_jumps_left decremented), `stone.active()` false.
- `no_pound_without_ability`: `abilities.stone=false`, Down+A in air → no pound.
- `no_pound_on_ground`: grounded, Down+A → normal ground jump (or nothing), `stone.active()` false.
- `pound_ends_on_landing`: pounding player driven into the floor via `move_and_collide` → after update `stone.just_landed()` true, `stone.active()` false.
- `grapple_wins_over_pound`: if `grapple.active()`, the grapple pull velocity (not the pound) is applied.
- `pound_into_thin_floor_stops_no_tunnel` **(TEST-3 — tunneling)**: a pounding player one tile above a 1-tile-thick solid floor (empty below it) — after `update` (which calls `move_and_collide`) the player is resting ON the floor (`body.on_ground`), did NOT pass through it, and `stone.just_landed()` is true. Build the `Tilemap` with a single solid row and assert the resolved `body.pos.y` snaps to the floor top (exact raw value, TEST-2). Confirms the engine's ≤1-tile substep handles the 8 px/frame pound.

- [ ] Steps: failing tests → implement → green → host_test + purity → commit `feat(logic): Down+A ground-pound in Player::update (overrides double-jump; grapple wins)`.

### Task 1.4: Level-data spawns for the Stone interactions

**Files:** Modify `include/logic/level_data.h`; Test: `test/test_level_data.cpp`.

Add the data the content + scene need (mirror the existing nullable-array+count pattern):
- **Heavy switch:** add `bool heavy = false;` to `PlateSpawn` (`struct PlateSpawn { int tx, ty, target_tx, target_ty; bool heavy = false; };`). A heavy plate trips ONLY on a Stone pound; a normal plate keeps its current step/block behavior. Default `false` preserves all existing plates.
- **Boulder:** add `struct BoulderSpawn { int tx, ty; };` + `const BoulderSpawn* boulders = nullptr; int boulder_count = 0;` to `LevelData`. (A breakable solid; distinct from the pushable `BlockSpawn`.)
- **Loose platform:** add `struct LoosePlatformSpawn { int tx, ty, len; };` (a horizontal run of `len` tiles, suspended; falls straight down on a nearby pound shockwave) + `const LoosePlatformSpawn* loose_platforms = nullptr; int loose_platform_count = 0;` to `LevelData`.
- CrackedFloor needs NO new struct — it reuses `GateSpawn` with `GateType::CrackedFloor` (already in `gates.h`, required=Stone, bg_tile 11).

**TESTS (`test_level_data.cpp`):** construct a `LevelData` with a heavy plate, a boulder, and a loose platform; read them back; assert defaults (`heavy==false`, null arrays / 0 counts) for a level that omits them.

- [ ] Steps: failing tests → implement → green → host_test + purity → commit `feat(logic): level-data spawns for heavy switch, boulder, loose platform`.

**After Phase 1:** review the batch (≥3 rounds): StoneState purity, precedence correctness (Down+A vs double-jump vs grapple), no float, exact-raw-value assertions, defaults preserve existing behavior. Update this phase's banner to ✅ SHIPPED with SHAs.

---

## Phase 2 — Level compiler: new symbols

**Execution Status:** ✅ SHIPPED — 2026-06-13. Commits: 54a4554 ('k' CrackedFloor gate), 612cac9 (heavy plate flag), a1925bd (boulder 'O' + loose-platform ':'), 10ce2d3 (header regen). 254/254 host tests pass.

**BEFORE starting:** read `tools/build_level.py` — the `TILE` map, the `CONTENT` set, the per-symbol entity-collection loop, the JSON sidecar mapping (study how `=`/plates, `B`/`P` blocks, and `G`/gates with `latch_id` are parsed + emitted), and `emit_header`. Read `tools/test_build_level.py` for the test style.

**Header-regen churn (expected):** adding a field to `PlateSpawn` (Task 2.2) and new arrays to `LevelData` (already done in Task 1.4) changes `emit_header`'s output, so EVERY level header (`include/game/levels/*.h`) regenerates with the new fields/defaults. `bash tools/host_test.sh` regenerates them. Commit the regenerated headers in a dedicated `chore(levels): regenerate headers for <field>` commit (exactly as M7 did for the `heart_containers` field) so the working tree stays clean. The defaulted fields (`heavy=false`, null/0 arrays) change no existing behavior.

### Task 2.1: CrackedFloor ASCII symbol

**Files:** `tools/build_level.py`, `tools/test_build_level.py`.

- `GateType::CrackedFloor` exists in `gates.h` (GATE_ENUM in build_level.py already has `'cracked_floor': 'CrackedFloor'`). `CrackedWall` uses ASCII `K`. Add a CrackedFloor ASCII symbol — pick an UNUSED char (verify against the `CONTENT`/`TILE` sets; suggested **`k`** lowercase, which is free). Map it to a `GateSpawn` with `GateType::CrackedFloor` and optional `latch_id` from the JSON `gates`/a dedicated list, mirroring how `K`/`V`/`I` set their gate type directly (they don't need a JSON `gates` entry — they hardcode the type). Follow the `K` (CrackedWall) handling exactly for the COMPILER side.
- **Scene-consumption note (forward-ref to Task 3.2):** the compiler just emits the `GateSpawn{tx,ty,CrackedFloor,latch_id}` marker. Unlike wall gates, the SCENE will NOT build a full-column body for it — it treats the marked tile as a solid FLOOR tile. No compiler change is needed for that distinction; just emit the marker like any gate.
- **TEST:** a level with `k` compiles to a gate `(tx,ty,GateType::CrackedFloor)`; a level without it still compiles.

- [ ] failing test → implement → `cd tools && python -m unittest test_build_level.py` green → commit `feat(compiler): 'k' = CrackedFloor gate (Stone-smashable)`.

### Task 2.2: Heavy-switch flag on plates

**Files:** `tools/build_level.py`, `tools/test_build_level.py`.

- Plates use `=` + JSON `"plates":[{"target":[tx,ty]}]`. Add an optional `"heavy": true` to a plate's JSON entry → set `PlateSpawn.heavy`. Emit the extra field in `emit_header` (the `*_PLATES[]` initializer gains the bool). Keep non-heavy plates unchanged (default false).
- **TEST:** `=` with `{"plates":[{"target":[3,4],"heavy":true}]}` emits a plate with `heavy=true`; a normal plate emits `heavy=false`; existing plate tests stay green.

- [ ] failing test → implement → unittest green → commit `feat(compiler): heavy (pound-only) flag on pressure plates`.

### Task 2.3: Boulder + loose-platform symbols

**Files:** `tools/build_level.py`, `tools/test_build_level.py`.

- **Boulder:** add an UNUSED symbol (suggested **`O`**) → `BoulderSpawn(tx,ty)`; emit `*_BOULDERS[]` + wire `boulders`/`boulder_count` into `*_DATA`.
- **Loose platform:** add an UNUSED symbol (suggested **`:`** for the left tile) → `LoosePlatformSpawn(tx,ty,len)` where `len` comes from a JSON `"loose_platforms":[{"len":N}]` (default 1, scan-order indexed like entrances); emit `*_LOOSE_PLATFORMS[]` + wire into `*_DATA`.
- Both follow the exact emit pattern of `heart_containers`/`braziers`. Levels without them: null arrays / 0 counts.
- **TESTS:** `O` → one boulder at the right tile; `:` with `{"loose_platforms":[{"len":3}]}` → a loose platform `(tx,ty,3)`; levels without them compile.

- [ ] failing tests → implement → unittest green + `host_test.sh` (regenerates all headers — confirm existing levels still compile) → commit `feat(compiler): boulder 'O' + loose-platform ':' symbols`.

**After Phase 2:** review (≥3 rounds): unused-symbol collisions, emit-order correctness vs `LevelData` field order, existing levels unaffected. Update banner ✅ SHIPPED.

---

## Phase 3 — Engine/scene wiring (pound input, impact resolution, falling terrain LAST, VFX)

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `src/game/scene_dungeon.cpp` — specifically the dash→CrackedWall block (`~424-430`: `if(!gi.open && gi.spawn.type==GateType::CrackedWall && player.dash.active() && aabb_overlap(player.body, gi.body)){ gi.open=true; open_column(...); persist_latch(...); }`), the plate resolution (`~500-505`), the enemy bolt/fire/contact block (`~440-453`), `open_column`/`fill_column` (`~89-104`), and `src/engine/input.cpp`. **Do NOT modify `src/logic/`** (Phase 1 is frozen).

### Task 3.1: Wire the pound input + Stone ability flag + shrine grant

**Files:** `src/engine/input.cpp`, `src/game/scene_dungeon.cpp`, `src/game/scene_hub.cpp`.

- `input.cpp` `read_input`: add `f.down = bn::keypad::down_held();`.
- `scene_dungeon.cpp` `play_room`: where ability flags are set each frame (`~314-318`), add `player.abilities.stone = world.has(logic::Ability::Stone);`.
- The Stone `F` shrine grants `Ability::Stone` — the shrine/pickup system already grants `pickups[i].ability`; just author the shrine with `ability:"stone"` in content (Phase 4). The `ABILITY_ENUM` in build_level.py already maps `'stone':'Stone'`.
- **Hub parity:** `scene_hub.cpp` — add `player.abilities.stone = world.has(logic::Ability::Stone);` to the per-frame ability flags (the hub already mirrors the kit since the M7 hub-parity work). A pound in the flat hub just slams to the ground harmlessly — consistent + harmless.
- No host test (engine glue); verify by ROM build. Self-review: `in.down` populated; stone flag set in both scenes.

- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(engine): wire Down input + Stone ability flag (dungeon + hub)`.

### Task 3.2: Pound impact resolution — cracked floor, heavy switch, crush, boulder

**Files:** `src/game/scene_dungeon.cpp`.

Resolve at the player's impact tile(s) on `player.stone.just_landed()`. Mirror the existing resolution idioms (the CrackedWall block `scene_dungeon.cpp:424-430`, plate `~500-505`, enemy loop `~440-453`):

**IMPORTANT — CrackedFloor is NOT a full-column gate.** The existing gate bodies are full-column vertical walls (`scene_dungeon.cpp:207` `gb.half_h = fx((level.h-3)*4)`) — correct for vine/ice/CrackedWall *walls* but WRONG for a horizontal *floor*. **In the existing gate-spawn loop (`scene_dungeon.cpp:204-217`), SKIP `GateType::CrackedFloor` entries — do NOT build a `GateInst`/full-column body for them.** Instead, collect CrackedFloor gates into a separate small list. **At spawn, the scene MUST make each cracked-floor tile solid:** the compiler emits content symbols on collision tile **0** (per build_level.py "content symbols sit on an empty collision tile"), so call `engine::set_collision_tile(tx, ty, 1)` for each CrackedFloor (exactly as the block spawn does at `scene_dungeon.cpp:252`, and as gates do via `fill_column`) and render `engine::set_level_tile(view, tx, ty, gate_info(CrackedFloor).bg_tile=11)`. The player then WALKS on the cracked floor normally; only a pound breaks it (Task-3.2 item 1 sets it back to 0 + clears the bg on smash). (No `GateInst`, no `fill_column`, no `gate_cleared_by` — those are for vertical wall gates.)

1. **CrackedFloor smash + continue the plunge (stacked floors):** cracked-floor tiles are SOLID, so a pound naturally *lands* on one → `just_landed()` fires at that tile. On that frame, if the landed-on tile is a CrackedFloor marker: remove it (`set_collision_tile(tx,ty,0)`, clear its bg tile across the cracked-floor run) AND **re-arm the pound** (`player.stone.start()`) so the next frame the player plunges into the area below. `persist_latch` if the cracked floor has a `latch_id` (a permanent route). Because the pound only *re-arms* on a cracked tile, it naturally **ends on the first NON-cracked solid tile** — so ONE pound chains through STACKED cracked floors and stops on real ground. (This is why `StoneState::start()` is re-armable — Task 1.2.)
2. **Heavy switch:** on `just_landed()`, for each plate with `heavy==true` whose body overlaps the impact, fire its target (the SAME `open_column`-at-`(target_tx,target_ty)` mechanism the normal plate uses) and `persist_latch` if it has a `latch_id`. **Two required changes to the EXISTING plate loop:** (a) in the existing normal-plate step/block trip loop (`~500-505`), SKIP plates with `heavy==true` (a heavy plate must NOT trip on a step or a pushed block); (b) add the heavy-plate trip in the `just_landed()` handler only. A heavy switch fires a GATE target — it does NOT drop loose platforms (those drop from the pound shockwave, Task 3.3).
3. **Crush enemies:** during an active pound (`player.stone.active()`, which grants i-frames), `aabb_overlap(player.body, enemy.body)` → kill the enemy (including `fire_immune`), refill magic like a bolt-kill (`magic.heal(25)`), hide sprite. Add to the existing enemy loop, guarded by `player.stone.active()` (parallel to the existing `player.dash.invincible()` contact guard).
4. **Break boulders:** spawn boulders as solid collision tiles + sprites (mirror `BlockInst`, but boulders are NOT pushable). On `just_landed()`, if a boulder tile is directly below the impact (the tile the player landed on top of, or the landed tile itself if the boulder is the floor), remove it (`set_collision_tile(...,0)`, hide sprite) so the path clears. Like the cracked floor, optionally re-arm the pound if you want the player to continue through a broken boulder; otherwise the player simply lands where the boulder was.

**TESTS:** these are scene-resolution (Butano) — not host-unit-testable directly. Where a rule is extractable to pure logic (e.g. "a heavy plate trips only on pound", "is the tile below a cracked floor"), prefer a small pure helper in `logic/` with a host test. Otherwise verify via the level-invariant tests (Phase 4) + emulator QA. Build the ROM to confirm compilation.

- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): Stone pound resolves cracked-floor/heavy-switch/crush/boulder`.

### Task 3.3: Falling terrain — loose platforms (LAST; bounded drop-to-rest)

**Files:** `src/game/scene_dungeon.cpp`.

- Spawn each `LoosePlatformSpawn` exactly like the pushable block (`scene_dungeon.cpp:246-255`): for each tile `(tx..tx+len-1, ty)`, `set_collision_tile(x, ty, 1)` (solid) + one sprite per tile (BG stays blank; the sprite shows it). Track a per-platform `falling`/`fallen` state + its current `ty`.
- On `player.stone.just_landed()`, any loose platform whose tile is within a **shockwave radius of Chebyshev distance ≤ 6 tiles** of the impact (matching `GrappleState::RANGE=6`; the value is feel-tunable but pick this definite starting value) that is not yet falling begins falling: each frame, if the tiles directly below the platform's run are all non-solid, move the run down one tile (update collision grid: clear old row, set new row solid; move sprites); stop when any tile below is solid (rest). **Drop-to-rest only — no momentum, no bounce.** The fall test considers ONLY the collision grid (solid tiles); the player is an entity, not a collision tile, so the platform does not detect or rest on the player — it would pass its row. Therefore the CONTENT must guarantee the player is never standing under a droppable platform's path; this is asserted by `d7_manipulable_objects_cannot_strand` (Task 4.3), which checks no loose platform's rest column overlaps a tile the intended path requires the player to occupy.
- Keep it simple and bounded; this is the only new dynamic-terrain system.

**TEST:** extract the "should this platform start falling given impact at (ix,iy)?" radius check and the "can it move down one tile?" check as pure helpers if practical (host-test the radius math); otherwise emulator QA + the Phase-4 invariant that no loose platform can crush the player or seal the only path.

- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): loose platforms fall (drop-to-rest) on a Stone-pound shockwave`.

### Task 3.4: Pound VFX + shrine/HUD polish (placeholder)

**Files:** `src/game/scene_dungeon.cpp`, `tools/make_placeholder_art.py` (+ a `graphics/*.bmp/.json` if a new sprite is added).

- A minimal pound cue: e.g. on `just_landed()`, a brief dust/impact sprite (reuse an existing sprite as placeholder, like the vine VFX reused `bolt`), and/or a 1–2 frame camera nudge. Keep it cheap + clearly commented as placeholder. The Stone shrine reuses the existing `shrine` sprite (no new art required); add a dedicated bmp only if trivial.
- No host test; ROM build.

- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): Stone pound impact VFX (placeholder)`.

**After Phase 3:** review (≥3 rounds): the cracked-floor "shatter-and-keep-falling vs stop-on-solid" logic, heavy-plate gating (pound-only), crush i-frame guard, boulder removal, loose-platform can't crush the player, `scene_dungeon` not regressed for existing dungeons (D1–D6 still build + play). Update banner ✅ SHIPPED.

---

## Phase 4 — Content: Quaking Quarry (3 rooms) + hub Door 7 + no-soft-lock invariants + QA

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read the D6 room files (`tools/levels/dungeon6_room*.txt/.json`) + `include/game/levels/dungeons.h` (the `DUNGEON6_DUNGEON` room set) as the authoring template, and `test/test_dungeon6_level.cpp` for the invariant style. Honor the room/camera constraint (≥30×20, content row 18, two-way doors).

### Task 4.1: Author the 3 Quaking Quarry rooms

**Files:** Create `tools/levels/dungeon7_room{0,1,2}.txt`+`.json` → generated `include/game/levels/dungeon7_room{0,1,2}.h`; modify `include/game/levels/dungeons.h` (add `DUNGEON7_DUNGEON`).

Author per the spec §4 (exact tiles finalized in the emulator; the compiler enforces rectangular ≥30×20 + solid border):
- **Room 0 — Quarry Mouth:** Stone `F` shrine (`{"pickups":[{"ability":"stone"}]}`), a tutorial cracked floor `k` to pound through (drops to a ledge with solid below), a first heavy switch (`=` + `"heavy":true`), a magic-source enemy `o`. Two-way doors to Room 1 (entrance 0) and Room 2 (entrance 0), each beside its return entrance (the D6 wiring pattern).
- **Room 1 — Cut & Fill (puzzle branch):** a boulder `O` blocking the path (pound to break); a **loose platform** `:` positioned so that pounding nearby (the shockwave, Chebyshev ≤6) drops it straight down into a gap to form a bridge; a separate **heavy switch** `=`(heavy) whose target opens the **latched shortcut** gate (`GateSpawn` with a `latch_id`, persisted to SRAM). (Keep the loose-platform-via-shockwave and the heavy-switch-opens-a-gate as two DISTINCT beats — a heavy switch does not itself drop platforms; see Tasks 3.2/3.3.) Optional **heart container** `H` (`{"heart_containers":[{"id":1}]}` — id 1; D6 used id 0) concretely Stone-gated: place it in a pocket sealed by a **boulder `O`** (pound to break in) or reachable only by **pounding a cracked floor `k`** into the pocket — i.e. unreachable without the Stone ability. Two-way return door to Room 0.
- **Room 2 — The Deep Shaft (descent + spronk):** stacked cracked floors `k` to pound through and descend onto safe ledges (each drop lands on SOLID, never a hazard — see the invariant); an armored **fire-immune** enemy `o` (`{"enemies":[{"patrol":[..],"fire_immune":true}]}`) mid-shaft to crush; one **sequential** carried-kit combo AFTER a descent (NOT stacked under a cracked floor): e.g. land on a ledge, then a **water gap `w` you freeze (Ice) to cross**, or a **grapple anchor `g`** to climb to a side ledge. The spronk `C` + exit `E` on the floor at the bottom. Two-way return. **Do NOT place a hazard (water/lava/spikes) directly below a cracked floor** — the Ice bolt's down-column scan (`consume_tile_hit`) stops at the solid cracked floor, so the player can't pre-freeze it and would pound into the hazard. Keep hazard crossings as separate horizontal beats on solid ledges.
- `dungeons.h`: `DUNGEON7_DUNGEON = { DUNGEON7_ROOMS, 3, 0 }` (start_room 0).
- Recompile each header via `python tools/build_level.py tools/levels/dungeon7_roomN.txt include/game/levels/dungeon7_roomN.h`.

- [ ] author → compile headers → `bash tools/host_test.sh` (level compiles) → commit `feat(content): Quaking Quarry — 3-room Dungeon 7 (Stone descent)`.

### Task 4.2: Hub Door 7

**Files:** `src/game/scene_hub.cpp` (`door_enterable`), `src/main.cpp` (dungeon dispatch), `tools/levels/hub.txt`+`.json` → `include/game/levels/hub.h`.

- `door_enterable`: add `|| (n == 7 && w.spronk_freed(6))`.
- `main.cpp`: add `else if(n == 7) lvl = &DUNGEON7_DUNGEON;` (and `#include` if needed). **Do NOT** re-add any QA scaffold.
- `hub.txt`: ensure Door 7 exists (a `7` door tile on the hub floor; doors render as a 2-wide×4-tall archway, see `scene_hub.cpp:57-64`). The plaza already holds doors 1–6 (~48 wide); if there's no room for a 7th 2-wide archway with spacing, **widen the hub** (keep ≥30 wide for the camera clamp, solid border, and the M7 grapple anchors/platforms intact) or reposition doors. Recompile `hub.h`. Verify the hub still passes `test_hub_level.cpp` (and update its door-count/spawn assertions if you moved things).

- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): hub Door 7 → Quaking Quarry (opens after D6 spronk)`.

### Task 4.3: Level invariants + **no-soft-lock invariant tests** (first-class)

**Files:** Create `test/test_dungeon7_level.cpp`.

Standard invariants (D6 style): shrine + spronk grounded on content row 18 with solid below; two-way doors with co-located return entrances; rooms ≥30×20, solid border; every `k` cracked floor reachable; descent solvable.

**No-soft-lock invariants (REQUIRED — per spec §6/§8):**
- `d7_every_cracked_floor_drop_has_forward_exit`: for each `CrackedFloor` gate, locate the **true** landing tile by scanning straight down PAST any further cracked-floor tiles (the pound chains through stacked cracked floors) to the first **non-cracked** solid tile, then run a **bounded reachability flood-fill** from that landing tile over the room's non-solid tiles, where a step connects horizontally-adjacent standable tiles AND allows a vertical climb of up to the double-jump height (~6 tiles) to a higher standable tile. Assert the reached set CONTAINS a forward exit (a `RoomDoorSpawn` tile, the cage `C`, or the exit `E`). This is tractable (a grid BFS with a fixed climb bound — no spell/grapple modeling needed; if the intended path needs grapple/glide, place a grapple anchor/exit WITHIN the climb-bounded reachable set so the structural check still passes). **Non-vacuity gate (REQUIRED):** the test MUST fail on a deliberately-sealed landing pocket — verify by temporarily walling a drop's landing area and seeing the assertion go red, then restoring. If the flood-fill can't be made to distinguish sealed-vs-open, STOP and raise it rather than shipping a check that always passes.
- `d7_manipulable_objects_cannot_strand`: for each boulder and loose platform, compute its post-interaction geometry (boulder removed → tiles cleared; loose platform → its rest row, by dropping straight down to the first solid). Then re-run the same bounded reachability flood-fill (as the cracked-floor invariant) on the resulting grid and assert a forward exit is still reachable from the room's entrance. For each heavy switch, assert its target opens a gate (does not seal one). **Tractability:** reuse the flood-fill helper; compute each object's deterministic outcome from the tile grid. **Non-vacuity gate (REQUIRED):** verify the test goes red if a loose platform is authored to rest across the only corridor (temporarily author that, see red, restore). If "the only route" can't be expressed structurally, assert the weaker concrete property — the object's outcome tiles do not overlap any `RoomDoorSpawn`/`C`/`E` tile and do not fully wall a 1-wide corridor — and `log()` what is/isn't covered (no silent vacuity).
- `d7_magic_beats_have_a_source_before_them`: for each room, if it contains a magic-gated beat (a `Water` tile crossable only by Ice, or a Fire-cleared gate), assert that room OR an earlier room on the path (lower room index along the room graph from start) contains an enemy `o` (a magic source), OR the beat has an always-available alternative (Stone/Dash/Grapple/Glide route). **Tractability:** a per-room enemy-presence + room-ordering check, not a full path solver. **Non-vacuity:** verify it goes red if you remove the enemy that funds a freeze beat.
- `d7_no_pound_pit_traps`: every tile a pound can land on is escapable — the landing spot is climbable out of (the player always has **Featherleap** by D7: a ground jump + one air-jump reaches ~**6–7 tiles**; `JUMP_VY` ≈ 3.5 tiles single — see `player.cpp`), OR has a reachable forward exit (door/grapple anchor), OR is the intended terminal (spronk). Use the ~6-tile double-jump climb as the concrete escapable-wall threshold; anything deeper MUST provide a forward exit. No pound deposits the player in an inescapable hole.
- `d7_no_pound_lands_in_hazard`: for every `CrackedFloor` tile, scan straight down through the full plunge path (passing through any stacked cracked-floor tiles, which get smashed) to the first non-cracked solid tile, and assert NONE of the traversed tiles nor the landing tile is a hazard (`Lava`/`Water`/`Spikes`) — a pound through a cracked floor must reach safe solid ground, never plunge into a hazard (the player can't pre-freeze through a solid cracked floor — see the Room 2 note in Task 4.1). Non-vacuity: goes red if a cracked floor is authored with water/lava directly beneath.

**These tests assert against the COMPILED `DUNGEON7_ROOM*` tile/entity data** (include the generated headers). Keep them concrete, not tautological.

```
BEFORE marking this task complete:
Review the soft-lock invariants against docs/pitfalls/testing-pitfalls.md.
If an invariant can't be expressed structurally and would race/flake, STOP and
raise it — do NOT weaken it into a tautology. A vacuous "soft-lock test" that
always passes is worse than none. Each invariant MUST be able to FAIL on a
deliberately-broken layout (verify by temporarily breaking a room and seeing red).
```

- [ ] write invariants → verify each FAILS on a deliberately broken layout, then PASSES on the real rooms → `bash tools/host_test.sh` green → commit `test(content): D7 level + no-soft-lock invariants (cracked-floor exit, object safety, resource, pit-escape)`.

### Task 4.4: Emulator QA (manual)

- Build the QA ROM. If Door 7 reachability requires it, use a TEMP uncommitted `src/main.cpp` scaffold (grant the carried kit + free spronks 1–6) — clearly commented `REVERT before final commit`, exactly as M7 did. Stone itself is still earned from the room-0 shrine.
- Hand to the user for round-based QA: pound triggers on Down+A; cracked-floor descent; heavy switches; crush the fire-immune enemy; break boulders; loose-platform bridge; the carried-kit combo; the latched shortcut; free spronk 7; **probe for soft-locks** (try to strand yourself — pound into pits, mis-trigger objects, exhaust magic). Iterate fixes round by round (TDD any logic fix; re-author content as needed).
- After QA passes: **revert the scaffold**, run a final full-branch review, then `superpowers:finishing-a-development-branch`.

- [ ] QA rounds → fixes → scaffold reverted → final review READY TO MERGE → merge.

**After Phase 4:** update the banner ✅ SHIPPED + the top-of-plan table; record any QA-driven deviations.

---

## Self-review (against the spec)

- **Spec §2 (mechanic)** → Phase 1 (StoneState + Down+A precedence + i-frames + just_landed). ✓
- **Spec §3 (5 interactions)** → Phase 1 data (1.4) + Phase 3 resolution (3.2 cracked-floor/heavy/crush/boulder; 3.3 loose platform). ✓
- **Spec §4 (3-room dungeon)** → Phase 4.1; **hub Door 7** → 4.2. ✓
- **Spec §5 (new vs reused, purity)** → Phase 1 pure, Phases 3–4 bn::. ✓
- **Spec §6/§8 (soft-lock guards + invariant tests)** → Task 4.3 (first-class, must-fail-on-broken-layout). ✓
- **Spec §8 (StoneState + compiler + level tests)** → 1.2/1.3 + Phase 2 + 4.3. ✓
- **No SaveData change** → Stone in `abilities` bitmask; latches for shortcut + container (id 1). ✓
- **Type consistency:** `StoneState`/`POUND_VY_RAW`/`stone.start()`/`stone.post()`/`stone.just_landed()`/`stone.active()`/`stone.invincible()` used consistently across 1.2→1.3→3.2; `PlateSpawn.heavy`, `BoulderSpawn`, `LoosePlatformSpawn{tx,ty,len}` consistent across 1.4→2.2/2.3→3.2/3.3→4.1. ✓
