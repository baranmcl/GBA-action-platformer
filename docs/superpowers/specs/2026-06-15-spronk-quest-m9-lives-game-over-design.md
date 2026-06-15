# Spronk Quest — Milestone 9 (Lives & Game Over) Design

**Status:** Approved (brainstorming complete, 2026-06-15). Next: implementation plan.

**One-liner:** Add an arcade **lives** system — a persistent (SRAM) pool that depletes on death and, when exhausted, shows a **Game Over** screen with **Continue** (restart the current dungeon, lives refilled) or **Quit to title**. Lives refill on dungeon clear, and **each freed spronk permanently raises the max** — rescuing spronks (the core goal) makes you more resilient.

A **systems milestone** layered onto the shipped scene/save/HUD framework (M2 + M3–M8). It changes the death loop from "infinite forgiving respawns" to "finite, counted, saved retries" while keeping the game's core forgiveness intact: SRAM progress (freed spronks, earned abilities, opened shortcuts) is NEVER lost — Game Over is a checkpoint reset + interruption, not a progress wipe.

---

## 1. Identity & arc

- **Scope:** not a dungeon — a cross-cutting core-loop system affecting all dungeons + the save + the HUD + a new scene.
- **Why now:** the user requested stakes for death; the game currently respawns the player at the room entrance with full health forever (M8's death-fix made that respawn reliable). M9 makes deaths a counted, persistent resource.
- **Forgiveness preserved:** because progress is SRAM-persisted and re-derivable (abilities from shrines, spronks permanently freed, latched shortcuts), Game Over cannot and does not cost progress. Continue restarts the current dungeon from room 0 with progress intact.

## 2. The lives model

- **`World.lives`** — a persistent (SRAM) current-lives count. A NEW game starts at **`STARTING_LIVES = 3`** (tunable constant).
- **Max lives** — `max_lives(World) = STARTING_LIVES + (spronks freed)`, derived directly from the EXISTING `World.spronks_freed` bitmask (popcount). Each rescued spronk permanently adds one max life. NO new save field and NO new latch range — it reuses `spronks_freed`, exactly the way the game already counts progress.
- **Losing a life:** in `run_dungeon`'s death/respawn path (`scene_dungeon.cpp`, the `health.is_empty()` block), on death: `lives -= 1` (persisted). If `lives <= 0` → **Game Over** (no respawn). Otherwise the existing respawn fires (room entrance, full health, the M8 60-frame grace). So the HUD shows e.g. 3 → 2 → 1, and the death that would take it to 0 triggers Game Over ("1 life" = your next death ends the run).
- **Freeing a spronk grants +1 permanent max life.** Because freeing a spronk IS clearing a dungeon, this single event both grows `max_lives` (via `spronks_freed`) AND is the natural refill point: on dungeon clear, set `lives = max_lives(world)` (persisted) — so you leave each dungeon with a fresh, now-larger pool.
- **Refill to max** (`lives = max_lives`, persisted) on: **dungeon clear / spronk freed** and **Continue**.
- **No separate extra-life pickups.** The spronks ARE the extra-life source (replacing the earlier "life-container" pickup idea) — simpler (no new collectible, compiler symbol, sprite, or latch range) and thematically tied to the core rescue goal.
- **Where lives live:** `World` (the SRAM-saved struct), NOT `PlayerState` (session-only), because lives are persistent. (`max_lives` is derived from `spronks_freed`, so only the current `lives` count needs a new saved field.)

## 3. Game Over scene + Continue flow

- **New scene `run_game_over(World&)`** in `src/game/scene_game_over.{h,cpp}`, mirroring `run_title`'s text-screen pattern (sprite text + key loop, fade in/out). Shows **"GAME OVER"** and two options: **Continue** (primary) and **Quit to title**. Navigate with the d-pad/A or map Continue→START and Quit→SELECT (final input mapping decided in the plan; keep it consistent with the title screen's START idiom).
- **Flow (in `run_dungeon`):** when a death depletes lives, fade out and call `run_game_over`. On **Continue**: `world.lives = max_lives(world)`, persist, reset to the dungeon's start room (room 0 entrance) with full health, and resume the `run_dungeon` loop. On **Quit to title**: return a new `DungeonResult::QuitToTitle` (or equivalent) that `main.cpp` routes to `run_title` (whose existing CONTINUE→reload-SRAM path resumes the hub with all progress). `main.cpp` is clean (no scaffold) and may be edited for this routing.
- **Boot safety:** on save load, clamp `lives` to `[1, max_lives]` (if a save somehow holds 0 — e.g. powered off at the Game Over screen — treat it as a fresh pool so the player never boots into an instant Game Over).

## 4. HUD lives counter

- Add a **lives indicator** to the existing HUD (shown in dungeons; optional in the hub). Either a small row of life icons (one per current life, up to a sensible cap) or a single icon + count ("◆ x3"). Reuse or add a small placeholder sprite (e.g. a Laurel-head or a simple life glyph via `make_placeholder_art.py`). The pure pip/string math goes in `logic/` (host-tested, like the health-bar math); the engine draws it.

## 5. Save format (v4)

- **Bump `SaveData` v3 → v4:** add a `uint8 lives` field (current count). The struct grows past 16 bytes (with `uint32 latches` alignment, `sizeof` becomes 20); update the static_asserts + the checksum coverage accordingly.
- **Migration:** add a `version == 3` → v4 branch in `load_save` that defaults `lives = STARTING_LIVES`. Keep the existing v1/v2/v3 migration branches working. Bump `SAVE_VERSION = 4`.
- **No other new fields:** `max_lives` is derived from the existing `spronks_freed`; heart containers stay in `latches` [24..31]; dungeon shortcuts stay in the low `latches` bits. No new latch range is needed (the dropped extra-life pickup would have used one).
- **Persist timing:** write the world to SRAM on the events that change lives — death (lives−−), dungeon clear / spronk freed (refill + the freed-spronk that grows max is already persisted), and Continue — in addition to the existing clear-time write.

## 6. What's new vs reused (three-layer purity preserved)

**New (pure logic):** `World.lives` + `max_lives()` (= `STARTING_LIVES + popcount(spronks_freed)`) + lives helpers (decrement/refill/clamp) in `world_state.h`; SaveData v4 + migration in `save.h`/`world_state.h`; lives-HUD math in `logic/`. **New (engine/scene):** `run_game_over` scene; HUD lives drawing; the death→lives→Game-Over flow in `run_dungeon`; `main.cpp` quit-to-title routing. **Reused:** the existing `spronks_freed` bitmask (the extra-life source — no new collectible), the title-screen scene pattern, the existing respawn/death path + the clear/spronk-rescue path, the SaveData migration framework, the HUD. **No new compiler symbol or pickup sprite.** **Three-layer rule:** all lives/save/HUD math that can be pure is host-tested in `logic/`; only `bn::` rendering/scene work lives in engine/game.

## 7. Out of scope (deferred)

Separate extra-life pickups / life-container collectibles (dropped — freeing a spronk grants the +max life instead); disposable classic 1-UPs; score/points; difficulty settings; per-dungeon life budgets; animated Game Over art (placeholder text only); D8/Light/Nightmare King (a later milestone); retroactively tuning each dungeon's difficulty to the lives economy (tune `STARTING_LIVES` in playtest instead).

## 8. Testing strategy

- **Pure-logic host tests:** `lives` decrements on death and clamps at 0; the "death at 1 → Game Over (0), death above 1 → respawn" boundary; refill-to-max on clear/Continue; `max_lives` derivation from `spronks_freed` (0 spronks → 3; freeing spronks grows max by one each); freeing a spronk both grows max AND refills; the boot clamp (loading lives 0 → max); heart-container latches [24..31] unaffected by the lives logic.
- **SaveData v4 tests:** v4 round-trip (lives survives save/load); **v3 → v4 migration defaults lives to `STARTING_LIVES`**; v1/v2 migrations still pass; `sizeof(SaveData)` + checksum correct; corrupted/empty SRAM → fresh game with full lives (TEST-4).
- **HUD math tests:** lives indicator shows the right count for 1..max and caps sensibly.
- **Build gates:** `bash tools/host_test.sh` green (N/N), `python tools/check_logic_purity.py` clean, `bash tools/build_rom.sh` → `ROM fixed!`. Manual emulator QA: die repeatedly → counter drops → Game Over → Continue restarts the dungeon with lives refilled → Quit-to-title reloads the save; dungeon clear refills; an extra-life pickup permanently raises max; lives persist across power-off.

## 9. Success criteria

- Death decrements a persistent lives count; at 0 a Game Over screen appears with **Continue** (restart current dungeon, lives refilled, progress kept) and **Quit to title**.
- Lives refill on dungeon clear; freeing a spronk permanently raises max lives (derived from `spronks_freed`); lives survive power-off; a save can never boot into an instant Game Over.
- The HUD shows current lives. SaveData migrates v3→v4 cleanly (existing saves keep their progress + get 3 lives).
- All host tests green, purity clean, ROM builds; the forgiving SRAM-progress model is preserved (Game Over never costs progress).
