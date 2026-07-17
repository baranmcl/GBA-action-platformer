# Agent 5: Content-Authoring DX (API Design & Developer Experience)
**Date:** 2026-07-16 23:28
**Scope:** Maintainability + token-efficiency for adding dungeons/bosses/enemies

### [CRITICAL] The content-adding recipe exists only as milestone-plan archaeology — no template, checklist, or doc an agent can follow

**Evidence:** `CLAUDE.md` documents only build commands, the three-layer rule, and the purity guard — nothing about `tools/build_level.py`, the `.txt`+`.json` sidecar format, `dungeons.h` assembly, hub door wiring, `BOSS_SYMBOL`, or `make_placeholder_art.py`. The actual recipes live in per-milestone plan files: `docs/plans/2026-06-26-spronk-quest-m13-d2-boss-plan.md` (1047 lines) and `docs/plans/2026-07-04-spronk-quest-m14-d3-boss-plan.md` (662 lines) each re-narrate the whole boss recipe for one boss. The only compact recipe statement is a "Files: Modified:" list buried at m14 plan line 104.
**Problem:** D4–D8 bosses are planned (5 more repetitions). Every future dungeon/boss/enemy task forces an agent to reconstruct the multi-file recipe by reading a 1400-line scene file plus 600–1000-line historical plans, then re-derive the same "remember to also" list (sprite registry, BOSS_SYMBOL, dungeons.h, art generator registration, hub wiring). The owner's stated goal is cheap-in-tokens content addition; this is the single largest token sink in the repo.
**Risk:** Each new boss costs a fresh ~600–1000-line plan document plus full-file reads of scene_dungeon.cpp; missed steps (below) fail silently, forcing debug loops on top.

### [CRITICAL] Generated level headers are not wired into any build — three test/build paths see different data

**Evidence:** `Makefile` (ROM) has no `build_level.py` rule (only `include $(LIBBUTANOABS)/butano.mak`); `test/Makefile:1-14` compiles `test_*.cpp` without regenerating headers; only `tools/host_test.sh:29-32` loops `tools/levels/*.txt` through `build_level.py`. Generated `include/game/levels/*.h` are committed. `docs/plans/2026-07-04...m14-d3-boss-plan.md:17` admits "Art is a MANUAL step… generated include/game/levels/*.h are committed."
**Problem:** Editing `dungeonN_roomK.txt` and running `make` (ROM) or `make -C test` builds against the stale committed header with zero warning. `AGENTS.md` ("Host tests: `make -C test`") actively directs agents onto the path that skips regeneration, while CLAUDE.md directs them to `host_test.sh` — the two instruction files disagree, and the difference is exactly the regeneration step.
**Risk:** A contributor/agent edits ASCII level art, tests pass (against old data), ROM ships old layout; or worse, tests pass against regenerated data locally but the ROM is built from a header nobody re-committed. Silent, hours-of-QA-class failure.

### [CRITICAL] Adding a dungeon requires extending 4 unlinked id-keyed extension points, all failing silently at runtime; the door-symbol scheme hard-caps at 9 dungeons

**Evidence:** Recipe trace for "add dungeon N": (1) `tools/levels/dungeonN_room{0,1,2}.txt` + `.json` (6 files); (2) regenerate headers (manual, see above); (3) `include/game/levels/dungeons.h:3-25` add 3 `#include`s + `DUNGEONN_ROOMS[]` + `DUNGEONN_DUNGEON`; (4) `src/main.cpp:65-72` extend the `if(n == 1)…else if(n == 8)` chain; (5) `src/game/scene_hub.cpp:39-49` extend the `door_enterable` chain (`n == 9 && spronk_count(w) == 8` finale gate also hardcoded); (6) `tools/levels/hub.txt` add a door glyph — `build_level.py` docstring: "`1-9`=dungeon door", `CONTENT` set contains only single chars, and `logic::DoorSpawn` comment `level_data.h:9` says "dungeon 1..8"; (7) allocate globally-unique latch/heart ids (see separate finding); (8) write `test/test_dungeonN_level.cpp` (copy-paste harness, see separate finding).
**Problem:** Steps 4–6 are three separate switch-over-dungeon-id sites plus a data file, with no shared table. Forgetting main.cpp routing hits `else continue;` (main.cpp:73) — the door enters and silently returns to the hub. Forgetting `door_enterable` renders the door locked forever. Neither fails at compile or test time. Dungeon 10+ is impossible without modifying the level compiler's one-character symbol scheme.
**Risk:** Every new dungeon repeats an 8-step, ~12-file recipe where the three most forgettable steps produce runtime-silent misbehavior.

### [MAJOR] Room-door `target_room`/`target_entrance` indices are unvalidated — a typo is out-of-bounds UB at runtime

**Evidence:** `RoomDoorSpawn { tx, ty, target_room, target_entrance }` (`include/logic/level_data.h:16`) holds raw ints authored per-room in the JSON sidecar (`tools/levels/dungeon3_room1.json`: `"room_doors":[{"target_room":0,...},{"target_room":2,...}]`). `run_dungeon` dereferences `*dungeon.rooms[cur_room]` (`src/game/scene_dungeon.cpp:1392`) with no bounds check. `build_level.py` cannot validate the index because the room count is assembled later, by hand, in `dungeons.h`; nothing else validates it either (only per-dungeon hand-written tests, if the author remembers).
**Problem:** The cross-room graph is split between per-room JSON (indices) and `dungeons.h` (the array they index), with no compile-time or generator-time consistency check. Entrance ids are similarly unchecked — a bad `target_entrance` silently falls back to the room's default spawn (`room_graph.h:11-15`), masking the authoring error.
**Risk:** Off-by-one in a room count or door target = OOB pointer read on GBA (garbage room / crash) or a door that silently teleports to the wrong spawn; discovered only in emulator QA.

### [MAJOR] New-boss recipe: 7 files across all three layers plus two Python scripts, with a silent-fallback sprite registry

**Evidence:** M14's own file list (plan line 104) for one boss: `include/logic/boss.h` (D3_DEF tables, boss.h:118-133), `test/test_boss.cpp`, `include/engine/boss_attacks.h`, `src/game/scene_dungeon.cpp` (sprite include line 26, `boss_sprite_for` lines 151-155, frame logic 382-388, block2 wiring 472-474), `tools/make_placeholder_art.py` (`draw_coldforge_frame`+`gen_coldforge`+`__main__` registration, lines 625/700/750), `tools/build_level.py` (`BOSS_SYMBOL` map, line ~57), `include/game/levels/dungeons.h`, level txt/json, plus `test/test_d1_boss_respawn.cpp` and `test/test_dungeonN_level.cpp`. `boss_sprite_for` pointer-compares defs and **defaults to the D1 guardian sprite** for any unregistered def (scene_dungeon.cpp:154).
**Problem:** Three separate name registries must stay in sync per boss: the C++ def symbol (`D3_DEF`), the level-compiler key (`BOSS_SYMBOL['d3']`), and the sprite mapping (`boss_sprite_for`). Only the second fails loudly (generator error). Forgetting the sprite mapping ships a D4 boss wearing D1's art with no diagnostic; forgetting the art-generator registration ships without graphics assets until the Butano build fails on a missing `bn_sprite_items_*` header — a confusing, far-from-cause error.
**Risk:** Five more bosses × three registries = fifteen sync points; one of them fails silently.

### [MAJOR] "Data-driven" bosses are accreting per-boss special cases inside `run_room_boss`

**Evidence:** `src/game/scene_dungeon.cpp`: rockfall rock count hardcoded `(b.phase == 0) ? 3 : 5` (line 414) — D2 tuning baked into shared scene code; projectile speed `(b.phase == 0) ? 2 : 3` with a D1-comparison comment (line 417); `elem_base` 4-frame logic special-cased on `expose_spell_alt != None` (lines 382-388); attack rotation `ORDER[4]` hardcodes exactly the 4 existing attack bits (lines 261-262); `TelegraphCue::show` takes exactly three sprite items (aimed/spiral/fan) with rockfall handled by a separate code path (boss_attacks.h:204-215, scene_dungeon.cpp:402-404). Cross-file numeric invariant documented only in comments: `attack_active_frames (30) MUST exceed RockfallEmitter::WARN_FRAMES (26)` (boss.h:101-102, boss_attacks.h:171-173) — nothing asserts it.
**Problem:** `BossDef` claims "a boss is fully described by data" (boss.h:42), but each shipped boss (M13 pacing+rockfall, M14 shift+frames) added if-branches to the shared fight loop keyed to def fields or phase indices. Any new boss with a new mechanic (D4–D8) must thread another special case through a 350-line function, and inherits D2's rock counts / speeds if it reuses ROCKFALL. A new attack bit requires touching boss.h, ORDER[], spawn_attack, TelegraphCue, and possibly scene_boss.cpp — five places for one attack.
**Risk:** run_room_boss grows monotonically; per-boss tuning collides (first boss to want 4 rocks in P1 must refactor or fork); the telegraph/active-frames invariant breaks silently (rocks land after the Active window and never drop).

### [MAJOR] No enemy type system — a "new enemy" means new magic flag bits and edits to 6+ files

**Evidence:** The only enemy is `logic::Enemy` with `bool fire_immune` (`include/logic/enemy.h:11`); the spawn encodes it as `param2` bit0 of the untyped `EntitySpawn { tx, ty, param0, param1, param2 }` (`level_data.h:7`, comment "param2 flags (bit0 = fire_immune)"). Scene decoding: `(s.param2 & 1) != 0` and a two-way sprite ternary `fire_enemy` vs `enemy` (`scene_dungeon.cpp:653-655`); combat semantics as an if-chain (lines 1124-1134: pound-crush / bolt / fire-with-immunity / contact). Behavior lives in `src/logic/enemy.cpp` (one patroller), sidecar schema in `build_level.py` (`"enemies":[{"patrol":[l,r],"fire_immune":false}]`), art in `make_placeholder_art.py` + `graphics/enemy.*`/`fire_enemy.*`.
**Problem:** There is no enemy-type enum, no per-type table (sprite, HP, behavior, immunities). Adding e.g. a flyer or shooter requires: a new `param2` bit or overloading params (undocumented bit allocation), extending the build_level JSON schema, a new branch in the spawn loop, new branches in the combat/update loop, a new logic struct or flags in `enemy.cpp`, art generation + JSON + a `bn_sprite_items_*` include. Nothing enforces the bit meanings match between `build_level.py` and the scene decode.
**Risk:** Every new enemy type re-derives an ad-hoc encoding; a bit mismatch between compiler and scene decodes as the wrong enemy with zero errors.

### [MAJOR] Hand-allocated global ID spaces (latch bits 0–23, heart-container ids 0–7) with no registry and silent collision behavior

**Evidence:** `world_state.h:25-27`: latch bits [24..31] reserved for heart containers, [0..23] "for dungeon shortcuts … must never overlap" — enforcement is a comment. Actual allocations are scattered across sidecar JSONs: `dungeon6_room1.json` `latch_id:0`, `dungeon7_room1.json` `latch_id:1`, `dungeon8_room1.json` `latch_id:2`; heart ids `0`,`1`,`2` in d6/d7/d8 jsons. No file lists the next free id; no generator or test checks cross-dungeon uniqueness.
**Problem:** An author adding D9 content must grep all 25 sidecar JSONs to find the next free latch id and heart id. A duplicate latch id silently links two shortcuts in different dungeons (opening one persists the other open); a duplicate heart id makes the second container permanently uncollectable on a save that took the first.
**Risk:** Save-file corruption class bugs that only reproduce on a played-through save — the most expensive kind to QA on GBA.

### [MAJOR] Per-dungeon structural tests are ~3,200 lines of copy-pasted flood-fill harness

**Evidence:** `test/test_dungeon1_level.cpp` (303), `test_dungeon2_level.cpp` (337), `test_dungeon3_level.cpp` (211), `test_dungeon6_level.cpp` (473), `test_dungeon7_level.cpp` (887), `test_dungeon8_level.cpp` (514), `test_dungeon9_level.cpp` (276) — each re-declares its own private `DnGrid`/`build_grid`/`standable`/BFS reachability (e.g. `D3Grid` in test_dungeon3_level.cpp:26-51 duplicating D1/D2's, per its own comment "The flood-fill harness is the SAME 2-wide x 4-tall body model used for D1/D2"). Commit 9462299 already had to clean up "unused copied test helpers (d3_tile/reaches_forward_exit/D3_ROOMS/D3_N)" — copy-paste residue.
**Problem:** The no-soft-lock invariant machinery (the most valuable test asset in the repo) is not a shared header; every new dungeon's test starts by copying 200-900 lines and mutating them. Physics-model drift between copies (each hardcodes `CLIMB`, hazard sets, movement rules for that dungeon's ability mix) means a fixed harness bug must be re-fixed N times.
**Risk:** ~300-900 lines of test boilerplate per new dungeon (direct token cost), plus divergent reachability models that pass in one dungeon's copy and would fail in another's.

### [MAJOR] Ability/spell plumbing is parallel if-chains in ~10 locations — adding one ability touches all of them, misses are silent

**Evidence:** Adding a 9th ability/spell requires: `Ability` enum (`world_state.h:5`); `SpellId` enum + `SpellState::owns/has_any/refresh/cycle` — four hand-maintained chains in one struct (`spell.h:29-49`); `ABILITY_ENUM` in `build_level.py`; `spell_for_ability` (`gates.h:40-45`) and possibly `GATE_TABLE`; the `player.abilities.X = world.has(...)` copy-block duplicated three times (`scene_dungeon.cpp:880-884`, `scene_dungeon.cpp:346-350` in run_room_boss, `scene_hub.cpp:173-177`); the spell-icon if-chain duplicated three times (`scene_hub.cpp:148-158`, `scene_dungeon.cpp:273-283`, `scene_dungeon.cpp:603-612`); `cast_spell` whitelists (`scene_dungeon.cpp:949-951`, `scene_dungeon.cpp:356-358`, `scene_hub.cpp:189-191`); plus scene_boss.cpp equivalents. Project memory already flags the scene_dungeon/scene_hub controller duplication as a deferred refactor.
**Problem:** These are parallel lists with no single source of truth. Omitting the new ability from one scene's copy-block compiles clean and simply makes the ability dead in that scene (the exact class of bug the M9-M10 comments show was hit before).
**Risk:** ~10 edit sites per ability; at least 6 of them fail silently at runtime.

### [MAJOR] Documentation drift: README is 8 milestones stale; AGENTS.md contradicts CLAUDE.md

**Evidence:** `README.md:6-7`: "It currently contains **Milestone 6 — Sunken Ruins + Blink (Dash)**" — the repo is at M14 with 9 dungeons, a lives system, boss framework, and 3 bosses. `AGENTS.md:6`: "Host tests: `make -C test` (runs on any machine with g++ and Python)" — `CLAUDE.md` says plain `make -C test` is "fragile here" and mandates `bash tools/host_test.sh`; and `make -C test` skips level-header regeneration entirely (see the build-wiring finding). `level_data.h:9` `DoorSpawn` comment "dungeon 1..8" while the hub ships door 9 (`hub.txt`, `scene_hub.cpp:48`).
**Problem:** An agent that reads README or AGENTS.md (the standard entry points) gets a wrong model of the project's scope and a wrong (data-stale) test command.
**Risk:** Wasted tokens rediscovering 8 milestones of features; tests run against stale generated headers by following AGENTS.md verbatim.

### [MINOR] Background tile-index allocation is manual and documented only in a comment block

**Evidence:** The canonical tile map is a comment (`gates.h:30-35`); raw literals are scattered through `scene_dungeon.cpp` — room-door `5`, hub-portal `26` with the comment "13 is lava, so 26 is the next free strip slot — see make_placeholder_art.py gen_tiles" (lines 796-800), exit `6` (808), brazier `14`/`15` (819, 1113), plate `17`/button `18` (825/835), `WATER_BG = 16, ICE_PLATFORM_BG = 19` pinned locally (1141).
**Problem:** Allocating a new gate/terrain tile requires cross-referencing a comment, a Python art generator, and grep-ing scene literals; two features claiming the same strip slot renders wrong art with no error.
**Risk:** Every new gate type or terrain feature costs a manual "find the next free slot" hunt; collisions are visual-only bugs caught in emulator QA.

### [MINOR] Scene entity caps silently truncate authored content

**Evidence:** All spawn loops clamp: `enemy_count && i < 8` (scene_dungeon.cpp:646), gates `< 24` (664), shrines `< 4` (692), hearts `< 4` (707), blocks `< 8` (725), boulders `< 8` (736), loose/hidden platforms `< 8` with per-run `len < 8` (748/752, 764/768), crystals `< 8` (780), room-doors `< 8` (792), braziers `< 16` (813), plates/buttons `< 16` (824/833). `build_level.py` does not validate these caps.
**Problem:** A room authored with a 9th enemy or a 9-tile loose platform compiles, passes level tests (which read the full arrays), and silently drops content in the ROM.
**Risk:** Author-vs-runtime divergence discovered only by playing; the caps aren't written down anywhere an author would look.

### [MINOR] `run_room_boss` hardcodes arena geometry and single-crystal assumptions

**Evidence:** Boss placed at `level.w / 2` with feet on `(level.h - 2) * 8` (scene_dungeon.cpp:200-205) — arenas must have their floor exactly at row h-2; pacing bounds assume 1-tile walls at columns 0 and w-1 (213-214); only `magic_crystals[0]` is honored (244-245); player projectile damage/invuln (`20`/`45`) repeated at lines 437, 440, 444 and again in the normal loop (1133, 1165).
**Problem:** These are unwritten authoring constraints on every future boss arena txt (flat floor at h-2, no interior pits under the center, at most one crystal). Violating them yields a floating or buried boss sprite, not an error.
**Risk:** Each D4–D8 arena must be authored to invisible constraints an agent can only learn by reading the fight loop.

### [MINOR] Misleading module name: generic spronk-rescue helper lives in `logic/dungeon1.h`

**Evidence:** `include/logic/dungeon1.h` contains only the generic `try_free_spronk` used by every dungeon (`scene_dungeon.cpp:40` includes it "// try_free_spronk"); there is no per-dungeon logic in it.
**Problem:** An agent searching for dungeon-1-specific logic reads it; an agent looking for the spronk-rescue rule won't guess this filename. Also implies (falsely) a per-dungeon logic-header pattern exists.
**Risk:** Small but recurring search/token waste on every dungeon task; invites cargo-cult creation of `dungeon2.h`… headers.
