# Agent 3: Test Quality
**Date:** 2026-07-16 23:28
**Scope:** Maintainability + token-efficiency for adding dungeons/bosses/enemies

### [CRITICAL] Six private copies of the player-movement reachability model, with divergent physics per copy

**Evidence:** `test/test_dungeon1_level.cpp`, `test_dungeon2_level.cpp`, `test_dungeon3_level.cpp`, `test_dungeon7_level.cpp`, `test_dungeon8_level.cpp`, `test_dungeon9_level.cpp` — each defines its own ~130–240-line `Grid`/`snap_start`/`reachable`/`stands_at` flood-fill (e.g. test_dungeon7_level.cpp:34–260, test_dungeon2_level.cpp:30–150, test_dungeon3_level.cpp:26–136). Jump-reach constants diverge: `CLIMB = 5` (D1/D2/D3), `CLIMB = 6` (D7), dual `CLIMB_RELIABLE=5 / CLIMB_MAX=7` (D8/D9). D2/D3 copies have a "horizontal double-jump over a gap" move (test_dungeon2_level.cpp:113–127) that D7 and D8 lack.
**Problem:** This is the load-bearing engine of every no-soft-lock invariant, and it is forked per dungeon. The D8 file's own comment (lines 27–42) documents that the old single-threshold model shipped a real soft-lock ("the light platform is not reachable") and was fixed with dual thresholds — but the fix was applied only to D8/D9; D7 still runs the single `CLIMB=6` model the comment calls unsound, and D1/D2/D3 use yet another value. None of the constants are derived from `JUMP_VY` in `src/logic/player.cpp` (whose comment shows the jump height has already been retuned once, from -4 to -812 raw).
**Risk:** A jump/gravity retune silently invalidates all six models at once — reachability tests stay green while the real player can no longer make the climbs (exactly the M10 bug class these tests exist to catch), or the tests block a layout the player can actually traverse. Each new dungeon pays ~200 lines of copied harness plus a coin-flip over which movement semantics it inherits.

### [CRITICAL] Death/respawn tests simulate the scene loop by hand — and have already drifted from it

**Evidence:** `test/test_death_respawn.cpp:22–24` ("Mirrors the per-frame ORDER in scene_dungeon.cpp::play_room… scene line ~676… ~739… ~741"); line 56 comment: "The current scene sets invuln=0 on respawn (no grace)" versus `src/game/scene_dungeon.cpp:1242` which sets `invuln = RESPAWN_IFRAMES` (60); the test's own respawn at line 124 sets `invuln = 0` (the old scene behavior); `respawn_grants_postrespawn_iframe_breaks_death_loop` (line 57) defines its own `RESPAWN_IFRAMES=60` inside the test and asserts against it.
**Problem:** Both tests exercise a copy of the frame loop written inside the test file, not the code in `scene_dungeon.cpp`. One test bakes in the fix, the other bakes in the pre-fix behavior; neither compiles or calls the scene. The comments' claims about "the current scene" are already stale, proving the mirror does not track the mirrored code.
**Risk:** Reordering damage/i-frame/respawn in `play_room`, dropping the grace window, or changing the hazard re-arm (45) can never fail these tests. The death-loop soft-lock they were written to prevent can be reintroduced with a fully green suite.

### [MAJOR] ~2,900 lines of scene/engine code have zero tests while src/logic is only 191 lines of .cpp

**Evidence:** `src/game/scene_dungeon.cpp` (1420 lines), `scene_boss.cpp` (445), `scene_hub.cpp` (275), `src/engine/boss_attacks.cpp` (216, the boss projectile/rockfall attack library) — no automated tests. Meanwhile per-dungeon tests hand-replicate scene constants to compensate: `fill_column` rows 1..h-3 re-encoded in test_dungeon7_level.cpp:164–169 and test_dungeon8_level.cpp:92–99, the brazier hit-zone "rows 14..19" in test_dungeon6_level.cpp:229–244, the 16px-sprite `ty+2` grounding rule in test_dungeon6_level.cpp:109–126.
**Problem:** The frame-loop damage resolution, room-transition state machine, gate open/close (`open_column`/`fill_column`), block-to-charge magic economy, latch persistence, and boss attack spawning are all decision-heavy logic living in bn::-coupled files, so the three-layer rule exempts them from testing. The tests' workaround — copying the constants into level invariants — is unlinked duplication: change `fill_column`'s row range or the brazier hit zone in the scene and every test that mirrors it keeps passing while asserting stale geometry.
**Risk:** Whole bug classes ship untested: a gate that fills one row short (jump-over bypass), a transition that carries velocity/i-frame state across rooms, a boss attack whose projectiles spawn inside the player. The MEMORY-noted duplicated spell/grapple controller in scene_dungeon/scene_hub is untested in both copies, so they can diverge silently.

### [MAJOR] D2/D3 room-0 solvability is explicitly not tested — the flood-fill abdicates exactly where the puzzles are

**Evidence:** `test/test_dungeon2_level.cpp:261–266` ("The Fire shrine and the onward door are puzzle-gated… their reachability is guaranteed by puzzle design, not free-traversal flood-fill"); the onward door gets only an existence check (`onward_exists`, line 289–292). `test/test_dungeon3_level.cpp:198–210` repeats the pattern ("the accepted D2 deviation"): onward door checked for existence only.
**Problem:** The rooms with brazier/plate/button puzzle chains — the highest-complexity, easiest-to-soft-lock content — are the ones where the reachability invariant is downgraded to "the door symbol exists in the data." Contrast D7/D8, which model gate states (`close_dark_veil`, `open_heavy_gate`, `reveal_hidden`) to prove gated-then-openable. The D2/D3 grids never model the puzzle gates at all.
**Risk:** Moving a wall pillar, plate, or brazier in D2/D3 room 0 can make the Fire/Ice shrine or the boss door unreachable and every test stays green; the player soft-locks in the first room of the dungeon.

### [MAJOR] No reachability or soft-lock coverage at all for D4, D5, D6

**Evidence:** `test/test_dungeon4_level.cpp` (51 lines) and `test_dungeon5_level.cpp` (60 lines) contain only existence checks (`d4_has_updraft_and_wind`, `d5_has_spike_tiles`, shrine-count, border-solid). `test_dungeon6_level.cpp` (473 lines) has structural/grounding checks but no flood-fill; its header says "Geometry/feel reachability is verified separately on mGBA" (line 7). `test_d1_boss_respawn.cpp` covers entrance-settle for D1/D2/D3 only — not D4–D9.
**Problem:** Three shipped dungeons have no machine check that the exit, shrine, or cage is reachable, and six dungeons have no respawn-settle check. Coverage per dungeon ranges from 51 to 887 lines with no floor.
**Risk:** A layout edit to D4/D5/D6 can strand the player with a green suite. The deferred D1–D5 multi-room retrofit (per project memory) will land precisely on the weakest suites, and there is no template forcing the new rooms to get the D7-grade invariants.

### [MAJOR] Per-dungeon structural boilerplate is re-copied, not data-driven

**Evidence:** Near-identical tests repeated across suites parameterized only by the data pointer: `d6_rooms_solid_border` / `d7_rooms_solid_border` / `d8_rooms_solid_border` / `d2_rooms_solid_border`; `d*_dungeon_table`, `d*_rooms_min_size`, `d*_has_latched_shortcut`, `d*_room_doors_resolve_and_two_way` (test_dungeon7_level.cpp:347–397 vs test_dungeon8_level.cpp:231–267 differ only in a D7-specific exception clause); `d4_hub_door_grounds_on_main_floor` / `d5_hub_door_grounds_on_main_floor` / `d6_room0_hub_door_grounds_on_main_floor` / `d7_room0_doors_ground_on_main_floor` are four hand-copies of the same floor-scan.
**Problem:** There is no shared test helper header beyond the 13-line `test_framework.h`; every universal invariant (solid border, door-entrance resolution, hub-door grounding, latched shortcut, content grounding) is re-authored per dungeon. `DUNGEON*_DUNGEON` tables already exist in `game/levels/dungeons.h` — one loop over a dungeon registry would cover all dungeons and automatically cover new ones.
**Risk:** Each new dungeon costs 300–900 lines of test authoring (directly against the stated cheap-content goal); worse, universal invariants silently don't apply to a new dungeon unless someone remembers to copy them (this already happened: the two-way-door and grounding checks exist for D6–D8 but not D4/D5's interiors).

### [MAJOR] Two test entry points validate different level data; the broken Makefile is still the discoverable one

**Evidence:** `tools/host_test.sh` regenerates every `include/game/levels/*.h` from `tools/levels/*.txt` before compiling tests; `test/Makefile` does not — it compiles whatever headers are committed. The Makefile is also documented as broken on this machine (CLAUDE.md, host_test.sh header) yet remains in the tree as the conventional entry point. host_test.sh additionally compiles all ~6,000 test lines + logic in a single `g++` invocation with no incremental build, and mutates the working tree (regenerated headers) as a side effect of "running tests."
**Problem:** `make -C test` on any machine where it does work (CI, another dev box) tests potentially stale generated headers — a `build_level.py` change or a `.txt` edit passes/fails differently depending on which runner you used. A test run that rewrites source-tree headers can also produce surprise diffs in unrelated commits.
**Risk:** Level-compiler regressions validated green against old headers; "works with host_test.sh, fails with make" split-brain; every added dungeon test increases the monolithic recompile time for all suites.

### [MINOR] A failed pointer CHECK does not stop the test — one missing door segfaults the entire runner

**Evidence:** `test_framework.h:11` — `CHECK` prints and continues. `test_dungeon7_level.cpp:831–836`: `CHECK(r1 != nullptr);` followed unconditionally by `r1->tx`. Same pattern at test_dungeon7_level.cpp:861–878 (`q`/`r2`). By contrast test_dungeon6_level.cpp:464–465 guards with `if(!qd) return;` — the convention is inconsistent.
**Problem:** The framework has no fatal-assert form, so pointer/precondition failures fall through to dereferences. Because all tests run in one process, the first such failure crashes the binary mid-run: no failure summary, all subsequent suites unreported.
**Risk:** A data regression that removes a room door manifests as a bare segfault instead of a named FAIL, and masks every other regression in the same run.

### [MINOR] Boss-def field-pinning tests are change detectors, and wound paths are only tested with exact multiples

**Evidence:** `test_boss.cpp:281–295` (`bossdef_d2_slagshell_fields`: `max_hp==70`, `end_hp==35`, attack masks), 345–360 (`bossdef_d3_coldforge_fields`), 24–29 (`boss_constants_sane`: `max_hp==90`); `d5_dims` (`w==64`) in test_dungeon5_level.cpp. Every `on_wound` call across test_boss.cpp passes exactly `def.wound_dmg`; `advance_phase_for_hp` (boss.h:174–183) can skip a phase on a large hit and clamp `hp<0`, but no test wounds with a non-multiple or oversized damage value.
**Problem:** The raw field pins restate the data table verbatim — they fail only on intentional tuning (requiring lockstep test edits per balance pass, a per-boss recurring cost) and can't catch behavioral bugs; the derived invariants (`switch_budget_holds_for_all_defs`, escalation ordering) are the valuable checks. Meanwhile genuinely risky arithmetic (multi-threshold phase skip, overkill clamp) is untested.
**Risk:** A future boss with HP not a multiple of `wound_dmg`, or an attack dealing more than one wound of damage, exercises the phase-skip/clamp path for the first time in production; each balance tweak generates false test failures that train the habit of editing tests to match.

### [MINOR] Cross-dungeon serial constants baked into content tests

**Evidence:** `test_dungeon8_level.cpp:299` — `CHECK_EQ(hc.id, 2)` with comment "D6 used id0, D7 id1"; row-number literals repeated as load-bearing values across suites (`cage_ty==18`, `d6_brazier_on_floor_row` `by==18` / solid at row 20, `d6_no_content_on_gap_or_floor_row` hardcoding rows 19/20, `d6_water_corridor_has_ceiling` rows 15/16/20).
**Problem:** Heart-container ids form an implicit global sequence enforced only by scattered per-dungeon literals; the row-18/19/20 scheme is duplicated as magic numbers in every check rather than named constants shared with the level compiler.
**Risk:** Adding a heart container to an earlier dungeon renumbers ids and breaks later dungeons' tests for no real bug; changing the room-height convention (e.g. taller rooms — D8 already deviates and had to fork the cage check) requires hand-auditing dozens of literals, with misses producing either false failures or checks that silently assert the wrong row.
