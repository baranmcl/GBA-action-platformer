# Spronk Quest — Milestone 9 (Lives & Game Over) Design

**Status:** Approved (brainstorming complete, 2026-06-15). Next: implementation plan.

**One-liner:** Add an arcade **lives** system — a persistent (SRAM) pool that depletes on death and, when exhausted, shows a **Game Over** screen with **Continue** (restart the current dungeon, lives refilled) or **Quit to title**. Lives refill on dungeon clear, and **extra-life pickups** permanently raise the max — parallel to the heart-container system.

A **systems milestone** layered onto the shipped scene/save/HUD framework (M2 + M3–M8). It changes the death loop from "infinite forgiving respawns" to "finite, counted, saved retries" while keeping the game's core forgiveness intact: SRAM progress (freed spronks, earned abilities, opened shortcuts) is NEVER lost — Game Over is a checkpoint reset + interruption, not a progress wipe.

---

## 1. Identity & arc

- **Scope:** not a dungeon — a cross-cutting core-loop system affecting all dungeons + the save + the HUD + a new scene.
- **Why now:** the user requested stakes for death; the game currently respawns the player at the room entrance with full health forever (M8's death-fix made that respawn reliable). M9 makes deaths a counted, persistent resource.
- **Forgiveness preserved:** because progress is SRAM-persisted and re-derivable (abilities from shrines, spronks permanently freed, latched shortcuts), Game Over cannot and does not cost progress. Continue restarts the current dungeon from room 0 with progress intact.

## 2. The lives model

- **`World.lives`** — a persistent (SRAM) current-lives count. A NEW game starts at **`STARTING_LIVES = 3`** (tunable constant).
- **Max lives** — `max_lives(World) = STARTING_LIVES + (extra-life containers collected)`, derived from a reserved `latches` bit range **[16..23]** (8 slots) — exactly parallel to `max_health_for` deriving max HP from heart-container latches [24..31]. NO separate max-lives save field.
- **Losing a life:** in `run_dungeon`'s death/respawn path (`scene_dungeon.cpp`, the `health.is_empty()` block), on death: `lives -= 1` (persisted). If `lives <= 0` → **Game Over** (no respawn). Otherwise the existing respawn fires (room entrance, full health, the M8 60-frame grace). So the HUD shows e.g. 3 → 2 → 1, and the death that would take it to 0 triggers Game Over ("1 life" = your next death ends the run).
- **Refill to max** (`lives = max_lives`, persisted) on: **dungeon clear** (freeing the spronk — the recovery reward) and **Continue**.
- **Extra-life pickup** — a new one-time collectible (a "life container"): on collect, set its reserved latch (bits [16..23], one-time, like heart containers) → `max_lives` grows by 1 → and `lives += 1` (current also bumps), persisted. Permanent (+max), so it is NOT wiped by the next refill. (Chosen over disposable classic 1-UPs, which a clear/Continue refill would erase.)
- **Where lives live:** `World` (the SRAM-saved struct), NOT `PlayerState` (session-only), because lives are persistent.

## 3. Game Over scene + Continue flow

- **New scene `run_game_over(World&)`** in `src/game/scene_game_over.{h,cpp}`, mirroring `run_title`'s text-screen pattern (sprite text + key loop, fade in/out). Shows **"GAME OVER"** and two options: **Continue** (primary) and **Quit to title**. Navigate with the d-pad/A or map Continue→START and Quit→SELECT (final input mapping decided in the plan; keep it consistent with the title screen's START idiom).
- **Flow (in `run_dungeon`):** when a death depletes lives, fade out and call `run_game_over`. On **Continue**: `world.lives = max_lives(world)`, persist, reset to the dungeon's start room (room 0 entrance) with full health, and resume the `run_dungeon` loop. On **Quit to title**: return a new `DungeonResult::QuitToTitle` (or equivalent) that `main.cpp` routes to `run_title` (whose existing CONTINUE→reload-SRAM path resumes the hub with all progress). `main.cpp` is clean (no scaffold) and may be edited for this routing.
- **Boot safety:** on save load, clamp `lives` to `[1, max_lives]` (if a save somehow holds 0 — e.g. powered off at the Game Over screen — treat it as a fresh pool so the player never boots into an instant Game Over).

## 4. HUD lives counter

- Add a **lives indicator** to the existing HUD (shown in dungeons; optional in the hub). Either a small row of life icons (one per current life, up to a sensible cap) or a single icon + count ("◆ x3"). Reuse or add a small placeholder sprite (e.g. a Laurel-head or a simple life glyph via `make_placeholder_art.py`). The pure pip/string math goes in `logic/` (host-tested, like the health-bar math); the engine draws it.

## 5. Save format (v4)

- **Bump `SaveData` v3 → v4:** add a `uint8 lives` field (current count). The struct grows past 16 bytes (with `uint32 latches` alignment, `sizeof` becomes 20); update the static_asserts + the checksum coverage accordingly.
- **Migration:** add a `version == 3` → v4 branch in `load_save` that defaults `lives = STARTING_LIVES`. Keep the existing v1/v2/v3 migration branches working. Bump `SAVE_VERSION = 4`.
- **No other new fields:** extra-life containers ride `latches` [16..23]; heart containers stay [24..31]; dungeon shortcuts stay in the low bits.
- **Persist timing:** write the world to SRAM on the events that change lives — death, extra-life collect, dungeon clear, and Continue — in addition to the existing clear-time write.

## 6. What's new vs reused (three-layer purity preserved)

**New (pure logic):** `World.lives` + `max_lives()` + lives helpers (decrement/refill/clamp, extra-life latch range) in `world_state.h`; SaveData v4 + migration in `save.h`/`world_state.h`; lives-HUD math in `logic/`; a `LifeContainerSpawn` (or reuse the heart-container pattern) in `level_data.h`. **New (engine/scene):** `run_game_over` scene; HUD lives drawing; the death→lives→Game-Over flow in `run_dungeon`; `main.cpp` quit-to-title routing; life-container sprite + collect. **New (compiler):** a symbol for the extra-life pickup. **Reused:** the heart-container pattern (one-time latch-persisted pickup + derived max), the title-screen scene pattern, the existing respawn/death path, the SaveData migration framework, the HUD. **Three-layer rule:** all lives/save/HUD math that can be pure is host-tested in `logic/`; only `bn::` rendering/scene work lives in engine/game.

## 7. Out of scope (deferred)

Disposable classic 1-UPs (we use permanent +max life containers); score/points; difficulty settings; per-dungeon life budgets; animated Game Over art (placeholder text only); D8/Light/Nightmare King (a later milestone); retroactively tuning each dungeon's difficulty to the lives economy (tune `STARTING_LIVES` in playtest instead).

## 8. Testing strategy

- **Pure-logic host tests:** `lives` decrements on death and clamps at 0; the "death at 1 → Game Over (0), death above 1 → respawn" boundary; refill-to-max on clear/Continue; `max_lives` derivation from extra-life latches [16..23]; extra-life one-time collect (sets latch, bumps max + current, idempotent); the boot clamp (loading lives 0 → max); latch-range isolation (extra-life [16..23] vs heart [24..31] vs shortcuts).
- **SaveData v4 tests:** v4 round-trip (lives survives save/load); **v3 → v4 migration defaults lives to `STARTING_LIVES`**; v1/v2 migrations still pass; `sizeof(SaveData)` + checksum correct; corrupted/empty SRAM → fresh game with full lives (TEST-4).
- **HUD math tests:** lives indicator shows the right count for 1..max and caps sensibly.
- **Build gates:** `bash tools/host_test.sh` green (N/N), `python tools/check_logic_purity.py` clean, `bash tools/build_rom.sh` → `ROM fixed!`. Manual emulator QA: die repeatedly → counter drops → Game Over → Continue restarts the dungeon with lives refilled → Quit-to-title reloads the save; dungeon clear refills; an extra-life pickup permanently raises max; lives persist across power-off.

## 9. Success criteria

- Death decrements a persistent lives count; at 0 a Game Over screen appears with **Continue** (restart current dungeon, lives refilled, progress kept) and **Quit to title**.
- Lives refill on dungeon clear; extra-life pickups permanently raise max lives (one-time, latch-persisted); lives survive power-off; a save can never boot into an instant Game Over.
- The HUD shows current lives. SaveData migrates v3→v4 cleanly (existing saves keep their progress + get 3 lives).
- All host tests green, purity clean, ROM builds; the forgiving SRAM-progress model is preserved (Game Over never costs progress).
