# M12 — Boss Framework + D1 Boss Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract a reusable, data-described boss framework from the M11 Nightmare King (pure-logic `BossDef`/`BossState` + an engine attack library + shared helpers), refactor the King onto it with no behaviour change, and prove the abstraction with one new per-dungeon boss — a simple `AlwaysVulnerable` D1 Whispering Woods Guardian fought in a dedicated boss room before the spronk.

**Architecture:** Three-layer (pure `logic/` host-tested, `engine/` Butano glue, `game/` scenes). The King-specific `BossState` becomes data-driven (`BossDef` holds HP/phases/vulnerability; `BossState` reads it, phase is an index so phase count is variable). The King's scene combat primitives extract into a reusable `engine` attack library. The King and the D1 boss are **two thin consumers** of the same framework (Approach B) — the King keeps teleport + dialogue local to its scene; D1 gets a minimal scene hook in `scene_dungeon`.

**Tech Stack:** C++17, Butano 21.6.0 (GBA). Host tests: `bash tools/host_test.sh`. ROM: `bash tools/build_rom.sh`. Purity guard: `python tools/check_logic_purity.py`. Level compiler: `tools/build_level.py`. Spec: `docs/superpowers/specs/2026-06-24-spronk-quest-m12-boss-framework-design.md`.

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

**Overall:** 1/4 phases shipped. Branch: `feat/m12-boss-framework` (off `main`).

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure-logic framework (BossDef/BossState + KING_DEF + D1_DEF) | ✅ Shipped | `2bcbfff` (P1.1–1.4), `7b5c59d` (P1.5) | 427/427; King regression guard held; ROM builds |
| 2 — Engine attack library + shared helpers | ✅ Shipped | `51a43f6` | `engine/boss_attacks` (AttackPool/SpiralEmitter/TelegraphCue/BossHpBar + templated block/resolve_damage); ROM builds, 427/427, no game/ include |
| 3 — King refactor onto the framework | ✅ Shipped | `985aec5` | King uses `boss_attacks` (inline copies deleted, −84 lines); constants verified faithful (telegraph/pips/spiral); ROM builds, 427/427 |
| 4 — D1 boss + integration + invariants + QA | ⬜ Not started | — | — |

### Deviations
- **Phase 1:** tasks 1.1–1.4 landed as ONE commit (`2bcbfff`) not four — same two files, and four commits would have left misleading partially-broken intermediate states. **Task 1.5** also re-pointed `KING_MAX_HP`/`WOUND_DMG` → `KING_DEF.*` in `scene_boss.cpp`'s HP-pip + damage code (beyond the plan's enumerated API-call swaps) — required since those globals were removed in 1.1; identical mechanical mapping, zero behaviour change.

### Discoveries
- _(none yet)_

---

## Critical context for every executor (read before any task)

- **The regression guard is sacred.** Phase 1 re-points `test/test_boss.cpp` to a `KING_DEF` that reproduces the SHIPPED King numbers EXACTLY. After the refactor, every existing King test MUST still pass with the SAME meaning. If a King test needs its *expected values* changed to pass, that is a BEHAVIOUR CHANGE — STOP and re-examine; the generalization must be behaviour-preserving for the King. (The only allowed test edits are mechanical: constructing `BossState` via `reset(KING_DEF)` and reading `KING_DEF` constants instead of the old globals.)
- **Current King numbers (the source of truth for KING_DEF)** from `include/logic/boss.h`: `KING_MAX_HP=90`, `P1_END_HP=60`, `P2_END_HP=30`, `WOUND_DMG=10`, `EXPOSE_FRAMES=75`, `HIT_IFRAMES=45`, `SWITCH_BUDGET=60`, `PHASE_PATTERNS[3] = {{72,30,40},{66,30,30},{60,30,20}}`, expose spell = `logic::SpellId::Light`, 3 phases with end_hp {60,30,0}.
- **Pitfalls:** IMPL-1 (NO `bn::` in `include/logic/` — `boss.h` stays pure), IMPL-2 (integer-only), TEST-1 (explicit `tick()` stepping, no frame-timing dependence), TEST-2 (exact raw int asserts). Read `docs/pitfalls/implementation-pitfalls.md` + `docs/pitfalls/testing-pitfalls.md` before each task.
- **No save-format change.** SaveData v5 (M11) is unchanged. The boss is NOT persisted; only the existing spronk-freed latch records progress.

## File Structure

**Modified (pure logic):**
- `include/logic/boss.h` — generalize: add `VulnMode`, `BossPhaseDef`, `BossDef`; rewrite `BossState` to be def-driven (phase index, `reset(const BossDef&)`); define `KING_PHASES`/`KING_DEF` + `D1_PHASES`/`D1_DEF`. Keep `PhasePattern`, `AttackStep`, `SWITCH_BUDGET`.
- `include/logic/level_data.h` — add `const logic::BossDef* boss = nullptr;` to `LevelData`.

**New (engine):**
- `include/engine/boss_attacks.h` + `src/engine/boss_attacks.cpp` — the attack library (aimed/spiral/fan + telegraph) + the shared attack-pool/HP-bar/telegraph/block/damage helpers, all driven by `logic::BossState`.

**Modified (game/scene):**
- `src/game/scene_boss.cpp` — `run_boss` rebuilt on the framework + `KING_DEF` (teleport + dialogue stay local).
- `src/game/scene_dungeon.cpp` — boss-room handling (a room with `level.boss != nullptr` runs a boss fight; victory opens the onward door).

**New / modified (content + art + tooling):**
- D1 restructured to 3 rooms: `tools/levels/dungeon1_room0.txt/.json` (entry — existing content, cage removed, room-door to room1), `dungeon1_room1.txt/.json` (boss room), `dungeon1_room2.txt/.json` (spronk room) → generated headers; `include/game/levels/dungeons.h` DUNGEON1 → 3-room aggregation.
- `tools/build_level.py` — optional `"boss"` JSON key emitting the room's `boss` field.
- `graphics/guardian.*` (Guardian placeholder sprite) via `tools/make_placeholder_art.py`.

**New / modified (tests):**
- `test/test_boss.cpp` — re-pointed to `KING_DEF` + new `D1_DEF`/AlwaysVulnerable/variable-phase/per-def-invariant cases.
- `test/test_dungeon1_level.cpp` — updated for the 3-room layout + boss-room/spronk reachability invariants (fail-on-broken). (Also check `test/test_dungeon1.cpp` still passes.)

---

## Phase 1 — Pure-logic framework

**Execution Status:** ✅ SHIPPED at `2bcbfff` (P1.1–1.4) + `7b5c59d` (P1.5) on 2026-06-24 — def-driven BossState + KING_DEF (regression guard held, all King tests re-pointed with unchanged values) + D1_DEF + per-def SWITCH_BUDGET invariant + minimal King-scene API migration. 427/427 host tests, purity OK, ROM builds.

**Why this matters:** the data-described core is the whole point of M12 + the M12-extraction seed realized. It MUST keep the King behaviour-identical (regression guard) while supporting a different-shaped boss (D1). This is the project's "discover the abstraction from a working example" discipline.

**Pitfalls:** IMPL-1 (no `bn::`), IMPL-2 (integer-only), TEST-1/TEST-2.

### Task 1.1: Add the data model (VulnMode / BossPhaseDef / BossDef) + def-driven BossState

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`.

BEFORE starting work:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

Context: today `BossState` hardcodes the King — a fixed `BossPhase{P1,P2,P3,Exposed,Defeated}` enum, global `KING_MAX_HP`/`PHASE_PATTERNS`, and methods reading those globals. Generalize so boss-specific data lives in a `BossDef` the state points at, and `phase` is an integer index (0..phase_count-1) so any phase count works. `Exposed` becomes the existing `expose_timer>0`; `Defeated` becomes `hp<=0` (drop the enum).

- [ ] **Step 1: Write failing tests** (`test/test_boss.cpp`, new cases — keep the file compiling by also doing Step 3's `KING_DEF` in this task; see note):

```cpp
// (added near the existing tests; KEEP the regression tests — Task 1.2 re-points them)
TEST(bossdef_king_reset_initial_state){
    BossState b; b.reset(KING_DEF);
    CHECK_EQ(b.hp, KING_DEF.max_hp);          // 90
    CHECK_EQ(b.phase, 0);
    CHECK_EQ(b.phase_start_hp, KING_DEF.max_hp);
    CHECK_EQ(b.expose_timer, 0);
    CHECK_EQ(b.hit_iframes, 0);
    CHECK(!b.exposed()); CHECK(!b.defeated());
}
TEST(bossdef_king_has_three_phases){ CHECK_EQ(KING_DEF.phase_count, 3); }
TEST(bossdef_d1_has_two_phases_always_vulnerable){
    CHECK_EQ(D1_DEF.phase_count, 2);
    CHECK((int)D1_DEF.vuln == (int)VulnMode::AlwaysVulnerable);
}
```

- [ ] **Step 2: Run, verify fail** — `bash tools/host_test.sh` (no `BossDef`/`reset(def)` yet).

- [ ] **Step 3: Implement** in `boss.h`:
  - Keep `PhasePattern`, `AttackStep`, `SWITCH_BUDGET`.
  - Add:
    ```cpp
    enum class VulnMode : uint8_t { SpellExpose, AlwaysVulnerable };
    struct BossPhaseDef { int end_hp; PhasePattern pattern; uint8_t attacks; };
    struct BossDef {
        int max_hp, wound_dmg, hit_iframes, expose_frames;
        VulnMode vuln; SpellId expose_spell;          // SpellId from spell.h
        const BossPhaseDef* phases; int phase_count;
    };
    ```
    (Add `#include "logic/spell.h"` for `SpellId`. Verify spell.h is pure — it is.)
  - Rewrite `BossState`:
    ```cpp
    struct BossState {
        const BossDef* def = nullptr;
        int hp=0, phase=0, phase_start_hp=0, expose_timer=0, attack_timer=0, hit_iframes=0;
        void reset(const BossDef& d){ def=&d; hp=d.max_hp; phase=0; phase_start_hp=d.max_hp;
                                      expose_timer=0; attack_timer=0; hit_iframes=0; }
        bool exposed() const { return expose_timer>0; }
        bool defeated() const { return hp<=0; }
        bool vulnerable() const { return def->vuln==VulnMode::AlwaysVulnerable || exposed(); }
        int active_phase() const { return phase; }   // (phase is the combat-phase index)
        const PhasePattern& pat() const { return def->phases[phase].pattern; }
        int phase_period() const { const PhasePattern& p=pat(); return p.telegraph_frames+p.attack_active_frames+p.recovery_frames; }
        // on_expose_hit / on_wound / tick / on_player_death / current_step in Tasks 1.2-1.4
    };
    ```
  - Define `KING_PHASES` + `KING_DEF` + `D1_PHASES` + `D1_DEF` (exact numbers refined in Task 1.2 for the King and Task 1.3 for D1; for now a minimal valid form so the file compiles + the Step-1 tests pass — they only check counts/vuln/reset). Use `attacks = 0` as a placeholder in the minimal phase tables here; the `BOSS_ATK_*` constants + real attack masks land in 1.2 (King) / 1.3 (D1). It is fine to define the defs fully here and refine.

  DO NOT keep the old `BossPhase` enum or the global `KING_MAX_HP`/`PHASE_PATTERNS` — they move into `KING_DEF`. (Task 1.2 migrates the tests that referenced them.)

- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): data-described BossDef/BossPhaseDef/VulnMode + def-driven BossState (M12 P1.1)"`

BEFORE marking complete: review vs testing-pitfalls; confirm no `bn::` (run `python tools/check_logic_purity.py`); integer-only.

### Task 1.2: KING_DEF reproduces the shipped King + re-point the regression tests

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`.

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

This task is the REGRESSION GUARD. Port the existing King methods (`on_light_hit`/`on_wound`/`tick`/`on_player_death`/`current_step`/`advance_phase_for_hp`) to be def-driven, define `KING_DEF` with the exact shipped numbers, and re-point ALL existing King tests to `KING_DEF` WITHOUT changing their expected values.

- [ ] **Step 1:** Define in `boss.h`:
  ```cpp
  inline constexpr BossPhaseDef KING_PHASES[3] = {
      { 60, {72,30,40}, KING_ATTACKS }, { 30, {66,30,30}, KING_ATTACKS }, { 0, {60,30,20}, KING_ATTACKS } };
  inline constexpr BossDef KING_DEF{ 90, 10, 45, 75, VulnMode::SpellExpose, SpellId::Light, KING_PHASES, 3 };
  ```
  **Attack-bit scheme (single source = `boss.h`):** define the named bit constants in `boss.h` (pure data — they live in the def): `inline constexpr uint8_t BOSS_ATK_AIMED=1<<0, BOSS_ATK_SPIRAL=1<<1, BOSS_ATK_FAN=1<<2;` and `inline constexpr uint8_t KING_ATTACKS = BOSS_ATK_AIMED|BOSS_ATK_SPIRAL|BOSS_ATK_FAN;`. The engine attack library (Phase 2) `#include`s `logic/boss.h` and uses these SAME constants to interpret a phase's `attacks` mask — so there is NO duplicated magic value (engine may include logic; logic must NOT include engine).
- [ ] **Step 2:** Port the methods to be def-driven (behaviour-identical):
  - `on_expose_hit(SpellId s)` (rename of `on_light_hit`): `if(defeated()||hit_iframes>0||def->vuln!=VulnMode::SpellExpose||s!=def->expose_spell) return; expose_timer=def->expose_frames;` (phase stays the index; "Exposed" is just `expose_timer>0`). NOTE: the old code stored `exposed_return`; with phase-as-index that is just `phase` (unchanged across expose), so `exposed_return` is ELIMINATED — verify the King tests still pass with this simplification.
  - `advance_phase_for_hp()`: walk `def->phases` to find the index whose band contains `hp` (phase i active while `hp > phases[i].end_hp`, except the last). On change set `phase=newidx; phase_start_hp=hp;`.
  - `on_wound(dmg)`: `if(defeated()||hit_iframes>0||!vulnerable()) return; hp-=dmg; if(hp<0)hp=0; advance_phase_for_hp(); hit_iframes=def->hit_iframes; if(def->vuln==VulnMode::SpellExpose) expose_timer=0;`
  - `tick()`: `if(defeated())return; if(hit_iframes>0)--hit_iframes; if(expose_timer>0){--expose_timer; return;} if(++attack_timer>=phase_period()) attack_timer=0;` (attack pattern frozen while exposed — same as today).
  - `current_step()`: index `pat()` (the active phase's pattern) for Telegraph/Active/Recovery — same thresholds.
- [ ] **Step 3:** Re-point the existing King tests: replace `b.reset()`→`b.reset(KING_DEF)`, `b.on_light_hit()`→`b.on_expose_hit(SpellId::Light)`, `KING_MAX_HP`→`KING_DEF.max_hp`, `WOUND_DMG`→`KING_DEF.wound_dmg`, `EXPOSE_FRAMES`→`KING_DEF.expose_frames`, `HIT_IFRAMES`→`KING_DEF.hit_iframes`, phase enum checks (`BossPhase::P2`)→phase index (`1`), `PHASE_PATTERNS[i]`→`KING_PHASES[i].pattern`. The `wound_once` helper updates to `on_expose_hit(SpellId::Light)`. **Do NOT change any expected numeric value or assertion meaning.**
- [ ] **Step 4: Run** `bash tools/host_test.sh` — ALL King tests green with unchanged meaning. If any fails, the port changed behaviour — fix the port, NOT the test.
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): KING_DEF reproduces shipped King; re-point regression tests to def (M12 P1.2)"`

BEFORE marking complete: confirm EVERY pre-existing King test passes with its original expected values; purity clean. The commit subject preserves the regression-guard intent (tests preserved, not weakened).

### Task 1.3: D1_DEF + AlwaysVulnerable behaviour tests

**Files:** Modify `include/logic/boss.h`, `test/test_boss.cpp`.

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Failing tests** for the AlwaysVulnerable path:
```cpp
TEST(d1_wound_lands_without_expose){
    BossState b; b.reset(D1_DEF);
    CHECK(b.vulnerable());                       // always vulnerable, no Light needed
    b.on_wound(D1_DEF.wound_dmg);
    CHECK_EQ(b.hp, D1_DEF.max_hp - D1_DEF.wound_dmg);
}
TEST(d1_hit_iframes_cap_wound_rate){
    BossState b; b.reset(D1_DEF);
    b.on_wound(D1_DEF.wound_dmg);                // lands
    b.on_wound(D1_DEF.wound_dmg);                // blocked by i-frames
    CHECK_EQ(b.hp, D1_DEF.max_hp - D1_DEF.wound_dmg);
    for(int i=0;i<D1_DEF.hit_iframes;++i) b.tick();
    b.on_wound(D1_DEF.wound_dmg);                // lands again after i-frames
    CHECK_EQ(b.hp, D1_DEF.max_hp - 2*D1_DEF.wound_dmg);
}
TEST(d1_no_expose_on_spell){
    BossState b; b.reset(D1_DEF);
    b.on_expose_hit(SpellId::Light);             // AlwaysVulnerable -> no-op
    CHECK_EQ(b.expose_timer, 0);
}
TEST(d1_two_phase_advance_and_defeat){
    BossState b; b.reset(D1_DEF);
    int guard=0;
    while(!b.defeated() && guard++<100){ b.on_wound(D1_DEF.wound_dmg); for(int i=0;i<D1_DEF.hit_iframes;++i) b.tick(); }
    CHECK(b.defeated());
    CHECK_EQ(guard, D1_DEF.max_hp / D1_DEF.wound_dmg);   // exact hit count to kill
}
```
- [ ] **Step 2: Run, verify fail.**
- [ ] **Step 3: Implement** `D1_PHASES` + `D1_DEF` in `boss.h`:
  ```cpp
  inline constexpr BossPhaseDef D1_PHASES[2] = {
      { 30, {80,30,40}, D1_ATTACKS_P1 }, { 0, {70,30,30}, D1_ATTACKS_P2 } };
  inline constexpr BossDef D1_DEF{ 60, 10, 30, 0, VulnMode::AlwaysVulnerable, SpellId::None, D1_PHASES, 2 };
  ```
  (`D1_ATTACKS_P1`/`P2` use the `BOSS_ATK_*` constants from Task 1.2 — e.g. `inline constexpr uint8_t D1_ATTACKS_P1 = BOSS_ATK_AIMED; inline constexpr uint8_t D1_ATTACKS_P2 = BOSS_ATK_AIMED|BOSS_ATK_FAN;` (finalized in Task 4.2). `expose_frames=0` + `expose_spell=SpellId::None` are unused for AlwaysVulnerable.)
- [ ] **Step 4: Run, verify green.**
- [ ] **Step 5: Commit** — `git commit -m "feat(logic): D1_DEF AlwaysVulnerable boss + behaviour tests (M12 P1.3)"`

BEFORE marking complete: review vs testing-pitfalls; cover wound-without-expose, i-frame cap, no-expose-on-spell, two-phase advance + exact defeat count; purity clean.

### Task 1.4: Per-def SWITCH_BUDGET invariant (non-vacuous)

**Files:** Modify `test/test_boss.cpp`.

BEFORE starting work: read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1: Failing test** — every def's telegraphs (+ expose window when SpellExpose) ≥ SWITCH_BUDGET:
```cpp
static void check_budget(const BossDef& d){
    for(int i=0;i<d.phase_count;++i) CHECK(d.phases[i].pattern.telegraph_frames >= SWITCH_BUDGET);
    if(d.vuln==VulnMode::SpellExpose) CHECK(d.expose_frames >= SWITCH_BUDGET);
}
TEST(switch_budget_holds_for_all_defs){ check_budget(KING_DEF); check_budget(D1_DEF); }
```
- [ ] **Step 2: Run — green** (the defs already satisfy it). Then VERIFY non-vacuity: temporarily set a `D1_PHASES` telegraph to 30, confirm this test FAILS, revert.
- [ ] **Step 3: Commit** — `git commit -m "test(logic): per-def SWITCH_BUDGET invariant (M12 P1.4)"`

BEFORE marking complete: document the non-vacuity break/revert you ran; purity clean; full suite green.

### Task 1.5: Migrate `scene_boss.cpp` to the new BossState API (minimal — keep the ROM building)

**Files:** Modify `src/game/scene_boss.cpp`.

**Why this is in Phase 1, not Phase 3:** Phase 1 REMOVES the old `BossState` API (`BossPhase` enum, `exposed_return`, `on_light_hit()`, `reset()`-no-arg) that `scene_boss.cpp` (the King — the only non-test consumer) still uses (e.g. `scene_boss.cpp:251` `b.exposed_return`, `:252/461` `BossPhase::P1/P2/P3`, the `b.reset()`/`on_light_hit()` calls). Without this task the ROM cannot build after Phase 1, and Phase 2's `ROM fixed!` gate would fail. This task does the MINIMAL compile-fix only; the deeper refactor (swap the King's inline attack code for the engine library) is Phase 3.

BEFORE starting work: read docs/pitfalls/implementation-pitfalls.md. This task changes NO behaviour — it is a pure API migration; the King must play identically (verified at Phase 4.6 QA).

- [ ] **Step 1:** Replace the King's old-API calls with the new def-driven API, KEEPING all inline attack/teleport/dialogue code as-is:
  - `logic::BossState b; b.reset();` → `b.reset(logic::KING_DEF);`
  - `b.on_light_hit()` → `b.on_expose_hit(logic::SpellId::Light)`
  - `auto combat_phase = [&]{ return b.exposed() ? b.exposed_return : b.phase; };` → `auto combat_phase = [&]{ return b.phase; };` (phase-as-index: `b.phase` already holds the combat phase during expose, since expose no longer overwrites it — this is the `exposed_return` elimination, verified harmless by the Phase-1 King tests).
  - `logic::BossPhase last_phase = logic::BossPhase::P1;` → `int last_phase = 0;` (and `last_phase = logic::BossPhase::P1;` resets → `last_phase = 0;`).
  - Dialogue triggers: `cp == logic::BossPhase::P2` → `cp == 1`; `cp == logic::BossPhase::P3` → `cp == 2`; the `cp != logic::BossPhase::Defeated` guard → `!b.defeated()`.
  - Any `current_step()`/`active_phase()` usage stays (same names, now def-driven).
  - Do NOT touch the inline attack pool / spawn_attack / telegraph / hp-bar / block / teleport / `boss_say` yet — Phase 3 swaps those for the library.
- [ ] **Step 2:** `bash tools/build_rom.sh` → `ROM fixed!` (the King compiles against the new API). `bash tools/host_test.sh` green.
- [ ] **Step 3: Commit** — `git commit -m "refactor(game): migrate King scene to def-driven BossState API (no behaviour change) (M12 P1.5)"`

BEFORE marking complete: ROM builds; host tests green; ONLY API calls changed (no attack/teleport/dialogue logic touched); commit subject states "no behaviour change".

**After completing Phase 1:**
Review the batch (min 3 rounds). Confirm: NO `bn::` in `boss.h`; the King is behaviour-identical (all original King tests pass unchanged in meaning); `phase`-as-index handles 2 and 3 phases; `exposed_return` elimination verified harmless (King tests + the migrated scene compile + play identically); the budget invariant fails-on-broken; **the ROM builds** (Task 1.5). Run `bash tools/host_test.sh` (green) + `python tools/check_logic_purity.py`.

---

## Phase 2 — Engine attack library + shared helpers

**Execution Status:** ✅ SHIPPED at `51a43f6` on 2026-06-24 — `engine/boss_attacks.{h,cpp}` (faithful extraction of the King's aimed/spiral/fan + telegraph + HP-bar + block + damage-resolution, driven by `logic::BossState`; block/resolve_damage templated on the pool/meter types so engine stays game/-free). ROM builds, 427/427, `scene_boss.cpp` untouched (King inline copies remain for Phase 3).

**Why this matters:** the reusable scene-side combat — extracting the King's primitives so a new boss is mostly data. Engine code is NOT host-testable (uses `bn::`), so correctness rides on the Phase-1 logic + Phase-3/4 ROM builds + QA. Do NOT add decision logic here that belongs in `BossState`.

**Pitfalls:** IMPL-1 (`bn::` allowed in engine, NOT logic), IMPL-2, IMPL-3 (mutate sprites only via Butano APIs). Gate: `bash tools/build_rom.sh` → `ROM fixed!`.

### Task 2.1: Create `engine/boss_attacks` — attack library + shared helpers

**Files:** Create `include/engine/boss_attacks.h`, `src/engine/boss_attacks.cpp`. Reference (copy from, do not yet delete) the attack code in `src/game/scene_boss.cpp` (the `AttackInst` pool, `launch`, `spawn_attack` aimed/spiral/fan, the telegraph orb, the King HP pips, the block defense, the damage-resolution).

Context: this is a faithful EXTRACTION — move the King's scene combat primitives into reusable functions, parameterized by boss/player positions + `logic::BossState`. The attack-bit scheme (`BOSS_ATK_AIMED/SPIRAL/FAN`) is defined ONCE in `logic/boss.h` (Task 1.2); `boss_attacks.h` `#include`s `logic/boss.h` and uses those SAME constants to interpret a phase's `attacks` mask — NO duplicated bit values (engine may include logic).

- [ ] **Step 1:** Define the attack bits + an `AttackPool` helper struct (the `AttackInst` vector + `launch` + advance/render/expire), a `telegraph cue` helper, a `boss-hp-pip bar` helper (pip count = `def->max_hp / def->wound_dmg`, bounded to a fixed cap — e.g. `bn::vector<…,16>` — so it works for the King's 9 pips and D1's 6), a `block` helper (player bolt/Fire/Ice vs the attack pool), and `spawn_attack(variant, pool, boss_pos, player_pos, phase_speed)` covering aimed/spiral/fan. Signatures take plain ints/`logic::Vec2` + a `bn::camera_ptr&` + sprite items — mirror how `scene_boss.cpp` calls them today.
- [ ] **Step 2:** Build: `bash tools/build_rom.sh` → `ROM fixed!` (the new TU compiles; not yet used). Host tests unaffected (engine isn't host-compiled) — run `bash tools/host_test.sh` to confirm still green.
- [ ] **Step 3: Commit** — `git commit -m "feat(engine): boss_attacks library — aimed/spiral/fan + telegraph/hp-bar/block/pool helpers (M12 P2.1)"`

BEFORE marking complete: ROM builds; the library is self-contained (no game/ includes); the attack-bit scheme matches `boss.h`'s comment. Do NOT delete the King's inline copies yet (Phase 3 swaps them).

**After completing Phase 2:** Review (min 3 rounds). Confirm: pure extraction (no behaviour invented); three-layer respected (engine only); ROM builds; bit scheme single-sourced via cross-ref comments.

---

## Phase 3 — King refactor onto the framework

**Execution Status:** ✅ SHIPPED at `985aec5` on 2026-06-24 — King's inline attack/telegraph/hp-bar/block/damage code replaced by `engine::boss_attacks` (teleport + dialogue + aim + pause stay local); `scene_boss.cpp` 525→441 lines. Library constants verified to match the shipped King exactly (telegraph −14/pulse-6, pips (−32+i·8, 68), spiral STEPS 5/cd 5 + 8-dir table). ROM builds, 427/427. (Behaviour identity to be confirmed in 4.6 emulator QA.)

**Why this matters:** proves the King reuses the framework with NO behaviour change. Highest regression risk — the King had 9 QA rounds. The Phase-1 host tests guard the logic; this phase guards the scene via ROM + manual QA.

**Pitfalls:** IMPL-1/2/3. Gate: `bash tools/build_rom.sh` → `ROM fixed!` + host tests green.

### Task 3.1: Swap the King's inline attack code for the `engine::boss_attacks` library

**Files:** Modify `src/game/scene_boss.cpp`.

Context: Task 1.5 already migrated the King to the def-driven `BossState` API (`reset(KING_DEF)`, `on_expose_hit`, phase-index, dialogue) — so the logic calls are DONE. This task replaces the King's INLINE attack pool / `spawn_attack` (aimed/spiral/fan) / telegraph orb / HP-pip bar / block defense / damage-resolution with calls into the `engine::boss_attacks` library (Phase 2), and DELETES the now-dead inline copies. KEEP LOCAL (do NOT move to the framework): the teleport-between-perches, the `boss_say` intro/phase/death dialogue, the King sprite, the arena camera. The King must look + play identically.

- [ ] **Step 1:** Replace the inline attack/telegraph/hp-bar/block/damage code with the `engine::boss_attacks` helpers; delete the dead inline copies (single source = the library). The attack SELECTION (which of the phase's `attacks` bits fires this Active window) uses `logic::KING_PHASES[b.phase].attacks` via a small local cycler reproducing today's 3-attack rotation. The HP-bar helper derives its pip count from `KING_DEF.max_hp / KING_DEF.wound_dmg` (= 9, unchanged). Keep the teleport + dialogue local.
- [ ] **Step 2:** Build: `bash tools/build_rom.sh` → `ROM fixed!`. Run `bash tools/host_test.sh` (King logic tests still green).
- [ ] **Step 3: Commit** — `git commit -m "refactor(game): King uses the boss_attacks library (teleport/dialogue stay local) (M12 P3.1)"`

BEFORE marking complete: ROM builds; host tests green; the King uses the shared library (NO leftover inline duplicate attack/telegraph/hp-bar/block code); teleport + dialogue still present + local. **Flag for QA:** the King must play identically — verified in Phase 4.6 emulator QA (a regression here is the top risk).

**After completing Phase 3:** Review (min 3 rounds). Confirm: behaviour-preserving (logic tests green); no decision logic added to the scene; idiosyncrasies stayed local; the inline attack duplicates are gone (single source = the library).

---

## Phase 4 — D1 boss + integration + invariants + QA

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** proves the framework with a different-shaped boss in a real dungeon, and wires the boss room into the dungeon flow. Restructures D1 into 3 rooms.

**Pitfalls:** the no-soft-lock invariants MUST fail-on-broken (non-vacuity). Restructuring D1 must not strand the spronk/exit. Reuse the 2-wide flood-fill from `test/test_dungeon8_level.cpp`.

### Task 4.1: `LevelData.boss` field + build_level.py support

**Files:** Modify `include/logic/level_data.h`, `tools/build_level.py`, `tools/test_build_level.py`.

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1:** Add `const logic::BossDef* boss = nullptr;` to `LevelData` (after `magic_crystals`). Default null = ordinary room. (Pure logic — `BossDef` is in `boss.h`; add `#include "logic/boss.h"` to `level_data.h` if not transitively present.)
- [ ] **Step 2:** Add compiler unit tests in `tools/test_build_level.py`: a room JSON with `"boss":"d1"` emits `&D1_DEF` for the `boss` field + the generated header `#include`s `"logic/boss.h"`; a room without `"boss"` emits `boss = nullptr` (i.e. omits the field / sets it null). Run `cd tools && python -m unittest test_build_level.py` → fail.
- [ ] **Step 3:** Implement in `build_level.py`: an optional `"boss"` JSON key looked up in an EXPLICIT name→symbol dict (start with `{"d1": "D1_DEF"}`) — emit `&<symbol>` into the `LevelData` initializer's `boss` field and add `#include "logic/boss.h"` to the generated `.h`. Use `D1_DEF` (the canonical symbol defined in `boss.h` Task 1.3) — this is the SAME symbol referenced in `boss.h`, `build_level.py`, and `dungeons.h`. An unknown boss name is a compile error (raise `LevelError`). Run unit tests → green.
- [ ] **Step 4:** `bash tools/host_test.sh` green (existing rooms still compile with `boss=nullptr`).
- [ ] **Step 5: Commit** — `git commit -m "feat(level): LevelData.boss field + build_level.py boss key (M12 P4.1)"`

BEFORE marking complete: review vs testing-pitfalls; existing levels unaffected (boss defaults null); compiler tests cover present + absent.

### Task 4.2: Guardian art + D1_DEF attacks finalized

**Files:** Modify `tools/make_placeholder_art.py` (+ generated `graphics/guardian.*`); confirm `D1_DEF` attacks in `include/logic/boss.h`.

- [ ] **Step 1:** Add `gen_guardian()` (a readable forest-guardian placeholder, e.g. 32x32, distinct silhouette; obey GBA 4bpp/16-colour + a Butano sprite size) + call it in `__main__`. Generate; confirm `graphics/guardian.json` valid.
- [ ] **Step 2:** Finalize D1's attacks (from the extracted library — REUSE existing primitives to keep scope tight; e.g. P1 = aimed bolt, P2 = aimed + fan). Update `D1_ATTACKS_P1/P2` bits in `boss.h` if they changed. (A new bespoke primitive like a lunge is OPTIONAL polish — do NOT add unless QA demands it.)
- [ ] **Step 3:** `bash tools/build_rom.sh` → `ROM fixed!` (art imports). `bash tools/host_test.sh` green.
- [ ] **Step 4: Commit** — `git commit -m "art: Guardian placeholder + finalize D1 attack set (M12 P4.2)"`

BEFORE marking complete: sprite imports cleanly; ROM builds.

### Task 4.3: Restructure D1 into 3 rooms (entry → boss → spronk)

**Files:** Create `tools/levels/dungeon1_room0.txt/.json`, `dungeon1_room1.txt/.json`, `dungeon1_room2.txt/.json`; **delete** `tools/levels/dungeon1.txt` + `dungeon1.json` AND the now-orphaned generated `include/game/levels/dungeon1.h` (replaced by the room0/1/2 headers — `host_test.sh` only regenerates headers for existing `.txt` files, so a deleted `.txt` leaves a stale `.h` that must be removed); regenerate headers; update `include/game/levels/dungeons.h` DUNGEON1 aggregation to 3 rooms (swap the `#include "game/levels/dungeon1.h"` for the three room headers). Model on the D8 multi-room files + `dungeons.h:48-50`.

BEFORE starting work: read docs/pitfalls/implementation-pitfalls.md. Reference the current `tools/levels/dungeon1.txt/.json` to preserve the entry content.

- [ ] **Step 1:** `dungeon1_room0` = the existing D1 entry content with the **cage removed** + a `room_door` to room1 (the Featherleap `F` shrine + spawn stay here). `dungeon1_room1` = the **boss room** (`has_cage=false`, `has_exit=false`; JSON `"boss":"d1"`; an arena floor + a return room-door + an onward room-door to room2 that the boss-victory opens; ≥30 wide; standard floor convention). `dungeon1_room2` = the **spronk room** (`has_cage=true` + `has_exit=true` + a return room-door). Put a header comment noting the M12 restructure.
- [ ] **Step 2:** Update `dungeons.h`: include the 3 room headers; `DUNGEON1_ROOMS[] = { &DUNGEON1_ROOM0_DATA, &DUNGEON1_ROOM1_DATA, &DUNGEON1_ROOM2_DATA }; DUNGEON1_DUNGEON{ ..., 3, 0 }`. The generated room1 header references `D1_DEF` (the canonical symbol from `boss.h`, via the `build_level.py` `"boss":"d1"` mapping) and `#include`s `"logic/boss.h"` — so `dungeons.h` needs no extra boss include beyond what the generated headers pull in. (`D1_DEF` is the ONE name used in boss.h + build_level.py + the generated header.)
- [ ] **Step 3:** `bash tools/host_test.sh` (regenerates headers) — expect the D1 LEVEL test to FAIL (it asserts the old single-room layout); that's fixed in Task 4.4. The compiler + other tests must still pass. `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 4: Commit** — `git commit -m "content: restructure D1 into entry -> boss -> spronk rooms (M12 P4.3)"`

BEFORE marking complete: headers generate; ROM builds; room0 has no cage; room1 has the boss + no cage/exit; room2 has the spronk + exit; the room-door graph connects 0→1→2 (+ returns).

### Task 4.4: scene_dungeon boss-room handling + D1 invariants

**Files:** Modify `src/game/scene_dungeon.cpp`; rewrite `test/test_dungeon1_level.cpp` (+ verify `test/test_dungeon1.cpp`).

BEFORE starting work: invoke /superpowers:test-driven-development; read docs/pitfalls/testing-pitfalls.md.

- [ ] **Step 1 (invariants first — TDD):** rewrite `test_dungeon1_level.cpp` for the 3-room layout (model on `test_dungeon8_level.cpp`'s 2-wide flood-fill): room0 entrance reaches the room-door to room1; room1 (boss arena) is traversable + the onward door reachable; room2 spronk + exit reachable with the D1 kit (bolt + double-jump only — treat no Fire/Ice/etc.); no pit/one-way traps; room1 has `boss != nullptr`, room2 has the cage. Each invariant must FAIL on a deliberately-broken layout (document the break/revert). Run → fail (until the layout + code support it).
- [ ] **Step 2 (boss-room handling):** add a self-contained helper `run_room_boss(const logic::LevelData& room, logic::World&, logic::PlayerState&, <the camera / loaded-level / HUD handles play_room already holds>)` in `scene_dungeon.cpp` (or an adjacent helper TU). It is a NEW thin consumer of the framework, **parallel to the King's `run_boss`** (NOT a shared monolith): it runs its OWN fight loop — `logic::BossState b; b.reset(*room.boss);` + `engine::boss_attacks` helpers + player movement + the shot-aim (`engine::read_aim_dy`) + pause (`engine::check_pause`) — using the dungeon ROOM as the arena (reads `*room.boss`; no separate arena `DungeonData`). Keep it minimal: **no teleport, no dialogue** (King-only). A **death with lives left** restarts the fight INSIDE the helper (full-fight restart). The loop returns one of: `Victory`, or `GameOver` (player hit 0 lives — NOT handled here).
  - **Game-Over reuses the dungeon's EXISTING flow (do NOT duplicate `run_game_over`):** the King's `run_boss` calls `run_game_over` itself, but the dungeon already has the path `play_room` → `RoomOutcome::GameOver` → `run_dungeon` → `run_game_over` (Continue re-runs `play_room`; QuitToTitle → `DungeonResult::QuitToTitle` → title) — see `scene_dungeon.cpp:~825` + the `run_dungeon` `case RoomOutcome::GameOver`. So `run_room_boss` does NOT call `run_game_over`; on lives==0 it returns `GameOver` and `play_room` returns `RoomOutcome{ RoomOutcome::GameOver }`, letting the existing flow handle Continue (re-runs `play_room` → re-fights the boss, since `boss_defeated` is a fresh local) / QuitToTitle.
  - **Integration in `play_room`:** add a `bool boss_defeated = false;` local. At the TOP of `play_room`, if `level.boss != nullptr && !boss_defeated`, call `run_room_boss(...)`: on `Victory` → set `boss_defeated = true` and fall through to the normal room loop (so the player walks to the onward `room_door` to room2); on `GameOver` → `return RoomOutcome{ RoomOutcome::GameOver };`. Because the fight BLOCKS until victory, the onward door needs no extra gating. For a boss room, `play_room` MUST SKIP its normal enemy/gate/cage/exit logic during the fight (a boss room has none — `run_room_boss` owns the screen). **Do NOT persist the boss** — `boss_defeated` is a per-`play_room` local, so re-entering re-fights it (the spronk-room exit means the player won't normally backtrack). No save change.
- [ ] **Step 3:** Run `bash tools/host_test.sh` (invariants + all green) + `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] **Step 4: Commit** — `git commit -m "feat(game): dungeon boss-room handling (boss fight on entry, victory opens onward door) + D1 invariants (M12 P4.4)"`

BEFORE marking this task complete:
1. Review tests vs docs/pitfalls/testing-pitfalls.md.
2. Each invariant fails on a deliberately-broken layout (document it).
3. `bash tools/host_test.sh` green + ROM builds.

### Task 4.5: Emulator QA (handed to user)

**Files:** none (tuning may touch `D1_DEF` constants or `dungeon1_room1` layout).

- [ ] **Step 1:** Build the ROM, hand to the user.
- [ ] **Step 2:** Verify in-emulator: the **King plays identically** post-refactor (no regression — top priority); the **D1 boss** is fair + winnable with ONLY bolt + double-jump (telegraphs readable, dodgeable; HP bar shows progress; i-frames pace it); boss-room → spronk flow works; death/lives/Game-Over + full-fight restart correct; re-entering the boss room re-fights it; D2–D8 + the hub unregressed.
- [ ] **Step 3:** Iterate per feedback (each fix its own commit; re-run host tests + ROM each round). Balance levers: `D1_DEF` HP/hit_iframes/phase patterns, `dungeon1_room1` arena layout.

BEFORE marking complete: user confirms the D1 boss is fair + the King is unregressed; host tests green; ROM builds; purity clean.

**After completing Phase 4:** Review (min 3 rounds). Confirm: D1 3-room flow solvable + invariants fail-on-broken; boss not persisted (re-fights on re-entry); no save change; D1–D8 + King + hub unregressed; purity clean; ROM builds.

---

## Final review + finish

After all four phases:
1. Full host suite green (`bash tools/host_test.sh`), purity clean, ROM builds.
2. Dispatch a final full-branch review over `git diff main...HEAD` (three-layer purity; King behaviour-identical — logic tests + the diff show only mechanical test re-pointing; the framework is genuinely data-driven; D1 invariants non-vacuous; no leftover inline King attack code; no save change; D2–D8/hub unregressed).
3. Use superpowers:finishing-a-development-branch (present the 4 options; merge per the user's choice).

---

## Self-Review (writing-plans)

- **Spec coverage:** §1 scope → phases; §2 data model → P1; §3 attack library → P2; §4 King refactor → P3; §5 D1 boss → P1.3/P4.2/P4.4; §6 integration → P4.1/P4.3/P4.4; §7 no-save-change → stated in P4 + critical-context; §8 new-vs-reused → File Structure; §9 testing → P1 tests + P4 invariants + P4.5 QA; §10 success criteria → across phases.
- **Placeholders:** none — KING_DEF numbers are exact (from boss.h); D1_DEF numbers are concrete starting values (QA-tuned in 4.5); all Phase-1 test code is real.
- **Type consistency:** `BossDef`/`BossPhaseDef`/`VulnMode`/`BossState.reset(def)`/`on_expose_hit`/`KING_DEF`/`D1_DEF`/`LevelData.boss`/`engine::boss_attacks` used consistently. `D1_DEF` is the ONE canonical D1 boss symbol everywhere (boss.h, build_level.py `"boss":"d1"`→`&D1_DEF` mapping, the generated room1 header, dungeons.h).
- **Cross-task conflicts:** `boss.h` edited across P1.1–P1.3 (sequential, same file — one phase/agent) + P4.1 (add include) + P4.2 (attack bits); `test_boss.cpp` P1.1–P1.4 (sequential); `scene_boss.cpp` only P3.1 (after P2 library exists); `scene_dungeon.cpp` only P4.4; `dungeons.h`/D1 levels only P4.3; `boss_attacks` only P2.1. Ordering dependency P1 → P2 → P3, and P1+P2 → P4, is explicit.
