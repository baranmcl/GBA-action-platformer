# Agent 4: Operational Readiness (ROM/runtime/build, adapted for GBA)
**Date:** 2026-07-16 23:28
**Scope:** Maintainability + token-efficiency for adding dungeons/bosses/enemies

### [CRITICAL] Level pipeline has no size guard against the engine's fixed buffers — silent EWRAM overrun path

**Evidence:** `src/engine/level_loader.cpp` (`CAP = 64*128`, `s_grid[y * level.w + x]` written for the full `w*h` with no bounds check); `src/engine/level_view.cpp` (`COLS = 64` — cells beyond column 63 are never rendered); `tools/build_level.py` validates rectangularity and solid borders but never checks `w <= 64`, `h <= 128`, or `w*h <= 8192`.
**Problem:** A level whose tile count exceeds 8192 makes `load_level` write past the 8KB `s_grid` EWRAM array — no assert, no diagnostic, just corruption of whatever BSS follows (e.g. the 16KB `s_cells` map). A level wider than 64 tiles doesn't overflow but silently renders nothing past column 63 while collision still uses the full width (invisible walls/floors). Three shipped levels (dungeon2_room0, dungeon3_room0, dungeon5) are already at exactly 64 wide — the limit is saturated with zero margin and zero enforcement, and "expand existing dungeons into richer multi-room layouts" is an explicitly deferred goal.
**Risk:** The first retrofitted 80x110 or 70-wide room compiles clean, passes host tests (pure-logic tests don't exercise the engine buffers), and ships as silent memory corruption or invisible-geometry desync discovered only by playing that room on hardware.

### [MAJOR] Shared `triggers` vector can overflow its capacity 16 — Butano hard-assert on room load

**Evidence:** `src/game/scene_dungeon.cpp` lines 823-845: `bn::vector<TriggerInst, 16> triggers;` filled by three loops each individually bounded at 16 (`plate_count && i < 16`, `button_count && i < 16`, `brazier_group_count && g < 16`).
**Problem:** The bounds are per-category but the vector is shared — a room with, say, 10 plates + 5 buttons + 3 brazier groups performs 18 `push_back`s into a 16-capacity `bn::vector`. `push_back` on a full `bn::vector` is a `BN_ASSERT` (project uses default Butano config; no `BN_CFG_ASSERT` override found), which halts the ROM on the assert screen. `build_level.py` emits unlimited counts and warns about nothing.
**Risk:** A content-only change (adding one more switch puzzle to an already trigger-heavy room) crashes the ROM at room load. Host tests can't catch it — the cap lives in scene code the host build never compiles.

### [MAJOR] Room-graph door targets are dereferenced unvalidated at runtime

**Evidence:** `src/game/scene_dungeon.cpp` `run_dungeon`: `cur_room = out.target_room; ... play_room(*dungeon.rooms[cur_room], ...)` with no check against `dungeon.room_count` (only the `-1` hub sentinel is special-cased earlier). `include/logic/room_graph.h` `find_entrance` silently falls back to the `@` spawn for an unknown entrance id.
**Problem:** `target_room`/`target_entrance` come from hand-typed JSON sidecars (`tools/levels/*.json`). A typo (`"target_room": 3` in a 3-room dungeon) is not caught by `build_level.py` (it compiles one room at a time and cannot see the graph) and not caught generically at runtime — `rooms[3]` reads past a 3-element pointer array and dereferences garbage. The only safety net is the hand-written per-dungeon host test remembering to assert each door's target; there is no generic room-graph validator.
**Risk:** A new dungeon (D4/D5 multi-room retrofits are planned) whose author forgets the door-target test ships a door that, when pressed Up on, dereferences a wild pointer mid-game on hardware. A wrong `target_entrance` is worse-hidden: silent fallback teleports the player to the room's `@` spawn, potentially skipping or breaking sequence with no error anywhere.

### [MAJOR] Silent entity truncation at scene spawn caps — dropped gates are sequence-breaks, not crashes

**Evidence:** `src/game/scene_dungeon.cpp`: every spawn loop clamps silently — enemies `&& i < 8`, gates `&& i < 24`, shrines `&& i < 4`, hearts `&& i < 4`, blocks/boulders/loose/hidden platforms `&& i < 8`, braziers `&& i < 16`, magic crystals `&& i < 8`. `build_level.py` imposes no per-type count limits and prints counts only informationally.
**Problem:** The pipeline and the runtime disagree about capacity and neither side enforces the contract. The 25th gate or 9th enemy in a room simply does not exist at runtime: a missing gate is a missing progression wall (player walks through what should be Fire-gated), a missing enemy/brazier breaks a puzzle count (a brazier group whose `total` includes an unspawned brazier can never open — a hard soft-lock).
**Risk:** Content growth crosses a cap without any build error, host-test failure, or runtime assert; the failure is discovered as "this dungeon's puzzle can't be solved" during (or after) a playthrough. Per-room structural host tests only cover invariants someone thought to write for that room.

### [MAJOR] Save robustness: one SRAM slot, weak checksum, and detected corruption = silent total progress wipe

**Evidence:** `src/engine/save.cpp` (single `bn::sram::write(s)` at offset 0); `include/logic/world_state.h` (`checksum_v5` = 8-bit additive sum); `src/main.cpp` lines 17-19 (`if(!engine::read_world(world)) world = logic::World{};`). Writes occur frequently: every death (`scene_dungeon.cpp:1232`), every latch (`persist_latch`), every heart container, every spronk, dungeon clear.
**Problem:** Versioning/migration (v1-v5) and magic+checksum rejection are present, but the design has no redundancy: there is exactly one copy of the save. A torn write (power removed / cart yanked during any of the frequent mid-gameplay writes) or any SRAM decay fails the checksum on next boot and the game silently starts fresh — no "save corrupted" message, no attempt at a backup slot, no distinction from a genuine first boot. The 8-bit additive checksum also passes any compensating two-byte corruption and all byte transpositions within the summed range.
**Risk:** A player loses 8 dungeons of progress and the game pretends nothing happened. On flash-cart/repro hardware with marginal SRAM batteries this is the classic end-of-life failure mode, and the code amplifies exposure by writing SRAM on every single death.

### [MAJOR] Nothing builds the ROM automatically, and host tests cannot see two of the three layers

**Evidence:** No `.github`/CI config anywhere in the repo. `tools/host_test.sh` compiles only `test_*.cpp + src/logic/*.cpp`; `src/engine/` (12 files) and `src/game/` (6 files, including the 1420-line `scene_dungeon.cpp`) compile only under devkitARM via `tools/build_rom.sh`.
**Problem:** A green `host_test.sh` run proves nothing about whether the ROM compiles, let alone runs. All engine/game-layer regressions — the trigger-vector overflow above, a broken sprite item reference, a `bn::` API misuse — are invisible until someone manually runs the ROM build and then manually plays the affected content. With 9 dungeons, 4 bosses, and no automated emulator smoke test, the manually-verified surface shrinks every milestone.
**Risk:** Engine-layer regressions land silently on the strength of passing logic tests; the project's own git history shows the review cadence relies on host tests as the gate ("459/459" in commit messages) while the actual shipping artifact is exercised ad hoc.

### [MAJOR] Build and test tooling is hardcoded to this one machine

**Evidence:** `tools/build_rom.sh` lines 13-15: `DKP_BASH="/c/devkitPro/msys2/usr/bin/bash.exe"`, `REPO="/c/Users/baranmcl/Code/GBA-action-platformer"`, `WINPY="/c/Users/baranmcl/AppData/Local/Programs/Python/Python312"`. `tools/host_test.sh` lines 13-14: `/c/msys64/mingw64/bin` and `TMP="C:/Users/baranmcl/AppData/Local/Temp"`. CLAUDE.md documents that plain `make -C test` is broken on this machine and was worked around in the script rather than fixed in `test/Makefile`.
**Problem:** The absolute `REPO` path means `build_rom.sh` doesn't even build a *second checkout on the same machine* — it silently builds the original clone's working tree instead of the one you invoked it from. A fresh clone (new machine, contributor, CI runner) can neither run the ROM build nor the documented "preferred" test entry point without editing scripts. The environment fragility was patched around per-machine instead of being made hermetic.
**Risk:** "Works on my machine" is literal here: any attempt to reproduce the build elsewhere (CI, a second dev, recovery after a disk loss) starts with reverse-engineering the toolchain assumptions; the wrong-checkout `REPO` bug can produce a ROM that doesn't contain the changes just made — a confusing false-verification during QA.

### [MINOR] Ability pickups are not persisted at grant time — a power-cycle can un-earn an ability

**Evidence:** `src/game/scene_dungeon.cpp` lines 1271-1277: the shrine loop calls `world.grant(...)` with no `engine::write_world`; `src/main.cpp` writes only on `Cleared` (line 79); the `Quit` path (SELECT or Q-door) never writes.
**Problem:** Latches, heart containers, spronks, and deaths all persist immediately, but abilities persist only as a side effect of some later write. Grab a shrine, exit to the hub via SELECT/Q, power off: the ability is gone on next boot. Recoverable (the shrine respawns since `world.has()` is false), but it also means a latch persisted mid-run can encode "shortcut opened with ability X" in a save that no longer has ability X — a state combination no test authors.
**Risk:** Player-visible progress loss on power-cycle after a legitimate quit-out, and untested save-state combinations for latch/ability interactions.

### [MINOR] HUD health bar saturates below the shipped max HP

**Evidence:** `include/logic/hud_math.h`: `MAX_HEALTH_PIPS = 16` with comment "fits 150-HP cap (15 pips)". Shipped content defines heart containers id 0, 1, 2 (d6/d7/d8 jsons) → max HP = 175; `World` reserves 8 heart-container bits (up to 300 HP).
**Problem:** `health_total_pips(175)` = 18, clamped to 16 — the bar maxes out at 160 HP, so the third heart container adds no visible bar length and the first ~15-20 HP of damage from full changes nothing on screen. The 150-HP comment is stale; the clamp silently eats future containers too.
**Risk:** Player reads a full bar while already damaged; each additional heart container widens the invisible band.

### [MINOR] Latch-id bit space is convention-only — no range or uniqueness validation

**Evidence:** `include/logic/world_state.h` (latch bits 24-31 reserved for heart containers, "must never overlap"); `tools/build_level.py` accepts any `latch_id` integer with no range check (`< 24`), no `>= 0` check, and no cross-dungeon uniqueness check; ids 0/1/2 currently in use are hand-coordinated across separate JSON files.
**Problem:** A `latch_id` of 24+ silently reads/writes heart-container state (phantom +25 max HP or a shortcut that "opens" when a heart is collected). A duplicated latch_id across two dungeons silently links their shortcuts. Both corrupt persisted save state, and the corruption is written to SRAM immediately via `persist_latch`.
**Risk:** A content author picking "the next latch id" wrong ships a save-corrupting bug that no compile step, host test, or runtime check flags.

### [MINOR] Sprite budget is unaccounted — a maximal room hard-asserts at load

**Evidence:** `src/game/scene_dungeon.cpp` `play_room` worst case: 8 loose platforms x 8 sprites + 8 hidden platforms x 8 sprites (= 128 alone) + 8 enemies + 8 blocks + 8 boulders + 8 crystals + HUD (16 health + 10 magic + shield + text) + vine 4 + icons; Butano `create_sprite` hard-asserts when sprite resources are exhausted.
**Problem:** Nothing in the pipeline, docs, or code sums a room's sprite cost against the GBA's 128-OAM / Butano item budget. Current rooms are far below the limit, but the failure mode is a crash screen at room load, and the caps that "protect" each category (8 platforms x 8 tiles each) already permit an over-budget room.
**Risk:** A dense late-game room (the deferred dungeon expansions) crashes on entry on hardware while building and passing all host tests.

### [MINOR] No debug dungeon/ability selector and no CPU meter — late-game content is effectively untestable per change

**Evidence:** Known deferred items (project memory: dungeon/progression debug selector; powers menu); no use of `bn::core::last_cpu_usage`/any perf readout anywhere in `src/`; reaching D8 or the finale for QA requires freeing 7 spronks or hand-crafting SRAM. Per-frame costs that would benefit from measurement exist: `fill_column`/`open_column` do ~70 `set_level_tile` calls each — every one re-flagging a full map reload via `reload_cells_ref()` (`src/engine/level_view.cpp:60`) — inside the frame when a plate toggles.
**Problem:** With 9 dungeons and 4 bosses, every boss/dungeon change is verified by whoever feels like replaying the approach path; frame-budget regressions (the 59.7fps budget) have no instrumentation at all, so a slow frame reads as subtle scroll judder nobody attributes.
**Risk:** Regressions in D6-D9 content and frame-time creep land unnoticed because the cost of checking them manually is a full playthrough.
