# Maintainability Health-Review Remediation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remediate all 36 validated findings from the 2026-07-16 maintainability health review so that adding a dungeon, boss, or enemy is cheap in code and LLM tokens, content errors fail at build time instead of silently on hardware, and the test suite exercises shared code instead of mirroring it.

**Architecture:** Seven dependency-ordered phases. Phase 0 fixes docs/tooling (zero code risk). Phase 1 teaches the level pipeline every invariant the runtime assumes (validation-first — it protects every later phase). Phase 2 replaces six forked test harnesses with one shared, gate-aware harness. Phase 3 ships save v6 (dual-slot + Fletcher-16 + boss-defeat persistence + grant persistence). Phase 4 extracts shared constants and the duplicated per-scene player controller. Phase 5 unifies the two boss fight loops around BossDef data. Phase 6 decomposes `play_room` into room-subsystem units and lands the remaining minors. Phases 0–3 are mutually independent; 4 → 5 → 6 are sequential and depend on 3 (save schema) landing first.

**Tech Stack:** Butano (GBA, C++17, no heap/exceptions/floats in gameplay), host tests via `bash tools/host_test.sh` (g++ + tiny in-repo framework), Python 3 content pipeline (`tools/build_level.py`).

**Source findings:** `docs/health-reviews/2026-07-16T23-28-maintainability-validated.md` (I1–I36, D1–D5, FP1–FP2). Each task cites its finding ids and health-review dimension(s).

## Global Constraints

- **Three-layer rule (IMPL-1):** nothing under `include/logic/` or `src/logic/` may name `bn::` types. Run `python tools/check_logic_purity.py` before every commit.
- **No float/double in gameplay code (IMPL-2);** fixed-point tests assert exact raw values (TEST-2).
- **SRAM only via `bn::sram` (IMPL-4);** save tests must cover corrupted/empty SRAM (TEST-4).
- **Host tests:** `bash tools/host_test.sh` — green run ends `N/N tests passed, 0 checks failed`. Do NOT use `make -C test` (fragile on this machine; also skips regeneration until Task 0.2 lands).
- **ROM build:** `bash tools/build_rom.sh` (regenerates level headers first). ROM output: `build/SpronkQuest.gba`. Emulator QA on mGBA.
- **Behavior preservation:** unless a task says otherwise, refactors are behavior-identical. When in doubt, prefer the smaller change; every task lists explicit "Do NOT" boundaries.
- **Generated files:** `include/game/levels/*.h` are generated from `tools/levels/*.txt|.json` — never hand-edit them; edit the source and regenerate (host_test.sh does this).
- **User decisions locked:** D1 persist room-boss defeats; D2 model puzzle gates in the shared harness; D3 save v6 = dual-slot + Fletcher-16 + grant persistence; D4 unify both boss loops; D5 door-glyph cap deferred (Appendix A).
- **Line numbers** cited in tasks are as of commit `9462299` (branch `feat/m14-d3-boss`). Earlier phases shift later phases' numbers — re-locate by the quoted code, not the number.

### Standard TDD Protocol (applies to EVERY task below)

BEFORE starting any task:
1. Invoke /superpowers:test-driven-development
2. Read docs/pitfalls/testing-pitfalls.md
Follow TDD: write failing test → implement → verify green.

BEFORE marking any task complete:
1. Review tests against docs/pitfalls/testing-pitfalls.md
2. Verify test coverage (error paths? edge cases?)
3. Run `bash tools/host_test.sh` and confirm green (`0 checks failed`), plus `python tools/check_logic_purity.py`.

If any test assertion races, flakes, or fails nondeterministically, the fix is deterministic synchronization — NOT assertion removal or weakening. If synchronization cannot make the assertion pass reliably, STOP and raise to the dispatching agent. Do not ship a weaker test. (Host tests here are single-threaded, so any nondeterminism is a bug in the test itself — find it.)

After completing each phase:
Review the phase's diff from multiple perspectives (correctness, behavior preservation, test honesty). Minimum 3 review rounds; if round 3 still finds issues, keep going until clean.

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
  See the stale-claim reclaim protocol in writing-plans-enhanced Step 5.
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

## Execution Status

**Overall:** 1/7 phases shipped (Phase 0); Phase 1 in progress on branch `fix/health-review-remediation`.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 0 — Docs & tooling quick wins | ✅ Shipped | `d6eee82..152c9ed` | 2026-07-17; 2 review-fix rounds (doc facts) |
| 1 — Content-pipeline validation | ✅ Shipped | `c2c7eec..d6cc701` | 2026-07-17; ROM gate green; mGBA smoke pending user |
| 2 — Shared level-test harness | ✅ Shipped | `4356383..7237840` | 2026-07-17; 495/495; exemptions removed, harness extended (object states, updraft/wind/grapple) |
| 3 — Save v6 | ✅ Shipped | `59b7ef8..1226bcd` | 2026-07-17; 513/513; mGBA QA pending user |
| 4 — Shared constants + player session | ✅ Shipped | `100f3a2..7c28fd8` | 2026-07-17; 517/517; boss loops deliberately untouched (Phase 5) |
| 5 — Boss-loop unification | 🚧 In progress | — | branch `fix/health-review-remediation` |
| 2 — Shared level-test harness | ⬜ Not started | — | — |
| 3 — Save v6 | ⬜ Not started | — | — |
| 4 — Shared constants + player session | ⬜ Not started | — | — |
| 5 — Boss-loop unification | ⬜ Not started | — | — |
| 6 — play_room decomposition + minors | ⬜ Not started | — | — |

### Deviations
- **Task 1.2 Rule 6:** arena crystal requirement inverted from "≥1" to "≤1" — D1/D3 arenas are intentionally crystal-less (TiredWindow / block-to-charge designs); the original rule was written believing all arenas had crystals.

### Discoveries
- **Task 2.6 (model-limit gaps, both documented in-code as absent assertions — NOT failures):** D4 spawn→exit full-path proof needs wind-push modeling (6-tile WindRight gust cols 19-24 rows 33-36 between staircase and updraft shaft); D5 spawn→exit needs dash-distance modeling (6-tile lava run row 22 cols 54-59 after the Dash shrine). Both rooms keep partial proofs (shrines, segments) + their historical mGBA verification. Revisit only if D4/D5 layouts change.
- **Task 2.2 (harness model caveats, inherited from all six retired forks — preserved for migration fidelity, revisit after Phase 2):** (a) the diagonal-climb move gates on the START column's head clearance, not the landing column's — a theoretical false-"reachable" if an obstruction sits only above the landing column; (b) there is NO same-column straight-down fall move — content directly below a ledge in a 1-column-wide shaft would read unreachable; (c) `open_columns` clearing runs after object placement in build_grid — a trigger-target column sharing a column with a block/platform would un-solid both.
- **Task 1.2:** `tools/levels/dungeon2_room1.txt`'s comment claims a '$' magic crystal is placed for anti-softlock, but none exists in the grid — the M13 fight ships on Fire-block-to-charge alone. Comment fixed to describe reality (codegen-neutral). **Open balance question for the user:** should D2's arena actually get the crystal its design comment promised? (Adding one is a one-character content edit + QA, deliberately NOT done here.)
- **Task 1.1:** shipped content already relied on the silent gate-JSON fallback the review flagged (I3f) — `tools/levels/dungeon7_room0.txt` has a 6-tile DarkVeil 'G' column with ONE `{"type":"dark_veil"}` sidecar entry, silently reused via `j_gates[-1]`. Resolution: padded the JSON to six explicit entries (generated header byte-identical) so the strict count check stands. Any future multi-tile 'G' column needs one JSON entry per symbol.

## Finding → Task traceability

| Finding | Task(s) | | Finding | Task(s) |
|---|---|---|---|---|
| I1 play_room god-function | 6.1–6.4 | | I19 boss_defeated flag | 0.5 |
| I2 dual boss loops | 5.1–5.5 | | I20 King ignores attacks mask | 5.1, 5.3 |
| I3 pipeline validates nothing | 1.1–1.4 | | I21 tuning literals | 4.2 |
| I4 forked test harness | 2.2–2.6 | | I22 test-only frost/fire_effect | 0.5 |
| I5 regen missing from make paths | 0.2 | | I23 dummy arrays in headers | 6.5 |
| I6 no content recipes | 0.4 | | I24 heavy-plate latch | 1.3 |
| I7 scene controller copy-paste | 4.3–4.5 | | I25 brazier hitbox rows | 6.2 |
| I8 dungeon-add scatter | 1.2 (manifest), 6.6 (registry) | | I26 hidden global collision grid | 6.5 |
| I9 boss triple registry | 5.1, 5.4 | | I27 O(n³) cracked-floor scan | 6.2 |
| I10 no enemy type system | 6.6 | | I28 changelog comments | 6.1–6.4 (rewrite as code moves) |
| I11 death/respawn test drift | 4.2 | | I29 non-fatal CHECK | 2.1 |
| I12 untested scene/engine, no CI | 4.2/4.3 (boundary shift); CI deferred → Appendix A | | I30 boss pin tests / wound paths | 5.1 |
| I13 D4–D6 no reachability | 2.6 | | I31 serial constants in tests | 2.3–2.6 |
| I14 save robustness | 3.1–3.3 | | I32 HUD pip clamp | 3.4 |
| I15 tile-index registry | 4.1 | | I33 grants not persisted | 3.3 |
| I16 hardcoded tooling paths | 0.1 | | I34 sprite budget | 1.2 |
| I17 doc drift | 0.3 | | I35 debug selector + CPU meter | 6.7 |
| I18 misleading names | 0.5 | | I36 arena authoring constraints | 0.4 (doc), 1.2 (validate) |

---

# Phase 0 — Docs & tooling quick wins

**Execution Status:** ✅ SHIPPED at `d6eee82..152c9ed` on 2026-07-17 (5 tasks, 7 commits incl. 2 review-fix rounds; host tests 455/455 after deleting 4 test-only-helper tests; ROM build verified post-phase)

Zero-gameplay-risk fixes. Everything here is independent of every other phase and of each other. Dimensions: Ops Readiness (0.1, 0.2), Content DX (0.2–0.4), Code Quality (0.5).

### Task 0.1: Make build_rom.sh work from any checkout (I16)

**Files:**
- Modify: `tools/build_rom.sh:13-25`

**Current → desired:** `REPO` is hardcoded to `/c/Users/baranmcl/Code/GBA-action-platformer`, so a second checkout silently builds this one. Derive it from the script's own location (host_test.sh already does this at its line 19); keep the toolchain paths as env-overridable defaults.

- [ ] **Step 1: Edit the three path lines**

```bash
DKP_BASH="${DKP_BASH:-/c/devkitPro/msys2/usr/bin/bash.exe}"
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WINPY="${WINPY:-/c/Users/baranmcl/AppData/Local/Programs/Python/Python312}"
```

- [ ] **Step 2: Verify** — `bash -n tools/build_rom.sh` (syntax), then `bash tools/build_rom.sh` from the repo root; expected: regenerates level headers, ends with a `.gba` in `build/`.
- [ ] **Step 3: Commit** — `git commit -m "fix(tools): derive REPO from script location; env-overridable toolchain paths (I16)"`

Do NOT: rewrite the script's devkitPro invocation strategy, add new flags, or touch host_test.sh (its hardcoded mingw64/TEMP lines are the documented machine workaround and already root-relative).

### Task 0.2: Wire level-header regeneration into the make paths (I5)

*(The "no CI / no automated ROM-compile check" half of I12 is intentionally NOT solved here — it needs infrastructure decisions (devkitPro container, runner) that this plan defers; see Appendix A. The testable-boundary half of I12 is Tasks 4.2/4.3.)*

**Files:**
- Modify: `test/Makefile`
- Modify: `Makefile` (root — Butano include-style makefile)
- Modify: `AGENTS.md:6`, `CLAUDE.md` (Host tests section)

**Current → desired:** Only `host_test.sh`/`build_rom.sh` regenerate `include/game/levels/*.h`; `make -C test` and raw `make` compile committed (possibly stale) headers, and AGENTS.md recommends the stale path.

- [ ] **Step 1: test/Makefile — add a `levels` regen prerequisite**

```make
LEVEL_TXT := $(wildcard ../tools/levels/*.txt)
levels:
	@for f in $(LEVEL_TXT); do \
	  python ../tools/build_level.py $$f ../include/game/levels/$$(basename $${f%.txt}).h; \
	done
all: purity levels $(BIN)
```
(Replace the existing `all: purity $(BIN)` line; keep everything else.)

- [ ] **Step 2: Root Makefile — add a documented convenience target** (do NOT restructure the Butano include; append at the end):

```make
# Level headers are generated from tools/levels/*.txt — `make levels` refreshes them.
# tools/build_rom.sh runs this automatically; raw `make` does NOT.
.PHONY: levels
levels:
	@for f in tools/levels/*.txt; do \
	  python tools/build_level.py $$f include/game/levels/$$(basename $${f%.txt}).h; \
	done
```

- [ ] **Step 3: Sync the docs.** AGENTS.md line 6 becomes: `- **Host tests:** run `bash tools/host_test.sh` (canonical — regenerates level headers, then compiles + runs). Plain `make -C test` is fragile on this machine; see CLAUDE.md.` Add one line to both AGENTS.md and CLAUDE.md build sections: `- **ROM:** `bash tools/build_rom.sh` (regenerates level headers, then builds via devkitPro).`
- [ ] **Step 4: Verify** — `bash tools/host_test.sh` green; `git diff` shows no unexpected regenerated-header churn.
- [ ] **Step 5: Commit** — `git commit -m "build: regenerate level headers in make paths; sync AGENTS.md/CLAUDE.md test+ROM commands (I5)"`

Do NOT: add CI config in this task, change host_test.sh, or convert the root Makefile to a wrapper.

### Task 0.3: Fix documentation drift (I17)

**Files:**
- Modify: `README.md` (lines 1–~40: the "currently contains Milestone 6" claim + "What's playable")
- Modify: `include/logic/level_data.h:9` (DoorSpawn comment "dungeon 1..8" → "dungeon 1..9; 9 = finale")

**Current → desired:** README says the repo is at M6; it is at M14 (9 dungeons, lives system, boss framework, D1/D2/D3 room bosses + the Nightmare King finale).

- [ ] **Step 1:** Rewrite README's intro + "What's playable" opening to describe the M14 state in ≤15 lines: 8 spronk dungeons + finale door, boss framework with 3 room bosses + the King, lives/Game-Over, heart containers, room-to-room dungeons. Keep the existing per-milestone history below as "Milestone history" (it is accurate as history). Update the build/test commands to match Task 0.2's wording.
- [ ] **Step 2:** Fix the DoorSpawn comment.
- [ ] **Step 3: Commit** — `git commit -m "docs: README reflects M14 reality; DoorSpawn comment covers door 9 (I17)"`

Do NOT: delete milestone history, restructure README, or document features that don't exist.

### Task 0.4: Write the content recipes doc (I6, I36-doc)

**Files:**
- Create: `docs/content-recipes.md`
- Modify: `CLAUDE.md` (add a References bullet: `- docs/content-recipes.md — checklists for adding a dungeon / boss / enemy / ability`)

**Content requirements** (the executor writes these from the current code; every step names its exact file):

1. **"Add a dungeon" checklist** — author `tools/levels/dungeonN_room*.txt` + `.json` (grid symbols per build_level.py docstring); register rooms in `tools/levels/manifest.json` (exists after Task 1.2); hand-add includes + `DUNGEONN_ROOMS[]`/`DUNGEONN_DUNGEON` to `include/game/levels/dungeons.h`; extend the dungeon table in `src/main.cpp`; hub door: digit glyph in `tools/levels/hub.txt` + gating in `scene_hub.cpp door_enterable`; allocate latch ids (next free, 0..23, globally unique) and heart ids (0..7) — the validator enforces both; write dungeon-specific tests on top of the shared harness (`test/level_harness.h`, after Phase 2); run `bash tools/host_test.sh` then `bash tools/build_rom.sh` + mGBA.
2. **"Add a boss" checklist** — `BossDef` + `BossId` in `include/logic/boss.h` (after Phase 5: per-phase `proj_speed`/`rock_count` live in the def); `BOSS_SYMBOL` in `tools/build_level.py`; sprite: `tools/make_placeholder_art.py` (draw fn + gen fn + `__main__` registration) → `graphics/*.bmp+.json`; id→sprite row in the boss sprite table (scene_dungeon.cpp, `boss_sprite_for`); `"boss": "dN"` in the arena room's JSON; def-invariant tests are automatic (test_boss.cpp loops all defs).
3. **"Add an enemy type" checklist** — (after Task 6.6) add the `EnemyType` enum value + row in the per-type table; sprite assets; `"type"` key in the room JSON.
4. **Arena authoring constraints (I36)** — flat solid floor at row h-2 across the boss's center column; 1-tile walls at columns 0/w-1 (pacing bounds); exactly one magic crystal honored; entrance authored safe.
5. **Global registries** — latch ids [0..23] / heart ids [0..7] with the current allocations (latch: 0=D6, 1=D7, 2=D8; heart: 0=D6, 1=D7, 2=D8); tile-index map pointer (gates.h now; `logic/tile_ids.h` after Task 4.1); entity caps table (the Task 1.1 caps).

- [ ] **Step 1:** Write the doc (target ≤200 lines; checklists, not prose).
- [ ] **Step 2:** Link from CLAUDE.md.
- [ ] **Step 3: Commit** — `git commit -m "docs: content recipes for dungeon/boss/enemy + arena constraints + registries (I6, I36)"`

Do NOT: duplicate build_level.py's symbol docs (link to the docstring); write speculative recipes for features that don't exist yet beyond the three above.

### Task 0.5: Dead code + misleading names (I18, I19, I22)

**Files:**
- Rename: `include/logic/dungeon1.h` → `include/logic/spronk_rescue.h` (same content, update header guard comment)
- Modify: `src/game/scene_dungeon.cpp:40` (include), `:564-568` (dead flag) — grep `dungeon1.h` for any other includer (tests) and update
- Delete: `include/logic/frost.h`, `include/logic/fire_effect.h`, `test/test_frost.cpp`, `test/test_fire_effect.cpp`
- Modify: `src/game/scene_dungeon.cpp:45-46` (remove the two includes)
- Modify: `include/logic/gates.h` (comment) and `include/logic/tilemap.h` (comment)

**Steps:**
- [ ] **Step 1:** Rename dungeon1.h → spronk_rescue.h; update all includers (`grep -r "logic/dungeon1.h"`). Run host tests; expected green.
- [ ] **Step 2:** Delete the dead `boss_defeated` local: replace scene_dungeon.cpp:564-568 with `if(level.boss != nullptr){ ... }` keeping the body and the comment's *useful* half (re-entering re-fights; NOTE: Phase 3 Task 3.3 replaces this line again — if Phase 3 already landed, skip this step). 
- [ ] **Step 3:** Delete frost.h/fire_effect.h + their tests + the two includes. These helpers are verified test-only (the scene freezes/melts whole runs inline; gates use `gate_cleared_by`). The M4 plan deliberately kept them "harmless" — they now mislead readers into editing the wrong code.
- [ ] **Step 4:** Naming disambiguation WITHOUT renames: add one-line comments at `GateType::Water` (gates.h) — `// the Ice-clearable waterfall GATE — unrelated to TileKind::Water (the damaging hazard tile)` — and the mirror comment at `TileKind::Water` in tilemap.h. Renaming the enums would touch build_level.py's public JSON schema and every level test for zero behavior gain.
- [ ] **Step 5:** `bash tools/host_test.sh` green (test count drops by the two deleted suites); `python tools/check_logic_purity.py` green.
- [ ] **Step 6: Commit** — `git commit -m "cleanup: rename dungeon1.h->spronk_rescue.h; drop dead boss_defeated flag + test-only frost/fire_effect helpers; disambiguate Water naming (I18, I19, I22)"`

Do NOT: rename `GateType::Water`/`TileKind::Water` or `EntitySpawn.param*` (param decoding is redesigned properly in Task 6.6); "fix" anything else you notice in scene_dungeon.cpp while there.

**After completing Phase 0:** run the 3-round phase review per the Standard TDD Protocol.

# Phase 1 — Content-pipeline validation

**Execution Status:** ✅ SHIPPED at `c2c7eec..d6cc701` on 2026-07-17 (4 tasks; ROM gate green; 1 deviation — arena crystal rule inverted; 2 discoveries — see top of plan. mGBA smoke for heavy-plate latch + D3 room walk PENDING user QA)

Teach the pipeline every invariant the runtime assumes (I3, I34, I24, I36-validate). Dimensions: Ops Readiness + Architecture + Content DX + Code Quality (flagged by all four agents). Pure Python + tiny scene edits; independent of Phases 0/2/3.

**The caps contract** (single source of truth for Tasks 1.1/1.2 — derived from the spawn loops in `src/game/scene_dungeon.cpp:645-845` and `bn::vector` capacities):

| entity | cap | | entity | cap |
|---|---|---|---|---|
| enemies | 8 | | magic crystals | 8 |
| non-CrackedFloor gates | 24 | | room_doors | 8 |
| CrackedFloor gates | 16 | | braziers | 16 |
| shrines (pickups) | 4 | | plates | 16 |
| heart containers | 4 | | buttons | 16 |
| blocks | 8 | | **plates + buttons + brazier_groups (shared trigger vector)** | **16** |
| boulders | 8 | | level width | ≤ 64 |
| loose platforms | 8 (len ≤ 8) | | level height | ≤ 128 |
| hidden platforms | 8 (len ≤ 8) | | w*h | ≤ 8192 |

### Task 1.1: Per-room validation in build_level.py (I3a, I3d, I3f)

**Files:**
- Modify: `tools/build_level.py` (add `validate_level(level)` called at the end of `compile_level`, before `return`)
- Test: `tools/test_build_level.py` (existing unittest file — read it first and follow its patterns)

**Interfaces:**
- Produces: `LevelError` raised with a message naming the entity kind, the count, and the cap — e.g. `"9 enemies > cap 8"`, `"level 70x120=8400 tiles > 8192 (EWRAM grid cap)"`, `"gate 'G' #3 has no JSON 'gates' entry (5 symbols, 3 entries)"`.

- [ ] **Step 1: Write failing tests** in `tools/test_build_level.py` (one per rule; build minimal in-memory levels via temp files, matching the existing tests' style):
  - a 9th enemy → `LevelError` mentioning `enemies`
  - a 65-wide rectangular bordered level → `LevelError` mentioning width
  - a level with 6 plates + 6 buttons + 5 brazier_groups (17 triggers) → `LevelError` mentioning `trigger`
  - a loose platform with `len: 9` → `LevelError`
  - 3 'G' symbols with only 2 JSON `gates` entries → `LevelError` (this replaces the current silent `j_gates[-1]` reuse at build_level.py:147)
  - a happy-path room at exactly the caps (8 enemies, 64 wide) → compiles clean
- [ ] **Step 2:** Run `( cd tools && python -m unittest test_build_level.py )` — expected: new tests FAIL.
- [ ] **Step 3: Implement `validate_level`** — check every row of the caps table above; make the 'G'-vs-JSON count mismatch a hard error (change line ~147 `entry = j_gates[g_idx] if g_idx < len(j_gates) else j_gates[-1]` to raise when `g_idx >= len(j_gates)`). Also error when JSON has MORE entries than symbols (an orphaned entry is a typo).
- [ ] **Step 4:** Tests green; then `bash tools/host_test.sh` — all 25 shipped rooms must still compile (they are within caps; if one is not, STOP and report — that is a live content bug, not a test to weaken).
- [ ] **Step 5: Commit** — `git commit -m "feat(pipeline): per-room cap/dimension/JSON-count validation in build_level.py (I3)"`

Do NOT: add warnings-instead-of-errors, validate cross-room concerns here (Task 1.2 owns those), or reformat build_level.py.

### Task 1.2: Dungeon manifest + whole-game validator (I3b, I3c, I3e, I34, I36)

**Files:**
- Create: `tools/levels/manifest.json`
- Create: `tools/validate_dungeons.py`
- Modify: `tools/host_test.sh` (run validator after the regen loop), `tools/build_rom.sh` (same)
- Test: `tools/test_validate_dungeons.py`

**manifest.json** — the room lists mirror `include/game/levels/dungeons.h` exactly (verify against it when writing):

```json
{
  "dungeons": {
    "1": ["dungeon1_room0", "dungeon1_room1", "dungeon1_room2"],
    "2": ["dungeon2_room0", "dungeon2_room1", "dungeon2_room2"],
    "3": ["dungeon3_room0", "dungeon3_room1", "dungeon3_room2"],
    "4": ["dungeon4"],
    "5": ["dungeon5"],
    "6": ["dungeon6_room0", "dungeon6_room1", "dungeon6_room2"],
    "7": ["dungeon7_room0", "dungeon7_room1", "dungeon7_room2"],
    "8": ["dungeon8_room0", "dungeon8_room1", "dungeon8_room2"],
    "9-approach": ["dungeon9_room0", "dungeon9_room1"],
    "9-arena": ["dungeon9_arena"]
  },
  "standalone": ["hub"]
}
```

**validate_dungeons.py** — imports `compile_level` from `build_level.py`; for each dungeon compiles its rooms and checks the rules below. Access compiled entity tuples by PREFIX position only (e.g. a plate's `p[0], p[1]` for tx/ty) or convert to dicts — Task 1.3 appends a field to the plate tuple, and the validator must not break when it lands.
1. **Room-graph integrity (I3b):** every `room_doors` `target_room` is `-1` or `0 <= target_room < len(rooms)`; every `target_entrance` matches an entrance id present in the target room (entrance ids from the compiled `entrances` list). Two-way check is a WARNING only (D7 has a deliberate one-way exception — see test_dungeon7 comments).
2. **dungeons.h drift check:** regex-scan `include/game/levels/dungeons.h` for `DUNGEON<N>_ROOMS[]` initializer lists; the referenced `*_DATA` symbols must match the manifest's room lists in order (uppercase basename + `_DATA`). Mismatch = error.
3. **Global latch registry (I3e):** collect every `latch_id` ≥ 0 from every room's gates/cracked_floors/brazier_groups; each must be in [0, 23]; duplicates allowed ONLY within the same room file (D7 uses latch 1 twice for one shortcut, same room) — cross-room duplicates are errors.
4. **Global heart-id registry:** every heart-container id in [0, 7], globally unique.
5. **Sprite-budget estimate (I34):** per room, `enemies + blocks + boulders + crystals + hearts + shrines + Σ loose len + Σ hidden len + 45` (HUD/player/vine/VFX reserve) must be ≤ 110; error above (Butano OAM budget is 128 — the margin is deliberate).
6. **Arena constraints (I36):** any room whose JSON has a `"boss"` key must have: a solid tile at `(w/2, h-2)` and at `(w/2 ± 1, h-2)` (flat floor under the boss), solid columns 0 and w-1 (already border-checked), and AT MOST 1 magic crystal ('$') — the fight loop honors only `magic_crystals[0]`, so a second crystal is silently dead content. Zero crystals is LEGAL: D1 (TiredWindow — bolts are free) and D3 (dual block-to-charge) ship crystal-less by documented design. *(Deviation from the original plan text, which required ≥1 — that requirement was falsified by shipped intentional designs; see Discoveries.)*
- [ ] **Step 1: Write failing tests** (`tools/test_validate_dungeons.py`): temp-dir fixtures with a 2-room mini-dungeon: OOB target_room; unknown target_entrance; cross-room duplicate latch_id; latch_id 24; duplicate heart id; boss room without a crystal; happy path.
- [ ] **Step 2:** Run unittest — FAIL. **Step 3:** Implement. **Step 4:** unittest green; run `python tools/validate_dungeons.py` against the real content — must pass (if a real violation surfaces, STOP and report it; do not tune thresholds to make it pass).
- [ ] **Step 5:** Wire into `tools/host_test.sh` (after the regen loop: `python tools/validate_dungeons.py`) and `tools/build_rom.sh` (after its regen loop). Update the unittest line in host_test.sh to also run `test_validate_dungeons.py`.
- [ ] **Step 6: Commit** — `git commit -m "feat(pipeline): whole-game validator (room graph, latch/heart registries, sprite budget, arena constraints) + manifest (I3, I34, I36)"`

Do NOT: generate dungeons.h from the manifest (registry generation is Task 6.6's decision point; the manifest is validation-only for now); make the two-way door check an error.

### Task 1.3: Heavy-plate latch persistence (I24)

**Files:**
- Modify: `include/logic/level_data.h` (PlateSpawn: add `int latch_id;` last field)
- Modify: `tools/build_level.py` (plates emit: `{tx,ty,ttx,tty,heavy,latch}` with JSON `"latch_id"` default -1; update the dummy to `'{0,0,0,0,false,-1}'`)
- Modify: `src/game/scene_dungeon.cpp` heavy-plate pound block (`:1016-1024`) and plate spawn loop (`:824-832`)
- Test: `tools/test_build_level.py` (emission), plus regen

- [ ] **Step 1:** Failing Python test: a plate with `"latch_id": 3` emits `...,3}`; without → `...,-1}`.
- [ ] **Step 2:** Implement schema + emitter. Regenerate headers (`bash tools/host_test.sh` does it) — all existing plates gain `-1`.
- [ ] **Step 3:** Scene: in the heavy-plate pound block add `persist_latch(world, p.latch_id);` after `open_column(...)` and delete the "persist not wired" comment; in the plate spawn loop, before the `heavy` skip, add: `if(p.heavy && p.latch_id >= 0 && world.latched(p.latch_id)){ open_column(lvl.view, p.target_tx, level.h); continue; }` (a latched heavy gate starts open on room load, mirroring the gate latch at :685).
- [ ] **Step 4:** Host tests green; ROM builds.
- [ ] **Step 5: Commit** — `git commit -m "feat(content): heavy plates support latch_id persistence like every other gate type (I24)"`

Do NOT: give normal (non-heavy) plates latch behavior — they are deliberately hold-to-open.

### Task 1.4: Runtime backstop asserts (I3b-runtime)

**Files:**
- Modify: `src/game/scene_dungeon.cpp` `run_dungeon` (`:1391-1392`)

- [ ] **Step 1:** Before `play_room(*dungeon.rooms[cur_room], ...)` add:

```cpp
BN_ASSERT(cur_room >= 0 && cur_room < dungeon.room_count,
          "room index out of range: ", cur_room, " of ", dungeon.room_count);
```
(`#include "bn_assert.h"` if not already transitively available.)
- [ ] **Step 2:** ROM builds; mGBA smoke: enter D3, walk rooms 0→1→2. 
- [ ] **Step 3: Commit** — `git commit -m "fix(engine): assert room index bounds in run_dungeon instead of OOB dereference (I3)"`

Do NOT: add asserts inside per-frame loops (cost); change `find_entrance`'s '@' fallback (the validator now guarantees entrance ids; the fallback stays as belt-and-braces).

**After completing Phase 1:** 3-round phase review; confirm `bash tools/host_test.sh` runs regen → validator → purity → compile → green, and `bash tools/build_rom.sh` still produces a ROM.

---

# Phase 2 — Shared level-test harness

**Execution Status:** ✅ SHIPPED at `4356383..7237840` on 2026-07-17 (6 tasks, 3 fix rounds; suite 455→495 with ~475 net lines deleted from per-dungeon forks; D7 sound at RELIABLE=5 with zero climb_max escapes; D2/D3 puzzle exemption removed — staged proofs green on shipped data; D4/D5 partial coverage — see Discoveries for the two model-limit gaps)

One gate-aware reachability harness replaces six private forks (I4, D2, I13, I29, I31). Dimension: Test Quality + Content DX. Test-only phase: zero production-code risk. Independent of Phases 0/1/3 (touches only `test/`).

**Design (locked):** the movement model is D8/D9's dual-threshold model — it is the only one that survived a real shipped soft-lock (see test_dungeon8_level.cpp:27-42): `CLIMB_RELIABLE = 5` (paths the player MUST be able to take) and `CLIMB_MAX = 7` (paths a gate must ensure the player CANNOT take). D2's horizontal double-jump move carries over. Constants are pinned against player physics by a canary test (Step 2.2-4). D7's single `CLIMB=6` model is retired.

### Task 2.1: REQUIRE macro (I29)

**Files:**
- Modify: `test/test_framework.h`
- Modify: `test/test_dungeon7_level.cpp:831-836, :861-878` (the two CHECK-then-dereference sites)

- [ ] **Step 1:** Add to test_framework.h:

```cpp
// Fatal precondition: prints like CHECK, then returns from the enclosing TEST
// (tests are void fns). Use before dereferencing pointers/indices.
#define REQUIRE(cond) do { if(!(cond)){ ++failures(); \
    std::printf("  FAIL %s:%d  REQUIRE(%s)\n", __FILE__, __LINE__, #cond); return; } } while(0)
```
- [ ] **Step 2:** Convert the two D7 sites (`CHECK(r1 != nullptr);` → `REQUIRE(r1 != nullptr);` etc.). Grep the rest of `test/` for `CHECK(.*nullptr)` followed by a dereference and convert those too.
- [ ] **Step 3:** Host tests green. **Step 4: Commit** — `git commit -m "test: add fatal REQUIRE; convert pointer-precondition CHECKs (I29)"`

### Task 2.2: test/level_harness.h — the shared model

**Files:**
- Create: `test/level_harness.h`
- Test: `test/test_level_harness.cpp` (the harness gets its own tests — it is load-bearing)

**Interfaces (Produces — later tasks and all future dungeon tests rely on these exact names):**

```cpp
#pragma once
#include "logic/level_data.h"
#include "logic/tilemap.h"
#include "logic/gates.h"
#include <vector>
#include <queue>
#include <set>

namespace harness {

// Movement constants — see test_dungeon8_level.cpp history: RELIABLE for must-reach,
// MAX for must-NOT-bypass. Canary-pinned against player physics in test_level_harness.cpp.
inline constexpr int CLIMB_RELIABLE = 5;
inline constexpr int CLIMB_MAX      = 7;

struct WorldModel {
    // Which gate indices are OPEN (cleared/latched) for this query.
    std::set<int> open_gates;            // index into level.gates
    std::set<int> open_columns;          // extra opened trigger-target columns (plate/button/brazier targets)
    bool hidden_platforms_shown = false; // Light reveal active
    bool water_frozen = false;           // Ice bridges cast on every water run
    bool boulders_broken = false;        // Stone pound cleared the boulders (D7 progression states)
    bool cracked_floors_broken = false;  // Stone pound smashed the cracked floors
    bool climb_max = false;              // use CLIMB_MAX instead of CLIMB_RELIABLE (bypass checks)
    // Task 2.6 adds: bool glide, bool grapple (wind/updraft/anchor rules)
};

struct Grid {
    int w = 0, h = 0;
    std::vector<uint8_t> solid;     // 1 = blocked for movement
    std::vector<uint8_t> standable; // 1 = feet can rest (solid or one-way top)
    std::vector<uint8_t> hazard;    // lava/water(unfrozen)/spikes
};

// Build the movement grid for a room under a given world model.
// Encodes: solid tiles, one-ways, closed gates as full-height 2-wide columns
// (rows 1..h-3, matching scene fill_column), cracked floors solid unless
// cracked_floors_broken, blocks solid at spawn, boulders solid unless
// boulders_broken, loose platforms solid at spawn row, hidden platforms
// solid iff shown, water hazard iff not frozen.
Grid build_grid(const logic::LevelData& level, const WorldModel& wm);

// Flood-fill of standable positions from the player spawn (or a given tile),
// using: walk, fall any height, climb (double-jump) up to CLIMB tiles,
// horizontal double-jump over gaps <= 4 tiles at same-or-lower landing
// (the D2 move), no passing through hazards.
std::set<std::pair<int,int>> reachable(const logic::LevelData& level, const WorldModel& wm);
std::set<std::pair<int,int>> reachable_from(const logic::LevelData& level, const WorldModel& wm, int tx, int ty);

bool reaches(const logic::LevelData& level, const WorldModel& wm, int tx, int ty); // from spawn
// Convenience: standable cell nearest-below a content tile (mirrors scene floor_row_below).
std::pair<int,int> ground_below(const Grid& g, int tx, int ty);

// ---- universal invariants (used by test_all_dungeons.cpp; return void, use CHECK/REQUIRE inside) ----
void check_solid_border(const logic::LevelData& level, const char* label);
void check_room_doors_resolve(const logic::DungeonData& dungeon, const char* label); // targets in range, entrances exist
void check_entrances_settle_safely(const logic::LevelData& level, const char* label); // 60-frame logic::Player settle from each entrance -> on_ground, no hazard overlap (the respawn-settle invariant from test_d1_boss_respawn.cpp)
}
```

- [ ] **Step 1: Failing harness self-tests** (`test/test_level_harness.cpp`): hand-built tiny LevelData fixtures (follow test_death_respawn.cpp's in-file map style, but as LevelData with a tiles array):
  - a 5-tile-up ledge is reachable at RELIABLE; a 6-tile ledge is not; the same 6-tile ledge IS reachable with `climb_max=true`; an 8-tile ledge is not even at MAX
  - a 4-tile gap crosses via the horizontal double-jump; a 5-tile gap does not
  - a closed Vine gate blocks; `open_gates={0}` unblocks
  - water blocks as hazard; `water_frozen=true` makes the run standable + non-hazard
  - hidden platform run standable only when `hidden_platforms_shown`
  - **physics canary:** simulate `logic::Player` (real `player.update`, TEST-1: fixed ticks) doing a double jump on a flat floor; assert the apex tile-height gain is ≥ CLIMB_RELIABLE and ≤ CLIMB_MAX. This test BREAKS when jump physics are retuned — that is its job; its failure message must say "retune harness::CLIMB_* to match player physics". For InputFrame field names and the jump/double-jump input sequence, read `include/logic/player.h` and copy the idiom from `test/test_player.cpp` (it already simulates jumps); set `player.abilities.featherleap = true` for the double jump.
- [ ] **Step 2:** FAIL run. **Step 3:** Implement `level_harness.h` — port the BFS core from test_dungeon8_level.cpp (the most evolved copy) and the horizontal double-jump from test_dungeon2_level.cpp:113-127; everything `inline` (single test binary — no ODR issue, which retires the M14 copy-mandate). **Step 4:** green.
- [ ] **Step 5: Commit** — `git commit -m "test: shared gate-aware dual-threshold level harness + physics canary (I4)"`

Do NOT: model wind/updraft in this task (Task 2.6 adds the two rules D4/D5 need); simulate real player physics inside the BFS (the canary pins the constants; the BFS stays a tile model); weaken the canary to always-pass.

### Task 2.3: Universal invariants over every dungeon (I31-part, I13-part)

**Files:**
- Create: `test/test_all_dungeons.cpp`

- [ ] **Step 1:** A registry table + loop (this is the pattern that makes new dungeons inherit coverage automatically):

```cpp
#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"

struct Entry { const char* name; const logic::DungeonData* d; };
static const Entry ALL[] = {
    {"D1", &DUNGEON1_DUNGEON}, {"D2", &DUNGEON2_DUNGEON}, {"D3", &DUNGEON3_DUNGEON},
    {"D4", &DUNGEON4_DUNGEON}, {"D5", &DUNGEON5_DUNGEON}, {"D6", &DUNGEON6_DUNGEON},
    {"D7", &DUNGEON7_DUNGEON}, {"D8", &DUNGEON8_DUNGEON},
    {"D9A", &DUNGEON9_APPROACH}, {"D9X", &DUNGEON9_ARENA},
};

TEST(all_dungeons_solid_borders){ for(auto& e : ALL) for(int r = 0; r < e.d->room_count; ++r) harness::check_solid_border(*e.d->rooms[r], e.name); }
TEST(all_dungeons_room_doors_resolve){ for(auto& e : ALL) harness::check_room_doors_resolve(*e.d, e.name); }
TEST(all_dungeons_entrances_settle){ for(auto& e : ALL) for(int r = 0; r < e.d->room_count; ++r) harness::check_entrances_settle_safely(*e.d->rooms[r], e.name); }
```
- [ ] **Step 2:** Green (these invariants already hold; a failure is a live content bug — STOP and report, do not exempt). 
- [ ] **Step 3: Commit** — `git commit -m "test: universal per-dungeon invariants over a registry loop — new dungeons inherit coverage (I4, I13)"`

Do NOT: fold dungeon-specific invariants (D7's one-way exception, D8's Light-required proof) into the loop — those stay in per-dungeon files.

### Task 2.4: Migrate D7/D8/D9 to the harness (I4)

**Files:**
- Modify: `test/test_dungeon7_level.cpp` (887 lines — the big one), `test_dungeon8_level.cpp`, `test_dungeon9_level.cpp`

- [ ] **Step 1:** For each file: delete the local Grid/flood-fill/constants block; re-express every reachability test via `harness::` calls with an explicit `WorldModel` per query. D7's tests were written against `CLIMB=6` — re-run each against RELIABLE=5 first; where a D7 invariant genuinely needs 6 (a documented reach), assert it under `climb_max` and add a comment justifying it (an expected handful; list them in the commit message). Preserve every ASSERTION; only the mechanism changes.
- [ ] **Step 2:** Delete tests that Task 2.3 now covers generically (solid border, min size, doors resolve, hub-door grounding, respawn settle). Keep dungeon-specific ones (D7 pound sequence, D8 dual-path Light proof, latched-shortcut behaviors). Also delete cross-dungeon serial-id pins (I31) — e.g. test_dungeon8_level.cpp:299 `CHECK_EQ(hc.id, 2)`: the Task 1.2 validator now enforces global heart/latch-id uniqueness, so per-dungeon tests only assert the id is `>= 0`, not its position in the global sequence.
- [ ] **Step 3:** Green, same-or-stronger invariant count. Expect net deletion of ~600+ lines across the three files.
- [ ] **Step 4: Commit** — `git commit -m "test: D7/D8/D9 on the shared harness; retire the unsound single-threshold D7 model (I4)"`

If a migrated D7 assertion FAILS under the dual-threshold model: that may be a real latent soft-lock (the D8 incident pattern). STOP, verify on mGBA, and report before changing either the level or the test.

### Task 2.5: Migrate D1/D2/D3 + model the puzzle gates (I4, D2 decision)

**Files:**
- Modify: `test/test_dungeon1_level.cpp`, `test_dungeon2_level.cpp`, `test_dungeon3_level.cpp`

- [ ] **Step 1:** Migrate the existing flood-fill tests to the harness (as 2.4).
- [ ] **Step 2:** NEW puzzle-solvability tests for D2/D3 room 0 (removing the documented exemption at test_dungeon2_level.cpp:261-266 / test_dungeon3_level.cpp:198-210). Pattern per puzzle stage, using WorldModel::open_columns:
  1. With nothing open: the puzzle's first trigger (plate tile / button tile / every brazier of group g) must be `harness::reaches(...)` from spawn.
  2. With that trigger's target column added to `open_columns`: the next trigger (or the shrine/onward door) must be reachable.
  3. Chain until the Fire/Ice shrine AND the onward 'D' door are reached.
  Derive each stage's trigger→target pairs from the room's compiled data (plates/buttons/brazier_groups arrays), not hardcoded coordinates — assert by array index so a re-layout updates automatically.
- [ ] **Step 3:** Green. If a stage is NOT reachable, that is a live soft-lock: STOP, verify on mGBA, report.
- [ ] **Step 4: Commit** — `git commit -m "test: D1-D3 on shared harness; D2/D3 puzzle rooms get staged gate-aware solvability proofs (I4, D2)"`

### Task 2.6: Backfill D4/D5/D6 reachability (I13)

**Files:**
- Modify: `test/level_harness.h` (two new movement rules), `test/test_level_harness.cpp`
- Modify: `test/test_dungeon4_level.cpp`, `test_dungeon5_level.cpp`, `test_dungeon6_level.cpp`

- [ ] **Step 1:** Failing harness tests for the two new rules, then implement:
  - **Updraft columns** (`TileKind::Updraft`, value 6): when `WorldModel.glide = true` (new field, default false), any cell in a contiguous vertical updraft run is reachable from any lower cell of that run (the lift). Without glide: no effect.
  - **Wind tiles** (WindLeft 7 / WindRight 8): passable, non-standable, non-hazard (a conservative model — wind pushes but doesn't block; do NOT model the push force).
- [ ] **Step 2:** D4: with `glide=true`, the exit and the Glide shrine must be reachable (shrine must be reachable WITHOUT glide — it grants it; assert both). D5: with full abilities + `water_frozen=true` where needed, spikes modeled as hazard: exit + shrine reachable; keep the existing existence checks. D6: room-graph walk — room 0 spawn reaches its 'D' doors; room 2 reaches the cage + exit (grapple anchors: model `TileKind::GrapplePoint` cells as climb targets when `WorldModel.grapple = true` — reachable if within CLIMB of a reachable cell OR vertically below the anchor within grapple range 6; add this rule with its own harness test).
- [ ] **Step 3:** Green (same STOP-and-report rule on failures). **Step 4: Commit** — `git commit -m "test: D4/D5/D6 gain reachability coverage via harness wind/updraft/grapple rules (I13)"`

Do NOT: model wind push force, glide trajectories, or dash distances — conservative tile rules only; if a room needs a finer model to prove reachability, flag it in the plan's Discoveries section instead of inventing physics in the harness.

**After completing Phase 2:** 3-round review. Diff target: net test-code reduction with strictly more invariants. Update the per-dungeon test-authoring section of `docs/content-recipes.md` (Task 0.4) to point at the harness.

# Phase 3 — Save v6: dual-slot, Fletcher-16, boss-defeat + grant persistence

**Execution Status:** ✅ SHIPPED at `59b7ef8..1226bcd` on 2026-07-17 (4 tasks, 0 fix rounds; 513/513; ROM gates green. PENDING USER mGBA QA: power-cycle persistence, slot-A hex-corruption recovery, defeated-boss no-re-fight, 18-pip HUD bar on a 3-heart save)

Implements decisions D1 + D3 and findings I14/I33/I32. Dimensions: Ops Readiness + Architecture. Schema-before-consumers: 3.1 (pure logic) → 3.2 (engine) → 3.3 (scene consumers). Independent of Phases 0/1/2. Pitfalls: IMPL-1 (all decision logic stays pure), IMPL-4 (bn::sram only), TEST-4 (corrupt/empty SRAM covered).

**Locked v6 layout** (24 bytes, trivially copyable; two slots at SRAM offsets 0 and 32):

```
[0..3]   magic       uint32  'SPRK'
[4..5]   version     uint16  6
[6..7]   seq         uint16  write counter (slot arbitration; int16-diff wraparound compare)
[8..9]   spronks     uint16
[10..11] abilities   uint16
[12..15] latches     uint32
[16]     lives       uint8
[17]     beaten      uint8
[18]     boss_defeats uint8  bit (d-1) = dungeon d's room boss defeated, d in 1..8 (D1 decision)
[19]     current_dungeon uint8
[20..21] checksum    uint16  Fletcher-16 over bytes [0..19]
[22..23] _pad        zeroed
```

Legacy v1–v5 data lives at offset 0 with `version <= 5` — a v6 slot-A read fails its version check on legacy data and falls through to the migration path. After the first v6 write, slot arbitration takes over.

### Task 3.1: Pure v6 save logic in world_state.h

**Files:**
- Modify: `include/logic/world_state.h`
- Test: `test/test_world_state_v6.cpp` (new)

**Interfaces (Produces):**

```cpp
// added to struct World:
uint8_t boss_defeats = 0;                       // bit (d-1), d in 1..8
bool boss_defeated(int d) const { return (boss_defeats >> (d-1)) & 1u; }
void set_boss_defeated(int d){ boss_defeats |= (uint8_t)(1u << (d-1)); }

// new, alongside the existing (kept) v1-v5 SaveData/load_save used for migration:
struct SaveDataV6 { /* fields per the locked layout; static_assert(sizeof==24) */ };
inline uint16_t fletcher16(const uint8_t* p, int n);
inline SaveDataV6 make_save_v6(const World& w, uint16_t seq);
inline bool load_save_v6(const SaveDataV6& s, World& out, uint16_t& seq_out); // magic+version+checksum gate
// Slot arbitration + legacy migration in ONE pure decision (engine stays dumb):
struct SaveDecision { bool valid; World world; uint16_t seq; int next_slot; }; // next_slot: 0 or 1 = slot to WRITE next
inline SaveDecision decide_load(const SaveDataV6& slot_a, const SaveDataV6& slot_b, const SaveData& legacy_slot0);
```

`decide_load` rules: validate both v6 slots; both valid → pick the one whose `(int16_t)(seq_x - seq_y) > 0` (wraparound-safe); one valid → it; none valid → try `load_save(legacy_slot0, ...)` (v1–v5, boss_defeats=0, seq=0); nothing valid → `{false, World{}, 0, 0}`. `next_slot` = the slot NOT holding the winning save (ping-pong), 0 when nothing/legacy won.

- [ ] **Step 1: Failing tests** (TEST-2/TEST-4 conventions — exact integer asserts):
  - `fletcher16` on a known vector (e.g. bytes "abcde" → 0xC8F0; verify the reference value when implementing and pin it)
  - v6 roundtrip: `make_save_v6` → `load_save_v6` reproduces every World field incl. boss_defeats
  - single flipped byte in each region (header, latches, boss_defeats) → load rejected — this is the class the old 8-bit additive sum missed (compensating/transposition errors); include one transposition case (swap bytes 8 and 9) and assert rejection
  - `decide_load`: both valid picks higher seq; wraparound (seq 0xFFFF vs 0x0000 → 0x0000 wins); torn write (slot A valid seq 5, slot B corrupt) → A wins, next_slot = 1
  - migration: build a v5 `SaveData` via the existing `make_save`, feed as `legacy_slot0` with two invalid v6 slots → World restored, boss_defeats == 0
  - first boot: all-zero slots + all-zero legacy → `valid == false` (TEST-4)
  - `spronk_freed`/`boss_defeated` bit independence: setting boss_defeated(3) doesn't disturb spronks/latches
- [ ] **Step 2:** FAIL. **Step 3:** Implement (Fletcher-16: standard two-sum mod 255). **Step 4:** green + purity check.
- [ ] **Step 5: Commit** — `git commit -m "feat(save): v6 layout — dual-slot arbitration, Fletcher-16, boss-defeat bits; pure decide_load (I14, D1, D3)"`

Do NOT: delete the v1–v5 structs/checksums (migration needs them); change SAVE_MAGIC; add fields beyond the locked layout ("while we're in here" is how migration chains break).

### Task 3.2: Engine dual-slot read/write

**Files:**
- Modify: `src/engine/save.cpp`, `include/engine/save.h`

- [ ] **Step 1:** Implement using `bn::sram::read_offset`/`write_offset` (verify exact Butano API names in `butano/include/bn_sram.h` before writing):

```cpp
namespace { constexpr int SLOT_B_OFFSET = 32; uint16_t s_seq = 0; int s_next_slot = 0; bool s_loaded = false; }
bool read_world(logic::World& out){
    logic::SaveDataV6 a, b; logic::SaveData legacy;
    bn::sram::read_offset(a, 0); bn::sram::read_offset(b, SLOT_B_OFFSET); bn::sram::read_offset(legacy, 0);
    logic::SaveDecision d = logic::decide_load(a, b, legacy);
    s_seq = d.seq; s_next_slot = d.next_slot; s_loaded = true;
    if(d.valid) out = d.world;
    return d.valid;
}
void write_world(const logic::World& w){
    if(!s_loaded){ logic::World tmp; read_world(tmp); }   // arbitration state before first write
    ++s_seq;
    logic::SaveDataV6 s = logic::make_save_v6(w, s_seq);
    bn::sram::write_offset(s, s_next_slot == 0 ? 0 : SLOT_B_OFFSET);
    s_next_slot ^= 1;
}
```
- [ ] **Step 2:** ROM builds. mGBA QA: fresh .sav boots fresh; play to a latch, power-cycle → progress kept; corrupt slot A's first byte in a hex editor → boots from slot B (not fresh).
- [ ] **Step 3: Commit** — `git commit -m "feat(save): ping-pong dual-slot SRAM writes via decide_load (I14, D3)"`

Do NOT: add a corruption-notice UI (explicitly descoped by the user — option (b) not (c)); write both slots per save (defeats torn-write protection).

### Task 3.3: Consumers — persist ability grants; skip defeated room bosses (I33, D1)

**Files:**
- Modify: `src/game/scene_dungeon.cpp` shrine loop (`:1271-1277`) and boss-room block (`:564-570`)

- [ ] **Step 1:** Shrine loop: after `spell.ensure_valid(world);` add `engine::write_world(world);  // persist the grant NOW — quit+power-cycle must not un-earn it (I33)`.
- [ ] **Step 2:** Boss-room block becomes:

```cpp
// A defeated room boss stays defeated (persisted, save v6) — re-entering the arena
// while backtracking must not re-trigger a mandatory fight (D1 decision).
if(level.boss != nullptr && d >= 1 && d <= 8 && !world.boss_defeated(d)){
    BossRoomOutcome bo = run_room_boss(level, world, ps, lvl, cam, player, spawn_pos, ent);
    if(bo == BossRoomOutcome::GameOver) return RoomOutcome{ RoomOutcome::GameOver };
    world.set_boss_defeated(d);
    engine::write_world(world);
    engine::fade_out(16);   // clear the boss screen; the normal room loop fades back in
}
```
(If Task 0.5 already removed the dead flag, this replaces that version; the King/D9 flow is untouched — it runs via `run_boss` + `world.beaten`.)
- [ ] **Step 3:** Host tests green (world-state tests from 3.1 cover the bits). mGBA QA: beat D1's guardian, exit to hub, re-enter D1 → room 1 plays as a normal room (no fight, door onward open); D9 finale unaffected.
- [ ] **Step 4: Commit** — `git commit -m "feat(game): persist ability grants at pickup; room-boss defeats persist across visits (I33, D1)"`

Do NOT: persist the King via boss_defeats (he has `beaten`); make defeated arenas skip their post-fight room loop (the walk-to-door flow stays).

### Task 3.4: HUD health-bar cap (I32)

**Files:**
- Modify: `include/logic/hud_math.h` (`MAX_HEALTH_PIPS` 16 → 20 + comment), `src/engine/hud.cpp` + `include/engine/hud.h` (pip sprite container capacity + layout — read them first; the health pips start at x=-116, 8px apart)
- Test: `test/test_hud_math.cpp`

- [ ] **Step 1:** Failing tests: `health_total_pips(175) == 18`, `health_total_pips(300) == 20` (clamped), `health_fill_pips(160, 175) == 16`.
- [ ] **Step 2:** Bump `MAX_HEALTH_PIPS = 20;   // fits the 200-HP band on screen (-116 + 20*8 = 44 < the +104 icon column)`; raise the engine-side pip container capacity to match (find the `bn::vector<bn::sprite_ptr, N>` in hud.cpp/hud.h and any hardcoded 16).
- [ ] **Step 3:** Green; mGBA QA with a 3-heart save: bar lengthens past 16 pips and damage from full is visible immediately.
- [ ] **Step 4: Commit** — `git commit -m "fix(hud): health bar renders up to 20 pips — 175-HP saves no longer saturate invisibly (I32)"`

Do NOT: redesign the HUD or change HP_PER_PIP.

**After completing Phase 3:** 3-round review; explicitly re-run the full migration matrix (v1..v5 → v6) tests and the mGBA power-cycle QA.

---

# Phase 4 — Shared constants + player-session controller + pure damage step

**Execution Status:** ✅ SHIPPED at `100f3a2..7c28fd8` on 2026-07-17 (4 tasks, 0 fix rounds; 517/517; ROM gates green. Deviations: player_session.h lives in include/game/ per repo convention; main.cpp keeps explicit boot-at-full refill alongside sync_health_cap — the brief's literal substitution would have regressed it. mGBA QA pending user: hub/dungeon control feel, crystals, death reset)

Extract the verbatim-duplicated scene plumbing (I7), single-source the pinned numbers (I15, I21), and move the damage/respawn frame-step into logic so the drifted tests exercise real code (I11, I12-part). Dimensions: Code Quality + Architecture + Content DX + Test Quality. MUST land before Phases 5/6 (both consume these units). Tasks 4.1/4.2 are pure-logic and parallelizable; 4.3→4.4 are sequential.

### Task 4.1: logic/tile_ids.h — the tile-index registry becomes code (I15)

**Files:**
- Create: `include/logic/tile_ids.h`
- Modify: `src/engine/level_view.cpp:31-33` (ternary chain → `logic::tiles::bg_for_kind`), `src/game/scene_dungeon.cpp` bg literals (`:674, :798, :808, :819, :826, :835, :1113, :1141`), `src/game/scene_hub.cpp:75` (5/6), `include/logic/gates.h:30-35` (comment now points at tile_ids.h)
- Test: `test/test_tile_ids.cpp`

- [ ] **Step 1:** Failing test: `bg_for_kind((int)TileKind::Lava) == tiles::LAVA (13)`, `bg_for_kind(4) == 16`, `bg_for_kind(1) == 1` (identity), plus `GATE_TABLE` cross-check: for each GateType, `gate_info(t).bg_tile` equals the matching `tiles::` constant (pins the two tables together).
- [ ] **Step 2:** Implement:

```cpp
#pragma once
namespace logic { namespace tiles {
// graphics/tiles.bmp strip indices — THE registry (was a comment in gates.h).
// make_placeholder_art.py gen_tiles draws these slots; keep in sync when adding art.
inline constexpr int BLANK=0, GROUND=1, ONE_WAY=2, GATE_CLOSED=3, CAGE=4,
    DOOR_OPEN=5, DOOR_LOCKED=6, VINE=7, ICE_WALL=8, WATERFALL=9, FIREWALL=10,
    CRACKED_FLOOR=11, DARK_VEIL=12, LAVA=13, BRAZIER_UNLIT=14, BRAZIER_LIT=15,
    WATER=16, PLATE=17, BUTTON=18, ICE_PLATFORM=19, UPDRAFT=20, WIND_LEFT=21,
    WIND_RIGHT=22, CRACKED_WALL=23, SPIKES=24, GRAPPLE_ANCHOR=25, HUB_PORTAL=26;
inline constexpr int bg_for_kind(int kind){
    return kind==3?LAVA : kind==4?WATER : kind==5?ICE_PLATFORM : kind==6?UPDRAFT
         : kind==7?WIND_LEFT : kind==8?WIND_RIGHT : kind==9?SPIKES
         : kind==10?GRAPPLE_ANCHOR : kind;
}
}}
```
- [ ] **Step 3:** Replace every listed literal call-site (e.g. `set_level_tile(..., 14)` → `..., logic::tiles::BRAZIER_UNLIT)`; delete the local `WATER_BG/ICE_PLATFORM_BG` at scene_dungeon.cpp:1141). Green + ROM builds + one mGBA room-load eyeball (tiles unchanged).
- [ ] **Step 4: Commit** — `git commit -m "refactor: tile-index registry is code (logic/tile_ids.h); scenes+level_view consume it (I15)"`

Do NOT: renumber any tile; touch make_placeholder_art.py (the header comment documents the sync obligation — generating Python constants is not worth the moving part).

### Task 4.2: logic/combat_rules.h — constants + the vitals frame-step; heal the drifted tests (I21, I11)

**Files:**
- Create: `include/logic/combat_rules.h`
- Modify: `src/game/scene_dungeon.cpp` — play_room sites ONLY (`:1127, :1129, :1133, :1165` + respawn block `:1240-1248` + crystal threshold `:1293-1300` area). Do NOT edit the boss-loop sites (`run_room_boss` :436-459, `scene_boss.cpp` :351/:355/:410) — Phase 5 deletes both loops, and its shared `boss_fight.cpp` uses these constants from birth.
- Modify: `test/test_death_respawn.cpp` (rewrite against the shared step; delete the stale "current scene sets invuln=0" narrative)
- Test: additions in `test/test_death_respawn.cpp`

**Interfaces (Produces):**

```cpp
#pragma once
#include "logic/meters.h"
namespace logic {
inline constexpr int CONTACT_DAMAGE    = 20;
inline constexpr int HIT_IFRAMES       = 45;   // re-arm window after any hit
inline constexpr int KILL_MAGIC_REFILL = 25;
inline constexpr int RESPAWN_IFRAMES   = 60;   // post-respawn grace
static_assert(RESPAWN_IFRAMES > HIT_IFRAMES, "grace must exceed re-arm or hazard spawns death-loop");
// One contact/hazard hit attempt, exactly as every scene runs it. Returns true if damage landed.
inline bool try_hit(Meter& health, int& invuln, bool dash_invincible, bool overlapping){
    if(invuln == 0 && !dash_invincible && overlapping){ health.damage(CONTACT_DAMAGE); invuln = HIT_IFRAMES; return true; }
    return false;
}
inline void tick_iframes(int& invuln){ if(invuln > 0) --invuln; }
inline void respawn_vitals(Meter& health, int& invuln){ health.cur = health.max; invuln = RESPAWN_IFRAMES; }
}
```
- [ ] **Step 1:** Failing tests: rewrite `test_death_respawn.cpp`'s two scenario tests to drive `try_hit`/`tick_iframes`/`respawn_vitals` (the map fixtures stay; the hand-rolled `if(invuln==0 && hazard_overlap(...)){...}` lines become `logic::try_hit(...)` calls); delete the local `RESPAWN_IFRAMES=60` redefinitions and the stale line-number/`invuln=0` comments. Keep the existing bounded-deaths simulation assertion (`deaths <= 300/RESPAWN_IFRAMES + 1`) — the grace-exceeds-re-arm relationship itself is enforced by the header's `static_assert`, so no extra test is needed for it.
- [ ] **Step 2:** Implement header; replace every scene literal site (`health.damage(20); invuln = 45;` → `logic::try_hit(health, invuln, player.dash.invincible(), <overlap-expr>)`; the respawn block calls `logic::respawn_vitals(health, invuln)`; `magic.heal(25)` → `magic.heal(logic::KILL_MAGIC_REFILL)`). Crystal threshold: `spell.h` defines the cast cost — locate it (grep `cost` in include/logic/spell.h); if it is a literal inside a function, hoist `inline constexpr int SPELL_COST = 10;` beside it. No scene crystal edits here — Task 4.3's CrystalStation is the consumer (`magic.cur < logic::SPELL_COST`); the old boss-loop `< 10` sites die with Phase 5.
- [ ] **Step 3:** Green; ROM builds; mGBA smoke (take a hit, die in lava, respawn with blink).
- [ ] **Step 4: Commit** — `git commit -m "refactor(logic): combat constants + vitals frame-step shared by scenes and tests — death/respawn tests exercise real code (I21, I11)"`

Do NOT: change any tuning value; move enemy-kill or boss-wound rules here (boss rules live in boss.h; Phase 5 owns them).

### Task 4.3: game/player_session — the shared scene controller (I7)

**Files:**
- Create: `src/game/player_session.h`, `src/game/player_session.cpp` (game layer — it names bn:: sprite items AND logic types; engine/ can't, logic/ can't)
- Test: none host-side (bn::-coupled); verification = Task 4.4's behavior-preservation QA. The decode logic it wraps is already logic-layer (`spell.cycle`, ability bits) and host-tested.

**Interfaces (Produces — Task 4.4, Phase 5, Phase 6 consume exactly these):**

```cpp
#pragma once
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include "logic/player.h"
#include "logic/player_state.h"
#include "logic/world_state.h"
#include "logic/spell.h"
namespace game {

// Screen-clamped follow camera (the 4x-duplicated lambda, verbatim semantics).
void set_clamped_cam(bn::camera_ptr& cam, int map_px_w, int map_px_h,
                     int level_w, int level_h, int cx, int cy);

struct SessionIntent { logic::InputFrame in; bool want_grapple = false; bool cast_spell = false; };

class PlayerSession {
public:
    PlayerSession(bn::camera_ptr& cam, logic::World& world, logic::PlayerState& ps, logic::Player& player);
    void sync_abilities();            // the 5-line world.has(...) block
    SessionIntent read_intent();      // read_input + spell cycle + grapple/cast decode; in.grapple_fire=false
                                      // (caller decides anchor vs pull, then sets it)
    void refresh_spell_icon();        // Fire/Ice/Grapple/Light icon swap-on-change
    logic::Vec2 muzzle() const;       // aim-adjusted muzzle (read_aim_dy)
    // Vine VFX: latched line to anchor, or the 10-frame miss animation. Call once per frame.
    void note_anchor_miss(int facing);
    void update_vine_vfx(int hw, int hh);
private:
    bn::camera_ptr& _cam; logic::World& _world; logic::PlayerState& _ps; logic::Player& _player;
    bn::sprite_ptr _spell_icon; logic::SpellId _last_icon = logic::SpellId::None;
    bn::vector<bn::sprite_ptr, 4> _vine_segs; int _miss_vine_t = 0; int _miss_vine_dir = 1;
};

// Respawning full-refill magic crystals (the 3x-duplicated M10 pattern).
// Handles ALL of level.magic_crystals (up to 8 — play_room's case); boss arenas have 1.
// Grounding differs between the two originals and MUST be preserved per caller:
//   TileCentre  — play_room's `mc.ty*8+8` placement (D8 rest-ledge visuals depend on it)
//   FloorBelow  — the boss loops' floor_row_below grounding (the M13 sank-into-floor fix)
class CrystalStation {
public:
    enum class Grounding { TileCentre, FloorBelow };
    // respawn_when_depleted preserves the two originals' DIFFERENT semantics:
    //   true  — boss loops: a collected crystal reappears once magic.cur < SPELL_COST (repeatable station)
    //   false — play_room: collected stays gone until reset() (death/attempt reset only)
    void spawn(const logic::LevelData& level, bn::camera_ptr& cam, const logic::Tilemap& map,
               int hw, int hh, Grounding g, bool respawn_when_depleted);
    void update(const logic::Body& player_body, logic::Meter& magic);
    void reset();                                                        // fight-restart / death path: all uncollected+visible
};
}
```
- [ ] **Step 1:** Move the implementations verbatim from their scene_dungeon.cpp originals (`refresh_spell_icon` :603-612, `set_clamped_cam` :540-547, ability sync :880-884, vine VFX :1322-1364, crystal :453-462 + :1293-1300 + :778-787, muzzle :1070-1071, intent decode :886-890 + :949-951). Where hub/boss variants differ trivially (wx/wy wrappers), normalize to the dungeon form.
- [ ] **Step 2:** Compile the ROM with the new TU added but not yet consumed (dead code for one commit is fine — it keeps 4.4 reviewable).
- [ ] **Step 3: Commit** — `git commit -m "feat(game): PlayerSession + CrystalStation + shared clamped camera — extracted from the 4x scene copies (I7)"`

Do NOT: extract the grapple block-pull/enemy-pull targeting (stays in play_room — it needs the room's blocks/enemies); invent capability flags beyond what the call sites need (hub = anchors-only is expressed by the CALLER setting `in.grapple_fire = true` on want_grapple, exactly as today).

### Task 4.4: Consume PlayerSession in all four loops (I7)

**Files:**
- Modify: `src/game/scene_hub.cpp`, `src/game/scene_dungeon.cpp` (`play_room` ONLY)

**Scope boundary (cross-task conflict avoidance):** do NOT migrate `run_boss` (scene_boss.cpp) or `run_room_boss` here — Phase 5 deletes both and its shared loop consumes PlayerSession/CrystalStation directly. Migrating them first would be work Phase 5 throws away, on the two riskiest loops.

- [ ] **Step 1:** Replace the two loops' local copies with the shared unit, one scene per commit, mGBA smoke after each: hub (cycle spells, cast, grapple a miss + an anchor, icon updates, camera clamps at plaza edges); dungeon play_room (same + block-pull + enemy-pull still work — they consume `intent.want_grapple`).
- [ ] **Step 2:** Also collapse the heart-cap sync triplication: add to `include/logic/player_state.h`: `inline void sync_health_cap(PlayerState& ps, const World& w){ ps.health.max = max_health_for(w); if(ps.health.cur > ps.health.max) ps.health.cur = ps.health.max; }`; use at main.cpp:29-30, run_dungeon :1388-1389, scene_hub :58-59.
- [ ] **Step 3:** Deletion audit: `grep -n "refresh_spell_icon\|set_clamped_cam" src/game/scene_hub.cpp src/game/scene_dungeon.cpp` → play_room defines neither (the boss-loop copies remain until Phase 5 deletes those loops wholesale).
- [ ] **Step 4: Commits** — one per scene: `refactor(scene_X): consume PlayerSession/CrystalStation; delete local copies (I7)`.

Do NOT: change input semantics, icon art choices, camera margins (120/80), or vine timings — this is a pure de-duplication; any observed behavior delta on mGBA is a bug in the migration.

**After completing Phase 4:** 3-round review with emphasis on behavior preservation: diff each scene against its pre-phase version and justify every non-mechanical line. Update `docs/content-recipes.md` pointers (tile_ids.h, combat_rules.h).

# Phase 5 — Boss-loop unification (D4)

**Execution Status:** ⬜ NOT STARTED

One data-driven fight loop replaces `run_boss` + `run_room_boss` (I2, I9, I20, I30). Dimensions: Code Quality + Architecture + Content DX. Depends on Phase 3 (boss_defeats consumer landed) and Phase 4 (PlayerSession/CrystalStation). Highest-QA phase: all four shipped fights must play identically.

**Behavior inventory to preserve** (from the verified sources — the executor re-verifies each on mGBA at 5.5):
- King (`run_boss`): perch teleports every 200f + teleport-on-wound + warp blink; bolt/Fire/Ice all block (`block_player_shots`); Light exposes; phase taunts at P2/P3; intro "YOU FINALLY MADE IT" / death "NOOOOO!"; no mid-fight quit; Game-Over → run_game_over inline.
- D1 Guardian: TiredWindow, no blocking, aimed→fan.
- D2 Slagshell: pacing, Fire block+charge, rockfall 3→5 rocks, proj speed 2→3.
- D3 Coldforge: stationary, dual-element shift (4-frame sprite), Fire+Ice both block.
- Room bosses: full-HP entry, magic carried in, restart-on-death replays intro line, Victory → walk-to-door in the normal room loop, Game-Over handed back to run_dungeon.

### Task 5.1: BossDef becomes the whole description (I2-data, I9-id, I20, I30)

**Files:**
- Modify: `include/logic/boss.h`
- Modify: `include/engine/boss_attacks.h` (compile-time invariant)
- Modify: `tools/build_level.py` (no change needed to BOSS_SYMBOL — keys stay)
- Test: `test/test_boss.cpp`

- [ ] **Step 1: Failing tests first** (test_boss.cpp):
  - **All-defs invariant loop replacing the raw field pins** (I30): `static const BossDef* DEFS[] = {&KING_DEF,&D1_DEF,&D2_DEF,&D3_DEF};` — for each: every phase `telegraph_frames >= SWITCH_BUDGET`; `end_hp` strictly decreasing, last == 0; `max_hp > 0`; every phase with ROCKFALL has `rock_count >= 1`; every phase `proj_speed >= 1`; `id` unique across defs. Delete `bossdef_d2_slagshell_fields` / `bossdef_d3_coldforge_fields` raw pins (the loop subsumes them; tuning passes stop breaking tests).
  - **Wound-path behavior** (I30): `on_wound(dmg=25)` on a 70-HP def crossing TWO thresholds in one hit lands in the correct band; `on_wound(1000)` clamps hp to 0 and `defeated()`; wound with damage not a multiple of wound_dmg still ends the expose window.
- [ ] **Step 2:** Extend the data model:

```cpp
enum class BossId : uint8_t { King=0, D1Guardian, D2Slagshell, D3Coldforge };
enum class BlockMode : uint8_t { None, SpellBlock, BoltAndSpellBlock };  // D1 / D2+D3 / King
struct BossPhaseDef { int end_hp; PhasePattern pattern; uint8_t attacks;
                      int proj_speed = 2; int rock_count = 0; };          // per-phase tuning OUT of the scene
struct Perch { int cx, cy; };                                             // tile coords — pure ints
// BossDef additions: BossId id; BlockMode block_mode; const Perch* perches = nullptr;
//   int perch_count = 0; int teleport_period = 0; bool teleport_on_wound = false;
//   const char* phase_lines[3] = {nullptr,nullptr,nullptr};  // taunt when ENTERING phase i (King P2/P3)
```
Move the King's perch table + period from scene_boss.cpp into `KING_DEF` (`teleport_on_wound=true`); set `D2_PHASES` `{proj_speed=2, rock_count=3}` / `{proj_speed=3, rock_count=5}`; existing `block_spell`/`block_spell2` express SpellBlock targets; King gets `BlockMode::BoltAndSpellBlock` + his three dialogue lines in the def. Keep `KING_ATTACKS` masks — attack selection will now genuinely read `phases[].attacks` (fixes I20; the King's mask rotation order AIMED→SPIRAL→FAN equals today's `KING_ATTACK_CYCLE`).
- [ ] **Step 3:** In `include/engine/boss_attacks.h` add the cross-file invariant as code (I2): `constexpr bool rockfall_fits(const logic::BossDef& d){ for each phase: if(attacks & ROCKFALL) require pattern.attack_active_frames > RockfallEmitter::WARN_FRAMES; }` + `static_assert(rockfall_fits(logic::D2_DEF), "rockfall must land inside the Active window");` (and for every def in the registry).
- [ ] **Step 4:** Green + purity. **Step 5: Commit** — `git commit -m "feat(boss): BossId/BlockMode/per-phase tuning/perches in BossDef; def-invariant test loop replaces field pins; rockfall window static_assert (I2, I9, I20, I30)"`

Do NOT: change any numeric tuning; add speculative fields for D4–D8 mechanics that don't exist yet (add fields when a boss needs them — the recipe doc explains where).

### Task 5.2: game/boss_fight — the one shared loop

**Files:**
- Create: `src/game/boss_fight.h`, `src/game/boss_fight.cpp`
- Interfaces: `enum class FightOutcome { Victory, GameOver, QuitToTitle };` `FightOutcome run_boss_fight(const logic::LevelData& level, const logic::BossDef& def, const bn::sprite_item& boss_sprite, logic::World& world, logic::PlayerState& ps, engine::LoadedLevel& lvl, bn::camera_ptr& cam, logic::Player& player, const logic::Vec2& spawn_pos, const logic::EntranceSpawn& ent, bool inline_game_over);` (sprite resolved by the caller: scene_boss passes `bn::sprite_items::king`; scene_dungeon passes `boss_sprite_for(level.boss)`)

- [ ] **Step 1:** Start from `run_room_boss` (scene_dungeon.cpp:167-521 — the more data-driven fork), generalized by def fields: locomotion pacing (existing); **teleport** when `perch_count > 0` (port the perch/timer/flash/wound-warp logic from scene_boss.cpp:120-128, 281-307, 392-399); **blocking** by `block_mode` (SpellBlock → existing `block_with_spell` on block_spell/block_spell2 + magic charge; BoltAndSpellBlock → `attacks.block_player_shots(bolts, spells)`); **attack selection** via the existing mask-rotation `next_attack_for_phase` for ALL bosses (King included — I20); **phase-entry taunts** via `phase_lines`; **sprite** resolved by the caller (passed as `const bn::sprite_item&` — keeps the id→sprite table at the call site); per-phase `proj_speed`/`rock_count` read from the def (delete the `(b.phase == 0) ? ...` literals). PlayerSession + CrystalStation from Phase 4 replace the local copies. `inline_game_over` selects King behavior (run run_game_over inside, restart or QuitToTitle) vs room behavior (return GameOver to the dungeon flow).
- [ ] **Step 2:** Compile-only commit (unit exists, unconsumed): `git commit -m "feat(game): shared def-driven boss fight loop (I2, D4)"`

Do NOT: change fight feel (timers, i-frames, camera); support >1 crystal (validator guarantees 1; documented in recipes); keep ANY per-boss `if(def == &X)` in the loop — per-boss variation must be a def field or it doesn't ship.

### Task 5.3: King on the shared loop

**Files:**
- Modify: `src/game/scene_boss.cpp` (shrinks to: load arena, spawn King sprite via `bn::sprite_items::king`, call `run_boss_fight(..., inline_game_over=true)`, map outcome to BossResult)

- [ ] **Step 1:** Rewrite; delete `KING_ATTACK_CYCLE`, the local perch table, restart_fight, boss_say, crystal, icon, camera copies (~350 lines → ~60).
- [ ] **Step 2:** mGBA QA against the 5.1 behavior inventory (teleports, blocks, taunts, expose-wound-warp, Game-Over Continue/Quit, victory → THE END flow via main.cpp).
- [ ] **Step 3: Commit** — `git commit -m "refactor(boss): the King runs on the shared fight loop; phases[].attacks is now live data (D4, I20)"`

### Task 5.4: Room bosses on the shared loop + id-keyed sprite table (I9)

**Files:**
- Modify: `src/game/scene_dungeon.cpp` — delete `run_room_boss` (:167-521); `boss_sprite_for` becomes:

```cpp
static const bn::sprite_item& boss_sprite_for(const logic::BossDef* def){
    switch(def->id){
        case logic::BossId::D2Slagshell: return bn::sprite_items::slagshell;
        case logic::BossId::D3Coldforge: return bn::sprite_items::coldforge;
        case logic::BossId::D1Guardian:  return bn::sprite_items::guardian;
        default: BN_ERROR("no sprite mapped for boss id ", (int)def->id);
    }
}
```
(the silent guardian fallback dies — an unmapped D4 boss now fails loudly at fight start); the Phase 3.3 boss block calls `run_boss_fight(level, *level.boss, ..., inline_game_over=false)`.
- [ ] **Step 1:** Rewire + delete. **Step 2:** mGBA QA: D1/D2/D3 fights per the behavior inventory (D2 rockfall counts, D3 element shift + 4-frame sprite, death-restart intro replay, victory walk-to-door, defeated-boss skip from Phase 3). Host tests green.
- [ ] **Step 3: Commit** — `git commit -m "refactor(boss): room bosses on the shared loop; sprite registry keyed by BossId with loud failure (D4, I9)"`

### Task 5.5: Full-matrix fight QA (manual gate)

- [ ] mGBA: all four fights end-to-end + one death each + one Game-Over each. Record results in this plan (Discoveries if anything differs). This gate blocks Phase 6.

**After completing Phase 5:** 3-round review; scene_boss.cpp + the boss parts of scene_dungeon.cpp should sum to LESS code than either original loop alone. Update recipes doc's boss checklist (id enum + table row replace pointer-compare guidance).

---

# Phase 6 — play_room decomposition + remaining minors

**Execution Status:** ⬜ NOT STARTED

Split the 850-line room loop into room-subsystem units with an explicit update order (I1); land the enemy-type seam (I10) and the remaining minors (I23, I25, I26, I27, I28, I35). Dimensions: Code Quality + Architecture + Content DX + Ops. Depends on Phases 4+5 (play_room already consumes PlayerSession; run_room_boss is gone).

**Extraction rules for 6.1–6.4** (every task follows these):
- New units live in `src/game/room/` + `include/game/room/`; each owns ONE entity family with `spawn(const logic::LevelData&, Ctx&)`, `update(Ctx&)`, `on_player_death(Ctx&)` as needed. `Ctx` (defined `include/game/room/room_ctx.h`) carries the shared refs: `logic::World& world; logic::Player& player; logic::PlayerState& ps; engine::LoadedLevel& lvl; bn::camera_ptr& cam; int hw, hh; int& invuln;` plus the cross-system queries systems genuinely need (e.g. `BlockSystem&` for triggers' plate check).
- Moves are verbatim-first: copy the block, compile, delete the original, THEN clean comments (I28: milestone war stories become one-line invariants; anything worth keeping longer goes to `docs/pitfalls/implementation-pitfalls.md` as a new IMPL-N entry).
- The frame-loop call order in `play_room` is written as a numbered comment block at the call site, preserving today's order exactly: input/intent → grapple targeting → player.update → pound resolution → loose platforms → bolts/spells → Light reveal → gates → braziers → enemies → freeze/melt → despawn_on_solid → hazards → blocks → triggers → i-frames/respawn → shrines → hearts → crystals → icon → spronk/exit → vine → HUD/camera. (The ordering constraints currently in comments — freeze/melt before despawn, gates before braziers before enemies — become this ONE list.)
- After each task: host tests green, ROM builds, one-room mGBA smoke of the moved family.

### Task 6.1: Extract doors/exit/cage/pickups (lowest-risk families)

**Files:** Create `src/game/room/pickups_system.{h,cpp}` (shrines, hearts, crystals→CrystalStation multi-instance, cage/spronk, exit) and `src/game/room/doors_system.{h,cpp}` (room-door render + Up-press resolution, exit archway render). Modify `scene_dungeon.cpp` accordingly.
- [ ] Move, wire, smoke (collect a shrine + heart in D7, exit D1). Commit: `refactor(room): pickups + doors systems extracted from play_room (I1)`.

### Task 6.2: Extract gates/cracked-floors/braziers/triggers; fix the two latent geometry issues

**Files:** Create `src/game/room/gates_system.{h,cpp}`, `src/game/room/triggers_system.{h,cpp}`. Modify `scene_dungeon.cpp`. Expose `GatesSystem::break_cracked_run_at(int impact_cx, int impact_floor) -> bool` and `TriggersSystem::trip_heavy_plate_at(int impact_cx, int impact_fy)` — Task 6.3's pound resolution calls them (see its ownership note).
- [ ] **Brazier hitbox (I25):** replace `tile_body(b.tx, 14, 6, 24)` with rows derived from the floor scan: `tile_body(b.tx, draw_ty - 5, 6, 24)` (identical for the row-20-floor rooms: draw_ty 19 → rows 14..19; correct for ledge/nonstandard rooms). Add a harness-level regression note in the D6 brazier test.
- [ ] **Cracked-floor span break (I27):** replace the triple-nested contiguity scan (:986-1010) with walk-left/walk-right from the impact tile:

```cpp
auto break_tile = [&](CrackedFloorInst& q){
    q.broken = true;
    engine::set_collision_tile(q.tx, q.ty, 0);
    engine::set_level_tile(lvl.view, q.tx, q.ty, logic::tiles::BLANK);
    persist_latch(world, q.latch_id);
};
auto cracked_at = [&](int x)->CrackedFloorInst*{ for(auto& q : cracked_floors)
    if(!q.broken && q.ty == cf.ty && q.tx == x) return &q; return nullptr; };
for(int x = impact_cx; CrackedFloorInst* q = cracked_at(x); --x) break_tile(*q);
for(int x = impact_cx + 1; CrackedFloorInst* q = cracked_at(x); ++x) break_tile(*q);
// `smashed = true` + the re-arm behavior stay exactly as today.
```
(C++17 for-loop condition-declarations are valid; the semantics are identical to the old maximal-contiguous-span break because a contiguous walk from the impact tile visits exactly that span.) Verify via D7 room 2's stacked-floor pound on mGBA (chain must still re-arm).
- [ ] Commit: `refactor(room): gates + triggers systems; brazier hitbox floor-derived; cracked-run break is a 2-direction walk (I1, I25, I27)`.

### Task 6.3: Extract enemies + blocks/boulders/platform families; wire the cross-system pound

**Files:** Create `src/game/room/enemies_system.{h,cpp}`, `src/game/room/terrain_system.{h,cpp}` (blocks, boulders, loose + hidden platforms, pound resolution). Modify `scene_dungeon.cpp` (grapple targeting stays in play_room but now queries the two systems via `Ctx`).

**Pound-resolution ownership (cross-system — decided here so no executor has to):** the pound is ONE event touching three families. `terrain_system` owns the `just_landed()` resolution sequence (it owns boulders + loose platforms + the impact-row math) and calls two methods the earlier tasks expose for it: `GatesSystem::break_cracked_run_at(impact_cx, impact_floor)` (Task 6.2 defines it — the walk-left/right break, returns `bool smashed` for the re-arm) and `TriggersSystem::trip_heavy_plate_at(impact_cx, impact_fy)` (Task 6.2 defines it). The resolution ORDER stays exactly today's: cracked-floor → heavy plate → boulder → loose-platform shockwave, then `if(smashed) player.stone.start();`.
- [ ] Move, wire, smoke (D7 pound chain incl. stacked-floor re-arm, D8 Light climb, block-pull puzzle, enemy grapple-pull). Commit: `refactor(room): enemies + terrain systems; pound event routed across systems in the original order (I1)`.

### Task 6.4: play_room becomes the orchestrator

- [ ] After 6.1–6.3, `play_room` should be ≤ ~250 lines: setup, the ordered update list, death/respawn (via combat_rules), room-outcome handling. Delete the now-dead instance structs from the top of scene_dungeon.cpp. Final I28 comment sweep of what remains; add harvested war stories to docs/pitfalls (expect ~4–6 new IMPL entries: brazier height, freeze-before-despawn, plate-vs-floor impact rows, respawn grace, camera clamp rationale).
- [ ] Full mGBA regression walk: one full room of each dungeon D1–D9 approach.
- [ ] Commit: `refactor(room): play_room is an orchestrator over room systems; war-story comments distilled to docs/pitfalls (I1, I28)`.

### Task 6.5: Generated-header hygiene + collision-grid guard (I23, I26)

**Files:** `tools/build_level.py` (`emit_array`), `tools/test_build_level.py`, `src/engine/level_loader.cpp`, `include/engine/level_loader.h`
- [ ] **I23:** empty entity lists emit `nullptr` + count 0 instead of a one-dummy-element array (`emit_array` returns `(f'inline constexpr {cpp_type}* {name}_{var} = nullptr;', 0)` — adjust the LevelData reference emission accordingly; pointers are only read under `i < count`, verified in the spawn loops). Python test: empty gates → `nullptr`; regen all headers; host tests + ROM green. Grep-check: `grep -l "GATES\[\] = { {0,0" include/game/levels/` returns nothing.
- [ ] **I26:** add a generation counter to level_loader: `static int s_generation = 0;` incremented in `load_level`; `LoadedLevel` gains `int generation;`; add `engine::level_generation()` accessor + a comment on `set_collision_tile` stating the single-level contract. (Documentation + a cheap tripwire for the future two-level feature; no behavior change.)
- [ ] Commit: `fix(pipeline): empty entity arrays emit nullptr; collision grid documents+tags its single-level contract (I23, I26)`.

### Task 6.6: Enemy-type seam + dungeon registry table (I10, I8-part)

**Files:** `include/logic/enemy.h`, `include/logic/level_data.h` (comment only), `tools/build_level.py`, `src/game/room/enemies_system.cpp`, `src/main.cpp`, `src/game/scene_hub.cpp`, `include/game/levels/dungeons.h`
- [ ] **Enemy seam (I10), minimum useful version:** `enum class EnemyType : uint8_t { Patroller = 0, PatrollerFireImmune = 1 };` in enemy.h + `inline EnemyType enemy_type_from_params(int param2){ return (param2 & 1) ? EnemyType::PatrollerFireImmune : EnemyType::Patroller; }` (host-test the decode). build_level.py accepts `"type": "patroller"|"patroller_fire_immune"` as the preferred JSON spelling (emitting the same param2 bit; `"fire_immune": true` stays accepted). enemies_system selects sprite/behavior via a single `switch(type)` — the one place a future flyer adds a row. Do NOT implement any new enemy behavior.
- [ ] **Dungeon registry (I8):** in dungeons.h add:

```cpp
inline constexpr const logic::DungeonData* DUNGEONS_BY_ID[8] = {
    &DUNGEON1_DUNGEON, &DUNGEON2_DUNGEON, &DUNGEON3_DUNGEON, &DUNGEON4_DUNGEON,
    &DUNGEON5_DUNGEON, &DUNGEON6_DUNGEON, &DUNGEON7_DUNGEON, &DUNGEON8_DUNGEON };
```
main.cpp's if-chain becomes `if(n >= 1 && n <= 8) lvl = DUNGEONS_BY_ID[n-1]; else continue;` and scene_hub's `door_enterable` becomes `return n == 1 || (n >= 2 && n <= 8 && w.spronk_freed(n - 1)) || (n == 9 && logic::spronk_count(w) == 8);`. Behavior identical (host-test door_enterable if it moves to logic; otherwise verify by inspection + hub mGBA smoke).
- [ ] Update `docs/content-recipes.md`: the "add a dungeon" checklist's main.cpp/door_enterable steps become "add one row to `DUNGEONS_BY_ID`" (the formula covers gating automatically); the "add an enemy" checklist points at the `EnemyType` switch.
- [ ] Commit: `refactor: enemy-type seam + dungeon registry table replace param-bit and if-chain growth points (I10, I8)`.

### Task 6.7: Debug selector + CPU meter (I35)

**Files:** Create `src/game/scene_debug.{h,cpp}`; modify `src/game/scene_title.cpp` (entry combo), `src/engine/pause.cpp` (CPU line)
- [ ] **Debug menu:** on the title screen, holding SELECT when pressing START enters a debug scene instead of the hub: D-pad picks dungeon 1–9; A toggles each ability; L grants spronks 1..k; START launches with the chosen `World` state (session-only). Because normal gameplay saves automatically (deaths, latches, pickups), session-only requires a save gate: add `engine::set_save_enabled(bool)` in save.h/save.cpp (default true; `write_world` becomes a no-op when disabled) and have the debug launch path disable it for the session. Without this gate the debug state silently overwrites the player's real SRAM — the exact failure the task exists to avoid. This replaces the deleted main.cpp QA scaffold (see project memory: deleting .sav was the old workaround).
- [ ] **CPU meter:** while paused (existing check_pause), render one text line `CPU <last>% / max <max>%` from `bn::core::last_cpu_usage()` (sampled each frame into a rolling max; multiply to int percent — IMPL-2: no float formatting, use `(usage * 100).to_int()` on the bn::fixed value ONLY inside engine code).
- [ ] mGBA QA: enter debug menu, launch D8 with all abilities; pause shows CPU. Commit: `feat(dev): title-screen debug selector + pause-screen CPU meter (I35)`.

Do NOT: persist debug-granted state to SRAM; add the meter outside the pause screen (zero cost when unused).

**After completing Phase 6:** 3-round review + the full-game mGBA regression walk + `bash tools/host_test.sh` + `python tools/check_logic_purity.py` + `bash tools/build_rom.sh`. Update project memory: the `share-player-ability-controller` and `dungeon-progression-debug-selector` memories are now DONE (update/remove them); update `boss-fight-scope` (framework unified).

---

# Appendix A: Issues Identified But Not Fixed in This Cycle

### The 9-dungeon door-glyph ceiling (D5 — user decision: defer)
**Severity:** MAJOR (as a future wall; zero current impact)
**Dimensions:** Architecture, Content DX
**Evidence:** `tools/build_level.py` door glyphs are single characters `'1'-'9'` (docstring + `CONTENT` set + the `c in '123456789'` branch); `logic::DoorSpawn.dungeon` is an int, so only the GRID SYMBOL scheme is capped, not the data model.
**Why deferred:** the project's stated direction (project memory) is richer existing dungeons via room-to-room retrofits, not more than 9 hub doors. The finale occupies door 9.
**Recommended approach when needed:** keep digit glyphs as positional door MARKERS and move dungeon identity to the hub's JSON sidecar (`"doors": [{"dungeon": 10}, ...]` matched in scan order) — no grid-format change, one build_level.py branch, one hub.json edit.

### No CI / automated ROM-compile check (I12-part — deferred)
**Severity:** MAJOR
**Dimensions:** Ops Readiness, Test Quality
**Why deferred:** needs infrastructure decisions this plan shouldn't make unilaterally: a devkitPro/Butano container image, a runner (GitHub Actions vs local pre-push hook), and secrets-free artifact handling. Host tests + the Phase 1 validator cover the logic + content layers; the ROM-compile gap remains manual (`bash tools/build_rom.sh`).
**Recommended approach:** GitHub Actions with the `devkitpro/devkitarm` container: one job runs `tools/host_test.sh` (needs only g++/python), a second runs `make` for the ROM compile check. Revisit after this plan ships.

### Design notes recorded, no action required
- `World.spronks_freed` is uint16 (≤16 dungeons) and the finale gate hardcodes `spronk_count == 8` — revisit only if the dungeon count changes (FP1 in the validated report: latch/heart budgets are NOT near exhaustion — 3/24 and 3/8 used).
- `GateType::Water` vs `TileKind::Water` naming (I18) — disambiguated by comments in 0.5; renaming would churn the JSON schema for zero behavior.

---

# Execution strategy recommendation

**Recommended: subagent-driven development from a FRESH session** (`/superpowers:subagent-driven-development`), with Phases 0–3 dispatched as parallel workstreams and Phases 4→5→6 executed sequentially afterward.

Reasoning: (1) this planning session's context is heavily consumed — a fresh session loses nothing because the plan + validated report are self-contained; (2) Phases 0–3 are LOGICALLY independent but four tasks across three of them touch `scene_dungeon.cpp` (0.5 dead-flag, 1.3 heavy-plate, 1.4 assert, 3.3 boss block) — so parallelize at most Phase 2 (test/-only) against ONE of {0, 1, 3} at a time, and serialize the scene_dungeon.cpp tasks in plan order (0.5 → 1.3 → 1.4 → 3.3); (3) Phases 5 and 6 are risky, behavior-preserving refactors of shipped fights and the hottest file — they deserve one focused agent each with the mGBA QA gates, not parallel dispatch; (4) per-task review gates matter here because most tasks are "mechanical move" tasks where the failure mode (silent behavior change) is exactly what a fresh reviewer catches.

