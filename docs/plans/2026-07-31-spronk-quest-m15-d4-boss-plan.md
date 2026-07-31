# M15 — D4 Gale Cliffs Boss "Galewing" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add D4 (Gale Cliffs)'s boss — the airborne "Galewing", which can only be reached by riding the dungeon's own updraft-and-Glide kit — and restructure D4 from one room into three (ascent → boss arena → spronk).

**Architecture:** Everything the fight needs is `BossDef` data plus one new movement mode. `Locomotion::Hovering` is mechanically `Pacing` with a fixed Y origin, so the existing drift branch in `run_boss_fight` widens rather than forks. Three additive `BossDef` fields (`hover_row`, `move_vel_raw`, `aim_horizontal`) carry the altitude, the drift speed, and an explicit aim model that replaces a fragile `perch_count == 0` derivation. The fight uses `VulnMode::AlwaysVulnerable` — no expose state machine — because bolts travel horizontally, so *altitude itself* is the damage gate.

**Tech Stack:** C++17, Butano 21.6.0, GBA (devkitARM). Host tests via `bash tools/host_test.sh`. ROM via `bash tools/build_rom.sh`. Purity guard `python tools/check_logic_purity.py`. Content validator `python tools/validate_dungeons.py`. Three-layer architecture (`include/logic`+`src/logic` pure C++ NO `bn::`; `src/engine` Butano glue; `src/game` scenes).

**Spec:** `docs/superpowers/specs/2026-07-31-spronk-quest-m15-d4-boss-design.md` (approved 2026-07-31). Section references below (§2.3, §3.1, …) point into it.

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

**Overall:** Not started. 0/6 phases shipped.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Boss data (`D4_DEF` + def fields) | ⬜ Not started | — | — |
| 2 — Hovering in the fight loop | ⬜ Not started | — | depends on Phase 1 |
| 3 — Art + pipeline wiring | ⬜ Not started | — | depends on Phase 1 (`BossId`) |
| 4 — D4 1→3 room restructure | ⬜ Not started | — | depends on Phase 1 (`hover_row` for the clearance test) |
| 5 — Wire the boss on + docs | ⬜ Not started | — | depends on Phases 2, 3, 4 |
| 6 — Final gates + mGBA QA | ⬜ Not started | — | human emulator gate; blocks merge |

---

## Global Constraints

Every task's requirements implicitly include this section.

- **Three-layer purity (HARD, IMPL-1):** NO `bn::` types in `include/logic/` or `src/logic/`. `python tools/check_logic_purity.py` MUST print `logic purity OK`.
- **No floating point (IMPL-2):** integer fixed-point only. The drift uses `logic::Fixed::from_raw(...)`, never `float`/`double`.
- **Fixed-point tests assert exact raw integers (TEST-2):** never float-compare.
- **No frame-timing dependence in logic tests (TEST-1):** drive `BossState` with explicit `tick()` counts.
- **Existing boss regression is a hard gate:** every existing assertion in `test/test_boss.cpp` for `KING_DEF` / `D1_DEF` / `D2_DEF` / `D3_DEF` MUST remain **unchanged** and green. If any needs editing, the new `BossDef` fields were placed wrongly — see Task 1.1.
- **No save-format change.** `World.boss_defeats` bit 3 is already reserved for D4 (`include/logic/world_state.h:18`). Do NOT touch `SaveDataV6` or any migration.
- **Green bars:** `bash tools/host_test.sh` MUST end `N/N tests passed, 0 checks failed`; `bash tools/build_rom.sh` MUST end `ROM fixed!` with zero warnings.
- **Art is a MANUAL step:** `python tools/make_placeholder_art.py` is not run by the build. Run it explicitly and commit the `graphics/*` outputs.
- **Emulator lock:** `objcopy: Invalid argument` on `SpronkQuest.gba` means mGBA has the file open. Close mGBA before rebuilding.
- **Level headers are generated:** `bash tools/build_rom.sh` regenerates `include/game/levels/*.h` from `tools/levels/`. Plain `make` does not — run `make levels` first if you use it. Generated headers ARE committed.

---

## Before the first task

`main` is at the merge of the health-review remediation cycle. Create the working branch before Task 1.1:

```bash
git checkout main && git pull --ff-only
git checkout -b feat/m15-d4-boss
```

Every task's commit lands on this branch. Task 6.1's regression check (`git diff main -- test/test_boss.cpp`) assumes it.

---

## File Structure

| File | Responsibility | Touched by |
|---|---|---|
| `include/logic/boss.h` | `Locomotion`/`BossId` enums, `BossDef` struct, `D4_PHASES` + `D4_DEF` | Task 1.1, 1.2 |
| `test/test_boss.cpp` | def-invariant + `BossState` behavior tests | Task 1.2 |
| `src/game/boss_fight.cpp` | the one shared fight loop — hover placement, drift, aim, restart | Task 2.1, 2.2 |
| `tools/make_placeholder_art.py` | `draw_galewing_frame` + `gen_galewing` | Task 3.1 |
| `graphics/galewing.bmp` / `.json` | generated sprite asset (committed) | Task 3.1 |
| `src/game/scene_dungeon.cpp` | `boss_sprite_for` id→sprite row ONLY | Task 3.2 |
| `tools/build_level.py` | `BOSS_SYMBOL['d4']` | Task 3.2 |
| `tools/levels/dungeon4_room{0,1,2}.{txt,json}` | the three room grids + sidecars | Task 4.1, 5.1 |
| `tools/levels/manifest.json` | dungeon 4 room list | Task 4.1 |
| `include/game/levels/dungeons.h` | `DUNGEON4_ROOMS[3]` + includes | Task 4.1 |
| `test/test_dungeon4_level.cpp` | 3-room reachability + the hover-clearance invariant | Task 4.2 |
| `docs/content-recipes.md` | arena constraints (hovering exception) + boss-test correction | Task 5.2 |

**Cross-task conflict control.** Task 1.1 and 1.2 both edit `include/logic/boss.h` — they are sequential, same phase. Tasks 4.1 and 5.1 both edit `tools/levels/dungeon4_room1.json` — 5.1 adds only the `"boss"` key, and MUST run after 4.1. No two tasks in different phases edit the same file.

**Parallel dispatch requires separate worktrees.** Phases 2, 3, and 4 touch disjoint files, but Phase 4 leaves the tree **temporarily uncompilable** between Tasks 4.1 and 4.2 (`test_dungeon4_level.cpp` still references the deleted `DUNGEON4_DATA`). Phase 2's and Phase 3's verification steps run `bash tools/host_test.sh` and would fail on Phase 4's half-finished state through no fault of their own. So: either dispatch Phases 2/3/4 into separate worktrees (`/superpowers:using-git-worktrees`), or run them sequentially in one tree. Do NOT run them concurrently in a shared working directory.

---

# Phase 1 — Boss data

**Execution Status:** ⬜ NOT STARTED

Adds the enum values, the three additive `BossDef` fields, and `D4_DEF` itself. Pure logic layer — no scene code, no Butano. Everything here is host-testable.

## Task 1.1: Additive `BossDef` fields + enum values

**Files:**
- Modify: `include/logic/boss.h` (`Locomotion` ~:28, `BossId` ~:33, `BossDef` struct :61-90, `KING_DEF` :116-132)
- Test: `test/test_boss.cpp` (regression only — no new test in this task)

**Interfaces:**
- Consumes: nothing.
- Produces: `logic::Locomotion::Hovering`; `logic::BossId::D4Galewing`; `BossDef::hover_row` (`int`, tile row of the drift altitude, default `0`); `BossDef::move_vel_raw` (`int`, `Fixed` raw drift speed, default `128`); `BossDef::aim_horizontal` (`bool`, default `false`). Task 1.2, 2.1, 2.2, 3.2, 4.2 all consume these exact names.

**CRITICAL — field placement.** Every existing def uses **positional aggregate initialization**. The three new fields MUST be appended **after `phase_lines[3]`**, the current last member. A field inserted anywhere else silently reassigns King/D1/D2/D3 values (e.g. `KING_DEF`'s `/*perch_count=*/6` would land in the wrong member) and the existing tests would need editing — which is the signal that you got it wrong. Do NOT edit any existing assertion to accommodate a field placement.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Capture the regression baseline**

Run: `bash tools/host_test.sh`
Expected: PASS, ending `N/N tests passed, 0 checks failed`. Record N — the count MUST NOT drop at any point in this task.

- [ ] **Step 2: Append `Hovering` to `Locomotion`**

In `include/logic/boss.h`, replace the enum (currently `enum class Locomotion : uint8_t { Stationary, Pacing };`) with:

```cpp
// M15: Hovering = holds an ALTITUDE (BossDef::hover_row) and drifts horizontally, ignoring
// gravity and the floor. Mechanically Pacing with a fixed Y origin — run_boss_fight shares
// one drift branch for both. Appended LAST so Stationary/Pacing keep their values.
enum class Locomotion : uint8_t { Stationary, Pacing, Hovering };
```

- [ ] **Step 3: Append `D4Galewing` to `BossId`**

```cpp
enum class BossId : uint8_t { King = 0, D1Guardian, D2Slagshell, D3Coldforge, D4Galewing };
```

- [ ] **Step 4: Append the three fields to `BossDef`**

Immediately after the `phase_lines[3]` member (the current last line before the struct's closing `};`), add:

```cpp
    // --- M15: airborne-boss data. Appended LAST: every def above uses positional aggregate
    //     init, so inserting anywhere else silently reassigns King/D1/D2/D3 members. ---
    int  hover_row = 0;           // Locomotion::Hovering: the drift altitude, in TILE coords
                                  // (the boss's CENTRE row). Unused (0) for other locomotions.
    int  move_vel_raw = 128;      // Pacing/Hovering horizontal speed, as a logic::Fixed RAW value.
                                  // 128 = 0.5 px/frame = the shipped D2 pace, so D2 is unchanged.
    bool aim_horizontal = false;  // true  = AIMED bolts fly HORIZONTALLY at the boss's own height
                                  // false = AIMED bolts get a velocity vector aimed AT the player.
                                  // Replaces run_boss_fight's `perch_count == 0` derivation (I-M15):
                                  // aim model and teleport model are independent concerns.
```

- [ ] **Step 5: Set `aim_horizontal` explicitly on `KING_DEF`**

`KING_DEF` is the only def that fires horizontal bolts today (it is the only perched boss). Because the new fields come after `phase_lines`, and `KING_DEF` initializes `phase_lines`, append to it. Replace `KING_DEF`'s final line:

```cpp
    /*phase_lines=*/{ nullptr, "NOW YOU'RE GETTING ME ANGRY", "I'M DONE TOYING WITH YOU" }
};
```

with:

```cpp
    /*phase_lines=*/{ nullptr, "NOW YOU'RE GETTING ME ANGRY", "I'M DONE TOYING WITH YOU" },
    /*hover_row=*/0,          // not a hovering boss (it teleports between perches)
    /*move_vel_raw=*/128,     // unused (Stationary)
    /*aim_horizontal=*/true   // M15: preserves the King's SHIPPED horizontal bolts, which
                              // run_boss_fight previously derived from perch_count > 0
};
```

Do NOT add trailing initializers to `D1_DEF`, `D2_DEF`, or `D3_DEF` — they stop before `phase_lines` and correctly take all three defaults (`aim_horizontal = false` = aim at the player, which is what they already do).

- [ ] **Step 6: Verify no regression**

Run: `bash tools/host_test.sh`
Expected: PASS with the SAME N as Step 1, `0 checks failed`. Then run `python tools/check_logic_purity.py` → `logic purity OK`.

If any existing assertion fails, revert the field placement and re-read the CRITICAL note above. Do NOT edit the failing assertion.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 7: Commit**

```bash
git add include/logic/boss.h
git commit -m "feat(boss): additive BossDef fields for airborne bosses (hover_row, move_vel_raw, aim_horizontal) + Locomotion::Hovering"
```

---

## Task 1.2: `D4_DEF` + def-invariant tests (and close the `switch_budget` gap)

**Files:**
- Modify: `include/logic/boss.h` (append after `D3_DEF`, ~:204)
- Modify: `test/test_boss.cpp` (append new tests; edit `switch_budget_holds_for_all_defs` at :278)

**Interfaces:**
- Consumes: Task 1.1's `Locomotion::Hovering`, `BossId::D4Galewing`, `hover_row`, `move_vel_raw`, `aim_horizontal`.
- Produces: `logic::D4_DEF` and `logic::D4_PHASES`. Task 3.2 (`BOSS_SYMBOL` → `logic::D4_DEF`), Task 4.2 (reads `D4_DEF.hover_row`), and Task 5.1 (arena JSON `"boss":"d4"`) consume `D4_DEF`.

**Assert relationships, not tuning values.** `max_hp`, `hit_iframes`, `move_vel_raw`, `hover_row`, and the phase timings are all QA-tunable in Phase 6 — expect them to change at the emulator. Tests here therefore assert *structure* (`end_hp` descends to 0, `hover_row > 0`, `move_vel_raw > 0`, `pattern.active >= 28`, wound count `== max_hp / wound_dmg`), never magic literals like `CHECK_EQ(D4_DEF.move_vel_raw, 192)`. A test pinning a tuning value turns every QA adjustment into a spurious failure and trains the next agent to edit tests to make the suite pass — the exact habit the regression gate depends on not existing. The one exception is the arena geometry test in Task 4.2, which pins a *derived safety* constant, not a feel value.

**Context — there is NO automatic all-defs loop.** `docs/content-recipes.md` step 2.7 implies def-invariant tests become automatic after the remediation plan's Phase 5. They did not. `test/test_boss.cpp:278` reads `TEST(switch_budget_holds_for_all_defs){ check_budget(KING_DEF); check_budget(D1_DEF); check_budget(AV_DEF); }` — a hand-written list that is **missing `D2_DEF` and `D3_DEF`**. This task hand-writes D4's def test AND closes that pre-existing gap. `AV_DEF` (`test_boss.cpp:255`) is a pre-existing *synthetic* AlwaysVulnerable def used for state-machine tests; leave it in the list and leave its tests untouched.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Write the failing tests**

Append to `test/test_boss.cpp`:

```cpp
// --- M15: D4 Gale Cliffs "Galewing" — the first AIRBORNE boss (AlwaysVulnerable + Hovering).
//     Altitude is the damage gate: player bolts fly horizontally, so a hovering boss cannot be
//     shot from any standing surface (the arena enforces the clearance; see test_dungeon4_level).
TEST(bossdef_d4_galewing_fields){
    CHECK(D4_DEF.vuln       == VulnMode::AlwaysVulnerable);
    CHECK(D4_DEF.locomotion == Locomotion::Hovering);
    CHECK(D4_DEF.id         == BossId::D4Galewing);
    CHECK(D4_DEF.block_mode == BlockMode::None);       // dodge-only: no magic economy in this fight
    CHECK_EQ(D4_DEF.phase_count, 2);
    // AlwaysVulnerable carries NO expose machinery.
    CHECK(D4_DEF.expose_spell     == SpellId::None);
    CHECK(D4_DEF.expose_spell_alt == SpellId::None);
    CHECK_EQ(D4_DEF.expose_frames, 0);
    CHECK_EQ(D4_DEF.tired_after,   0);
    // Hovering, not perching: it drifts, it never teleports.
    CHECK_EQ(D4_DEF.perch_count,     0);
    CHECK_EQ(D4_DEF.teleport_period, 0);
    CHECK(!D4_DEF.teleport_on_wound);
    CHECK(D4_DEF.hover_row > 0);          // an altitude was actually authored
    CHECK(D4_DEF.move_vel_raw > 0);       // it actually drifts
    CHECK(!D4_DEF.aim_horizontal);        // bolts must TRACK a player below, not fly at boss height
    // Phase thresholds descend to zero.
    CHECK(D4_DEF.phases[0].end_hp > D4_DEF.phases[1].end_hp);
    CHECK_EQ(D4_DEF.phases[1].end_hp, 0);
    // Rockfall phases must give the rock drop room inside the Active window (>= 28; see D2_PHASES).
    for(int i = 0; i < D4_DEF.phase_count; ++i)
        if(D4_DEF.phases[i].attacks & BOSS_ATK_ROCKFALL){
            CHECK(D4_DEF.phases[i].rock_count > 0);
            CHECK(D4_DEF.phases[i].pattern.active >= 28);
        }
}

TEST(d4_always_vulnerable_never_exposes){
    BossState b; b.reset(D4_DEF);
    CHECK(b.vulnerable());                 // vulnerable from frame 0, with no spell cast
    b.on_expose_hit(SpellId::Fire);        // AlwaysVulnerable has no expose mechanic...
    CHECK(!b.exposed());                   // ...so no expose window ever opens
    b.on_expose_hit(SpellId::Ice);
    CHECK(!b.exposed());
    CHECK(b.vulnerable());                 // ...and it stays vulnerable regardless
}

TEST(d4_takes_exactly_max_hp_over_wound_dmg_wounds){
    BossState b; b.reset(D4_DEF);
    int wounds = 0;
    while(!b.defeated() && wounds < 100){
        b.on_wound(D4_DEF.wound_dmg);
        ++wounds;
        for(int i = 0; i < D4_DEF.hit_iframes; ++i) b.tick();   // wait out the re-arm
    }
    CHECK(b.defeated());
    CHECK_EQ(wounds, D4_DEF.max_hp / D4_DEF.wound_dmg);
}

TEST(d4_iframes_reject_a_second_wound_in_the_same_window){
    BossState b; b.reset(D4_DEF);
    b.on_wound(D4_DEF.wound_dmg);
    CHECK_EQ(b.hp, D4_DEF.max_hp - D4_DEF.wound_dmg);
    b.on_wound(D4_DEF.wound_dmg);                               // blocked by i-frames
    CHECK_EQ(b.hp, D4_DEF.max_hp - D4_DEF.wound_dmg);
    for(int i = 0; i < D4_DEF.hit_iframes; ++i) b.tick();
    b.on_wound(D4_DEF.wound_dmg);                               // lands once the window drains
    CHECK_EQ(b.hp, D4_DEF.max_hp - 2 * D4_DEF.wound_dmg);
}
```

Then replace `test/test_boss.cpp:278` — closing the pre-existing coverage gap in the same edit:

```cpp
// M15: D2_DEF and D3_DEF were missing from this list (shipped bosses with no budget check).
TEST(switch_budget_holds_for_all_defs){
    check_budget(KING_DEF); check_budget(D1_DEF); check_budget(D2_DEF);
    check_budget(D3_DEF);   check_budget(D4_DEF); check_budget(AV_DEF);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `bash tools/host_test.sh`
Expected: FAIL — compile error, `'D4_DEF' was not declared in this scope`.

- [ ] **Step 3: Implement `D4_PHASES` + `D4_DEF`**

Append to `include/logic/boss.h` after `D3_DEF` (~:204):

```cpp
// --- D4 Gale Cliffs boss: "Galewing" (AlwaysVulnerable, Hovering, 2 phases). The first AIRBORNE
//     boss. Player bolts travel HORIZONTALLY (read_aim_dy is a muzzle offset, not a velocity), so a
//     boss holding an altitude cannot be shot from any standing surface — ALTITUDE is the damage
//     gate, which is why this fight needs no expose/tired state at all. The player rides the arena's
//     updraft columns (which require the Glide ability D4 itself grants) and fires while gliding
//     down through the boss's band. Rockfall reads as wind-flung debris and is the DOWNWARD threat
//     that keeps the climb and the descent from being rest periods. ---
inline constexpr uint8_t D4_ATTACKS_P1 = BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL;
inline constexpr uint8_t D4_ATTACKS_P2 = BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL | BOSS_ATK_FAN;
inline constexpr BossPhaseDef D4_PHASES[2] = {
    { 40, { 80, 30, 40 }, D4_ATTACKS_P1, /*proj_speed=*/2, /*rock_count=*/3 },  // P1 80->40
    {  0, { 70, 30, 30 }, D4_ATTACKS_P2, /*proj_speed=*/3, /*rock_count=*/5 },  // P2 40->0 (+ fan)
    // active (30) MUST stay >= RockfallEmitter::WARN_FRAMES (26) — same constraint as D2_PHASES.
};
inline constexpr BossDef D4_DEF{
    80, 10, 60, 0, VulnMode::AlwaysVulnerable, SpellId::None, D4_PHASES, 2,
    /*tired_after=*/0,
    /*intro_line=*/"The cliffs answer to me.",
    /*death_line=*/"The wind... stills...",
    /*locomotion=*/Locomotion::Hovering,
    /*block_spell=*/SpellId::None,
    /*expose_spell_alt=*/SpellId::None,
    /*block_spell2=*/SpellId::None,
    /*id=*/BossId::D4Galewing,
    /*block_mode=*/BlockMode::None,      // dodge-only; the free bolt wounds, so no magic is needed
    /*perches=*/nullptr,
    /*perch_count=*/0,
    /*teleport_period=*/0,
    /*teleport_on_wound=*/false,
    /*phase_lines=*/{ nullptr, "THEN FALL WITH THE REST", nullptr },
    /*hover_row=*/4,                     // arena is 32x24; row 4 keeps ledges (rows 18/20) 14 and 16
                                         // tiles clear, against the 13-tile minimum (spec 3.1).
    /*move_vel_raw=*/192,                // 0.75 px/frame — faster than D2's ground pace (0.5)
    /*aim_horizontal=*/false             // bolts TRACK the player below
};
```

- [ ] **Step 4: Run tests to verify they pass**

Run: `bash tools/host_test.sh`
Expected: PASS, `0 checks failed`, with N increased by 4 new tests. `switch_budget_holds_for_all_defs` now covers 6 defs.

Then: `python tools/check_logic_purity.py` → `logic purity OK`.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 5: Commit**

```bash
git add include/logic/boss.h test/test_boss.cpp
git commit -m "feat(boss): D4_DEF 'Galewing' — AlwaysVulnerable airborne boss; strengthen switch-budget coverage to D2/D3/D4"
```

**After completing Phase 1:** run a 3-round review of the batch from multiple perspectives (correctness, regression risk to shipped bosses, test rigor). If round 3 still finds issues, keep going until clean.

---

# Phase 2 — Hovering in the fight loop

**Execution Status:** ⬜ NOT STARTED

Four localized edits to `src/game/boss_fight.cpp`. This is the riskiest phase: the file runs all four shipped fights, and the failure mode is a silent behavior change. Nothing here restructures the loop.

## Task 2.1: Hover placement, drift, and death-restart

**Files:**
- Modify: `src/game/boss_fight.cpp` (placement lambdas :93-119; restart block ~:202-203; pacing branch :263-271)

**Interfaces:**
- Consumes: `logic::Locomotion::Hovering`, `BossDef::hover_row`, `BossDef::move_vel_raw` (Task 1.1); `logic::D4_DEF` (Task 1.2).
- Produces: nothing new — behavior only.

**Context.** `place_boss()` (:97) centres a boss on the arena floor at `(level.h - 2) * 8`. `set_perch()` (:112) positions the King from tile coords. Hovering needs a third placement: the same X centring, but Y from `hover_row`. The drift math already exists in the pacing branch (:265) — it is reused, not duplicated.

**M13 QA lesson, do NOT skip:** a *moving* boss that is not re-placed on death-restart respawns the player pinned against it (the "boss vanished on death" bug). `restart_fight` re-places a Pacing boss; Hovering needs the same treatment.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

**Note on TDD scope here:** `boss_fight.cpp` is `src/game/` (Butano-linked) and is NOT host-testable — there is no unit test that can drive it. The testable contract for this task lives in Task 1.2's def tests (already green) and Task 4.2's geometry test. The verification gate for this task is therefore: ROM builds clean, all existing host tests stay green, and the mGBA checks in Phase 6. Do NOT invent a host test that fakes Butano; do NOT move drift math into `logic/` to make it testable — `Pacing` lives here too, and splitting one of two movement modes across layers is worse than keeping both together (spec §5.4).

- [ ] **Step 1: Add the hover placement lambda**

In `src/game/boss_fight.cpp`, immediately after the `place_boss` lambda (which ends ~:101), add:

```cpp
    // Hovering placement (Locomotion::Hovering = D4): the boss holds an ALTITUDE instead of the
    // floor — its centre sits on def.hover_row, ignoring gravity and the floor row entirely.
    // Same X centring as place_boss so both modes start mid-arena.
    auto place_hover = [&]{
        boss_body.pos = { fx(boss_cx_tile * 8 - boss_body.half_w.to_int()),
                          fx(def.hover_row * 8 - boss_body.half_h.to_int()) };
    };
```

- [ ] **Step 2: Route initial placement**

Replace the initial-placement branch (~:119):

```cpp
    if(def.perch_count > 0){ set_perch(0); teleport_timer = def.teleport_period; }
    else                   { place_boss(); }
```

with:

```cpp
    if(def.perch_count > 0){ set_perch(0); teleport_timer = def.teleport_period; }
    else if(def.locomotion == logic::Locomotion::Hovering){ place_hover(); }
    else                   { place_boss(); }
```

- [ ] **Step 3: Make the drift speed data-driven and widen the branch to Hovering**

Replace the `PACE_VEL` declaration (~:106):

```cpp
    const logic::Fixed PACE_VEL = logic::Fixed::from_raw(128);   // 0.5 px/frame
```

with:

```cpp
    // M15: drift speed is def data (was a hardcoded 128 raw). D2 sets the default 128 = 0.5 px/frame,
    // so its shipped pace is bit-identical; D4 hovers faster. IMPL-2: raw Fixed, never float.
    const logic::Fixed DRIFT_VEL = logic::Fixed::from_raw(def.move_vel_raw);
```

Then replace the pacing branch (:265-271):

```cpp
        if(def.locomotion == logic::Locomotion::Pacing && !b.exposed()){
            boss_body.pos.x = boss_body.pos.x + (pace_dir > 0 ? PACE_VEL : -PACE_VEL);
```

with:

```cpp
        // ---- drift (Pacing on the floor, Hovering in the air): identical X math, different Y
        //      origin — the two modes differ ONLY in placement. Paused while EXPOSED so a clean
        //      wound window doesn't also require tracking a moving target (AlwaysVulnerable bosses
        //      never expose, so D4 drifts continuously). ----
        if((def.locomotion == logic::Locomotion::Pacing ||
            def.locomotion == logic::Locomotion::Hovering) && !b.exposed()){
            boss_body.pos.x = boss_body.pos.x + (pace_dir > 0 ? DRIFT_VEL : -DRIFT_VEL);
```

The replacement above already carries the updated comment block. Leave the remainder of the branch — the `pace_min_cx` / `pace_max_cx` reversal and clamping — byte-for-byte as-is: those bounds are what keep a drifting boss inside the arena walls, and they are correct for both modes.

- [ ] **Step 4: Re-place a hovering boss on death-restart**

Replace the restart branch (~:202-203):

```cpp
        if(def.perch_count > 0){ set_perch(0); teleport_timer = def.teleport_period; teleport_flash = 0; }
        else                   { place_boss(); pace_dir = 1; }   // re-centre a Pacing boss off the entrance
```

with:

```cpp
        if(def.perch_count > 0){ set_perch(0); teleport_timer = def.teleport_period; teleport_flash = 0; }
        else if(def.locomotion == logic::Locomotion::Hovering){ place_hover(); pace_dir = 1; }
        else                   { place_boss(); pace_dir = 1; }   // re-centre a Pacing boss off the entrance
        // M13 QA lesson: a MOVING boss (Pacing or Hovering) MUST be re-placed here. Without it the
        // player respawns pinned against wherever the boss drifted to ("boss vanished on death").
```

- [ ] **Step 5: Verify no regression**

Run: `bash tools/host_test.sh`
Expected: PASS, same N as after Task 1.2, `0 checks failed`.

Run: `bash tools/build_rom.sh`
Expected: ends `ROM fixed!`, zero warnings. (Close mGBA first if you see `objcopy: Invalid argument`.)

Grep-verify the old constant is fully gone: `grep -n "PACE_VEL" src/game/boss_fight.cpp` → no matches.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 6: Commit**

```bash
git add src/game/boss_fight.cpp
git commit -m "feat(boss): Locomotion::Hovering — altitude placement, def-driven drift speed, death-restart re-place"
```

---

## Task 2.2: Decouple the aim model from the perch model

**Files:**
- Modify: `src/game/boss_fight.cpp` (:176-179)

**Interfaces:**
- Consumes: `BossDef::aim_horizontal` (Task 1.1), which `KING_DEF` sets `true`.
- Produces: nothing new — behavior only.

**Context (spec §2.3).** The line reads:

```cpp
const bool aim_full = (def.perch_count == 0);
```

Aim model and teleport model are independent concerns that happen to correlate today because the King is the only perched boss. A hovering, perch-less boss would inherit aimed-at-player behavior *by accident*. Worse, the coupling actively blocks a future boss that wants both perches and tracking. Task 1.1 already added the explicit field and set `KING_DEF.aim_horizontal = true`, so this substitution is behavior-preserving for all four shipped fights.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Substitute the derivation**

Replace :176-179's comment and line:

```cpp
    // Aimed bolts: a grounded boss (perch_count==0) aims a velocity VECTOR at the player (aim_full);
    // the perched King fires HORIZONTAL bolts at its perch height (the read that tested well). This
    const bool aim_full = (def.perch_count == 0);
```

with:

```cpp
    // Aimed bolts: aim_full = a velocity VECTOR aimed at the player (D1/D2/D3/D4); otherwise
    // HORIZONTAL bolts at the boss's own height (the King — the read that tested well).
    // M15: read from an explicit def field instead of deriving it from perch_count. The aim model
    // and the teleport model are independent; they only correlated because the King was the sole
    // perched boss. An airborne boss MUST track a player below it, or its bolts sail overhead and
    // the climb carries no threat at all.
    const bool aim_full = !def.aim_horizontal;
```

Keep the rest of the original comment block that explains the emitter behavior.

- [ ] **Step 2: Verify the King is untouched**

Confirm `KING_DEF` has `/*aim_horizontal=*/true` (Task 1.1 Step 5) — with it, `aim_full` is `false` for the King exactly as `perch_count == 0` produced. Confirm D1/D2/D3 omit the field (default `false`) → `aim_full == true`, matching `perch_count == 0` today.

Run: `bash tools/host_test.sh` → PASS, unchanged N.
Run: `bash tools/build_rom.sh` → `ROM fixed!`, zero warnings.

Grep-verify: `grep -n "perch_count == 0" src/game/boss_fight.cpp` → the aim site is gone. Other `perch_count > 0` uses (teleport, bolt sprite choice at :138) are correct and MUST remain.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 3: Commit**

```bash
git add src/game/boss_fight.cpp
git commit -m "refactor(boss): aim model is an explicit BossDef field, not a perch_count derivation"
```

**After completing Phase 2:** 3-round review with emphasis on behavior preservation — diff `boss_fight.cpp` against its pre-phase version and justify every non-mechanical line. All four shipped fights must be byte-for-byte equivalent in behavior for non-hovering defs.

---

# Phase 3 — Art + pipeline wiring

**Execution Status:** ⬜ NOT STARTED

Independent of Phase 2; both depend only on Phase 1. MAY run in parallel with Phases 2 and 4.

## Task 3.1: Galewing placeholder sprite

**Files:**
- Modify: `tools/make_placeholder_art.py` (add `draw_galewing_frame` + `gen_galewing`; register in `__main__` ~:780)
- Create (generated, committed): `graphics/galewing.bmp`, `graphics/galewing.json`

**Interfaces:**
- Consumes: nothing.
- Produces: the Butano sprite item `bn::sprite_items::galewing`, consumed by Task 3.2's `boss_sprite_for`.

**Context — single frame, deliberately.** `run_boss_fight` swaps sprite frames on EXPOSE only (`boss_has_frames` at ~:126 is `tiles_item().graphics_count() > 1`). An `AlwaysVulnerable` boss never exposes, so extra frames would never render — exactly the King's situation (1 graphic, blink-only). Author ONE 32×32 frame.

The helper API in this file: `new_img(w, h)`, `rect(im, x0, y0, x1, y1, pal)`, `px(im, x, y, pal)`, `write(im, name, meta)`. Palette indices in use: 1 near-black outline, 4/5 greens, 6 gold, 8 cyan, 9 white, 11 dark brown, 12 stone grey, 13 red, 14 shadow-blue, 15 white glint.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Add the draw function**

Insert into `tools/make_placeholder_art.py` beside the other boss draw functions (after `draw_coldforge_frame`, ~:745):

```python
def draw_galewing_frame(im, oy):
    """One 32x32 Galewing frame at vertical offset oy — D4 Gale Cliffs' AIRBORNE boss.
    A wide-winged storm raptor: pale storm-cyan wing planes (pal 8) with white leading
    edges (pal 9), slate body (pal 12), near-black outline (pal 1), white glints (pal 15).
    Read at a glance it MUST say AIRBORNE: the wingspan fills the full 32px width and there
    are deliberately NO feet and NO base shadow line — every other boss (guardian, slagshell,
    coldforge) has ground contact, and this one must not. Integer pixel ops only (no floats)."""
    # ---- wing planes: a shallow V spanning the full width ----
    rect(im, 0,  oy + 12, 9,  oy + 15, 8)    # left wing plane
    rect(im, 22, oy + 12, 31, oy + 15, 8)    # right wing plane
    rect(im, 0,  oy + 12, 9,  oy + 12, 9)    # left leading edge (white)
    rect(im, 22, oy + 12, 31, oy + 12, 9)    # right leading edge (white)
    rect(im, 2,  oy + 16, 8,  oy + 17, 1)    # left underwing shadow
    rect(im, 23, oy + 16, 29, oy + 17, 1)    # right underwing shadow
    # swept wingtips angled UP, so the silhouette reads as gliding rather than perched
    rect(im, 0,  oy + 9,  3,  oy + 11, 8)
    rect(im, 28, oy + 9,  31, oy + 11, 8)
    # ---- body: a compact slate fuselage ----
    rect(im, 10, oy + 10, 21, oy + 21, 12)
    rect(im, 10, oy + 10, 21, oy + 10, 1)    # top outline
    rect(im, 10, oy + 21, 21, oy + 21, 1)    # bottom outline
    # ---- head, crest, beak ----
    rect(im, 13, oy + 5,  18, oy + 9,  12)
    rect(im, 13, oy + 5,  18, oy + 5,  1)    # brow
    rect(im, 15, oy + 2,  16, oy + 4,  9)    # white crest
    px(im, 14, oy + 8, 15); px(im, 17, oy + 8, 15)   # white eye glints
    rect(im, 15, oy + 10, 16, oy + 11, 1)    # beak
    # ---- tail: trails DOWN-BACK; no feet, it never lands ----
    rect(im, 14, oy + 22, 17, oy + 27, 8)
    rect(im, 15, oy + 27, 16, oy + 29, 9)
```

- [ ] **Step 2: Add the gen function**

Insert after `gen_coldforge` (~:758):

```python
def gen_galewing():
    """D4 Gale Cliffs boss placeholder 32x32 — ONE frame. Galewing is AlwaysVulnerable, so
    run_boss_fight never swaps frames (boss_has_frames requires graphics_count() > 1); a second
    frame would be dead weight, exactly as for the King."""
    im = new_img(32, 32)
    draw_galewing_frame(im, 0)
    write(im, "galewing", {"type": "sprite", "height": 32})
```

- [ ] **Step 3: Register it**

In the `__main__` block (~:780), add `gen_galewing()` alongside `gen_coldforge()` / `gen_slagshell()` / `gen_guardian()`.

- [ ] **Step 4: Generate and verify the asset**

Run: `python tools/make_placeholder_art.py`
Expected: writes `graphics/galewing.bmp` + `graphics/galewing.json` with no traceback, ending `placeholder sprites + bg tiles + hud + ember art generated.`

Verify the frame count is 1: `graphics/galewing.json` must contain `"height": 32` and the bitmap must be 32×32. A 32×64 bitmap would give `graphics_count() == 2` and silently re-enable expose-driven frame swapping on a boss that never exposes.

**The `__main__` block regenerates EVERY asset, not just yours** (`gen_laurel()` through `gen_health_pickup()`). Run `git status` afterwards: only `graphics/galewing.*` should appear as new. If other `graphics/*` files show as modified, do NOT commit them — the generator is deterministic, so unrelated churn means either a pre-existing uncommitted edit or a generator change, and either way it does not belong in this commit. Investigate before proceeding.

- [ ] **Step 5: Commit**

```bash
git add tools/make_placeholder_art.py graphics/galewing.bmp graphics/galewing.json
git commit -m "feat(art): Galewing placeholder sprite — single 32x32 airborne frame (no ground contact)"
```

Confirm the commit contains exactly 3 files: `git show --stat HEAD`.

---

## Task 3.2: Pipeline wiring — boss symbol + id→sprite row

**Files:**
- Modify: `tools/build_level.py` (`BOSS_SYMBOL` :61-64)
- Modify: `src/game/scene_dungeon.cpp` (`boss_sprite_for` :67-75)

**Interfaces:**
- Consumes: `logic::D4_DEF` (Task 1.2), `logic::BossId::D4Galewing` (Task 1.1), `bn::sprite_items::galewing` (Task 3.1).
- Produces: the room-JSON spelling `"boss": "d4"`, consumed by Task 5.1.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Add the boss symbol**

In `tools/build_level.py`, extend `BOSS_SYMBOL`:

```python
BOSS_SYMBOL = {
    'd1': 'logic::D1_DEF',
    'd2': 'logic::D2_DEF',
    'd3': 'logic::D3_DEF',
    'd4': 'logic::D4_DEF',
}
```

- [ ] **Step 2: Add the sprite row**

In `src/game/scene_dungeon.cpp`, add a case to `boss_sprite_for` before the `default:`:

```cpp
        case logic::BossId::D4Galewing:  return bn::sprite_items::galewing;
```

Do NOT touch the `default: BN_ERROR(...)` arm — an unmapped boss id must keep failing loudly at fight start rather than silently rendering the guardian.

Add the generated sprite header alongside the other boss sprite includes at `src/game/scene_dungeon.cpp:11-14`:

```cpp
#include "bn_sprite_items_galewing.h"  // M15: D4 Gale Cliffs boss sprite (Galewing, 1 frame)
```

(Butano generates `bn_sprite_items_<name>.h` from `graphics/<name>.json` at build time, so this header does not exist until Task 3.1's asset is committed and the ROM has been built.)

- [ ] **Step 3: Verify the build**

Run: `bash tools/host_test.sh` → PASS, unchanged N.
Run: `bash tools/build_rom.sh` → `ROM fixed!`, zero warnings.

Verify the unknown-boss guard still bites: temporarily add `"boss": "d9"` to any room JSON and confirm `bash tools/build_rom.sh` fails with `LevelError: unknown boss 'd9'`. **Revert that edit before committing.**

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 4: Commit**

```bash
git add tools/build_level.py src/game/scene_dungeon.cpp
git commit -m "feat(pipeline): wire boss 'd4' -> D4_DEF and BossId::D4Galewing -> galewing sprite"
```

**After completing Phase 3:** 3-round review (asset correctness, frame-count trap, loud-failure preservation).

---

# Phase 4 — D4 restructure: 1 room → 3

**Execution Status:** ⬜ NOT STARTED

Turns D4 into ascent → arena → spronk. The arena is authored here but stays **bossless** (no `"boss"` key) so this phase lands as a playable, testable dungeon on its own; Phase 5 arms it.

## Task 4.1: Author the three rooms

**Files:**
- Rename: `tools/levels/dungeon4.txt` → `tools/levels/dungeon4_room0.txt`; `dungeon4.json` → `dungeon4_room0.json`
- Modify: `tools/levels/dungeon4_room0.txt` (top of the shaft only), `dungeon4_room0.json` (add `entrances` + `room_doors`)
- Create: `tools/levels/dungeon4_room1.txt` + `.json` (arena), `dungeon4_room2.txt` + `.json` (spronk)
- Modify: `tools/levels/manifest.json`, `include/game/levels/dungeons.h`
- Delete (generated): `include/game/levels/dungeon4.h` — replaced by the three generated room headers

**Interfaces:**
- Consumes: nothing from earlier tasks (the arena carries no boss key yet).
- Produces: `DUNGEON4_ROOM0_DATA`, `DUNGEON4_ROOM1_DATA`, `DUNGEON4_ROOM2_DATA` and `DUNGEON4_ROOMS[3]`, consumed by Task 4.2's tests.

**Context.** D4 today is a single 30×56 vertical shaft: `@` spawn + `Q` hub-return at the bottom, the Glide shrine (`F` with `"ability":"glide"`), one patroller (`o`), an updraft column (`u`), wind-push tiles (`>`), one-way ledges (`^`), and the spronk cage `C` + exit `E` at the top. Note `'^'` is `TileKind::OneWay` (TILE 2), **not** spikes (spikes are `'s'` = 9); D4 contains no spikes.

Room-door and entrance authoring follows `tools/levels/dungeon3_room1.json` exactly:

```json
{ "tileset": "tiles",
  "entrances": [ { "id": 0, "facing": 1 }, { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 0, "target_entrance": 1 },
                  { "target_room": 2, "target_entrance": 0 } ] }
```

`'N'` = entrance, `'D'` = room-door, both matched in **grid scan order** (top-to-bottom, left-to-right) against the JSON arrays.

Arena authoring constraints that MUST hold (`docs/content-recipes.md` §4): solid 1-tile walls at columns 0 and `w-1` (the drift reversal bounds assume it), a flat solid floor at row `h-2`, and an entrance authored safe.

**This is the first boss arena TALLER than the viewport.** D1/D2/D3 arenas are 19 tiles tall against a 20-tile screen, so they never scroll; at 24 tall this one does. The camera must stay clamped to the level bounds (IMPL-9) — an unclamped camera scrolls into the fixed-size background's wrapping region and shows the far edge of the level folded onto the near edge. The shared `set_clamped_cam` helper already handles this for `play_room`, and the fight loop uses the same helper, so no code change is expected — but vertical scrolling *during a boss fight* has never shipped, which is why Task 6.2 checks it explicitly. If the camera misbehaves, that is a code finding to raise, not a reason to shrink the arena (shrinking it would break the 13-tile clearance rule).

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Rename room 0 and register three rooms**

```bash
git mv tools/levels/dungeon4.txt tools/levels/dungeon4_room0.txt
git mv tools/levels/dungeon4.json tools/levels/dungeon4_room0.json
```

In `tools/levels/manifest.json`, replace `"4": ["dungeon4"],` with:

```json
    "4": ["dungeon4_room0", "dungeon4_room1", "dungeon4_room2"],
```

- [ ] **Step 2: Re-terminate room 0**

The top of the shaft currently reads (row/col are 0-indexed; row 0 is the first grid line):

```
row5   #........................C...#     <- C (spronk cage) at col 25
row6   #.........................E..#     <- E (exit) at col 26
row7   #......................^^^^^^#     <- one-way ledge, cols 22-27
```

The `^` run at row 7 is the platform the player lands on at the end of the ascent, so row 6 is its standing row — which is exactly where the door belongs.

Edit `tools/levels/dungeon4_room0.txt`:
- **Row 5:** replace the `C` at col 25 with `.` (the cage moves to room 2).
- **Row 6:** replace the `E` at col 26 with `D` (the room-door to the arena), and replace the `.` at col 24 with `N` (the return entrance).
- Leave `@`, `Q`, the `F` glide shrine, the `o` patroller, and every `u` / `>` / `^` tile untouched. Do NOT restructure the shaft; re-terminating its top is the entire scope of this edit.

Edit **by column index**, replacing single characters in place — do not retype the rows. Every row must stay exactly 30 characters; a transcription that drops or adds one character silently shifts the entire row's contents. Verify after editing:

```bash
awk '!/^;/ { print length($0) }' tools/levels/dungeon4_room0.txt | sort -u
```

Expected: a single line, `30`.

Then add the entrance/door metadata to `tools/levels/dungeon4_room0.json` (keeping the existing `enemies` and `pickups` arrays exactly as they are):

```json
  "entrances": [ { "id": 0, "facing": 1 } ],
  "room_doors": [ { "target_room": 1, "target_entrance": 0 } ]
```

**`'Q'` is a room-door too, but it takes no JSON entry.** `tools/build_level.py:230-233` compiles `'Q'` into a `room_doors` entry with the sentinel `target_room = -1` and does **not** advance the JSON index — only `'D'` glyphs consume `room_doors[]` entries, in D-scan order. So room 0's single-element `room_doors` array below maps to the new `D`, while the pre-existing `Q` is appended automatically. The compiled `room_door_count` for room 0 will be **2**. Do NOT add a JSON entry for the `Q`; doing so would silently steal the `D`'s target.

**Entrance/door wiring across all three rooms must stay mutually consistent.** `'N'` and `'D'` glyphs are matched in **grid scan order** (top-to-bottom, then left-to-right) against the JSON `entrances` / `room_doors` arrays. The three JSON blocks in this task resolve as: room 0's door → room 1 entrance 0; room 1's left door → room 0 entrance 0; room 1's right door → room 2 entrance 0; room 2's door → room 1 entrance 1. In room 1's grid, the left `N`/`D` (cols 2/3) are scanned before the right pair (cols 27/28), which is why room 1's `entrances` array lists `id 0` first. `check_room_doors_resolve` in Task 4.2 verifies the whole graph — if it fails, the scan order and the array order disagree.

(D3 happens to use entrance `id 1` for its room 0; that is a local convention, not a requirement. What matters is that each door's `target_entrance` names an id its target room actually declares.)

- [ ] **Step 3: Author the arena (room 1)**

Create `tools/levels/dungeon4_room1.txt` with exactly this 32×24 grid (comment lines starting `;` are stripped by the loader):

```
; D4 Gale Cliffs — Room 1 (BOSS ARENA). "Galewing" (D4_DEF, AlwaysVulnerable + Hovering) is armed
; in Task 5.1 via JSON "boss":"d4". VERTICAL DUEL: the boss holds row 4 and drifts; player bolts fly
; HORIZONTALLY, so it can only be hit from the air. Two updraft columns (x=6, x=25) carry a GLIDING
; player up; the one-way ledges sit at rows 18 and 20 — 14 and 16 tiles below the hover line, against
; the 13-tile minimum derived in the M15 spec 3.1. DO NOT raise a ledge without re-reading that rule:
; test_dungeon4_level.cpp fails if any standing surface comes within 13 tiles of hover_row.
; 'N'(id0)+'D' left = arrival/return to room 0; 'N'(id1)+'D' right = onward to room 2.
################################
#..............................#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u..................u.....#
#.....u...^^^^^^.........u.....#
#.....u..................u.....#
#.....u........^^^^^^....u.....#
#@.ND.u..................u.ND..#
################################
################################
```

Create `tools/levels/dungeon4_room1.json` (NO `"boss"` key yet — Task 5.1 adds it):

```json
{ "tileset": "tiles",
  "entrances": [ { "id": 0, "facing": 1 }, { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 0, "target_entrance": 0 },
                  { "target_room": 2, "target_entrance": 0 } ] }
```

- [ ] **Step 4: Author the spronk room (room 2)**

Create `tools/levels/dungeon4_room2.txt` with exactly this 30×19 grid — a small, calm room holding the `C` cage and `E` exit relocated from the old shaft top, plus the `N` entrance and the `D` room-door back to the arena, all on one flat floor:

```
; D4 Gale Cliffs — Room 2 (SPRONK). The exhale after the fight: one flat floor, no hazards, no
; enemies. 'N'(id0)+'D' left = arrival/return to the arena (room 1); 'C' = the spronk cage;
; 'E' = the exit archway back to the hub. Mirrors D2/D3 room 2.
##############################
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#............................#
#.ND...........C....E........#
##############################
##############################
```

Row 16 is the standing row; rows 17–18 are the solid floor (`h - 2 = 17`). The room is 30×19, exactly the `check_min_room_size` minimum.

**IMPL-5 applies to this room.** The cage and exit sprites are room-authored visuals that get grounded at runtime by scanning DOWN from the authored tile for the first solid/one-way row (`floor_row_below`) — never by a fixed offset. Authoring them on this flat floor is therefore safe. If you later move either onto a ledge, they will still ground correctly, but do NOT "help" by nudging their authored row to compensate for a perceived offset: that double-corrects and sinks the sprite into the floor. This trap has been hit twice in this codebase (the M3 brazier hit-body, the M7 heart-container sprite).

Create `tools/levels/dungeon4_room2.json`:

```json
{ "tileset": "tiles",
  "entrances": [ { "id": 0, "facing": 1 } ],
  "room_doors": [ { "target_room": 1, "target_entrance": 1 } ] }
```

- [ ] **Step 5: Register the rooms in the dungeon registry**

In `include/game/levels/dungeons.h`, replace `#include "game/levels/dungeon4.h"` with the three room includes, and replace the two D4 lines:

```cpp
inline constexpr const logic::LevelData* DUNGEON4_ROOMS[] = { &DUNGEON4_DATA };
inline constexpr logic::DungeonData DUNGEON4_DUNGEON{ DUNGEON4_ROOMS, 1, 0 };
```

with:

```cpp
// DUNGEON4 — Gale Cliffs (M15 restructure): room 0 = the updraft/Glide ascent shaft (+ '@' spawn,
// hub-return 'Q', the Glide shrine); room 1 = the "Galewing" arena (D4_DEF, AlwaysVulnerable +
// Hovering — fought on entry; victory opens the onward door); room 2 = the spronk + exit.
inline constexpr const logic::LevelData* DUNGEON4_ROOMS[] = {
    &DUNGEON4_ROOM0_DATA, &DUNGEON4_ROOM1_DATA, &DUNGEON4_ROOM2_DATA };
inline constexpr logic::DungeonData DUNGEON4_DUNGEON{ DUNGEON4_ROOMS, 3, 0 };
```

`DUNGEONS_BY_ID[3]` already points at `DUNGEON4_DUNGEON` and needs NO edit. Do NOT touch `src/main.cpp` or hub door-4 gating.

- [ ] **Step 6: Regenerate and verify the pipeline**

Run: `bash tools/build_rom.sh`
Expected: regenerates `include/game/levels/dungeon4_room{0,1,2}.h`, validator passes, ends `ROM fixed!` with zero warnings.

Run: `python tools/validate_dungeons.py`
Expected: passes (sprite budget, entity caps, room-door resolution for the new rooms).

Verify every authored grid is rectangular before building — one dropped character shifts a whole row:

```bash
for f in tools/levels/dungeon4_room0.txt tools/levels/dungeon4_room1.txt tools/levels/dungeon4_room2.txt; do
  echo "$f: $(awk '!/^;/ { print length($0) }' "$f" | sort -u | tr '\n' ' ')"
done
```

Expected: `room0: 30`, `room1: 32`, `room2: 30` — exactly one width per file.

If `include/game/levels/dungeon4.h` still exists after regeneration, `git rm` it — it is a stale generated header for a level that no longer exists.

Note: `test/test_dungeon4_level.cpp` will FAIL to compile at this point — it includes `game/levels/dungeon4.h` and references `DUNGEON4_DATA`. That is expected and is Task 4.2's job. Do NOT patch it here beyond what Task 4.2 specifies.

- [ ] **Step 7: Commit (with Task 4.2 — see note)**

This task and Task 4.2 land in ONE commit, because the tree does not compile between them. Complete Task 4.2, then commit both together using the command in Task 4.2 Step 5.

---

## Task 4.2: Rewrite `test_dungeon4_level.cpp` on the shared harness

**Files:**
- Modify: `test/test_dungeon4_level.cpp` (full rewrite; currently 107 lines against the single-room `DUNGEON4_DATA`)

**Interfaces:**
- Consumes: `DUNGEON4_ROOM{0,1,2}_DATA` + `DUNGEON4_DUNGEON` (Task 4.1); `logic::D4_DEF.hover_row` (Task 1.2); `test/level_harness.h`.
- Produces: nothing consumed downstream.

**Context.** The existing file asserts single-room properties, including `d4_cage_exit_vertical` (`CHECK(DUNGEON4_DATA.has_cage); CHECK(DUNGEON4_DATA.has_exit);` in the same room) — false by construction after the restructure; the cage/exit assertions move to room 2.

The shared harness (`test/level_harness.h`) provides: `harness::WorldModel` (set `.glide = true` to model Glide-in-updraft lift, `.climb_max = true` for must-NOT-bypass checks), `reachable(level, wm)`, `reaches(level, wm, tx, ty)`, `stands_at(R, tx, ty)`, `build_grid(level, wm)`, `Grid::stand(x, y)`, plus universal checks `check_solid_border`, `check_min_room_size`, `check_room_doors_resolve`, `check_entrances_settle_safely`.

**The clearance invariant is the point of this task.** It encodes spec §3.1 so a future arena tweak cannot silently turn the fight into a stand-and-plink. Express it in the same terms as the derivation: a standing FEET row `y` rests on surface row `y + 1`, and that surface must satisfy `y + 1 >= hover_row + MIN_HOVER_CLEARANCE`.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Write the rewritten test file**

Replace the entire contents of `test/test_dungeon4_level.cpp`:

```cpp
#include "test_framework.h"
#include "level_harness.h"
#include "logic/boss.h"                       // D4_DEF.hover_row — the clearance invariant reads it
#include "game/levels/dungeon4_room0.h"
#include "game/levels/dungeon4_room1.h"
#include "game/levels/dungeon4_room2.h"
#include "game/levels/dungeons.h"
using namespace logic;

// M15: D4 Gale Cliffs is a 3-room boss dungeon (was one 30x56 shaft).
//   room 0 = the updraft/Glide ascent (spawn, hub-return, Glide shrine) -> door to the arena
//   room 1 = the "Galewing" arena (AlwaysVulnerable + Hovering)
//   room 2 = the spronk cage + exit

// ---- universal room invariants ----
TEST(d4_rooms_are_well_formed){
    harness::check_solid_border(DUNGEON4_ROOM0_DATA, "D4R0");
    harness::check_solid_border(DUNGEON4_ROOM1_DATA, "D4R1");
    harness::check_solid_border(DUNGEON4_ROOM2_DATA, "D4R2");
    harness::check_min_room_size(DUNGEON4_ROOM0_DATA, "D4R0", 30, 19);
    harness::check_min_room_size(DUNGEON4_ROOM1_DATA, "D4R1", 30, 19);
    harness::check_min_room_size(DUNGEON4_ROOM2_DATA, "D4R2", 30, 19);
    harness::check_room_doors_resolve(DUNGEON4_DUNGEON, "D4");
    harness::check_entrances_settle_safely(DUNGEON4_ROOM0_DATA, "D4R0");
    harness::check_entrances_settle_safely(DUNGEON4_ROOM1_DATA, "D4R1");
    harness::check_entrances_settle_safely(DUNGEON4_ROOM2_DATA, "D4R2");
}

TEST(d4_is_a_three_room_dungeon){
    CHECK_EQ(DUNGEON4_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON4_DUNGEON.start_room, 0);
}

// ---- room 0: the ascent (preserved from the pre-M15 single room) ----
TEST(d4r0_is_vertical){
    CHECK(DUNGEON4_ROOM0_DATA.h > 22);
    CHECK(DUNGEON4_ROOM0_DATA.h <= 128);          // big-map bg cap
}

TEST(d4r0_glide_shrine){
    CHECK_EQ(DUNGEON4_ROOM0_DATA.pickup_count, 1);
    CHECK(DUNGEON4_ROOM0_DATA.pickups[0].ability == Ability::Glide);
}

TEST(d4r0_has_updraft_and_wind){
    bool up = false, wind = false;
    for(int i = 0; i < DUNGEON4_ROOM0_DATA.w * DUNGEON4_ROOM0_DATA.h; ++i){
        uint8_t t = DUNGEON4_ROOM0_DATA.tiles[i];
        if(t == (uint8_t)TileKind::Updraft) up = true;
        if(t == (uint8_t)TileKind::WindLeft || t == (uint8_t)TileKind::WindRight) wind = true;
    }
    CHECK(up); CHECK(wind);
}

TEST(d4r0_cage_and_exit_moved_out){
    CHECK(!DUNGEON4_ROOM0_DATA.has_cage);         // both relocated to room 2 by the M15 restructure
    CHECK(!DUNGEON4_ROOM0_DATA.has_exit);
}

// PRESERVED from the pre-M15 test file (d4_has_vine_gate): D4's Fire beat, near the bottom of the
// shaft. Unchanged by the restructure — re-pointed at room 0 so the coverage does not evaporate.
TEST(d4r0_has_vine_gate){
    bool vine = false;
    for(int i = 0; i < DUNGEON4_ROOM0_DATA.gate_count; ++i)
        if(DUNGEON4_ROOM0_DATA.gates[i].type == GateType::Vine) vine = true;
    CHECK(vine);
}

// PRESERVED from the pre-M15 test file (d4_has_hub_return_door). 'Q' compiles to a room-door with
// the sentinel target_room == -1 (tools/build_level.py:230-233), so room 0 carries TWO room-doors
// after M15: the new 'D' to the arena and the pre-existing 'Q' back to the hub.
TEST(d4r0_has_hub_return_and_arena_doors){
    CHECK_EQ(DUNGEON4_ROOM0_DATA.room_door_count, 2);
    int hub = 0, arena = 0;
    for(int i = 0; i < DUNGEON4_ROOM0_DATA.room_door_count; ++i){
        if(DUNGEON4_ROOM0_DATA.room_doors[i].target_room == -1) ++hub;
        if(DUNGEON4_ROOM0_DATA.room_doors[i].target_room ==  1) ++arena;
    }
    CHECK_EQ(hub, 1);
    CHECK_EQ(arena, 1);
}

TEST(d4r0_ascent_reaches_the_arena_door_with_glide){
    harness::WorldModel wm; wm.glide = true;      // the shrine is on the path; the climb needs it
    // Find the door by TARGET, not by index: room_doors is in grid scan order and also holds the
    // 'Q' hub-return, so index 0 is an implementation detail that a future edit could reorder.
    int found = -1;
    for(int i = 0; i < DUNGEON4_ROOM0_DATA.room_door_count; ++i)
        if(DUNGEON4_ROOM0_DATA.room_doors[i].target_room == 1) found = i;
    REQUIRE(found >= 0);
    const auto& d = DUNGEON4_ROOM0_DATA.room_doors[found];
    CHECK(harness::reaches(DUNGEON4_ROOM0_DATA, wm, d.tx, d.ty));
}

// The shrine GRANTS Glide, so it must be reachable WITHOUT Glide or D4 is unwinnable. This is a
// pre-existing property of the shaft, unchanged by M15 — it is asserted here because the shared
// harness can now express it, not because this milestone put it at risk.
TEST(d4r0_shrine_is_reachable_before_the_climb){
    harness::WorldModel wm;                       // NO glide: the shrine must be gettable without it
    const auto& p = DUNGEON4_ROOM0_DATA.pickups[0];
    CHECK(harness::reaches(DUNGEON4_ROOM0_DATA, wm, p.tx, p.ty));
}

// ---- room 1: the arena ----
TEST(d4r1_arena_dimensions){
    CHECK_EQ(DUNGEON4_ROOM1_DATA.w, 32);
    CHECK_EQ(DUNGEON4_ROOM1_DATA.h, 24);
}

// The columns are the ONLY way to reach boss altitude, so assert the functional property — they
// span from a standable row up to at least the hover line — not merely that updraft tiles exist.
// A tile COUNT would pass even if the columns floated unreachably in mid-air.
TEST(d4r1_updraft_columns_span_floor_to_hover_line){
    const auto& L = DUNGEON4_ROOM1_DATA;
    int topmost = L.h, bottommost = -1, count = 0;
    for(int y = 0; y < L.h; ++y)
        for(int x = 0; x < L.w; ++x)
            if(L.tiles[y * L.w + x] == (uint8_t)TileKind::Updraft){
                ++count;
                if(y < topmost)    topmost    = y;
                if(y > bottommost) bottommost = y;
            }
    REQUIRE(count > 0);
    CHECK(topmost <= D4_DEF.hover_row);      // reaches the boss's altitude...
    CHECK(bottommost >= L.h - 3);            // ...and starts at the standing row above the floor
}

// THE GOVERNING INVARIANT (M15 spec 3.1). Player bolts fly HORIZONTALLY, so the fight only works
// if no standing surface sits close enough to the hover line for a standing-or-jumping player to
// land a shot. Derived minimum = 2.0 (player centre above its surface) + 1.75 (up-aimed muzzle
// offset) + 7.0 (Featherleap double jump) + 2.0 (boss hitbox half-height) = 12.75 -> 13 tiles.
// Raise MIN_HOVER_CLEARANCE here (one place) if emulator QA shows a jump-shot still connects.
static constexpr int MIN_HOVER_CLEARANCE = 13;

TEST(d4r1_no_standing_surface_within_clearance_of_the_hover_line){
    harness::WorldModel wm; wm.glide = true;
    harness::Grid g = harness::build_grid(DUNGEON4_ROOM1_DATA, wm);
    const int limit = D4_DEF.hover_row + MIN_HOVER_CLEARANCE;   // surface rows must be >= this
    int checked = 0;
    for(int y = 0; y < g.h; ++y)
        for(int x = 0; x < g.w; ++x)
            if(g.stand(x, y)){                                   // (x,y) is a legal FEET position
                ++checked;
                const int surface_row = y + 1;                   // the tile the feet rest on
                if(surface_row < limit)
                    printf("  [clearance] standable feet (%d,%d) rests on row %d < %d\n",
                           x, y, surface_row, limit);
                CHECK(surface_row >= limit);
            }
    CHECK(checked > 0);                                          // non-vacuity: the arena IS standable
}

TEST(d4r1_boss_hover_row_is_inside_the_arena){
    CHECK(D4_DEF.hover_row > 0);
    CHECK(D4_DEF.hover_row < DUNGEON4_ROOM1_DATA.h - 2);
}

TEST(d4r1_arena_has_no_crystal_and_no_cage){
    CHECK_EQ(DUNGEON4_ROOM1_DATA.magic_crystal_count, 0);   // dodge-only fight: no magic economy
    CHECK(!DUNGEON4_ROOM1_DATA.has_cage);
    CHECK(!DUNGEON4_ROOM1_DATA.has_exit);
}

// ---- room 2: the spronk ----
TEST(d4r2_holds_the_cage_and_exit){
    CHECK(DUNGEON4_ROOM2_DATA.has_cage);
    CHECK(DUNGEON4_ROOM2_DATA.has_exit);
}

TEST(d4r2_cage_and_exit_are_reachable_from_the_entrance){
    harness::WorldModel wm; wm.glide = true;
    CHECK(harness::reaches(DUNGEON4_ROOM2_DATA, wm,
                           DUNGEON4_ROOM2_DATA.cage_tx, DUNGEON4_ROOM2_DATA.cage_ty));
    CHECK(harness::reaches(DUNGEON4_ROOM2_DATA, wm,
                           DUNGEON4_ROOM2_DATA.exit_tx, DUNGEON4_ROOM2_DATA.exit_ty));
}
```

**Every field name above is verified against the current headers** — `has_cage` / `cage_tx` / `cage_ty` / `has_exit` / `exit_tx` / `exit_ty` / `pickup_count` / `magic_crystal_count` / `room_door_count` / `boss` in `include/logic/level_data.h:26-45`, `RoomDoorSpawn{tx, ty, ...}` at `:16`, `AbilityPickup{tx, ty, ability}` in `include/logic/ability_pickup.h:5`, and `DungeonData{rooms, room_count, start_room}` at `:47-50`. If one nonetheless fails to compile, do NOT invent a name and do NOT delete the assertion: grep the header for the real spelling. The assertion's intent is fixed; only its spelling is negotiable.

- [ ] **Step 2: Run tests to verify they fail**

Run: `bash tools/host_test.sh`
Expected: FAIL. Task 4.1 already created the rooms and regenerated the headers, so this is not a missing-header failure — it is the geometry assertions reporting what the authored grids do not yet satisfy (most likely room 0's new `N`/`D` reachability and room 2's layout). Read the failures; they are the to-do list for Step 3.

If instead everything passes on the first run, do not celebrate — confirm the tests actually ran (`N` grew by the number of tests added) rather than the file silently failing to compile into the binary.

- [ ] **Step 3: Iterate the room geometry until green**

The three `.txt` grids from Task 4.1 are the implementation. Adjust them — NOT the assertions — until every test passes. Expect to iterate on room 0's new top platform (the `N`/`D` placement must be standable and reachable with `glide = true`) and room 2's layout.

If any assertion races or fails nondeterministically, the fix is deterministic geometry, NOT weakening the assertion. If you cannot make an assertion pass, STOP and raise it — do not ship a weaker test. In particular, do NOT lower `MIN_HOVER_CLEARANCE` to make the arena pass; move the ledges down instead. Lowering it silently deletes the fight's core constraint.

**Special case — `d4r0_shrine_is_reachable_before_the_climb`.** If this fails, it is NOT a geometry bug you introduced: the shaft is unchanged by M15 apart from its top two rows. It means either the harness's climb model is more conservative than the real physics, or D4 has a genuine pre-existing reachability problem. Either is a finding worth having. STOP and raise it to the dispatching agent with your analysis; do NOT relax the test to `wm.glide = true` (which would assert that Glide is needed to reach the shrine that grants Glide — an assertion that is false and, worse, self-satisfying), and do NOT delete it. Record the outcome under **Discoveries** at the top of this plan.

- [ ] **Step 4: Run the full gate**

Run: `bash tools/host_test.sh` → PASS, `0 checks failed`, N increased.
Run: `python tools/check_logic_purity.py` → `logic purity OK`.
Run: `python tools/validate_dungeons.py` → passes.
Run: `bash tools/build_rom.sh` → `ROM fixed!`, zero warnings.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 5: Commit Tasks 4.1 + 4.2 together**

```bash
git add tools/levels/ include/game/levels/ test/test_dungeon4_level.cpp
git commit -m "feat(content): D4 Gale Cliffs restructured 1 -> 3 rooms (ascent / arena / spronk); tests strengthened onto the shared harness with the hover-clearance invariant"
```

**After completing Phase 4:** 3-round review — geometry correctness, the clearance invariant's non-vacuity, and whether room 0's ascent still plays as authored.

---

# Phase 5 — Arm the boss + docs

**Execution Status:** ⬜ NOT STARTED

## Task 5.1: Arm the arena

**Files:**
- Modify: `tools/levels/dungeon4_room1.json` (add the `"boss"` key)

**Interfaces:**
- Consumes: `BOSS_SYMBOL['d4']` (Task 3.2), `logic::D4_DEF` (Task 1.2), the arena room (Task 4.1).
- Produces: a live D4 boss fight.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Write the failing test**

Append to `test/test_dungeon4_level.cpp`:

```cpp
TEST(d4r1_arena_is_armed_with_galewing){
    REQUIRE(DUNGEON4_ROOM1_DATA.boss != nullptr);   // the room JSON's "boss":"d4" resolved
    CHECK(DUNGEON4_ROOM1_DATA.boss->id == BossId::D4Galewing);
    CHECK(DUNGEON4_ROOM1_DATA.boss->locomotion == Locomotion::Hovering);
}

TEST(d4_only_room1_carries_a_boss){
    CHECK(DUNGEON4_ROOM0_DATA.boss == nullptr);
    CHECK(DUNGEON4_ROOM2_DATA.boss == nullptr);
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `bash tools/host_test.sh`
Expected: FAIL on `REQUIRE(DUNGEON4_ROOM1_DATA.boss != nullptr)` — the room compiles with `nullptr` while the JSON has no `"boss"` key.

- [ ] **Step 3: Add the boss key**

`tools/levels/dungeon4_room1.json` becomes:

```json
{ "tileset": "tiles",
  "boss": "d4",
  "entrances": [ { "id": 0, "facing": 1 }, { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 0, "target_entrance": 0 },
                  { "target_room": 2, "target_entrance": 0 } ] }
```

- [ ] **Step 4: Run the full gate**

Run: `bash tools/build_rom.sh` (regenerates the header so `boss` is non-null) → `ROM fixed!`, zero warnings.
Run: `bash tools/host_test.sh` → PASS, `0 checks failed`.
Run: `python tools/validate_dungeons.py` → passes.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 5: Commit**

```bash
git add tools/levels/dungeon4_room1.json include/game/levels/dungeon4_room1.h test/test_dungeon4_level.cpp
git commit -m "feat(content): arm the D4 arena with Galewing (boss: d4)"
```

---

## Task 5.2: Update `docs/content-recipes.md`

**Files:**
- Modify: `docs/content-recipes.md` (§2 "Add a boss" steps 1/4/7; §4 "Arena authoring constraints")

**Interfaces:**
- Consumes: everything shipped in Phases 1–5.
- Produces: corrected authoring guidance for D5–D8.

**Context.** Three statements in the recipes doc are now wrong or incomplete, and each would mislead the next boss author:

1. §2 step 4 still describes `boss_sprite_for` as "a **pointer-compare branch** — `if(def == &logic::D3_DEF)`". It has been an id `switch` with a loud `BN_ERROR` default since remediation Phase 5.
2. §2 step 7 says def-invariant tests become automatic "after the remediation plan's Phase 5 lands". They did **not** — Task 1.2 found `switch_budget_holds_for_all_defs` hand-listing three defs and missing `D2_DEF`/`D3_DEF`.
3. §4's arena constraints assume a floor-placed boss ("a gap or hazard tile there drops the boss through the floor"). That does not apply to `Locomotion::Hovering`, and a D5–D8 author reading it would not know an airborne option exists.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

- [ ] **Step 1: Correct §2 step 4 (sprite mapping)**

Replace the pointer-compare guidance with:

```markdown
4. Add an id-to-sprite row in `boss_sprite_for()` in `src/game/scene_dungeon.cpp` — a
   `switch(def->id)` returning the `bn::sprite_item` for your new `BossId` value. Add your
   `case` before the `default:`, and leave the `default: BN_ERROR(...)` arm alone: an
   unmapped boss id MUST keep failing loudly at fight start rather than silently rendering
   the D1 guardian.
```

- [ ] **Step 2: Correct §2 step 7 (def tests)**

Replace it with:

```markdown
7. Def-invariant tests are **hand-written** — there is no all-defs registry loop, despite what
   earlier drafts of this doc implied. Copy the pattern of `bossdef_d4_galewing_fields` in
   `test/test_boss.cpp` (vuln/locomotion/id/block-mode fields, descending `end_hp` to 0, and
   `pattern.active >= 28` for any ROCKFALL phase) AND add your def to
   `switch_budget_holds_for_all_defs`. Forgetting the second half is how `D2_DEF` and `D3_DEF`
   shipped without a budget check until M15 closed the gap. Still write a dungeon-level test for
   the arena room's integration (§1 step 6).
```

- [ ] **Step 3: Add the hovering exception to §4**

Add to the arena authoring constraints list:

```markdown
- **Hovering bosses (`Locomotion::Hovering`, M15/D4) ignore the floor rule.** They are placed at
  `BossDef::hover_row` (a TILE row, the boss's centre) and never touch the floor, so the
  "flat solid floor at `w/2`" requirement applies only to the player. In exchange they impose a
  stricter rule: because player bolts travel HORIZONTALLY, **no standing surface may sit within 13
  tiles below the hover line**, or the fight can be won from a ledge without ever using the arena's
  traversal kit. The derivation and the enforcing test live in
  `docs/superpowers/specs/2026-07-31-spronk-quest-m15-d4-boss-design.md` §3.1 and
  `test/test_dungeon4_level.cpp`'s `d4r1_no_standing_surface_within_clearance_of_the_hover_line`.
  A hovering boss also needs `aim_horizontal = false` so its aimed bolts track a player below it.
```

- [ ] **Step 4: Verify**

Run: `bash tools/host_test.sh` → PASS (docs-only change; the gate is that nothing regressed).

Re-read the three edited sections against the shipped code and confirm every file/symbol they name exists.

BEFORE marking this task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run tests and confirm green

- [ ] **Step 5: Commit**

```bash
git add docs/content-recipes.md
git commit -m "docs(recipes): correct boss sprite mapping + def-test guidance; document the Hovering arena rules"
```

**After completing Phase 5:** 3-round review across the whole M15 change set — cross-phase coherence, spec conformance, and whether a fresh reader could add D5's boss from the recipes alone.

---

# Phase 6 — Final gates + mGBA QA

**Execution Status:** ⬜ NOT STARTED

Agents build the ROM; they cannot play it. This phase is the human gate and it **blocks merge**.

## Task 6.1: Automated gates

- [ ] **Step 1: Run every gate on the final tree**

```bash
bash tools/host_test.sh          # must end "N/N tests passed, 0 checks failed"
python tools/check_logic_purity.py   # must print "logic purity OK"
python tools/validate_dungeons.py    # must pass
bash tools/build_rom.sh          # must end "ROM fixed!" with zero warnings
```

- [ ] **Step 2: Confirm the regression claim**

Verify that no assertion for `KING_DEF`, `D1_DEF`, `D2_DEF`, or `D3_DEF` was edited during M15:

```bash
git diff main -- test/test_boss.cpp
```

Expected: only ADDED tests, plus the one-line widening of `switch_budget_holds_for_all_defs`. Any modified existing assertion is a red flag — investigate before proceeding.

## Task 6.2: mGBA QA checklist (human)

Record results in this plan under **Discoveries** if anything differs from the spec.

- [ ] The core loop: climb an updraft holding A, glide out, land a hit, fall, repeat to a kill.
- [ ] **The 13-tile margin holds:** stand on the row-18 ledge, double-jump, fire up-aimed — the bolt MUST miss.
- [ ] Aimed bolts track a grounded/climbing player (the Task 2.2 fix). If they sail overhead, `aim_horizontal` is wrong.
- [ ] **Rockfall's leap offset on a hovering boss** (spec §3.5's known risk): the boss bobs upward before rocks drop. If it reads as a glitch rather than a wing-beat, zero `rockfall.leap_offset()` for `Hovering`.
- [ ] Death-restart re-places the boss at its hover line and does NOT pin the player (the M13 lesson).
- [ ] Phase 2 transition: taunt fires, fan attack appears, rockfall escalates 3 → 5.
- [ ] Defeated-boss skip: kill it, exit to hub, re-enter → no re-fight, onward door open (save v6 bit 3).
- [ ] D4 room 0 still plays as before after re-termination; the Glide shrine is still gettable pre-climb.
- [ ] Room 2's cage frees the spronk; exit returns to the hub; the ability/spronk persists.
- [ ] Camera behaves in the 24-tall arena — no scrolling past the level bounds (IMPL-9).
- [ ] **Regression:** King, D1, D2, D3 fights play identically to `main` — one full fight each, plus one death each.

## Task 6.3: Close out

- [ ] Update this plan's Execution Status table + every phase banner with ship SHAs.
- [ ] Update project memory: `boss-fight-scope` (D4 shipped; D5–D8 remain), and note that M15 proved the airborne mode.
- [ ] Run `/superpowers:finishing-a-development-branch` to merge.

---

## Execution strategy recommendation

**Recommended: subagent-driven development (`/superpowers:subagent-driven-development`) from a fresh session.**

Reasoning:

1. **This planning session's context is heavily consumed** — the plan plus the spec are self-contained, so a fresh session loses nothing.
2. **Most tasks are mechanical-move or data-entry tasks whose failure mode is a silent behavior change** — precisely what a fresh per-task reviewer catches and what a batch executor misses. Task 1.1's field placement and Task 2.2's aim substitution are both one-line changes that would break four shipped fights if done wrong, and both are invisible to the test suite if the tests were edited to accommodate them (which is why the plan forbids that explicitly).
3. **Phase 1 must land alone and first.** Phases 2, 3, and 4 all consume its symbols and touch disjoint files (`boss_fight.cpp` / `build_level.py`+`scene_dungeon.cpp`+art / `tools/levels/`+tests), so they MAY be dispatched in parallel afterwards — but only after Phase 1's commit is on the branch, and only in **separate worktrees** (see File Structure's conflict-control note: Phase 4 leaves the tree uncompilable mid-phase, which would fail Phases 2 and 3 gates spuriously). In a single tree, run them sequentially.
4. **Phase 4 is iterative in a way the others are not.** Level geometry converges by running the harness tests repeatedly; give it one focused agent with the freedom to iterate the `.txt` grids, and make sure that agent understands it may not touch `MIN_HOVER_CLEARANCE`.
5. **Phase 6 cannot be automated at all.** Budget for it as a human session, and expect at least one round of tuning commits afterward — M13 and M14 each needed two.

Suggested branch: `feat/m15-d4-boss`.
