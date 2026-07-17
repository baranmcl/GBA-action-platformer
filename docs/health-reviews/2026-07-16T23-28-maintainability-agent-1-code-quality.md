# Agent 1: Code Quality & Idiom
**Date:** 2026-07-16 23:28
**Scope:** Maintainability + token-efficiency for adding dungeons/bosses/enemies

### [MAJOR] Two parallel ~350-line boss fight loops that have already diverged

**Evidence:** `src/game/scene_boss.cpp:76-444` (`run_boss`, the King) vs `src/game/scene_dungeon.cpp:167-521` (`run_room_boss`); the comment at scene_dungeon.cpp:158-164 declares it "PARALLEL to the King's scene_boss.cpp run_boss (NOT a shared monolith)".
**Problem:** Both functions are the same fight loop: settle-player intro, `boss_say`, `restart_fight`, telegraph/Active/Recovery attack dispatch, `attacks.advance` + contact damage, `resolve_damage`, magic-crystal respawn, i-frame blink, death/lives/Game-Over flow, spell-icon refresh, clamped camera. They were forked instead of shared, and the fork is already drifting: blocking is `block_player_shots` (free bolt blocks) in the King but `block_with_spell` (data-driven) in room bosses; dialogue is hard-coded strings in the King but data-driven (`intro_line`/`death_line`) in room bosses; pacing/rockfall/dual-element exist only in `run_room_boss`; the King's teleport only in `run_boss`. Every new boss mechanic must be implemented (or deliberately not implemented) twice.
**Risk:** Bug fixes land in one loop and not the other (this already happened once — the "bolts vanish before the wall" fix is documented as being ported between them, scene_boss.cpp:339-344). D4-D8 bosses (the stated next milestones) will keep growing `run_room_boss` while the King rots; unifying later gets more expensive every milestone.

### [MAJOR] The same scene-control boilerplate is copy-pasted 2-4x across every scene

**Evidence:** `refresh_spell_icon` lambda appears verbatim 4 times (scene_dungeon.cpp:273-283 and 602-613, scene_hub.cpp:148-158, scene_boss.cpp:178-188). `set_clamped_cam` appears 4 times (scene_dungeon.cpp:285-292 and 540-547, scene_hub.cpp:89-96, scene_boss.cpp:193-200). The 5-line ability-flag sync appears 4 times (scene_dungeon.cpp:346-350 and 880-884, scene_hub.cpp:173-177, scene_boss.cpp:261-265). The ~40-line vine VFX + miss-vine animation block is duplicated byte-for-byte (scene_dungeon.cpp:1322-1364, scene_hub.cpp:228-265). The heart-container max-HP sync is in three places (main.cpp:29-30, scene_dungeon.cpp:1388-1389, scene_hub.cpp:58-59). Crystal collect/respawn logic in three (scene_boss.cpp:405-413, scene_dungeon.cpp:453-462 and 1293-1300).
**Problem:** This is the suspected scene_hub/scene_dungeon duplication, confirmed and wider than suspected. None of these belong in scenes — they are player-controller/HUD/camera concerns with zero scene-specific variation (the hub's grapple branch is the dungeon's minus blocks/enemies). There is no shared "player session controller" even though `engine/` exists precisely for such glue.
**Risk:** Any input remap, new spell, or camera tweak requires finding and editing 3-4 sites; a missed site produces a scene-specific behavior bug (e.g. a fifth SpellId would silently show the wrong HUD icon only in whichever scene was forgotten).

### [MAJOR] `play_room` is an ~850-line monolith containing every entity system inline

**Evidence:** `src/game/scene_dungeon.cpp:523-1376`. One function holds spawn/setup for 15 entity kinds (enemies, gates, cracked floors, shrines, hearts, blocks, boulders, loose platforms, hidden platforms, crystals, braziers, plates, buttons, room-doors, spronk cage) plus a frame loop that interleaves grapple targeting, pound resolution, spell/terrain interaction, triggers, death/respawn, and VFX.
**Problem:** Each entity type is smeared across three distant regions of the function: setup (~640-845), per-frame update (~960-1300), and death-reset (~1240-1267). Adding one mechanic means understanding and editing the whole file — the exact opposite of the owner's "cheap to add dungeons/enemies" goal, and the single most token-expensive file in the repo (1420 lines, unreadable in one context window with commentary).
**Risk:** New features keep being appended into the loop (the M8/M10 blocks show this pattern is accelerating), coupling grows (e.g. grapple input logic at 892-948 reaches directly into `blocks` and `enemies`), and regressions from edit-distance mistakes become likely.

### [MAJOR] Authored level content is silently truncated by hard caps

**Evidence:** Every spawn loop clamps with `&& i < N` and drops the rest without any assert: enemies `i < 8` (scene_dungeon.cpp:646), gates `i < 24` (664), shrines `i < 4` (692), hearts `i < 4` (707), blocks `i < 8` (725), room-doors `i < 8` (792), plates/buttons/brazier-groups `i < 16` (824/833/839); platform runs clamp `dx < 8` (752, 768).
**Problem:** A level author (or generator) who places a 9th enemy or a 9-tile loose platform gets a compiling, booting game where content is just missing. Nothing checks `level.enemy_count <= 8` at build time (`tools/build_level.py` doesn't enforce the engine caps) or at load time (`BN_ASSERT` is available and free in release).
**Risk:** A future dungeon quietly ships with a missing gate or door — in this game that class of bug is a soft-lock (unreachable spronk/exit), the exact failure mode the project's invariant tests exist to prevent.

### [MAJOR] Adding one boss requires edits in four unrelated places, keyed by pointer identity

**Evidence:** A new boss needs (1) a `BossDef` in `include/logic/boss.h`, (2) an entry in `BOSS_SYMBOL` in `tools/build_level.py:56-60`, (3) a branch in `boss_sprite_for` at `src/game/scene_dungeon.cpp:151-155`, which pointer-compares `def == &logic::D2_DEF` and (4) the matching `#include "bn_sprite_items_*.h"` at the top of scene_dungeon.cpp.
**Problem:** The boss framework is data-driven in logic, but the sprite binding is a hand-maintained pointer-identity switch in the game layer with a silent fallback: any def not listed renders as the D1 guardian. Pointer comparison also silently breaks if a def is ever copied instead of referenced. There's no single "boss registry" tying def, sprite, and builder name together.
**Risk:** For D4-D8 (five more bosses planned) this is 5x4 scattered edits; the failure mode of forgetting step 3 is not an error but a wrong sprite discovered only in play-testing.

### [MINOR] Logic helpers exist only to be tested; production reimplements the behavior inline

**Evidence:** `include/logic/frost.h` (`ice_freezes`/`fire_melts`) and `include/logic/fire_effect.h` (`fire_clears_gate`) are referenced only from `test/test_frost.cpp` and `test/test_fire_effect.cpp`. scene_dungeon.cpp includes both (lines 45-46) but implements freeze/melt as raw `set_collision_tile(..., (int)TileKind::IcePlatform)` loops (1141-1161) and gate clearing via `gate_cleared_by`.
**Problem:** The host tests verify functions the shipped game never calls — the tests can stay green while the actual inline freeze/melt logic regresses. The two headers plus their includes are dead weight in production.
**Risk:** False confidence from the test suite; future readers assume `ice_freezes` is the mechanism and modify the wrong code.

### [MINOR] Vestigial `boss_defeated` flag

**Evidence:** `src/game/scene_dungeon.cpp:564-568` — `bool boss_defeated = false; if(level.boss != nullptr && !boss_defeated){ ... boss_defeated = true; ... }`.
**Problem:** The local is initialized false immediately before the only read, so `!boss_defeated` is always true and the `= true` assignment is dead. It looks like persistent state ("re-entering re-fights the boss") but does nothing; the accompanying comment describes semantics the variable doesn't implement.
**Risk:** A future contributor "fixes" boss persistence by promoting this flag, believing it already gates something, or wastes time reasoning about it.

### [MINOR] Tuning constants duplicated as bare literals across scenes

**Evidence:** Crystal-respawn threshold `magic.cur < 10` hard-codes `SpellCast::cost` in two places (scene_boss.cpp:410 — with a comment admitting the coupling — and scene_dungeon.cpp:459). Contact damage `20` / i-frames `45` repeated 8+ times (scene_dungeon.cpp:436, 440, 444, 1133, 1165; scene_boss.cpp:351, 355). Magic-refill `25` in 5 places.
**Problem:** Changing spell cost or damage tuning requires a grep-and-pray across three scene files; the crystal threshold silently desynchronizes if `SpellCast::cost` changes.
**Risk:** A tuning pass changes one site and not another, producing scene-inconsistent combat (already the kind of thing the QA notes chase).

### [MINOR] Background tile indices are hard-coded at use sites; the authoritative map lives in a comment

**Evidence:** `include/logic/gates.h:30-35` documents the tile map only in prose; consumers hard-code numbers: door bg `26`/`5`/`6` (scene_dungeon.cpp:798, 808), brazier `14`/`15` (819, 1113), plate `17` / button `18` (826, 835), `WATER_BG=16, ICE_PLATFORM_BG=19` re-pinned locally (1141), and `src/engine/level_view.cpp:31-36` re-encodes the whole kind→tile mapping as a ternary chain.
**Problem:** The same mapping exists in at least four places (comment, level_view chain, scene literals, `tools/make_placeholder_art.py` implied). Adding a tile requires syncing them all with no compiler help.
**Risk:** Art/tile reshuffles produce wrong visuals with no error — the codebase's own comments record this has already bitten ("13 is lava, so 26 is the next free strip slot", "NOT 10: tile 10 is FireWall's flame art").

### [MINOR] Brazier hitbox hard-codes rows 14-19 while its visual is floor-scanned

**Evidence:** `src/game/scene_dungeon.cpp:817-819` — `draw_ty` is computed per-room via `floor_row_below`, but the hit body is `tile_body(b.tx, 14, 6, 24)` (fixed rows 14-19, per the comment).
**Problem:** The sprite grounds itself to whatever floor exists, but the Fire-hit region assumes the D1-D7 room layout (floor near row 20). A brazier on a ledge or in a taller/shorter room draws in one place and is hittable in another.
**Risk:** Latent content bug for any future room that doesn't share the legacy 19-tall floor convention — exactly the multi-room retrofits the owner has deferred in memory.

### [MINOR] Level collision state is a hidden global mutated by free functions

**Evidence:** `src/engine/level_loader.cpp:5-8` — `s_grid/s_w/s_h` static EWRAM buffer; `set_collision_tile(tx,ty,v)` writes it with no reference to any level object; `LoadedLevel.map.cells` aliases it. Same pattern for `s_cells` in level_view.cpp.
**Problem:** Only one level can exist, and nothing enforces it: a stale `LoadedLevel` from a previous room still "works" but reads the new room's grid. Calls like `engine::set_collision_tile(...)` scattered through scene_dungeon.cpp look like they target `lvl` but actually target whichever level loaded last.
**Risk:** Any future feature that briefly holds two levels (room-transition effects, minimap, previews) corrupts collision silently; sequencing bugs in room transitions are invisible in review.

### [MINOR] Cracked-floor run-breaking is a convoluted O(n³) scan

**Evidence:** `src/game/scene_dungeon.cpp:986-1010` — for each cracked tile, for each other same-row tile, test contiguity by iterating every column of the span and, per column, scanning the whole `cracked_floors` list again.
**Problem:** Three nested loops plus a re-scan express what a simple "walk left, walk right from impact" would express in ~8 lines. Bounded (n≤16) so not a perf issue, but the intent ("break the maximal contiguous span") is buried and easy to break when editing.
**Risk:** Modifications (e.g. vertical cracked runs, larger caps) multiply the cost and the chance of an off-by-one in the span test.

### [MINOR] Hand-unrolled progression/dungeon tables

**Evidence:** `src/game/scene_hub.cpp:39-49` — `door_enterable` is 9 hand-written disjuncts encoding "door n needs spronk n-1". `src/main.cpp:65-72` — 8-branch if-chain mapping n to `DUNGEONn_DUNGEON`.
**Problem:** Both are one-line-per-dungeon lists that should be `n >= 2 && n <= 8 ? w.spronk_freed(n-1) : ...` and a `const DungeonData* const DUNGEONS[8]` array (which `dungeons.h` could own next to the data it already declares).
**Risk:** Adding a dungeon means touching two more hand-lists (on top of the boss's four); an inconsistent edit yields a door that opens to the wrong dungeon or never opens.

### [MINOR] Misleading names that will trip new contributors

**Evidence:** `include/logic/dungeon1.h` contains `try_free_spronk`, the generic spronk-rescue helper used by every dungeon (scene_dungeon.cpp:40, 1307) — the filename says it's D1-specific. `logic::EntitySpawn.param0/param1/param2` (level_data.h:7) are patrol-left/patrol-right/flag-bits, decipherable only via a trailing comment and the decoding site (scene_dungeon.cpp:652-653). `GateType::Water` (a Ice-clearable gate) vs `TileKind::Water` (a damaging, freezable hazard tile) are unrelated concepts sharing a name.
**Problem:** Names actively point the reader in the wrong direction; the param-tuple pattern will get worse as entity kinds gain options (it's already a bitfield in param2).
**Risk:** Wrong-file edits and misconfigured spawns; every new entity option piled into `paramN` degrades the level format further.

### [MINOR] Heavy-plate progress cannot persist and silently resets

**Evidence:** `src/game/scene_dungeon.cpp:1016-1023` — heavy plate opens its gate via `open_column`, with a comment admitting "persist not wired through PlateSpawn (no latch_id field), so the open_column holds for the visit."
**Problem:** Every other progress-gate type (gates, brazier groups, cracked floors) supports `latch_id`; heavy plates are the odd one out purely because the field was never added. The inconsistency is handled by comment rather than by code or by a build-time restriction.
**Risk:** A level author reasonably authors a heavy-plate shortcut expecting Zelda-style persistence and ships a gate that re-locks on every room entry.

### [MINOR] Generated level headers are hostile to reading and unsafe to hand-edit

**Evidence:** `include/game/levels/dungeon3_room1.h:6-22` — every empty entity list is emitted as a one-dummy-element array that looks like real content (`GATES[] = { {0,0,logic::GateType::Gap,-1} }` paired with a count of 0 elsewhere), and `LevelData` is initialized with a single ~40-field positional aggregate where pointers and counts must stay pairwise aligned by position (level_data.h:22-44).
**Problem:** The build tool mitigates this for generation, but the headers are what's checked in and what an LLM/contributor reads and greps. Dummy entries produce false grep hits ("which rooms have gates?"), and a transposed count/pointer pair in the positional initializer compiles clean and misreads memory. Designated initializers (C++20 not available, but named per-array `_COUNT` constants are) would eliminate the pairing hazard.
**Risk:** Hand-tweaking a room header (the natural quick-fix path during QA) is a landmine; false-positive content when auditing levels for invariants.

### [MINOR] Comments are changelog narrative rather than invariants, inflating the hottest file

**Evidence:** `src/game/scene_dungeon.cpp` is saturated with milestone history — "M12 QA r1: swap guardian frame 1" (line 29), "the M3 brazier-height lesson" (1139), "the QA bug where bolts 'didn't reach the wall'" (427-428), "M7 hard-coded a tile-centre position assuming a row-18 floor-2-below layout; in D7's tight alcove..." (715-718).
**Problem:** Roughly a third of the 1420 lines are prose retelling why past bugs happened, in the file every dungeon task must read. The information belongs in `docs/pitfalls/` (which exists for exactly this) with a one-line invariant comment in code.
**Risk:** Directly taxes the owner's LLM-token budget on every future edit of the most-edited file, and stale narratives (several already contradict the code per the latest cleanup commit) mislead more than they help.
