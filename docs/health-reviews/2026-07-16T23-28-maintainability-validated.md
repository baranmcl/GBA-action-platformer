# Spronk Quest Maintainability Health Review — Validated Findings

**Date:** 2026-07-16
**Scope:** Maintainability + token-efficiency for adding dungeons/bosses/enemies
**Source:** Project health review (5-dimension adversarial); synthesis at `2026-07-16T23-28-maintainability-project-health-review.md`
**Verification:** Every finding below was checked against the actual code by the runner (full reads of scene_dungeon.cpp, scene_boss.cpp, scene_hub.cpp, main.cpp, world_state.h, boss.h, gates.h, hud_math.h, dungeon1.h, dungeons.h, level_loader.cpp, level_view.cpp, save.cpp, build_level.py, build_rom.sh, host_test.sh, test/Makefile, test_death_respawn.cpp, test_framework.h, AGENTS.md, README + targeted greps). Synthesis findings 1–36 are all accounted for below.

---

## Confirmed Issues

### I1. `play_room` is an ~850-line god-function; every mechanic is smeared across 3 inline regions
**Severity:** CRITICAL — **Dimensions:** Code Quality, Architecture — **Synthesis:** #1
**Location:** `src/game/scene_dungeon.cpp:523-1376` (file total 1,420 lines)
**Evidence (verified):** 15+ entity kinds each with a spawn block (~640–845), per-frame update block (~960–1300), and death-reset block (~1240–1267); ordering enforced by comments ("MUST precede despawn_on_solid", spell-resolution ORDER comment at 1094).
**Blast radius:** Large but mechanical — extract per-entity-system helpers/structs into separate engine/game units; no behavior change. Touches the hottest file; needs the shared-controller work (I7) coordinated.
**Fix approach:** Mechanical decomposition into room-subsystem units (spawn/update/reset per entity kind) with an explicit ordered update list. No redesign.

### I2. Two parallel ~350-line boss fight loops that have already diverged; per-boss behavior accreting as branches in the shared loop
**Severity:** CRITICAL — **Dimensions:** Code Quality, Architecture, Content DX — **Synthesis:** #2
**Location:** `scene_boss.cpp:76-444` (`run_boss`) vs `scene_dungeon.cpp:167-521` (`run_room_boss`)
**Evidence (verified):** Same loop structure (settle intro, boss_say, restart_fight, telegraph/Active/Recovery, resolve_damage, crystal respawn, death/lives). Divergences confirmed: `block_player_shots` (King) vs `block_with_spell` (room bosses); hardcoded dialogue vs data-driven; Recovery-clear fix ported by hand (comment at scene_boss.cpp:339-343). D2 tuning in shared code confirmed: rock count `(b.phase==0)?3:5` (scene_dungeon.cpp:414), speed `?2:3` (417). `expose_spell_alt` sprite math (382-388) and `block_spell2` (472-473) are per-boss branches. Cross-file invariant `attack_active_frames > WARN_FRAMES` is comment-only (boss.h:101-102).
**Blast radius:** Medium-large; unification risks regressing 4 shipped fights — needs to be behavior-preserving and QA'd.
**Fix approach:** Extract one shared fight-loop engine unit parameterized by BossDef; move per-boss tuning (rock counts, projectile speeds) into BossDef fields; static_assert the telegraph/WARN invariant.

### I3. The content pipeline validates almost nothing the runtime assumes — failures are silent truncation, OOB reads, or hard-asserts on hardware
**Severity:** CRITICAL — **Dimensions:** all four of Code Quality, Architecture, Ops, Content DX — **Synthesis:** #3
**Location:** `tools/build_level.py` + `scene_dungeon.cpp` spawn loops + `level_loader.cpp` + `run_dungeon`
**Evidence (verified):**
- (a) 15 spawn loops silently clamp (`i < 8/16/24/4`, scene_dungeon.cpp:646-845); build_level.py checks only rectangularity + solid borders.
- (b) `bn::vector<TriggerInst,16>` filled by 3 loops each individually capped at 16 (scene_dungeon.cpp:823-845) → Butano hard-assert possible from content alone.
- (c) `dungeon.rooms[cur_room]` dereferenced unbounded (scene_dungeon.cpp:1392); `target_room`/`target_entrance` come from hand-typed JSON; `find_entrance` silently falls back to '@'.
- (d) No `w<=64` / `w*h<=8192` check vs `level_loader.cpp` CAP (64*128) and `level_view.cpp` COLS=64; oversized level = silent EWRAM overrun, overwide = invisible-geometry desync.
- (e) latch_id: no range check (>=24 silently aliases heart-container bits) and no cross-dungeon uniqueness check (allocations verified scattered: d6=0, d7=1, d8=2).
- (f) *(found during verification)* `build_level.py:147`: with fewer JSON `gates` entries than 'G' symbols, it silently reuses `j_gates[-1]` — a missing entry becomes a duplicated gate type.
**Blast radius:** Almost entirely in `build_level.py` + a small dungeon-manifest validation step + a few load-time BN_ASSERTs. Low risk, high leverage.
**Fix approach:** Teach the compiler every engine cap and invariant; add a whole-dungeon validation pass (room-graph targets, latch/heart uniqueness+range, dimensions, trigger-sum); assert instead of falling back.

### I4. The no-soft-lock test harness is forked six times with divergent physics; universal invariants re-copied per dungeon
**Severity:** CRITICAL — **Dimensions:** Test Quality, Content DX — **Synthesis:** #4 (+ boilerplate half of #13)
**Location:** `test/test_dungeon{1,2,3,7,8,9}_level.cpp`
**Evidence (verified by grep):** `CLIMB = 5` (D1/D2/D3), `CLIMB = 6` (D7), `CLIMB_RELIABLE=5/CLIMB_MAX=7` (D8/D9). D8's header comment documents the single-threshold model as having shipped a real soft-lock; D7 still uses it. None derived from player physics. Solid-border/two-way-door/grounding checks hand-copied per suite; M14 mandated copying to dodge ODR.
**Blast radius:** Test-only; zero production risk. Large deletion + one new shared header.
**Fix approach:** One shared `test/level_harness.h` (grid, flood-fill with the dual-threshold model, gate-state modeling, universal invariant suite over a dungeon registry); per-dungeon files shrink to dungeon-specific invariants.

### I5. Level-header regeneration is missing from the raw `make` paths that the docs point at
**Severity:** MAJOR (downgraded from agent's CRITICAL — see FP2) — **Dimensions:** Test Quality, Content DX — **Synthesis:** #5
**Location:** `Makefile`, `test/Makefile`, `AGENTS.md`, CLAUDE.md
**Evidence (verified):** `tools/host_test.sh:29-32` and `tools/build_rom.sh:19-23` both regenerate; raw `make` (CLAUDE.md's documented ROM command) and `make -C test` (AGENTS.md's documented test command) do not. AGENTS.md:6 also claims `make -C test` "runs on any machine," contradicting CLAUDE.md's fragility warning.
**Blast radius:** Small: Makefile rules + doc sync.
**Fix approach:** Add a regen (or staleness-check) step to both Makefiles; fix AGENTS.md to match CLAUDE.md.

### I6. The content-adding recipes exist only as milestone-plan archaeology
**Severity:** CRITICAL (for the token-cost goal) — **Dimensions:** Content DX — **Synthesis:** #6
**Location:** CLAUDE.md (verified: build + layers + purity only), `docs/plans/*m13*/*m14*` (600–1050 lines each)
**Evidence (verified):** No document describes: level .txt/.json format ownership, `dungeons.h` assembly, main.cpp routing, hub-door wiring, `BOSS_SYMBOL`, `boss_sprite_for`, art-generator registration, latch/heart id allocation. Every recipe must be re-derived from code + historical plans.
**Fix approach:** `docs/content-recipes.md` with the three checklists (dungeon/boss/enemy), linked from CLAUDE.md; kept short and updated as part of each content milestone's definition of done.

### I7. Scene-control/ability plumbing copy-pasted 3–4x across scenes (already drifted)
**Severity:** MAJOR — **Dimensions:** Code Quality, Architecture, Content DX — **Synthesis:** #7 — *already noted in project memory as a deferred refactor; review confirms it is wider than remembered*
**Location (verified):** `refresh_spell_icon` x4 (scene_dungeon 273/603, scene_hub 148, scene_boss 178); `set_clamped_cam` x4; ability-sync block x4; vine VFX ~40 lines x2 (scene_dungeon 1322-1364 ≡ scene_hub 228-265); crystal collect/respawn x3; HP-cap sync x3 (main.cpp 29, run_dungeon 1388, scene_hub 58); cast whitelist x4. Drift confirmed: grapple enemy-pull exists only in `play_room`.
**Fix approach:** Shared player-session controller (engine or game/common) parameterized by scene capabilities; scenes keep only what genuinely differs.

### I8. Adding a dungeon = ~8 steps across ~12 files; three steps fail silently; door digits cap at 9
**Severity:** MAJOR — **Dimensions:** Architecture, Content DX — **Synthesis:** #8
**Evidence (verified):** `main.cpp:65-73` if-chain with silent `else continue`; `scene_hub.cpp:39-49` nine hand-written disjuncts (the n>=2 pattern is formulaic); `dungeons.h` hand-assembled includes+tables; `build_level.py:236` door glyphs are single chars '1'-'9'.
**Fix approach:** Dungeon registry table in dungeons.h consumed by main.cpp + hub gating formula. The 10+ door-glyph ceiling is a design decision (D5) — likely defer.

### I9. Boss identity spread over three unlinked registries; sprite mapping is pointer-compare with a silent guardian fallback
**Severity:** MAJOR — **Dimensions:** Code Quality, Architecture, Content DX — **Synthesis:** #9
**Evidence (verified):** `boss_sprite_for` (scene_dungeon.cpp:151-155) pointer-compares `&logic::D2_DEF`/`&logic::D3_DEF`, defaults to guardian; `BOSS_SYMBOL` (build_level.py:52-56); plus sprite include + `make_placeholder_art.py` registration per boss.
**Fix approach:** Add a `BossId` enum to BossDef; single id→sprite table; unknown id = BN_ASSERT, not fallback.

### I10. No enemy type system — one struct + one flag bit in untyped params
**Severity:** MAJOR — **Dimensions:** Architecture, Content DX — **Synthesis:** #10
**Evidence (verified):** `EntitySpawn{tx,ty,param0,param1,param2}`; `(s.param2 & 1)` decode + sprite ternary (scene_dungeon.cpp:653-655); combat if-chain (1124-1134); one patroller in `src/logic/enemy.cpp`.
**Fix approach:** `EnemyType` enum + per-type table (sprite key, behavior params, immunities) in logic; build_level.py emits the enum. Do minimally now (seam only) unless new enemy types are imminent.

### I11. Death/respawn tests hand-mirror the scene frame loop and have already drifted from it
**Severity:** MAJOR — **Dimensions:** Test Quality — **Synthesis:** #11
**Evidence (verified):** `test_death_respawn.cpp:54-56` claims "the current scene sets invuln=0 on respawn (no grace)" — the scene sets `invuln = RESPAWN_IFRAMES` (=60) at scene_dungeon.cpp:1242. The second test simulates `invuln = 0` (line 124). Both tests re-implement the loop instead of calling shared code.
**Fix approach:** Extract the damage/i-frame/respawn frame-step into a pure logic function that the scene calls and the tests exercise. (Part of the I12 boundary shift.)

### I12. ~2,900 lines of scene/engine code have zero automated coverage; no CI, no ROM-build check
**Severity:** MAJOR — **Dimensions:** Test Quality, Ops — **Synthesis:** #12
**Evidence (verified):** host_test.sh compiles only `test_*.cpp + src/logic/*.cpp`; no `.github/` anywhere; scene/engine only compile under devkitARM via manual build_rom.sh.
**Fix approach:** Two-pronged: (1) move decision logic (gate fill/open ranges, damage/respawn step, magic economy, trigger latching) into logic/ where existing tests can reach it; (2) at minimum wire a scripted ROM-compile smoke check into the workflow.

### I13. Reachability coverage floor is zero for D4/D5/D6
**Severity:** MAJOR — **Dimensions:** Test Quality — **Synthesis:** #13 (D2/D3 exemption split out as design decision D2)
**Evidence (verified):** test_dungeon4 (51 lines) / test_dungeon5 (60) existence-only; D6 "verified separately on mGBA"; respawn-settle checks cover D1–D3 only.
**Fix approach:** Once I4's shared harness exists, backfilling D4–D6 is cheap; do it there.

### I14. Save system: one SRAM slot, 8-bit additive checksum, detected corruption = silent fresh start
**Severity:** MAJOR — **Dimensions:** Ops — **Synthesis:** #14 (bit-budget half corrected — see FP1; boss-persistence half moved to D1)
**Evidence (verified):** `save.cpp` single `bn::sram::write(s)` at offset 0; `checksum_v5` additive 8-bit; `main.cpp:18-19` silently starts fresh on a failed read. Saves are written frequently (every death/latch/heart/spronk).
**Fix approach:** v6: two save slots (ping-pong with a sequence counter), 16-bit checksum (or CRC), and boot-time distinction between "empty" and "corrupt but recoverable from the other slot." Scope choices in D3.

### I15. Tile-index registry is a comment; indices re-pinned by hand at 4+ sites
**Severity:** MAJOR — **Dimensions:** Code Quality, Architecture, Content DX — **Synthesis:** #15
**Evidence (verified):** gates.h:30-35 comment is the registry; level_view.cpp:31-33 ternary chain re-encodes it; scene literals (5/6/26 at 798-808, 14/15 at 819/1113, 17/18 at 826/835, WATER_BG/ICE_PLATFORM_BG at 1141); make_placeholder_art.py + build_level.py TILE dict.
**Fix approach:** `logic/tile_ids.h` constants consumed by scenes + level_view; a build_level.py cross-check (or generated Python constants) for the tooling side.

### I16. Build tooling hardcoded to this machine, including a wrong-checkout REPO path
**Severity:** MAJOR — **Dimensions:** Ops — **Synthesis:** #16
**Evidence (verified):** build_rom.sh:13-15 hardcodes DKP bash, `REPO=/c/Users/baranmcl/Code/GBA-action-platformer`, Python path — invoked from another checkout it builds *this* one. host_test.sh:13-14 hardcodes msys64/TEMP (with fallback rationale documented).
**Fix approach:** Derive `REPO` from the script's own location (like host_test.sh already does with `BASH_SOURCE`); keep machine paths as env-overridable defaults.

### I17. Documentation drift: README 8 milestones stale; AGENTS.md contradicts CLAUDE.md
**Severity:** MAJOR (for an LLM-driven repo) — **Dimensions:** Content DX — **Synthesis:** #17
**Evidence (verified):** README:6 says "Milestone 6"; AGENTS.md:6 recommends `make -C test` on any machine; `level_data.h` DoorSpawn comment says 1..8 while door 9 ships.
**Fix approach:** Sync README/AGENTS.md; fold the recipe doc (I6) into the same pass.

### I18–I36 (MINOR, all verified as claimed unless noted)
- **I18. Misleading names** — `logic/dungeon1.h` holds the generic `try_free_spronk` (verified); `GateType::Water` vs `TileKind::Water`; `EntitySpawn.param0/1/2`. *(Syn #18)*
- **I19. Vestigial `boss_defeated` flag** — scene_dungeon.cpp:564-568; dead the moment it's read (verified). *(Syn #19)*
- **I20. King ignores `phases[].attacks`** — `KING_ATTACK_CYCLE` hardcoded (scene_boss.cpp:147-151, comment admits it "reproduces" the mask); editing KING_PHASES attack masks does nothing. *(Syn #20)*
- **I21. Tuning literals duplicated** — damage 20/i-frames 45 (8+ sites), magic refill 25, crystal threshold `<10` hardcoding SpellCast::cost (scene_boss.cpp:410 comment admits it). *(Syn #21)*
- **I22. Test-only logic helpers** — `frost.h`/`fire_effect.h` used only by tests; scene reimplements inline. Note: the M4 plan *deliberately* left `fire_clears_gate` unused ("harmless, keeps its passing test") — deliberate then, misleading now. *(Syn #22)*
- **I23. Generated headers emit dummy one-element arrays + 40-field positional aggregate** — verified in build_level.py `emit_array`. *(Syn #23)*
- **I24. Heavy plates lack `latch_id` persistence** — comment-documented gap (scene_dungeon.cpp:1021-1022). *(Syn #24)*
- **I25. Brazier hitbox pins rows 14–19 while its sprite floor-scans** — scene_dungeon.cpp:817-818. *(Syn #25)*
- **I26. Collision grid is a hidden global** — level_loader.cpp `s_grid/s_w/s_h`; one-level-at-a-time unenforced. *(Syn #26)*
- **I27. Cracked-floor run-breaking is an O(n³) scan** — scene_dungeon.cpp:986-1010; bounded but intent-obscuring. *(Syn #27)*
- **I28. Changelog-narrative comments saturate the hottest file** — verified throughout scene_dungeon.cpp; belongs in docs/pitfalls with one-line invariants in code. *(Syn #28)*
- **I29. Test `CHECK` is non-fatal; failed pointer checks fall through to dereference** — test_framework.h:11 verified; no REQUIRE form exists. *(Syn #29)*
- **I30. Boss-def field-pin tests are change detectors; wound paths untested with non-multiple/overkill damage** — pins verified; note the code itself clamps hp>=0 and band-walks phases correctly (boss.h:184-192), so this is purely a test gap + maintenance tax. *(Syn #30)*
- **I31. Cross-dungeon serial constants baked into tests** — heart-id sequence + row literals verified. *(Syn #31)*
- **I32. HUD health bar saturates at 16 pips = 160 HP; shipped max is 175** — hud_math.h verified; stale "fits 150-HP cap" comment. *(Syn #32)*
- **I33. Ability grants not persisted at grant time** — shrine loop (scene_dungeon.cpp:1271-1277) has no write_world; hearts/spronks/latches do. Quit+power-cycle un-earns an ability (recoverable; shrine respawns). *(Syn #33)*
- **I34. Room sprite budget unaccounted** — caps admit 128+ sprites from platforms alone; no pipeline check; Butano hard-asserts. Fold into I3's cap validation (sum sprite cost per room). *(Syn #34)*
- **I35. No debug dungeon/ability selector, no CPU meter** — verified no `cpu_usage` use anywhere; *already tracked in project memory* (dungeon-progression-debug-selector). *(Syn #35)*
- **I36. `run_room_boss` hardcodes arena authoring constraints** — floor at h-2, wall pacing bounds, `magic_crystals[0]` only (scene_dungeon.cpp:200-214, 244-245). Document in the recipe (I6) + validate in the pipeline (I3). *(Syn #36)*

---

## Design Decisions Requiring User Input

### D1. Should room-boss defeats persist (no re-fight on re-entry)?
**Flagged by:** Architecture. **Current behavior is deliberate** (comment at scene_dungeon.cpp:561-563: boss not persisted, re-entering re-fights).
**Why it needs a decision:** Fine for the current linear puzzle→boss→spronk layout; actively hostile to the planned D1–D5 multi-room retrofits with backtracking (heart containers behind arena rooms). Persisting defeats costs save bits (plenty free — see FP1) and a small save change.
**Options:** (a) keep re-fights (Zelda-boss convention; free), (b) persist defeat per dungeon-boss (1 bit each; needs save v6 anyway if D3 chosen).
**Recommendation:** (b), piggybacked on the D3 save work — but only if dungeon retrofits with backtracking through arenas are still the plan.

### D2. D2/D3 room-0 puzzle reachability: accept the documented exemption, or model puzzle gates in the shared harness?
**Flagged by:** Test Quality. The exemption is deliberate and documented in the tests ("guaranteed by puzzle design, not free-traversal flood-fill").
**Options:** (a) accept (cheap; relies on emulator QA for those rooms), (b) extend the shared harness (I4) with brazier/plate/button gate-state modeling like D7/D8 already do, and backfill D2/D3.
**Recommendation:** (b) — D7/D8 prove the pattern works, and the shared harness makes it a one-time cost that all future puzzle rooms inherit.

### D3. Save robustness scope for v6
**Flagged by:** Ops.
**Options:** (a) minimal: 16-bit checksum + persist-on-grant fix (I33) only; (b) standard: (a) + dual-slot ping-pong write so torn writes/corruption recover from the previous save; (c) full: (b) + a visible "save restored/corrupt" notice.
**Recommendation:** (b). Dual-slot is the real protection; a notice screen is polish that can wait.

### D4. Boss-loop unification scope
**Options:** (a) unify `run_boss` (King) and `run_room_boss` into one engine fight loop now; (b) freeze the King as-is and only refactor `run_room_boss` to be data-clean for D4–D8; (c) defer all of it.
**Recommendation:** (a) if the King will ever be touched again (it will — it's the finale); the Recovery-clear incident shows double-fix cost is already being paid. But (b) is defensible if minimizing risk to the shipped finale matters more.

### D5. The 9-dungeon door-glyph ceiling
**Options:** (a) defer (project memory says the goal is richer existing dungeons, not more dungeons); (b) re-key door glyphs now (e.g. letters or a JSON-side dungeon id).
**Recommendation:** (a) defer; record in the plan's deferred appendix.

---

## False Positives / Corrected Claims

### FP1. "Global save bit-budgets are nearly exhausted" (Architecture agent, part of its save finding)
**Why invalid:** Verified 3 of 24 shortcut-latch bits in use (latch ids 0/1/2) and 3 of 8 heart bits. Not a near-term constraint. The *shape* concerns stand as design notes (uint16 spronks assumes ≤16 dungeons, finale gate hardcodes `spronk_count==8`), and are folded into D1/D3 planning rather than treated as an active defect.

### FP2. "Generated level headers are not wired into any build" (Content-DX agent, rated CRITICAL)
**Why corrected:** `tools/build_rom.sh:19-23` regenerates all level headers before every ROM build. The residual (real) gap is the raw `make` / `make -C test` paths and the docs that point at them — kept as I5 at MAJOR.

---

## Known / Already Tracked

### K1. Shared player-ability controller (== I7)
**Where tracked:** project memory `share-player-ability-controller`. The review confirms it and shows it is ~2x wider than the memory records; included in the plan as I7.

### K2. Debug dungeon/progression selector (== I35)
**Where tracked:** project memory `dungeon-progression-debug-selector`. Absence is now also a QA-cost finding (no per-change way to reach late dungeons).

---

## Completeness check
Synthesis findings #1–#36 map to: I1–I17 (majors/criticals), I18–I36 (minors), with #13 split (I13 + D2), #14 split (I14 + D1 + FP1), #5 corrected (I5 + FP2). Nothing dropped.

## Reviewer notes (patterns observed)
- False-positive rate was remarkably low (~2 partial corrections out of ~50 raw findings) — the codebase's comment discipline made claims easy to verify, and most "problems" are structural growth patterns rather than bugs.
- The dominant cross-cutting theme is **silent failure as default** (fallbacks, clamps, skipped chain-edits) — one validation pass in the level compiler neutralizes a disproportionate share of the risk.
- Several "defects" turned out to be *documented deliberate deferrals* (boss re-fight, fire_effect.h leftover, D2/D3 test exemption, heavy-plate latch) — the milestone process records intent well but never revisits it; the fix plan should convert these into explicit decisions.
