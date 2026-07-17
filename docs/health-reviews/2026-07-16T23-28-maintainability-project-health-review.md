# Project Health Review — Spronk Quest
**Date:** 2026-07-16 23:28
**Scope:** Maintainability + token-efficiency for adding dungeons/bosses/enemies (full review, 5 dimensions)

Individual agent reports (raw findings, full evidence):
- `2026-07-16T23-28-maintainability-agent-1-code-quality.md`
- `2026-07-16T23-28-maintainability-agent-2-architecture.md`
- `2026-07-16T23-28-maintainability-agent-3-test-quality.md`
- `2026-07-16T23-28-maintainability-agent-4-ops-readiness.md`
- `2026-07-16T23-28-maintainability-agent-5-content-dx.md`

Findings below are deduplicated across agents; **Dimensions** lists which agents flagged each. Severity is the synthesized rating (cross-dimensional agreement weighted).

---

## Critical Findings

### 1. `play_room` is an ~850-line god-function; every new mechanic grows it inline
**Dimensions:** Code Quality, Architecture
**Evidence:** `src/game/scene_dungeon.cpp:523-1376` — 15 entity kinds, each smeared across spawn (~640–845), per-frame update (~960–1300), and death-reset (~1240–1267) regions with comment-enforced ordering ("MUST precede despawn_on_solid"). File total: 1420 lines, the most token-expensive file in the repo and the one every content task must read.
**Problem:** "Kind of thing in a room" is not a concept; each kind is a struct plus 3–5 hand-placed code blocks. Adding one mechanic means understanding the whole function.
**Risk:** Per-milestone growth ~50–150 lines; ordering bugs multiply; unverifiable without full regression QA of 9 dungeons.
**Suggested approach:** Split into per-entity-system units with an explicit update order; keep behavior identical (mechanical extraction, no redesign).

### 2. Two parallel ~350-line boss fight loops that have already diverged; BossDef accretes per-boss flags with matching scene branches
**Dimensions:** Code Quality, Architecture, Content DX
**Evidence:** `run_boss` (`scene_boss.cpp:76-444`) vs `run_room_boss` (`scene_dungeon.cpp:167-521`) — same loop, forked deliberately; the Recovery-clear fix had to be applied to both. D2 tuning is baked into the shared loop (`(b.phase==0)?3:5` rocks, `?2:3` projectile speed); each new BossDef field (locomotion, block_spell, expose_spell_alt, block_spell2) added a hand-written branch. Cross-file invariant (`attack_active_frames > WARN_FRAMES`) enforced only by comments.
**Problem:** Every boss mechanic must be implemented (or deliberately skipped) twice; "data-driven" is drifting back to code-driven with flag soup.
**Risk:** Five more bosses (D4–D8) at this trajectory → unprincipled BossDef union + a second god-function; fixes keep landing in one loop only.
**Suggested approach:** Unify the two loops around one shared fight-loop helper parameterized by BossDef; hoist per-boss tuning (rock counts, speeds) into BossDef fields.

### 3. The content pipeline validates almost nothing the runtime assumes — failures are silent truncation, OOB reads, or hard-asserts on hardware
**Dimensions:** Code Quality, Architecture, Ops Readiness, Content DX (all four)
**Evidence:** (a) 15 spawn loops silently clamp (`i < 8/16/24/4`) with no build-time cap check; a dropped gate/brazier is a sequence-break or soft-lock. (b) `dungeon.rooms[target_room]` dereferenced unvalidated — a JSON typo is an OOB pointer read on GBA; bad `target_entrance` silently falls back to '@'. (c) `build_level.py` never checks `w<=64 / w*h<=8192` against `level_loader.cpp`'s fixed EWRAM buffer (3 shipped rooms already at w=64 exactly) — oversized level = silent memory corruption; overwide = invisible-geometry desync (view COLS=64). (d) Shared `bn::vector<TriggerInst,16>` filled by three loops each individually capped at 16 → Butano hard-assert at room load. (e) Latch ids (0–23) and heart ids (0–7) hand-allocated across scattered sidecar JSONs with no uniqueness/range check — collisions silently corrupt persisted save state.
**Problem:** The pipeline's whole value is catching content errors at build time; it checks rectangularity and borders but none of the invariants that actually bite.
**Risk:** At 12 dungeons/30+ rooms, one content edit produces a hardware-only, hard-to-bisect failure — exactly the class the three-layer architecture was built to prevent.
**Suggested approach:** Teach `build_level.py` (plus a small dungeon-manifest step) every engine cap and cross-room/cross-dungeon invariant; add load-time `BN_ASSERT`s as backstop.

### 4. The no-soft-lock test harness is forked six times with divergent physics models; per-dungeon suites are 200–900 lines of copy-paste
**Dimensions:** Test Quality, Content DX
**Evidence:** Six private `Grid`/`reachable` flood-fills (D1/D2/D3/D7/D8/D9) with diverging jump constants (`CLIMB=5` vs `6` vs dual `5/7`); D8's own comment documents the single-threshold model as unsound, yet D7 still uses it. Universal invariants (solid border, door two-way, hub-door grounding) re-authored per dungeon; D4/D5 have only existence checks, D6 has no flood-fill at all; D2/D3 room-0 (the puzzle rooms) explicitly excluded from reachability. None of the constants derive from `player.cpp` physics.
**Problem:** The most valuable test asset in the repo is not shared; each new dungeon pays ~300–900 test lines and inherits a coin-flip physics model.
**Risk:** A jump retune silently invalidates all six models (green tests, real soft-locks); new dungeons silently skip universal invariants.
**Suggested approach:** Extract one shared reachability/invariant harness header parameterized by dungeon + ability set, with jump constants derived from (or pinned against) player physics; run universal invariants over a dungeon registry loop.

### 5. Level-data regeneration is wired into only one of three build paths — tests and ROM can silently disagree with source `.txt`
**Dimensions:** Test Quality, Content DX
**Evidence:** Only `tools/host_test.sh` regenerates `include/game/levels/*.h` from `tools/levels/*.txt`; ROM `make` and `make -C test` compile committed (possibly stale) headers. AGENTS.md recommends the path that skips regeneration; CLAUDE.md recommends the other.
**Problem:** Edit a level, run the wrong entry point, and you test/ship data that doesn't match the source of truth — with zero warning.
**Risk:** "Tests green, ROM ships old layout" or vice versa; hours-of-QA-class silent failure.
**Suggested approach:** Make regeneration a build step in both Makefiles (or a staleness check that fails loudly), and align AGENTS.md/CLAUDE.md.

### 6. The content-adding recipe exists only as milestone-plan archaeology — no template, checklist, or doc
**Dimensions:** Content DX
**Evidence:** CLAUDE.md covers build + three-layer rule only; the actual add-a-dungeon/boss/enemy recipes live in 600–1050-line historical plan files (M13/M14) and in the code itself.
**Problem:** Every content task forces re-derivation of a multi-file recipe (~8–12 files, several silent-failure steps) from a 1420-line scene file plus plan archaeology. This is the single largest recurring token sink given the stated goal.
**Risk:** Each new boss costs a fresh ~600–1000-line plan + full-file reads + debug loops on silently-missed steps.
**Suggested approach:** Write `docs/content-recipes.md` (or CLAUDE.md-linked checklists) for the three recipes, maintained as part of Definition of Done; shrink the recipes themselves via findings 2, 7, 8.

---

## Major Findings

### 7. Scene-control/ability plumbing is copy-pasted 3–4x across scenes with no shared controller
**Dimensions:** Code Quality, Architecture, Content DX
**Evidence:** `refresh_spell_icon` x4, `set_clamped_cam` x4, ability-sync block x4, vine VFX ~40 lines byte-for-byte x2, crystal collect/respawn x3, heart-container HP sync x3, cast whitelists x3+. Already tracked in project memory as a deferred refactor — confirmed and wider than suspected. Variants have drifted: grapple-pull-enemy exists only in `play_room`.
**Risk:** Any new spell/ability/input change is a 3–4-site edit; misses are silent per-scene dead features (a class already hit in M9–M10).
**Suggested approach:** Extract a shared player-session controller in `src/engine/` (or `src/game/common`), parameterized by scene capabilities (blocks? enemies? anchors-only?).

### 8. Adding a dungeon = ~8 steps across ~12 files, three of them silent-failure; door digits hard-cap at 9 dungeons
**Dimensions:** Architecture, Content DX
**Evidence:** txt+json authoring → manual header regen → `dungeons.h` hand-assembly → `main.cpp` if-chain (silent `else continue`) → `scene_hub.cpp` `door_enterable` 9 hand-written disjuncts → hub.txt door glyph (single chars `1-9` only) → global latch/heart id allocation → copied test suite.
**Risk:** Forgotten steps are runtime-silent (door routes nowhere / stays locked); dungeon 10 is impossible without changing the level compiler's symbol scheme.
**Suggested approach:** Single dungeon registry table (data) consumed by main.cpp/hub gating; formula-ize `door_enterable`; widen or re-key the door symbol scheme when needed.

### 9. Boss identity is spread over three unlinked registries, one keyed by pointer comparison with a silent wrong-sprite fallback
**Dimensions:** Code Quality, Architecture, Content DX
**Evidence:** `BossDef` symbol in boss.h + `BOSS_SYMBOL` in build_level.py + `boss_sprite_for` pointer-compare (`def == &logic::D2_DEF`, default: D1 guardian sprite) + sprite include + art-generator registration.
**Risk:** 5 more bosses × 3 registries = 15 sync points; forgetting the sprite mapping ships the wrong art with no diagnostic.
**Suggested approach:** Give `BossDef` an id enum; one game-layer table maps id → sprite; make the fallback an assert.

### 10. No enemy type system — a new enemy means new magic param bits and edits in 6+ files
**Dimensions:** Architecture, Content DX
**Evidence:** One `Enemy` struct + `bool fire_immune` encoded as `param2` bit0 of untyped `EntitySpawn{param0,param1,param2}`; sprite ternary; combat if-chain; no enforcement that build_level.py bit meanings match scene decode.
**Risk:** First flyer/shooter enemy re-derives an ad-hoc encoding; bit mismatch decodes as the wrong enemy silently.
**Suggested approach:** Enemy type enum + per-type table (sprite key, behavior, immunities) in logic; build_level.py writes the enum.

### 11. Death/respawn tests simulate a hand-copied frame loop that has already drifted from the scene
**Dimensions:** Test Quality
**Evidence:** `test_death_respawn.cpp` mirrors "the per-frame ORDER in scene_dungeon.cpp" by line-number comments; one test bakes the pre-fix behavior (`invuln=0`) while the scene sets `RESPAWN_IFRAMES=60`; the tests define their own copies of the constants they assert.
**Risk:** The death-loop soft-lock these tests exist to prevent can be reintroduced with a green suite.
**Suggested approach:** Extract the damage/respawn frame-step into a pure logic function the scene calls and the tests exercise directly.

### 12. ~2,900 lines of scene/engine code have zero automated coverage; no CI, no ROM-build check
**Dimensions:** Test Quality, Ops Readiness
**Evidence:** scene_dungeon (1420) + scene_boss (445) + scene_hub (275) + boss_attacks.cpp (216) untested; no CI config; host tests compile only logic; green "459/459" gates commits while the shipping artifact is exercised ad hoc.
**Risk:** Gate-fill off-by-one, transition state leaks, boss projectile spawn bugs all ship untested; ROM compile breakage lands silently.
**Suggested approach:** Move decision logic (damage resolution, gate open/fill, magic economy, trigger latching) into logic/; add at minimum a scripted ROM-build smoke check.

### 13. Reachability coverage floor is zero for shipped dungeons D4/D5/D6, and D2/D3 puzzle rooms are explicitly exempt
**Dimensions:** Test Quality
**Evidence:** test_dungeon4 (51 lines) / test_dungeon5 (60) are existence checks; D6 has no flood-fill ("verified separately on mGBA"); D2/D3 room-0 shrine/boss-door reachability is "guaranteed by puzzle design," i.e. untested; respawn-settle checks cover D1–D3 only.
**Risk:** Layout edits strand the player with a green suite, precisely on the dungeons slated for multi-room retrofits.
**Suggested approach:** Once finding 4's shared harness exists, backfill D4–D6 and model puzzle-gate states like D7/D8 already do.

### 14. Save system: single SRAM slot, 8-bit additive checksum, corruption = silent full wipe; bit budgets shaped around "exactly 8 dungeons"
**Dimensions:** Ops Readiness, Architecture
**Evidence:** One `bn::sram::write` at offset 0; checksum passes compensating corruption; failed read → fresh `World{}` with no message. `latches` uint32_t = 24 shortcut bits + 8 heart bits game-wide; `spronk_count==8` finale gating; room-boss defeat deliberately not persisted (re-entering re-fights); ability grants not written until next incidental save.
**Risk:** Torn write on a real cart = silent total progress loss; planned dungeon retrofits collide with the bit budget and the re-fight behavior; a sixth ad-hoc migration branch.
**Suggested approach:** Double-buffered save slots + 16-bit checksum (v6 migration); widen/restructure progress bits at the same time; persist ability grants and (decision) boss defeats.

### 15. The tile-index "registry" is a comment; indices re-pinned by hand at 4+ use sites
**Dimensions:** Code Quality, Architecture, Content DX
**Evidence:** Canonical map = comment in `gates.h:30-35`; literals in scene_dungeon (5/6/26, 14/15, 17/18, WATER_BG=16, ICE_PLATFORM_BG=19), level_view.cpp ternary chain, build_level.py TILE dict, make_placeholder_art.py.
**Risk:** The eventual real-art pass re-derives every pinned index; a miss renders wrong tiles with no error.
**Suggested approach:** One `logic/tile_ids.h` constants header consumed by scenes/level_view; generate or cross-check the Python side against it.

### 16. Build tooling is hardcoded to this machine, including a wrong-checkout REPO path
**Dimensions:** Ops Readiness
**Evidence:** `build_rom.sh` hardcodes DKP bash, Python, and `REPO=/c/Users/baranmcl/Code/GBA-action-platformer` (a second checkout silently builds the original clone); host_test.sh hardcodes msys64 + TEMP paths.
**Risk:** False verification during QA (ROM built from the wrong tree); zero portability to CI/second machine.
**Suggested approach:** Derive paths from script location/env with the current values as fallback defaults.

### 17. Documentation drift: README 8 milestones stale; AGENTS.md contradicts CLAUDE.md on the test command
**Dimensions:** Content DX
**Evidence:** README says "Milestone 6"; AGENTS.md says `make -C test` (the stale-data path CLAUDE.md warns against); `DoorSpawn` comment says "dungeon 1..8" while door 9 ships.
**Risk:** Agents start every session with a wrong model and possibly the wrong test command — recurring token waste.
**Suggested approach:** Sync README/AGENTS.md/CLAUDE.md; fold into finding 6's recipe doc.

---

## Minor Findings

### 18. Misleading names: `logic/dungeon1.h` holds the generic spronk helper; `GateType::Water` vs `TileKind::Water`; `EntitySpawn.param0/1/2`
**Dimensions:** Code Quality, Architecture, Content DX

### 19. Vestigial `boss_defeated` flag in `play_room` (dead the moment it's read)
**Dimensions:** Code Quality, Architecture — scene_dungeon.cpp:564-568

### 20. The King ignores `BossDef.phases[].attacks` — the framework's flagship data field is decorative for its original consumer
**Dimensions:** Architecture — scene_boss.cpp `KING_ATTACK_CYCLE` hardcoded

### 21. Tuning constants duplicated as bare literals (contact damage 20 / i-frames 45 x8+, magic refill 25 x5, crystal threshold hardcodes SpellCast::cost x2)
**Dimensions:** Code Quality

### 22. Test-only logic helpers: `frost.h` / `fire_effect.h` verified by tests but never called by the game (scene reimplements inline)
**Dimensions:** Code Quality

### 23. Generated level headers emit dummy one-element arrays for empty lists and ~40-field positional aggregates — hostile to grep and hand-edit
**Dimensions:** Code Quality

### 24. Heavy plates are the one gate type without `latch_id` persistence (comment-documented gap)
**Dimensions:** Code Quality

### 25. Brazier hitbox hardcodes rows 14–19 while its sprite floor-scans — breaks in nonstandard rooms
**Dimensions:** Code Quality

### 26. Level collision grid is a hidden global (`s_grid`) mutated by free functions; only one level can exist, unenforced
**Dimensions:** Code Quality

### 27. Cracked-floor run-breaking is a convoluted O(n³) scan (bounded, but intent-obscuring)
**Dimensions:** Code Quality

### 28. Changelog-narrative comments inflate scene_dungeon.cpp (~1/3 of the hottest file is milestone history; belongs in docs/pitfalls/)
**Dimensions:** Code Quality

### 29. Test `CHECK` is non-fatal; failed pointer checks fall through to dereference → one data regression segfaults the whole runner
**Dimensions:** Test Quality — test_framework.h:11

### 30. Boss-def field-pin tests are change detectors; wound/phase math never tested with non-multiple or overkill damage
**Dimensions:** Test Quality

### 31. Cross-dungeon serial constants baked into tests (heart id sequence, row 18/19/20 literals)
**Dimensions:** Test Quality

### 32. HUD health bar clamps at 16 pips = 160 HP; shipped max is already 175
**Dimensions:** Ops Readiness — hud_math.h

### 33. Ability grants not persisted until a later incidental save; quit+power-cycle un-earns the ability
**Dimensions:** Ops Readiness

### 34. Room sprite budget unaccounted; a maximal legal room can exceed 128 OAM and hard-assert at load
**Dimensions:** Ops Readiness

### 35. No debug dungeon/ability selector, no CPU meter — QA cost of late-game content is a full playthrough (already tracked as deferred in project memory)
**Dimensions:** Ops Readiness

### 36. `run_room_boss` hardcodes arena geometry (floor at h-2, 1-tile walls, single crystal) as unwritten authoring constraints
**Dimensions:** Content DX

---

## Cross-Cutting Themes

1. **Silent failure is the default failure mode.** Caps truncate, fallbacks substitute (guardian sprite, '@' spawn), chains skip (`else continue`), ids collide — nothing asserts. Findings 3, 8, 9, 10, 16, 33, 34 share this root cause: the pipeline and runtime have no contract enforcement.
2. **Per-content marginal cost is rising, not falling.** The boss framework (2), the dungeon recipe (8), enemy encoding (10), test suites (4, 13) all show cost-per-addition growing with each milestone — the opposite of the project's goal. Root cause: content lands as code branches instead of data rows, and the "recipe" is undocumented (6).
3. **Tests mirror instead of exercise.** Findings 4, 11, 12, 13: the test suite re-implements scene behavior (physics models, frame loops, geometry constants) rather than calling it, because the decision logic lives in bn::-coupled files the host can't compile. The three-layer rule is sound; the boundary is drawn too low.
4. **Duplication with drift.** Boss loops (2), scene controllers (7), tile registries (15), doc entry points (17): every copy pair has already diverged at least once, and each divergence was discovered by a bug.
