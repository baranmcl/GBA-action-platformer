# M11 — Nightmare King Finale Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the game's finale — Door 9 (opens when all 8 spronks are freed) → a 2-room traversal approach → a fixed-arena 3-phase Nightmare King boss fight → a victory screen + persisted completion — giving Spronk Quest its first real win state.

**Architecture:** Three-layer (pure `logic/` host-tested, `engine/` Butano glue, `game/` scenes). The boss's state machine is a pure-logic `BossState` (host-tested, written to generalize for the M12 framework extraction). The arena is a bespoke `run_boss()` scene; the 2-room approach reuses the existing room-to-room framework; damage reuses the existing `SpellPool`/`BoltPool` `consume_hit` hooks; death/Game-Over reuses the M9 lives flow; completion persists via a new save v5 field stored in existing struct padding (no `sizeof` change).

**Tech Stack:** C++17, Butano 21.6.0 (GBA). Host tests: `bash tools/host_test.sh`. ROM: `bash tools/build_rom.sh`. Purity guard: `python tools/check_logic_purity.py`. Level compiler: `tools/build_level.py`. Spec: `docs/superpowers/specs/2026-06-15-spronk-quest-m11-nightmare-king-design.md`.

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

**Overall:** 4/4 phases shipped; emulator QA (4.6) complete after 9 rounds; ready for final review + merge. Branch: `feat/m11-nightmare-king` (off `main`, 33 commits).

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure logic: BossState + attack table + SWITCH_BUDGET | ✅ Shipped | `c6e25df` (P1.1–P1.6) | 14 host tests; 404/404; non-vacuity verified |
| 2 — Save v5: World.beaten + migrations | ✅ Shipped | `649c7c4`, `20c45ee` | 409/409; sizeof==20; all migrations + clamp ports |
| 3 — Engine/scene: run_boss() + King + art | ✅ Shipped | `9a8204f`,`021fd4b`,`b565821`,`71c69bb` | ROM builds; scene reviewed vs BossState contract |
| 4 — Content + integration + invariants + QA | ✅ Shipped | `4e84503`,`6e93f25`,`1424bd1`,`31c2e6c`,`1b02c94` + QA r1–r9 | 4.1–4.5 + 9 emulator QA rounds (419/419, ROM builds) |

### Recommended execution order (dependency-aware)

Tasks are grouped by phase for readability, but dependencies force this execution order for a subagent-driven run:

`1.1 → 1.2 → 1.3 → 1.4 → 1.5 → 1.6` (BossState, all host-tested) → `2.1 → 2.2` (save v5) → **`4.2`** (D9 approach + arena level data — a dependency of Phase 3) → `3.1` (King art) → `3.2 → 3.3 → 3.4` (run_boss scene) → `4.1` (hub Door 9) → **`4.4`** (run_victory + title — a dependency of 4.3) → `4.3` (main.cpp dispatch — needs run_boss, DUNGEON9_* symbols, AND run_victory) → `4.5` (invariants) → `4.6` (QA).

Reason: 4.2 (content) unblocks the scene; 4.4 (`run_victory`) must exist before 4.3 references it; everything else is intra-phase sequential. Taking this order is NOT a deviation (the phase numbers are organizational, not execution-order); document it only if you diverge from THIS list.

### Deviations
- **Phase 2 (Task 2.1):** also edited `test/test_world_state_v3.cpp` (3 assertions `SAVE_VERSION==4`/make_save-produces-v4 → `==5`), beyond the plan's named file list. Necessary + mechanical (no logic change) — the suite could not go green otherwise. Folded into commit `649c7c4`.
- **Task 4.2:** extended `tools/build_level.py` to support `;`-prefixed comment lines in level `.txt` files (commit `4e84503`). The level format had no comment syntax, but the plan required documenting `KING_SPAWN_TX/TY` + the per-phase firing tiles inside `dungeon9_arena.txt`. `;` is unambiguous (not a grid symbol; distinct from `#` wall). Verified non-breaking: level-compiler unit tests + all regenerated dungeon headers + host suite stay green (409/409). Task 4.2 also executed BEFORE Phase 3 per the plan's own "Recommended execution order" (not a deviation, but noting the actual order).
- **Task 4.1:** extended `tools/build_level.py` to accept the `9` door digit (CONTENT set + door scan + docstring) — the compiler previously only handled doors 1–8. Mechanical, non-breaking (commit `6e93f25`). Hub kept at width 58, door pitch 6 (2-wide archway + 4-tile gap), 9 doors cols 3..51 on row 15.
- **Task 4.5:** also modified the Task-4.2 arena (`dungeon9_arena.txt`) to fix completability (see Discoveries) and made the `reaches_forward_exit` test helper room-index-aware so a backtrack room-door can't mask a walled-off forward exit (needed for the room1 non-vacuity break). Committed in `1b02c94`.
- **Task 4.6 (emulator QA r1–r2) — significant deviations from the written scene design, driven by playtest:**
  - **Camera:** the plan specified a FIXED arena camera; the arena is larger than the GBA screen, so a corner spawn was off-screen. Switched to scene_dungeon's clamped follow-camera (`fix(boss): clamped follow-camera...`).
  - **King position + attacks:** the spec's hovering King + 3 attack types (incl. a player-height-tracking sweep) were unhittable (horizontal-only player projectiles) and unfair. Redesigned: King at FLOOR-centre (directly shootable from the ground) + ONE readable fixed-height ground bolt the player jumps over (`fix(boss): QA r2 ...`). The Task-4.5 `d9_arena_expose_positions_reachable` invariant now passes via the open floor (firing is from the ground, not the platforms); the platform-tile checks are over-specified but still hold — a follow-up could retarget them.
  - **New features (user-requested in QA):** King contact damage; a 9-pip King HP bar; magic crystal now respawns when magic < one cast (repeatable recharge); King intro/death dialogue ("YOU FINALLY MADE IT" / "NOOOOO!").
  - **Global pause (control-scheme change):** new `engine::check_pause()` (START → "GAME PAUSED") in boss + dungeon + hub. START was the dungeon's manual "restart room"; that anti-soft-lock reset moved to a deliberate **L+R hold (~30 frames)**. SELECT=quit unchanged. New files `include/engine/pause.h` + `src/engine/pause.cpp`.
- **Task 4.6 QA rounds r3–r9** (all in `scene_boss.cpp` unless noted) — combat polish from playtest:
  - r2: King lowered to floor-hittable + 3 attacks + spawn-on-floor + crystal-respawn + contact damage + King HP bar.
  - r3: dedicated `king_hp` pip sprite (`make_placeholder_art.py`) + boss HP bar moved to the bottom (no HUD overlap).
  - r4: King **teleports** between perches (was a sitting duck); attacks moved to a pool.
  - r5: **telegraph cue** orb at the King (red/gold/cyan per attack) + all attacks King-sourced + **spiral** + phase **banners**.
  - r6: **King hit i-frames** (logic — `boss.h` + tests, 419/419): a wound ends the expose + grants i-frames → ONE wound per Light (fixes the stun-spam exploit); King teleports onto **platforms**; spiral two-sided; banners → in-character **dialogue** ("NOW YOU'RE GETTING ME ANGRY" / "I'M DONE TOYING WITH YOU").
  - r7: spiral back to **one arm, aimed at the player's side**; **block defense** (bolt/Fire/Ice destroy boss projectiles on contact; Light exempt); King **teleports away after a wound** (no instant retaliation).
  - r8–r9: **Zelda II high/mid/low shot aim** (`engine::read_aim_dy()`, UP/DOWN+B) — boss first, then universal across dungeon + hub + boss; low shot raised to clear the floor.

### Discoveries
- `test/test_world_state_v3.cpp` pins the CURRENT save version (asserts `SAVE_VERSION==4` in 3 places), not just v3-on-disk migration. Any future save-version bump must update it too (the plan's Phase 2 only anticipated `test_world_state_v4.cpp`). Fixed in `649c7c4`.
- **Task 4.5:** the committed Task-4.2 arena had the P1/P3 firing platforms (row 19) floating ~11 tiles above the floor with the grapple anchor beyond grapple RANGE — the documented firing tiles were unreachable, so the arena was not completable. Fixed in `1b02c94` by adding two-step staircases (solid stubs at rows 20+25, staggered per the M10 overhead-clip lesson, ≤5-tile steps) so P1(19,18)/P3(26,18) are base-reachable. The `d9_arena_expose_positions_reachable` invariant now guards this.
- **RESOLVED (4.6 QA) — King projectile-reach:** spell projectiles travel purely horizontally, so a high King was unhittable. Resolved over the QA rounds: the King now floats at floor-hittable height and **teleports** between floor + platform perches (player repositions to its height), plus the **Zelda II high/mid/low shot aim** lets the player line up shots vertically. The static-unwinnable risk is gone (the fight is winnable + was played end-to-end in QA).

---

## Constants (single source of truth — define once, share across scene + tests)

These live in `include/logic/boss.h` (Phase 1). Starting values; **QA-tuned in Phase 4**. All are named constants so the scene and the host tests reference the same numbers.

```cpp
namespace logic {
// --- Shared-button switch budget (see spec §2). Frames at 60fps. ---
// Worst-case time to cycle (L) to the needed spell + react + cast: reaction(~15)
// + worst chain 2 presses x ~10 + cast/travel(~15) ~= 50, rounded up with margin.
inline constexpr int SWITCH_BUDGET = 60;

// --- King HP + phase thresholds. Defeat at hp <= 0. Wound subtracts WOUND_DMG. ---
inline constexpr int KING_MAX_HP = 90;
inline constexpr int P1_END_HP   = 60;   // crossing 60 -> advance to P2
inline constexpr int P2_END_HP   = 30;   // crossing 30 -> advance to P3
inline constexpr int WOUND_DMG   = 10;   // damage per bolt/element hit while EXPOSED (3 hits/phase)

// --- Expose window (vulnerable). MUST be >= SWITCH_BUDGET (so the player can
//     switch to a wound spell, or just bolt). ---
inline constexpr int EXPOSE_FRAMES = 75;

// --- Per-phase attack pattern (data-described; escalating). telegraph MUST be
//     >= SWITCH_BUDGET for every phase. Escalation = shorter telegraph each phase
//     but never below the budget floor (P3 sits AT the floor). ---
struct PhasePattern { int telegraph_frames; int attack_active_frames; int recovery_frames; };
inline constexpr PhasePattern PHASE_PATTERNS[3] = {
    { 72, 30, 40 },   // P1 (aerial)  — telegraph 72 >= 60
    { 66, 30, 30 },   // P2 (ground)  — telegraph 66 >= 60
    { 60, 30, 20 },   // P3 (all-in)  — telegraph 60 == SWITCH_BUDGET (tightest allowed)
};
}
```

---

## File Structure

**New (pure logic):**
- `include/logic/boss.h` — `BossPhase`, `BossState`, constants above, `PHASE_PATTERNS`. Header-only inline (like `reveal.h`/`stone.h`).

**New (engine/game):**
- `include/game/scene_boss.h` — `BossResult { Victory, QuitToTitle }` + `BossResult run_boss(const logic::DungeonData& arena, logic::World&, logic::PlayerState&)`.
- `src/game/scene_boss.cpp` — the bespoke fixed-arena fight scene.
- `include/game/scene_victory.h` + `src/game/scene_victory.cpp` — the "THE END — CONGRATULATIONS" screen (`run_victory(const logic::World&)`).

**New (content + art):**
- `tools/levels/dungeon9_room0.txt/.json`, `dungeon9_room1.txt/.json` (2-room approach), `dungeon9_arena.txt/.json` (boss arena) → generated `include/game/levels/*.h`.
- `graphics/king.*` (placeholder King sprite) + JSON.
- `test/test_boss.cpp` (BossState), `test/test_world_state_v5.cpp` (save v5), `test/test_dungeon9_level.cpp` (no-soft-lock invariants).

**Modified:**
- `include/logic/world_state.h` — `World.beaten`, `SaveData.beaten`, `SAVE_VERSION=5`, `checksum_v5`, v5 load branch + v1/v2/v3/v4→v5 migration default `beaten=false`.
- `src/game/scene_hub.cpp` + `tools/levels/hub.txt`/`hub.json` — Door 9 (re-spaced to 9 doors), `door_enterable(9)`.
- `src/main.cpp` — Door 9 → approach → `run_boss` → victory/ending dispatch.
- `src/game/scene_title.cpp` — reflect completion when `world.beaten`.
- `test/test_hub_level.cpp` — 9-door layout + Door-9-enterable assertions.
- `test/test_world_state_v4.cpp` — repurpose to a v4-on-disk → v5 migration test (its make_save-based "current version" tests move to v5; see Task 2.2).

---

## Phase 1 — Pure logic: BossState + attack table + SWITCH_BUDGET

**Execution Status:** ✅ SHIPPED at `c6e25df` on 2026-06-15 (tasks P1.1–P1.6; `include/logic/boss.h` + `test/test_boss.cpp`, 14 tests, 404/404 green, SWITCH_BUDGET + expose-decay non-vacuity verified)

**Why this matters:** the boss's entire decision logic is pure and host-tested here, so Phase 3's scene is "just rendering." This is also the M12-extraction seed — keep naming generic (`BossPhase`, not `KingRage`) and attacks data-described.

**Pitfalls (read before starting):** IMPL-1 (NO `bn::` in `include/logic/`), IMPL-2 (integer-only — no float; all timers/HP are `int`), TEST-1 (no frame-timing dependence — drive `tick()` explicitly), TEST-2 (assert exact raw int values).

### Task 1.1: BossState skeleton + constants + initial-state tests

**Files:**
- Create: `include/logic/boss.h`
- Create: `test/test_boss.cpp`

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Write the failing test** (`test/test_boss.cpp`)

```cpp
#include "test_framework.h"
#include "logic/boss.h"
using namespace logic;

TEST(boss_initial_state){
    BossState b;
    b.reset(); // sets P1 + full HP + cleared timers
    CHECK_EQ((int)b.phase, (int)BossPhase::P1);
    CHECK_EQ(b.hp, KING_MAX_HP);
    CHECK_EQ(b.phase_start_hp, KING_MAX_HP);
    CHECK_EQ(b.expose_timer, 0);
    CHECK(!b.exposed());
    CHECK(!b.defeated());
}

TEST(boss_constants_sane){
    CHECK_EQ(KING_MAX_HP, 90);
    CHECK(P1_END_HP > P2_END_HP);
    CHECK(P2_END_HP > 0);
    CHECK_EQ(KING_MAX_HP % WOUND_DMG, 0); // HP is a whole number of wounds
}
```

- [ ] **Step 2: Run, verify it fails** — `bash tools/host_test.sh` → FAIL (no `logic/boss.h`).

- [ ] **Step 3: Implement `include/logic/boss.h`** — the constants block from the "Constants" section above, plus:

```cpp
#pragma once
#include <cstdint>
namespace logic {
// (constants from the Constants section go here: SWITCH_BUDGET, KING_MAX_HP,
//  P1_END_HP, P2_END_HP, WOUND_DMG, EXPOSE_FRAMES, PhasePattern, PHASE_PATTERNS)

// The 2-room approach runs OUTSIDE BossState; BossState begins at P1 when run_boss()
// starts. EXPOSED is a phase overlay: on_light_hit() saves the current phase into
// exposed_return and sets phase=Exposed; when expose_timer hits 0, phase returns to
// exposed_return. Generic naming so M12 can lift this into a shared boss framework.
enum class BossPhase : uint8_t { P1=0, P2, P3, Exposed, Defeated };

struct BossState {
    int hp = KING_MAX_HP;
    BossPhase phase = BossPhase::P1;
    BossPhase exposed_return = BossPhase::P1; // phase to resume when expose ends
    int phase_start_hp = KING_MAX_HP;         // HP at the active phase's entry
    int expose_timer = 0;                     // >0 while EXPOSED (vulnerable)
    int attack_timer = 0;                     // drives the per-phase attack pattern

    void reset(){ hp=KING_MAX_HP; phase=BossPhase::P1; exposed_return=BossPhase::P1;
                  phase_start_hp=KING_MAX_HP; expose_timer=0; attack_timer=0; }
    bool exposed() const { return expose_timer > 0; }
    bool defeated() const { return hp <= 0 || phase == BossPhase::Defeated; }
    // (on_light_hit / on_wound / tick / on_player_death / current_step added in later tasks)
};
}
```

- [ ] **Step 4: Run, verify green** — `bash tools/host_test.sh`.
- [ ] **Step 5: Commit** — `git add include/logic/boss.h test/test_boss.cpp && git commit -m "feat(logic): BossState skeleton + boss constants (M11 P1.1)"`

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify coverage (initial state + constants invariants)
3. Run `bash tools/host_test.sh` and confirm green

### Task 1.2: on_light_hit / exposed (enter the vulnerable window)

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Failing test**

```cpp
TEST(boss_light_hit_exposes){
    BossState b; b.reset();
    b.on_light_hit();
    CHECK(b.exposed());
    CHECK_EQ((int)b.phase, (int)BossPhase::Exposed);
    CHECK_EQ((int)b.exposed_return, (int)BossPhase::P1); // remembers underlying phase
    CHECK_EQ(b.expose_timer, EXPOSE_FRAMES);
}

TEST(boss_light_hit_while_already_exposed_refreshes){
    BossState b; b.reset();
    b.on_light_hit();
    b.tick(); // expose_timer 75 -> 74
    b.on_light_hit(); // refresh, do NOT overwrite exposed_return with Exposed
    CHECK_EQ(b.expose_timer, EXPOSE_FRAMES);
    CHECK_EQ((int)b.exposed_return, (int)BossPhase::P1);
}
```

- [ ] **Step 2: Run, verify fail.**
- [ ] **Step 3: Implement** in `BossState`:

```cpp
void on_light_hit(){
    if(defeated()) return;
    if(phase != BossPhase::Exposed) exposed_return = phase; // only capture the real phase
    phase = BossPhase::Exposed;
    expose_timer = EXPOSE_FRAMES;
}
```
Do NOT change `hp` here (Light exposes; it does not wound). Do NOT capture `exposed_return` when already Exposed (the refresh case) — otherwise a re-cast would set `exposed_return = Exposed` and the fight would never leave the overlay.

- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): BossState.on_light_hit exposes the King (M11 P1.2)"`

BEFORE marking complete: review vs testing-pitfalls; cover the refresh edge; run host tests green.

### Task 1.3: tick (expose-window decay + return-to-phase; attack_timer advance)

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md. This is timing-sensitive logic — drive `tick()` explicitly (TEST-1), assert exact frame boundaries (TEST-2).

- [ ] **Step 1: Failing test** — exact-frame decay boundary:

```cpp
TEST(boss_expose_decays_to_exact_zero_then_returns){
    BossState b; b.reset();
    b.on_light_hit();                       // expose_timer = 75, phase = Exposed
    for(int i = 0; i < EXPOSE_FRAMES - 1; ++i) b.tick();
    CHECK(b.exposed());                      // still >0 at frame 74
    CHECK_EQ(b.expose_timer, 1);
    CHECK_EQ((int)b.phase, (int)BossPhase::Exposed);
    b.tick();                                // 1 -> 0
    CHECK(!b.exposed());
    CHECK_EQ((int)b.phase, (int)BossPhase::P1); // returned to exposed_return
}

TEST(boss_attack_timer_advances_and_wraps_per_phase){
    BossState b; b.reset();                  // phase P1
    int period = PHASE_PATTERNS[0].telegraph_frames
               + PHASE_PATTERNS[0].attack_active_frames
               + PHASE_PATTERNS[0].recovery_frames;
    for(int i = 0; i < period; ++i) b.tick();
    CHECK_EQ(b.attack_timer, 0);             // wrapped exactly at the phase period
}

TEST(boss_attack_frozen_while_exposed){
    // The expose window is a CLEAN damage window — the King's attack pattern must NOT
    // advance while EXPOSED (so the player can safely switch spells + wound; co-design rule).
    BossState b; b.reset();
    for(int i = 0; i < 5; ++i) b.tick();     // advance the attack pattern a little
    int frozen = b.attack_timer;
    b.on_light_hit();                        // EXPOSED
    for(int i = 0; i < 10; ++i) b.tick();    // ticks during expose
    CHECK_EQ(b.attack_timer, frozen);        // unchanged while exposed
}
```

- [ ] **Step 2: Run, verify fail.**
- [ ] **Step 3: Implement** `tick()`:

```cpp
int phase_period(BossPhase p) const {
    int i = (p==BossPhase::P2)?1 : (p==BossPhase::P3)?2 : 0;
    const PhasePattern& pp = PHASE_PATTERNS[i];
    return pp.telegraph_frames + pp.attack_active_frames + pp.recovery_frames;
}
void tick(){
    if(defeated()) return;
    if(expose_timer > 0){
        // EXPOSED = a clean damage window: the King is stunned, so the attack pattern is
        // FROZEN (do not advance attack_timer). This honors the co-design rule (never force
        // the player to switch spells AND dodge on the same clock).
        if(--expose_timer == 0) phase = exposed_return; // window closed -> shield back up
        return;
    }
    // attack pattern advances only while NOT exposed
    if(phase==BossPhase::P1 || phase==BossPhase::P2 || phase==BossPhase::P3){
        if(++attack_timer >= phase_period(phase)) attack_timer = 0;
    }
}
```

- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): BossState.tick expose decay + attack timer (M11 P1.3)"`

BEFORE marking this task complete (assertion-rigor mandate — timing-sensitive logic):
These are deterministic host tests (explicit `tick()` stepping, no real time, no concurrency — they cannot legitimately flake). If the exact-frame boundary assertion (`expose_timer == 1` at frame 74, `!exposed()` at frame 75) ever fails, the fix is to correct the decrement/return logic — NEVER to weaken the assertion to a range/tolerance or delete it. A frame-exact expose window is the contract the SWITCH_BUDGET balance (Task 1.6) and the scene depend on; a loosened boundary silently erodes that guarantee. If you cannot make the exact assertion pass, STOP and raise it — do not ship a weaker test. Also review vs testing-pitfalls (TEST-1 explicit stepping, TEST-2 exact ints) and run host tests green.

### Task 1.4: on_wound (damage only while exposed; phase advance; defeat)

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Failing test**

```cpp
TEST(boss_wound_only_while_exposed){
    BossState b; b.reset();
    b.on_wound(WOUND_DMG);          // NOT exposed -> no effect (shielded)
    CHECK_EQ(b.hp, KING_MAX_HP);
    b.on_light_hit();
    b.on_wound(WOUND_DMG);          // exposed -> subtract
    CHECK_EQ(b.hp, KING_MAX_HP - WOUND_DMG);
}

TEST(boss_wound_crossing_threshold_advances_phase){
    BossState b; b.reset();
    // wound down to just above P1_END while exposed
    b.on_light_hit();
    while(b.hp > P1_END_HP) b.on_wound(WOUND_DMG); // 90 -> 60 (crosses)
    CHECK_EQ(b.hp, P1_END_HP);
    CHECK_EQ((int)b.exposed_return, (int)BossPhase::P2); // underlying phase advanced
    CHECK_EQ(b.phase_start_hp, P1_END_HP);
}

TEST(boss_wound_to_zero_defeats){
    BossState b; b.reset(); b.on_light_hit();
    for(int i = 0; i < KING_MAX_HP / WOUND_DMG; ++i) b.on_wound(WOUND_DMG);
    CHECK(b.defeated());
    CHECK_EQ((int)b.phase, (int)BossPhase::Defeated);
}
```

- [ ] **Step 2: Run, verify fail.**
- [ ] **Step 3: Implement** `on_wound`:

```cpp
void advance_phase_for_hp(){
    // Update the UNDERLYING phase (exposed_return) based on HP thresholds.
    BossPhase np = (hp <= 0)        ? BossPhase::Defeated
                 : (hp <= P2_END_HP)? BossPhase::P3
                 : (hp <= P1_END_HP)? BossPhase::P2
                 :                    BossPhase::P1;
    if(hp <= 0){ phase = BossPhase::Defeated; return; }
    if(np != exposed_return){ exposed_return = np; phase_start_hp = hp; }
}
void on_wound(int dmg){
    if(!exposed() || defeated()) return;   // shielded or dead -> immune
    hp -= dmg; if(hp < 0) hp = 0;
    advance_phase_for_hp();
}
```
Note: while EXPOSED, `phase==Exposed` and the real phase rides in `exposed_return`; advancing updates `exposed_return` so when the window closes `tick()` returns to the NEW phase. Defeat overrides `phase` immediately (so `defeated()` is true mid-window).

- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): BossState.on_wound damage/phase-advance/defeat (M11 P1.4)"`

BEFORE marking complete: review vs testing-pitfalls; cover shielded-immunity, threshold crossing, and defeat-at-zero; run host tests green.

### Task 1.5: on_player_death (full-fight restart)

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Failing test**

```cpp
TEST(boss_player_death_restarts_full_fight){
    BossState b; b.reset();
    b.on_light_hit();
    while(b.hp > P2_END_HP) b.on_wound(WOUND_DMG); // deep into the fight (P3)
    b.on_player_death();
    CHECK_EQ((int)b.phase, (int)BossPhase::P1);
    CHECK_EQ(b.hp, KING_MAX_HP);
    CHECK_EQ(b.phase_start_hp, KING_MAX_HP);
    CHECK_EQ(b.expose_timer, 0);
    CHECK_EQ(b.attack_timer, 0);
    CHECK(!b.exposed());
}
```

- [ ] **Step 2: Run, verify fail.**
- [ ] **Step 3: Implement** — `void on_player_death(){ reset(); }` (full-fight restart per the spec; the design decision was "Full-fight restart"). Keep it as its own named method (not just calling `reset()` at the call site) so the intent is explicit and M12 can override per-boss.
- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): BossState.on_player_death full-fight restart (M11 P1.5)"`

BEFORE marking complete: review vs testing-pitfalls; confirm ALL fields reset; run host tests green.

### Task 1.6: Attack-pattern accessor + SWITCH_BUDGET invariant

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md. This task encodes the shared-button balance guarantee from spec §2 as a host-tested invariant.

- [ ] **Step 1: Failing test** — the budget invariant (must FAIL if any window < budget) + a determinism check:

```cpp
TEST(switch_budget_invariant_all_windows){
    // No telegraph window and no expose window may be tighter than the worst-case
    // time to cycle (L) to the right spell + react + cast. If this fails, a window
    // was set below SWITCH_BUDGET -> the phase would be unfair regardless of skill.
    for(int i = 0; i < 3; ++i)
        CHECK(PHASE_PATTERNS[i].telegraph_frames >= SWITCH_BUDGET);
    CHECK(EXPOSE_FRAMES >= SWITCH_BUDGET);
}

TEST(phase_pattern_escalates_but_respects_budget){
    // Telegraph shrinks each phase (escalation) but never below the floor.
    CHECK(PHASE_PATTERNS[0].telegraph_frames >= PHASE_PATTERNS[1].telegraph_frames);
    CHECK(PHASE_PATTERNS[1].telegraph_frames >= PHASE_PATTERNS[2].telegraph_frames);
    CHECK(PHASE_PATTERNS[2].telegraph_frames >= SWITCH_BUDGET);
}

TEST(boss_phase_step_is_deterministic){
    // Same phase + same attack_timer -> same pattern step (no hidden state/RNG).
    BossState a; a.reset(); BossState b; b.reset();
    for(int i=0;i<40;++i){ a.tick(); b.tick(); }
    CHECK_EQ(a.attack_timer, b.attack_timer);
    CHECK_EQ((int)a.current_step(), (int)b.current_step());
}
```

- [ ] **Step 2: Run, verify fail** (`current_step()` not defined yet).
- [ ] **Step 3: Implement** a small deterministic step accessor (telegraph vs active vs recovery, derived from `attack_timer` and the active phase's `PhasePattern`):

```cpp
enum class AttackStep : uint8_t { Telegraph=0, Active, Recovery };
AttackStep current_step() const {
    BossPhase active = (phase==BossPhase::Exposed) ? exposed_return : phase;
    int i = (active==BossPhase::P2)?1 : (active==BossPhase::P3)?2 : 0;
    const PhasePattern& pp = PHASE_PATTERNS[i];
    if(attack_timer < pp.telegraph_frames) return AttackStep::Telegraph;
    if(attack_timer < pp.telegraph_frames + pp.attack_active_frames) return AttackStep::Active;
    return AttackStep::Recovery;
}
```

- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): boss attack-step accessor + SWITCH_BUDGET invariant test (M11 P1.6)"`

BEFORE marking complete: review vs testing-pitfalls; the budget invariant MUST be non-vacuous — temporarily set `PHASE_PATTERNS[2].telegraph_frames = 30` and confirm the test FAILS, then revert. Run host tests green.

**After completing Phase 1:**
Review the batch from multiple perspectives. Minimum 3 review rounds. If round 3 still finds issues, keep going until clean. Confirm: NO `bn::` anywhere in `boss.h` (run `python tools/check_logic_purity.py`); all timers/HP integer; the EXPOSED-overlay model is internally consistent (wounding mid-window advances `exposed_return`; `tick` returns to it); the budget invariant fails-on-broken.

---

## Phase 2 — Save v5: World.beaten + migrations

**Execution Status:** ✅ SHIPPED at `649c7c4`, `20c45ee` on 2026-06-15 (World.beaten in padding byte [17]; SAVE_VERSION 5; checksum_v5; v1–v4→v5 migrations; sizeof==20; 409/409 green. See Deviations re: test_world_state_v3.cpp.)

**Why this matters:** the game's first persisted completion flag. The change MUST keep `sizeof(SaveData)==20` (store `beaten` in existing padding) and cover EVERY migration (TEST-4).

**Pitfalls:** IMPL-4 (SRAM only via `bn::sram` — but that's the engine `save.cpp`; the logic here is pure serialization), TEST-4 (cover corrupted/empty SRAM + every migration v1/v2/v3/v4→v5). Reference the existing pattern in `include/logic/world_state.h:103-155` and `test/test_world_state_v4.cpp`.

### Task 2.1: World.beaten + SaveData v5 + checksum_v5 + migrations

**Files:** Modify `include/logic/world_state.h`. Create `test/test_world_state_v5.cpp`.

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

Context (current layout, `world_state.h:56-78`): `SaveData` is 20 bytes; `[16]=lives`, `[17..19]=compiler padding`. We add `uint8_t beaten` as a real field at offset `[17]`, leaving `[18..19]` as padding — `sizeof` stays 20. `checksum_v5` extends `checksum_v4` to also cover `[17]`.

- [ ] **Step 1: Write the failing tests** (`test/test_world_state_v5.cpp`) — current-version round-trip + every migration + corruption (mirror `test_world_state_v4.cpp` structure):

```cpp
#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

TEST(v5_save_version_is_5){ CHECK_EQ((int)SAVE_VERSION, 5); }
TEST(v5_save_data_size_is_20){ CHECK_EQ((int)sizeof(SaveData), 20); }

TEST(v5_roundtrip_beaten_true){
    World w; w.free_spronk(1); w.beaten = true; w.lives = 4;
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 5);
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.beaten == true);
    CHECK_EQ((int)w2.lives, 4);
}
TEST(v5_roundtrip_beaten_false_default){
    World w; SaveData s = make_save(w);
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.beaten == false);
}
TEST(v5_checksum_covers_beaten_byte){
    World w; w.beaten = true; SaveData s = make_save(w);
    uint8_t buf[sizeof(SaveData)]; std::memcpy(buf, &s, sizeof(s));
    buf[17] ^= 0xFFu; // corrupt beaten without recomputing checksum
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2; CHECK(load_save(s2, w2) == false);
}
TEST(v4_on_disk_migrates_to_v5_beaten_false){
    // Hand-build a valid v4 buffer (version=4, checksum_v4). Must load, beaten=false.
    uint8_t buf[sizeof(SaveData)]; std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 4; std::memcpy(buf + 4, &ver, 2);
    uint16_t spr = 0x00FF; std::memcpy(buf + 6, &spr, 2); // all 8 spronks
    uint16_t abil = 0x00FF; std::memcpy(buf + 8, &abil, 2);
    buf[10] = 0;
    uint32_t lat = 0; std::memcpy(buf + 12, &lat, 4);
    buf[16] = 5; // lives (will clamp to max=11; fine)
    // checksum_v4: [0..10] + [12..16]
    uint32_t sum = 0; for(int i=0;i<11;++i) sum+=buf[i]; for(int i=12;i<17;++i) sum+=buf[i];
    buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(s));
    World w; CHECK(load_save(s, w) == true);
    CHECK(w.spronk_freed(8)); CHECK(w.beaten == false);
}
// v1/v2/v3 migration tests: copy the three from test_world_state_v4.cpp and add
// `CHECK(w.beaten == false);` to each.
TEST(v5_empty_sram_rejected){ SaveData s{}; World w2; CHECK(load_save(s, w2) == false); }
TEST(v5_bad_checksum_rejected){ World w; SaveData s = make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s, w2) == false); }
```
ALSO port (adapted to v5 — `make_save` now produces v5) the lives-coverage tests that Task 2.2 deletes from the v4 file, so their coverage is not lost: `v5_roundtrip_zero_lives_clamped_on_load`, `v5_roundtrip_lives_above_max_clamped_on_load` (recompute the buffer checksum with `checksum_v5` over `[0..10]+[12..17]`), and `v5_checksum_covers_lives_byte` (corrupt byte `[16]`, expect reject). These are REQUIRED (TEST-4 + boot-safety coverage).

- [ ] **Step 2: Run, verify fail** (`World.beaten` and v5 don't exist).
- [ ] **Step 3: Implement in `world_state.h`:**
  - Add `bool beaten = false;` to `struct World` (runtime field; place after `latches`).
  - In `struct SaveData`: change `uint8_t lives;` trailing comment + add `uint8_t beaten;` after `lives` (now `[17]`); update the layout comment block to reflect `[17] beaten`, `[18..19] padding`.
  - Bump `static constexpr uint16_t SAVE_VERSION = 5;`
  - Add `checksum_v5` = `checksum_v4` range plus byte `[17]`:
    ```cpp
    inline uint8_t checksum_v5(const uint8_t* b){
        uint32_t s = 0;
        for(int i = 0; i < 11; ++i) s += b[i];   // [0..10]
        for(int i = 12; i < 18; ++i) s += b[i];  // [12..17]: latches + lives + beaten
        return (uint8_t)(s & 0xFF);
    }
    ```
  - `make_save`: set `s.beaten = w.beaten;` and `s.checksum = checksum_v5(...)`.
  - `load_save`: ALL existing migration branches (v1/v2/v3/v4) set `out.beaten = false;`. Add a `version == 5` branch (mirror v4: validate `checksum_v5`, copy fields incl. `out.beaten = s.beaten;`, then `clamp_lives_on_load`).
  - Keep the v4 branch INTACT (a real v4 save on disk must still load + migrate) — do NOT delete it.
  - Update the `static_assert(sizeof(SaveData) == 20, ...)` message to say "v5".

- [ ] **Step 4: Run, verify green** — `bash tools/host_test.sh`.
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): save v5 — persist World.beaten in padding byte + v1-v4->v5 migrations (M11 P2.1)"`

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md — TEST-4 demands corrupted/empty + every migration. Confirm v1, v2, v3, v4-on-disk, v5-roundtrip, empty, bad-magic, bad-checksum, checksum-covers-beaten are ALL present.
2. Confirm `sizeof(SaveData)==20` unchanged.
3. Run `bash tools/host_test.sh` and confirm green.

### Task 2.2: Repurpose test_world_state_v4.cpp (current→migration)

**Files:** Modify `test/test_world_state_v4.cpp`.

BEFORE starting work: read docs/pitfalls/testing-pitfalls.md.

Context: `test_world_state_v4.cpp` currently calls `make_save()` and asserts `version==4` / `SAVE_VERSION==4`. After Task 2.1, `make_save()` produces v5 and `SAVE_VERSION==5`, so those tests are now WRONG (they would fail). The v5 file (Task 2.1) already owns current-version round-trips. This task removes the stale "current==4" assumptions, keeping only what still makes sense.

- [ ] **Step 1:** DELETE from `test_world_state_v4.cpp` every test that calls `make_save()` and asserts v4-as-current: `v4_save_version_is_4`, `v4_roundtrip_lives_survives`, `v4_roundtrip_full_lives`, `v4_roundtrip_zero_lives_clamped_on_load`, `v4_roundtrip_lives_above_max_clamped_on_load`, `v4_bad_magic_rejected`, `v4_bad_checksum_rejected`, `v4_empty_sram_rejected`, `v4_checksum_covers_lives_byte`, `v4_checksum_function_covers_correct_range`, `v4_save_data_size_is_20`. (Their coverage now lives in `test_world_state_v5.cpp`, which uses `make_save`.)
- [ ] **Step 2:** KEEP (these read hand-built on-disk buffers and assert migration, still valid): `v3_migrates_to_v4_sets_lives_to_starting`, `v2_migrates_sets_lives_to_starting`, `v1_migrates_sets_lives_to_starting`. RENAME each `..._to_v4_...`→`..._to_v5_...` only if the name says v4; add `CHECK(w.beaten == false);` to each (they now migrate all the way to a v5 World). Do NOT change the hand-built buffer bytes (a v1/v2/v3 on-disk buffer is unchanged; only the resulting World gains `beaten=false`).
- [ ] **Step 3:** Run `bash tools/host_test.sh`, confirm green.
- [ ] **Step 4: Commit** — `git commit -m "test: repurpose v4 save tests to v5 migration (current-version coverage moved to v5) (M11 P2.2)"`

BEFORE marking complete: confirm no test still asserts `SAVE_VERSION==4` or `make_save` producing v4. Run host tests green.

**After completing Phase 2:**
Review the batch (min 3 rounds). Confirm: `sizeof(SaveData)==20`; every migration path (v1/v2/v3/v4 on disk + v5 round-trip) covered AND defaults/persists `beaten`; corruption + empty rejected; checksum covers the beaten byte; purity clean.

---

## Phase 3 — Engine/scene: run_boss() + King + art

**Execution Status:** ⬜ NOT STARTED

**Execution Status:** ✅ SHIPPED at `9a8204f`,`021fd4b`,`b565821`,`71c69bb` on 2026-06-15 (King art; `scene_boss.{h,cpp}` run_boss — Light-expose, wound-only-while-exposed, attacks-frozen-while-exposed, M9 death/lives/full-fight-restart, magic crystal, fixed camera; ROM builds; reviewed against the BossState contract. Deviations: P3 uses a single descending projectile placeholder (not 2–3) per "keep minimal"; added a 60-frame respawn i-frame grace; persists `lose_life` immediately.)

**Why this matters:** the playable fight. The DECISIONS are already host-tested in Phase 1 — this scene only renders `BossState` and routes input/damage through existing pools. Scene code is NOT host-testable (it uses `bn::`), so correctness rides on the Phase-1 logic tests + manual emulator QA (Phase 4). Keep ALL new decision logic in `BossState` — do NOT add gameplay branching here that isn't backed by a logic test.

**Pitfalls:** IMPL-1 (`bn::` is allowed here, NOT in logic), IMPL-2 (integer math), IMPL-3 (mutate sprites only via Butano APIs). Build gate per task: `bash tools/build_rom.sh` → `ROM fixed!` (host tests still run for any logic touched).

**On TDD for this phase (mandate satisfied differently):** scene code uses `bn::` and is NOT host-testable, so the usual per-task "failing test → green" loop does not apply here. The quality-gate intent is met three other ways, which are REQUIRED for every Phase-3 task: (a) ALL decision logic is already host-tested in `BossState` (Phase 1) — do NOT introduce gameplay decisions in the scene that aren't backed by a Phase-1 logic test; (b) each task ends with a green `bash tools/build_rom.sh` (`ROM fixed!`) and, if any `logic/` file was touched, a green `bash tools/host_test.sh`; (c) behavior is verified in the Phase-4.6 emulator QA pass. Read `docs/pitfalls/implementation-pitfalls.md` before starting any task in this phase.

### Task 3.1: King + arena placeholder art

**Files:** Create `graphics/king.bmp`+`graphics/king.json` (and any arena decoration) via `tools/make_placeholder_art.py` (mirror how `light_proj`/`magic_crystal` were added in M10). Reuse `light_proj` (Light), existing bolt/fire/ice projectile sprites, and `magic_crystal`.

- [ ] **Step 1:** Add a King entry to `tools/make_placeholder_art.py` (a readable large-ish placeholder, e.g. a 32x32 dark/violet figure with a distinct silhouette; obey GBA 4bpp/16-color + sprite-size limits — use a Butano-supported sprite size).
- [ ] **Step 2:** Generate the art; confirm `graphics/king.json` is valid (palette ≤16 colors).
- [ ] **Step 3:** `bash tools/build_rom.sh` → confirm `ROM fixed!` (art imports cleanly).
- [ ] **Step 4: Commit** — `git commit -m "art: King placeholder sprite (M11 P3.1)"`

BEFORE marking complete: confirm the sprite imports without Butano palette/size errors; ROM builds.

### Task 3.2: scene_boss skeleton — arena load, King render, BossState tick + attack playback

**Files:** Create `include/game/scene_boss.h`, `src/game/scene_boss.cpp`.

**Implementation context (IMPORTANT — there is NO shared helper to call):** `play_room` is a ~1000-line `static` function inside `scene_dungeon.cpp:132`; its player/pool/HUD/camera setup is NOT exposed as a reusable API. "Reuse" here means **copy the relevant setup pattern**, modeling on `scene_dungeon.cpp`, using these concrete types/values (all already in the codebase):
- Player + input: `logic::Player` (`logic/player.h`), the input gather + per-frame movement exactly as `play_room` does (see `scene_dungeon.cpp` around the `in.*` / `player` update + `engine/input.h`, `engine/spell_input.h`).
- Pools: `engine::BoltPool bolts(...)` and `engine::SpellPool spells(...)` constructed as at `scene_dungeon.cpp:177-178`; cast via `spells.update_and_cast(cast_spell, ps.spell, magic, muzzle, player.facing, lvl.map)` (returns the fired `logic::SpellId`) and `bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map)` as at `scene_dungeon.cpp:659-661`.
- Vitals: use the carried-in `ps.health` / `ps.magic` (references, NOT new meters) — exactly as `play_room` binds them at `scene_dungeon.cpp:166-167`. A local `int invuln = 0;` provides player i-frames (decremented each frame, like `play_room`).
- HUD/camera: `engine::Hud` + `hud.update(ps.health, ps.magic, world.lives)`; FIXED camera (set once at the arena center — do NOT scroll-follow).
- Arena room access: `const logic::LevelData& level = *arena.rooms[arena.start_room];` (the arena is a 1-room `DungeonData` — see Task 4.2).

Do NOT refactor `play_room` or extract a shared helper in this milestone (that is the deferred share-player-ability-controller refactor; out of scope here).

> **Ordering note:** Task 4.2 (arena + approach level data) is a dependency of Tasks 3.2–3.4 — do it BEFORE 3.2 per the "Recommended execution order" near the top of this plan.

- [ ] **Step 1:** `include/game/scene_boss.h`:
```cpp
#pragma once
#include "logic/world_state.h"
#include "logic/player_state.h"
#include "logic/level_data.h"
namespace game {
enum class BossResult { Victory, QuitToTitle };
BossResult run_boss(const logic::DungeonData& arena, logic::World& world, logic::PlayerState& ps);
}
```
Note: `run_boss` has exactly TWO exits — `Victory` (King defeated) and `QuitToTitle` (chosen at the Game-Over screen when lives hit 0). There is intentionally NO mid-fight quit-to-hub (no `Q`/SELECT quit inside the arena); once you enter, you win or game-over. Do NOT add a hub-return path.
- [ ] **Step 2:** `src/game/scene_boss.cpp`: define `KING_SPAWN_TX`/`KING_SPAWN_TY` constants (matching the arena `.txt` marker from Task 4.2). Load the arena room (`const logic::LevelData& level = *arena.rooms[arena.start_room];`), spawn the player at `logic::find_entrance(level, 0)`, create a `logic::Body king_body` sized to the King sprite positioned at `KING_SPAWN_*` + the King `bn::sprite_ptr`, instantiate `logic::BossState b; b.reset();`, and start the fight at full vitals (`ps.health.cur = ps.health.max; ps.magic.cur = ps.magic.max;` — and sync `ps.health.max = logic::max_health_for(world)` first, as `run_dungeon` does at `scene_dungeon.cpp:975`). Run the main loop: player update (per the Implementation-context types above), `b.tick()`, render the King + a telegraph/attack VFX driven by `b.current_step()` and `b.exposed()` (e.g. King sprite flashes/visibly changes while EXPOSED; a telegraph tint during `AttackStep::Telegraph`). Fixed camera (no scroll).
- [ ] **Step 3:** Attack contact. Use these CONCRETE placeholder attacks (geometry is scene-side — positions need `bn::`; the TIMING already lives in `PHASE_PATTERNS`). All are simple moving `logic::Body` hitboxes spawned at the start of `AttackStep::Active` and cleared at `Recovery`; all QA-tunable in 4.6:
  - **P1 (aerial sweep):** one horizontal projectile-body crossing the arena at the player's approximate height (telegraphed by a tint during `Telegraph`). Dodge by being elsewhere vertically (grapple/glide).
  - **P2 (ground slam):** a ground-level shockwave body expanding outward from the King along the floor; dodge by being airborne (dash/jump).
  - **P3 (spread):** two-to-three projectile-bodies in a fan from the King; the bullet-hell finish.
  On overlap with the player AND `invuln==0`: `ps.health.damage(20); invuln = 45;` (the dungeon's contact convention at `scene_dungeon.cpp:720,752`). Render each with a placeholder VFX (reuse the bolt sprite like the vine/pound VFX). Do NOT add more attack variety than the three above — keep it minimal for this framework pass.
  - **Spawn/apply attacks ONLY while `!b.exposed()`.** During the expose window the King is stunned and the attack pattern is frozen in logic (Task 1.3), so the scene must also stop spawning attacks and clear/hide any active attack body + its VFX — the expose window is a clean damage window (no contact damage while exposed).
- [ ] **Step 4:** `bash tools/build_rom.sh` → `ROM fixed!`. (No host test — logic is already covered; this is rendering.)
- [ ] **Step 5: Commit** — `git commit -m "feat(game): run_boss skeleton — arena, King render, attack playback (M11 P3.2)"`

BEFORE marking complete: ROM builds; the King visibly cycles telegraph→attack→recovery; EXPOSED state is visually distinct. No new decision logic outside BossState.

### Task 3.3: Damage resolution — Light exposes, bolt/elements wound (only while exposed)

**Files:** Modify `src/game/scene_boss.cpp`.

Context: reuse the EXACT damage hooks the dungeon uses — `SpellPool::consume_hit(body, SpellId)` and `BoltPool::consume_hit(body)` (see `scene_dungeon.cpp:681-717`). The King is a `logic::Body`.

- [ ] **Step 1:** Each frame, after updating the pools: if `spells.consume_hit(king_body, logic::SpellId::Light)` → `b.on_light_hit();` (mirror the M10 Light-clears-DarkVeil hook, repointed to expose). 
- [ ] **Step 2:** For wounding, ONLY while `b.exposed()`: if `bolts.consume_hit(king_body)` OR `spells.consume_hit(king_body, logic::SpellId::Fire)` OR `spells.consume_hit(king_body, logic::SpellId::Ice)` → `b.on_wound(logic::WOUND_DMG);` and on a successful ELEMENTAL wound also `ps.magic.heal(25);` (matches the dungeon's kill-refills-magic feel at `scene_dungeon.cpp:714-717`, sustaining casts). While NOT exposed (`!b.exposed()`), the King is immune — do NOT call `consume_hit` against the King for wounding (so the projectile passes/expires), OR call it and ignore the result; either way `on_wound` is never invoked while shielded. Note: the Light `consume_hit` in Step 1 runs every frame regardless of exposed state (Light always exposes/refreshes).
- [ ] **Step 3:** On `b.defeated()` → break the loop and return `BossResult::Victory` (the caller handles the ending). 
- [ ] **Step 4:** `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 5: Commit** — `git commit -m "feat(game): boss damage — Light exposes, bolt/elements wound only while exposed (M11 P3.3)"`

BEFORE marking complete: ROM builds; in QA-later terms, the wiring matches the host-tested BossState contract (Light→expose, wound-only-while-exposed, defeat→Victory). No chip damage while shielded.

### Task 3.4: Death / lives / Game-Over + full-fight restart + magic crystal

**Files:** Modify `src/game/scene_boss.cpp`.

Context: reuse the M9 flow from `scene_dungeon.cpp` (`run_dungeon`'s GameOver handling at the bottom, and the death/`lose_life` block). The magic crystal uses the M10 `MagicCrystalSpawn` overlap→full-refill + reset-on-respawn pattern.

- [ ] **Step 1:** Define a `restart_fight()` helper (lambda/local fn) used by every restart path: `b.on_player_death();` (BossState → P1 full HP, timers cleared), `ps.health.cur = ps.health.max; ps.magic.cur = ps.magic.max;`, reset the player `Body` to `find_entrance(level, 0)`, reset the magic crystal to un-collected, clear `invuln`. When player `ps.health.is_empty()`: `logic::lose_life(world);` then if `world.lives > 0`: call `restart_fight()` and continue the same `while` loop — **the approach is NOT replayed** (we're already inside `run_boss`). Mirror the M9 death block at `scene_dungeon.cpp:817-835`.
- [ ] **Step 2:** If `world.lives == 0`: call `game::run_game_over(world)`; then `logic::refill_lives(world); engine::write_world(world);` (M9 empty-bar fix + persist the refilled lives); on `Continue` → call `restart_fight()` (from Step 1; it refills vitals + resets the fight) and continue the loop; on `QuitToTitle` → return `BossResult::QuitToTitle`. (`run_game_over` is the SAME scene the dungeons use — `include/game/scene_game_over.h`.)
- [ ] **Step 3:** Magic crystal: spawn from the arena `MagicCrystalSpawn`; on player overlap, `magic.cur = magic.max;` reset it to un-collected on every respawn/restart (so each attempt has it). Reachable by base movement (verified by the Phase-4 invariant).
- [ ] **Step 4:** `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 5: Commit** — `git commit -m "feat(game): boss death/lives/Game-Over + full-fight restart + magic crystal (M11 P3.4)"`

BEFORE marking complete: ROM builds; lives decrement on death; lives==0 routes to Game-Over; Continue restarts the fight (not the approach); QuitToTitle returns; crystal refills + resets each attempt.

**After completing Phase 3:**
Review the batch (min 3 rounds). Confirm: NO decision logic in the scene that isn't backed by a BossState logic test; damage uses the existing pools; M9 flow reused (not reimplemented); fixed camera; ROM builds. Logic purity still clean (`python tools/check_logic_purity.py`).

---

## Phase 4 — Content + integration + invariants + QA

**Execution Status:** ✅ SHIPPED — 4.1–4.5 at `4e84503`,`6e93f25`,`1424bd1`,`31c2e6c`,`1b02c94`; 4.6 emulator QA complete after 9 rounds (combat polish — see Deviations). 419/419 host tests, ROM builds, user-confirmed.

**Why this matters:** wires Door 9 → approach → arena → ending into a playable, winnable game, and guards it with the no-soft-lock invariant harness. Includes the balance QA pass.

**Pitfalls:** the no-soft-lock invariant tests MUST be able to FAIL on a deliberately-broken layout (non-vacuity), per the M8–M10 discipline. Reuse the 2-wide flood-fill from `test/test_dungeon8_level.cpp`. Hub re-spacing must keep all 9 doors on-map and on the floor row.

### Task 4.2: Approach rooms + boss-arena level data

> Numbered 4.2 to match the file-structure grouping, but executed EARLY (dependency of Phase 3 — see the ordering note in Task 3.2).

**Files:** Create `tools/levels/dungeon9_room0.txt/.json`, `dungeon9_room1.txt/.json` (2-room approach, no combat), `dungeon9_arena.txt/.json` (boss arena) → generate `include/game/levels/dungeon9_room0.h`, `dungeon9_room1.h`, `dungeon9_arena.h` via `tools/build_level.py`. Then assemble the `DungeonData` symbols in `include/game/levels/dungeons.h`, following the EXACT DUNGEON8 pattern there (`dungeons.h:14-16` includes + `dungeons.h:48-50` arrays). Add:
```cpp
#include "game/levels/dungeon9_room0.h"
#include "game/levels/dungeon9_room1.h"
#include "game/levels/dungeon9_arena.h"
// D9 approach: 2 traversal rooms (no combat); room1 exit hands off to the boss arena.
inline constexpr const logic::LevelData* DUNGEON9_APPROACH_ROOMS[] = { &DUNGEON9_ROOM0_DATA, &DUNGEON9_ROOM1_DATA };
inline constexpr logic::DungeonData DUNGEON9_APPROACH{ DUNGEON9_APPROACH_ROOMS, 2, 0 };
// D9 arena: a single boss room (run_boss reads arena.rooms[arena.start_room]).
inline constexpr const logic::LevelData* DUNGEON9_ARENA_ROOMS[] = { &DUNGEON9_ARENA_DATA };
inline constexpr logic::DungeonData DUNGEON9_ARENA{ DUNGEON9_ARENA_ROOMS, 1, 0 };
```
(The generated symbol names — `DUNGEON9_ROOM0_DATA`, `DUNGEON9_ROOM1_DATA`, `DUNGEON9_ARENA_DATA` — follow `build_level.py`'s `<UPPER_BASENAME>_DATA` convention; confirm against the generated headers.) Model the room files on the D8 room files.

BEFORE starting work: read docs/pitfalls/implementation-pitfalls.md.

- [ ] **Step 1:** Author `dungeon9_room0` + `room1`: a 2-room traversal runway (≥30 wide, standard floor convention content row 18 / floor row 20 per the M10 convention), exercising carried traversal powers (e.g. a grapple anchor + glide gap in room0; a dash gap + featherleap ledge in room1), NO enemies/combat. Room0 has the player entrance + a `room_door` to room1 (target_room=1) + the M9 hub-return `Q` door; room1 has its return `room_door` back to room0 + the **dungeon exit tile** (`has_exit=true`, `has_cage=false`) — landing on it makes `run_dungeon` return `Cleared`, which `main.cpp` (Task 4.3) treats as "reached the arena handoff." **Intentionally minimal (framework/testing pass) — may be expanded later** (note this in a comment in the .txt header).
- [ ] **Step 2:** Author `dungeon9_arena`: a fixed-screen arena (a flat-ish combat floor with a couple of platforms + a grapple anchor for P1, a weak/`pound`-target platform for P2, a hazard to freeze for P3), ONE `MagicCrystalSpawn` reachable by base movement, the player entrance (id 0). The arena has **`has_exit=false` and `has_cage=false`** — the fight ends via boss defeat inside `run_boss`, NOT by landing on an exit tile, and there is no spronk here. **King position:** the King has no level-data spawn type; instead define named constants `KING_SPAWN_TX`/`KING_SPAWN_TY` in `scene_boss.cpp` (Task 3.2) and place a comment marker in `dungeon9_arena.txt` at the matching tile so the authored arena and the code agree. Keep the arena traversable so the expose-opening positions are reachable each phase.
- [ ] **Step 3:** Generate headers (`bash tools/host_test.sh` regenerates them; or run `tools/build_level.py` per file), then `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 4: Commit** — `git commit -m "content: D9 approach (2 rooms) + King arena level data (M11 P4.2)"`

BEFORE marking complete: headers generate; ROM builds; spronk/cage NOT present (this is a boss dungeon, not a spronk rescue — `has_cage=false`).

### Task 4.1: Hub Door 9 (re-space to 9 doors)

**Files:** Modify `tools/levels/hub.txt`, `tools/levels/hub.json`, `src/game/scene_hub.cpp`, `test/test_hub_level.cpp`. Regenerate `include/game/levels/hub.h`.

Context: the hub map is 56 wide; the 8 doors sit on row 15 at ~7-col spacing (`#...1......2......3......4......5......6......7......8...#`), the rightmost at col ~53. A 9th door at the same pitch (col ~60) would fall off-map — RE-SPACE to fit 9 doors. `door_enterable` is at `scene_hub.cpp:36-45`.

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Failing test** in `test_hub_level.cpp`: UPDATE any existing assertion that expects 8 doors (e.g. `door_count == 8`) to `== 9` — do NOT leave a stale 8-door assertion (it would fail or contradict). Assert `HUB_DATA.door_count == 9`; all 9 door `tx` within `[1, map_w-2]` and on the floor row; door dungeons are `1..9` in order; keep/extend any existing spacing assertion (recompute expected spacing for the new 9-door layout). Run → fail (until the .txt/.json + regenerated `hub.h` provide 9 doors).
- [ ] **Step 2:** Re-space `hub.txt` row 15 to 9 evenly-spaced doors that all fit (e.g. start col 3, pitch ~6: `1` at 3, `2` at 9, … `9` at 51 — verify each is ≤ `map_w-2` and the digits don't collide). Update `hub.json` if door metadata is enumerated there. Add the 9th door (`9`).
- [ ] **Step 3:** `door_enterable` (`scene_hub.cpp`): add `|| (n == 9 && logic::spronk_count(w) == 8)` (Door 9 opens only when ALL 8 spronks are freed). Confirm the door-render + door-entry loops already iterate `door_count` (they do, `scene_hub.cpp:69,106,206`) so Door 9 renders + is enterable automatically.
- [ ] **Step 4:** Regenerate `hub.h` + run `bash tools/host_test.sh` (hub level test green) and `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 5: Commit** — `git commit -m "feat(hub): Door 9 (opens at 8 spronks), re-spaced to 9 doors (M11 P4.1)"`

BEFORE marking complete: all 9 doors on-map + on the floor; Door 9 enterable iff `spronk_count==8`; existing 8 doors unchanged in behavior; host tests + ROM green.

### Task 4.3: main.cpp dispatch — Door 9 → approach → run_boss → ending

**Files:** Modify `src/main.cpp`. (See current loop at `src/main.cpp:24-48`.)

**Depends on:** Task 3.2 (`game::run_boss`, `game::BossResult`), Task 4.2 (`DUNGEON9_APPROACH`, `DUNGEON9_ARENA`), Task 4.4 (`game::run_victory`). Do 4.3 AFTER all three (per the "Recommended execution order").

Context: `run_hub` returns `enter_dungeon` (now `1..9`). For `n==9`: run the 2-room approach as a dungeon, then on clear hand off to `run_boss`, then on Victory persist `beaten` + show the ending. A `DungeonResult::Quit` from the approach (the M9 hub-return `Q` door) needs no special handling — it falls through to the shared `world.current_dungeon = 0; continue;` at the end of the `n==9` block, resuming the hub (same as other dungeons).

BEFORE starting work: read docs/pitfalls/implementation-pitfalls.md.

- [ ] **Step 1:** In the dispatch, handle `n == 9` specially (it is NOT a normal dungeon): 
  ```cpp
  if(n == 9){
      world.current_dungeon = 9;
      game::DungeonResult ar = game::run_dungeon(DUNGEON9_APPROACH, world, ps);
      if(ar == game::DungeonResult::Cleared){
          game::BossResult br = game::run_boss(DUNGEON9_ARENA, world, ps);
          if(br == game::BossResult::Victory){
              world.beaten = true;
              engine::write_world(world);     // persist completion
              game::run_victory(world);       // "THE END" screen
          }
          // QuitToTitle (and post-victory) -> fall through to title/hub
          game::run_title(world);
      } else if(ar == game::DungeonResult::QuitToTitle){
          game::run_title(world);
      }
      world.current_dungeon = 0;
      continue;
  }
  ```
  (Confirm the approach `DungeonResult::Quit` path resumes the hub like other dungeons.) Include the new headers (`game/scene_boss.h`, `game/scene_victory.h`, the D9 level headers).
- [ ] **Step 2:** Keep the existing `n==1..8` dispatch unchanged.
- [ ] **Step 3:** `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 4: Commit** — `git commit -m "feat(game): Door 9 dispatch — approach -> run_boss -> persisted victory (M11 P4.3)"`

BEFORE marking complete: ROM builds; the approach not replayed after entering the arena; victory persists `beaten` before the ending; D1–D8 dispatch unchanged.

### Task 4.4: Victory screen + title completion reflect

**Files:** Create `include/game/scene_victory.h`, `src/game/scene_victory.cpp`. Modify `src/game/scene_title.cpp`.

Context: model `run_victory` on `run_title` (`scene_title.cpp`) — a text screen with a fade, returns on START.

BEFORE starting work: read docs/pitfalls/implementation-pitfalls.md.

- [ ] **Step 1:** `run_victory(const logic::World&)`: fade in; render "THE END" + "CONGRATULATIONS" (+ a small flourish line, e.g. spronk count); wait for START; fade out; return. Mirror `scene_title.cpp`'s text-gen + fade pattern.
- [ ] **Step 2:** `scene_title.cpp`: when `world.beaten`, add ONE extra centered text line reading `GAME COMPLETE` (generate it like the existing title/prompt lines at `scene_title.cpp:22-26`, e.g. at y≈40, into its own `bn::vector`). Keep the existing title + START prompt + START behavior unchanged. Do NOT redesign the title screen, change fonts/colors, or alter the START flow — this is a single additive line gated on `world.beaten`.
- [ ] **Step 3:** `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 4: Commit** — `git commit -m "feat(game): victory 'THE END' screen + title completion reflect (M11 P4.4)"`

BEFORE marking complete: ROM builds; victory screen shows + returns on START; a beaten save shows completion on the title.

### Task 4.5: No-soft-lock invariants for the approach + arena

**Files:** Create `test/test_dungeon9_level.cpp`. Model on `test/test_dungeon8_level.cpp` (2-wide×4-tall flood-fill).

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Write the invariants** (each must FAIL on a deliberately-broken grid — non-vacuity gate):
  - `d9_approach_traversable`: with the carried kit (all 8 abilities), each approach room's entrance reaches its exit (2-wide flood-fill). REUSE the ability-reachability conventions already established in prior dungeon level tests rather than inventing new ones — read `test/test_dungeon4_level.cpp` (glide gaps), `test/test_dungeon5_level.cpp` (dash gaps), `test/test_dungeon6_level.cpp` (grapple reach), `test/test_dungeon8_level.cpp` (2-wide climb/flood-fill) and apply the same gap/reach allowances. If the approach rooms only use gaps already covered by those conventions (recommended — keep the approach simple), no new reachability modeling is needed.
  - `d9_arena_magic_crystal_reachable`: the arena's `MagicCrystalSpawn` is reachable from the player entrance by BASE movement (no magic/Light needed) — no magic soft-lock.
  - `d9_arena_expose_positions_reachable`: the position(s) from which the player can land a Light hit on the King (per phase: P1 aerial via grapple-anchor, P2 ground, P3 via a frozen foothold) are reachable — the arena is solvable. Model conservatively: assert that each intended firing tile (the grapple-anchor approach tile, a ground tile adjacent to the King, the frozen-foothold base tile) is within the 2-wide flood-fill reachable set from the entrance. These exact tile coordinates are authored in Task 4.2 — document them as comments in `dungeon9_arena.txt` AND reference the same constants in the test so the two cannot silently drift.
  - `d9_no_pit_traps` / `d9_no_one_way_traps`: a fall in the arena/approach lands somewhere escapable (M8/M9 guards).
  - `d9_arena_no_cage`: the arena has `has_cage==false` and a valid player entrance (it's a boss room, not a spronk rescue).
- [ ] **Step 2:** Run → confirm they pass on the real layout; then temporarily break the arena (e.g. wall off the crystal) and confirm the relevant invariant FAILS; revert.
- [ ] **Step 3:** `bash tools/host_test.sh` green.
- [ ] **Step 4: Commit** — `git commit -m "test: D9 approach + arena no-soft-lock invariants (fail-on-broken) (M11 P4.5)"`

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md.
2. Confirm EACH invariant fails on a deliberately-broken layout (non-vacuity) — document the break/revert you tested.
3. Run `bash tools/host_test.sh` and confirm green.

### Task 4.6: Emulator QA + balance pass (handed to user)

**Files:** none (tuning may touch `include/logic/boss.h` constants only).

This is the manual round-based QA loop (as in M3–M10). The user plays the ROM in mGBA; iterate on findings.

- [ ] **Step 1:** Build the ROM (`bash tools/build_rom.sh`), hand to the user.
- [ ] **Step 2:** Balance levers to tune (all named constants in `boss.h`): `SWITCH_BUDGET` (must stay ≥ the real worst-case; the invariant in Task 1.6 enforces every window ≥ it after any change), `EXPOSE_FRAMES`, `KING_MAX_HP`/`P1_END_HP`/`P2_END_HP`/`WOUND_DMG`, and the `PHASE_PATTERNS` telegraph/active/recovery. After ANY constant change, re-run `bash tools/host_test.sh` so the SWITCH_BUDGET + escalation invariants re-verify.
- [ ] **Step 3:** Verify in-emulator: all 3 phases solvable; Light exposes; no chip damage while shielded; the shared-button switch never feels impossible (no window tighter than a cycle-and-cast); magic crystal prevents soft-lock; death restarts the whole fight (approach not replayed); lives/Game-Over correct; victory screen + persisted completion + title reflect; all 8 abilities genuinely used across the phases.
- [ ] **Step 4:** Iterate per user feedback (each fix its own commit), re-running host tests + ROM each round.

BEFORE marking complete: user confirms the fight is fair + winnable; host tests green; ROM builds; purity clean.

**After completing Phase 4:**
Review the batch (min 3 rounds). Confirm: Door 9 gated on 8 spronks; approach + arena solvable + invariants fail-on-broken; victory persists `beaten`; title reflects; D1–D8 unregressed; no save-format regression beyond the v5 add; purity clean; ROM builds.

---

## Final review + finish

After all four phases:
1. Run the full host suite (`bash tools/host_test.sh` → N/N green), purity (`python tools/check_logic_purity.py`), ROM (`bash tools/build_rom.sh` → `ROM fixed!`).
2. Dispatch a final full-branch code review over `git diff main...HEAD` (three-layer purity, no leftover scaffold/debug, the BossState↔scene wiring, save v5 + all migrations, no-soft-lock non-vacuity, D1–D8 unregressed, SWITCH_BUDGET invariant intact).
3. Use superpowers:finishing-a-development-branch (present the 4 options; merge per the user's choice).

---

## Self-Review (writing-plans)

- **Spec coverage:** §1 identity → P3/P4 (Door 9, arena, win state); §2 fight loop + phases + magic + SWITCH_BUDGET → P1 (constants/invariant) + P3 (wiring) + P4.6 (tune); §3 BossState → P1; §4 encounter/scene wiring → P3 + P4.1/4.2/4.3; §5 death/lives → P3.4; §6 victory + save v5 → P2 + P4.3/4.4; §7 new-vs-reused → file structure; §9 testing → P1 tests, P2 migration tests (TEST-4), P4.5 invariants, P4.6 QA; §10 success criteria → covered across phases.
- **Placeholders:** none — all constants have concrete starting values (QA-tuned in P4.6); all test code is real.
- **Type consistency:** `BossPhase`/`BossState`/`AttackStep`/`PhasePattern`/`BossResult`/`World.beaten`/`checksum_v5`/`DUNGEON9_APPROACH`/`DUNGEON9_ARENA` used consistently across tasks.
- **Cross-task conflicts:** `world_state.h` only in P2 (2.1 edits, 2.2 edits a different test file); `scene_boss.cpp` only in P3 (sequential 3.2→3.3→3.4); `boss.h` in P1 (sequential) + P4.6 (constant tuning, after P1 ships); `main.cpp` only in 4.3; hub files only in 4.1; arena/approach data only in 4.2. Ordering dependency (4.2 before 3.2) is called out explicitly.
```
