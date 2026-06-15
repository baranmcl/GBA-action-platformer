# M9 — Lives & Game Over Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a persistent (SRAM) **lives** system: death decrements a saved lives count; at 0 a **Game Over** scene offers **Continue** (restart the current dungeon, lives refilled) or **Quit to title**. Max lives = `3 + spronks freed` (each rescued spronk permanently adds one); lives refill on dungeon clear + Continue. A HUD lives counter shows the current count.

**Architecture:** Lives live in the SRAM-saved `World` (current count) with `max_lives` derived from the existing `spronks_freed` bitmask — no new pickup/collectible/compiler-symbol. The death→lives→Game-Over flow sits in `run_dungeon`/`play_room` (reusing the M8 respawn path); a new `run_game_over` scene mirrors `run_title`; `main.cpp` routes a new `QuitToTitle` result to the title. SaveData bumps v3→v4 (one `lives` byte) with a migration. Progress (spronks/abilities/latches) is never lost — Game Over is a checkpoint reset, not a wipe.

**Tech Stack:** C++17 (three-layer: pure `logic/`, Butano `engine/`, scenes `game/`), Butano 21.6.0 (GBA), host unit tests (`bash tools/host_test.sh`), ROM build (`bash tools/build_rom.sh`).

**Design spec:** `docs/superpowers/specs/2026-06-15-spronk-quest-m9-lives-game-over-design.md`

**Branch:** create `feat/m9-lives-game-over` off `main` before Task 1.1.

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

**Overall:** Phase 1 shipped.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure logic: World.lives + max_lives + SaveData v4 | ✅ Shipped 2026-06-15 | a3f2ac3, 0de0075 | 345/345 host tests green; purity clean |
| 2 — Engine/scene: death→Game-Over flow + run_game_over + main routing | ⬜ Not started | — | — |
| 3 — HUD lives counter | ⬜ Not started | — | — |
| 4 — Emulator QA | ⬜ Not started | — | — |

### Deviations
- _(none yet)_

### Discoveries
- `engine/save.cpp` uses `bn::sram::read(s)`/`write(s)` with the struct directly (template deduces size) — no hardcoded 16, so the 20-byte v4 struct is handled automatically with no changes needed.

---

## Conventions every task must follow

- **Three-layer rule** (`CLAUDE.md`): nothing under `include/logic/` or `src/logic/` may include or name a `bn::` type. Phase 1 stays host-testable; Phases 2–3 are the `bn::` work.
- **Host tests:** `bash tools/host_test.sh` (NOT bare `make -C test`). Green ends `N/N tests passed, 0 checks failed`. It also runs the purity guard + Python compiler tests + regenerates level headers.
- **ROM build (this Windows machine):** `bash tools/build_rom.sh` (plain `make` fails — `DEVKITPRO` misconfigured). Clean build prints `ROM fixed!`.
- **Purity guard** before any logic commit: `python tools/check_logic_purity.py`.
- **Pitfalls:** read `docs/pitfalls/implementation-pitfalls.md` + `docs/pitfalls/testing-pitfalls.md`. Most relevant here: **IMPL-4** (SRAM only via `bn::sram` — the save layer already does this; do NOT add raw pointer SRAM access), **IMPL-1** (no `bn::` in logic), **TEST-4** (save tests MUST cover corrupted/empty SRAM + every migration path — critical for the v4 bump), **TEST-2** (assert exact integer values).
- **TDD mandate (every code task):**
  ```
  BEFORE starting work:
  1. Invoke superpowers:test-driven-development
  2. Read docs/pitfalls/testing-pitfalls.md
  Follow TDD: write failing test → implement → verify green.

  BEFORE marking this task complete:
  1. Review tests against docs/pitfalls/testing-pitfalls.md
  2. Verify coverage (error paths? edge cases? migration paths?)
  3. Run tests and confirm green
  ```
- **After each phase:** review the batch from multiple perspectives, minimum 3 rounds; continue until clean.
- **Single committer:** one implementer per task; commit before the next. No parallel implementers on overlapping files.

---

## Phase 1 — Pure logic: World.lives + max_lives + SaveData v4

**Execution Status:** ✅ SHIPPED 2026-06-15 | Task 1.1: a3f2ac3 | Task 1.2: 0de0075

**BEFORE starting:** read `include/logic/world_state.h` (the `World` struct + the v3 `SaveData` layout, `make_save`/`load_save`, `checksum_v3`, the v1/v2/v3 migration branches, `SAVE_VERSION`, the `static_assert(sizeof(SaveData)==16)`), `include/logic/player_state.h`, and `test/test_world_state_v3.cpp` for the save-test style. Note `World.spronks_freed` (uint16, bit d-1 = dungeon d) + `free_spronk`/`spronk_freed`.

### Task 1.1: `World.lives` + `max_lives` + lives helpers (host-tested)

**Files:** Modify `include/logic/world_state.h`; Test: `test/test_world_state_v3.cpp` (or a new `test/test_lives.cpp`).

Add to `World` + as free functions (mirror `max_health_for` from M7):
```cpp
static constexpr int STARTING_LIVES = 3;   // a NEW game's lives (also the base before spronks)
// in struct World, near spronks_freed/abilities:
uint8_t lives = STARTING_LIVES;            // current lives (persistent); max is DERIVED (below)
```
Free functions (after the struct):
```cpp
inline int spronk_count(const World& w){ return __builtin_popcount((unsigned)w.spronks_freed); }
inline int max_lives(const World& w){ return STARTING_LIVES + spronk_count(w); }  // each freed spronk = +1 max
inline void refill_lives(World& w){ w.lives = (uint8_t)max_lives(w); }            // current -> max
inline void lose_life(World& w){ if(w.lives > 0) --w.lives; }                     // clamp at 0
inline void clamp_lives_on_load(World& w){                                        // boot safety: never 0 / never > max
    int m = max_lives(w);
    if(w.lives == 0 || w.lives > m) w.lives = (uint8_t)m;
}
```
**TESTS** (TDD — write failing first; assert exact ints, TEST-2):
- `max_lives` = 3 with 0 spronks; grows by 1 per freed spronk (free spronk 1 → 4; free 1,2,3 → 6; all 8 → 11).
- `lose_life` decrements; clamps at 0 (lose at 0 stays 0).
- `refill_lives` sets `lives = max_lives` (e.g. with 2 spronks → 5).
- `clamp_lives_on_load`: lives 0 → max; lives > max → max; lives in [1,max] → unchanged.
- Death-boundary semantics (documented for Phase 2): starting `lives=3`, three `lose_life` calls give 2,1,0 — the call that reaches 0 is the Game-Over trigger.
- [x] failing tests → implement → `bash tools/host_test.sh` green → `python tools/check_logic_purity.py` clean → commit `feat(logic): World.lives + max_lives (from spronks_freed) + lives helpers`.

### Task 1.2: SaveData v4 (add `lives`) + migration (host-tested)

**Files:** Modify `include/logic/world_state.h` (the SaveData block + `make_save`/`load_save`/checksum/`SAVE_VERSION`); Test: create `test/test_world_state_v4.cpp` (mirror `test_world_state_v3.cpp`).

Current v3 SaveData is 16 bytes: `magic[0..3] version[4..5] spronks[6..7] abilities[8..9] current_dungeon[10] checksum[11] latches[12..15]`. Bump to **v4**:
- Add `uint8_t lives;` as the last field (offset 16). With `uint32_t latches` alignment the struct becomes **sizeof 20** (16 + 1, padded to a 4-byte multiple). Update `static_assert(sizeof(SaveData)==20, ...)`.
- `SAVE_VERSION = 4`.
- Add `checksum_v4` covering the header bytes [0..10] + latches [12..15] + the new lives byte [16] (skip the checksum byte [11] and the alignment padding [17..19]). `make_save` writes `s.lives = w.lives` and uses `checksum_v4`.
- `load_save`: add a `version == 4` branch — validate `checksum_v4`, read `out.lives = s.lives`, then call `clamp_lives_on_load(out)`. Keep the existing `version == 1/2/3` branches; in EACH of them set `out.lives = STARTING_LIVES` (those formats predate lives) — this is the **v1/v2/v3 → v4 migration** (existing saves keep all progress and get 3 lives).
- Do NOT use raw SRAM access (IMPL-4) — only the existing struct/`bn::sram` path. **VERIFY `engine/save.cpp` handles the new `sizeof(SaveData)` generically:** read `src/engine/save.cpp` and confirm `read_world`/`write_world` use `bn::sram::read(s)`/`write(s)` (or `sizeof(SaveData)`), NOT a hardcoded `16`-byte length or a fixed buffer that would truncate the 20-byte v4 struct. If anything hardcodes 16, fix it to use `sizeof(SaveData)`. (Reading 20 bytes over an old 16-byte v3 save is fine — the version field at offset [4..5] is within the first 16 bytes, so the v3→v4 migration branch fires; the extra bytes are ignored for v3.)

**TESTS** (`test_world_state_v4.cpp`, TDD; TEST-4 — cover migration + corruption):
- v4 round-trip: a World with `lives=2`, some spronks/abilities/latches → `make_save` → `load_save` → identical `lives`, spronks, abilities, latches.
- `sizeof(SaveData)==20`; `SAVE_VERSION==4`.
- **v3→v4 migration:** hand-build a v3-layout buffer (version=3, valid v3 checksum, some spronks/abilities/latches), `load_save` → progress preserved AND `lives == STARTING_LIVES`.
- v1 and v2 migrations still succeed and set `lives == STARTING_LIVES` (keep the existing v1/v2 tests green).
- Corrupt/empty SRAM (bad magic / bad checksum) → `load_save` returns false (caller starts a fresh game with `lives = STARTING_LIVES` by `World{}` default).
- Boot clamp: a v4 buffer with `lives=0` → after load, `lives == max_lives` (clamped).
- [x] failing tests → implement → host_test green → purity clean → commit `feat(logic): SaveData v4 — persist lives + v1/v2/v3 migration (default 3)`.

**After Phase 1:** review (≥3 rounds): the derivation (`max_lives` from spronks), the boundary (`lose_life` to 0 = game over), v4 sizeof/checksum/migration correctness, all prior save tests green, no `bn::` in logic. Update the banner ✅ SHIPPED.

---

## Phase 2 — Engine/scene: death→Game-Over flow + `run_game_over` + main routing

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `src/game/scene_dungeon.cpp` — `RoomOutcome` (~line 56: `ExitDungeon, Quit, Restart, GoToRoom`), the death/respawn block (~747 `if(health.is_empty()){ ... pos=spawn_pos ... invuln=RESPAWN_IFRAMES ... }`), `run_dungeon` (~867) + its `RoomOutcome` switch (`ExitDungeon→Cleared`, `Quit→Quit`, `Restart→refill+replay`, `GoToRoom→…`), the spronk-free (`try_free_spronk`, ~798), and `engine::write_world` usage (heart-collect path). Read `include/game/scene_dungeon.h` (`DungeonResult`), `src/game/scene_title.cpp` (the text-screen scene pattern + `engine::fade_in/out`), `src/main.cpp` (the hub↔dungeon loop + `DungeonResult` handling), `include/engine/save.h`. `main.cpp` is clean (no scaffold) and MUST be edited here for routing.

### Task 2.1: `run_game_over` scene

**Files:** Create `include/game/scene_game_over.h` + `src/game/scene_game_over.cpp`.

Mirror `run_title` (sprite text + key loop + fade). Signature:
```cpp
namespace game {
enum class GameOverChoice { Continue, QuitToTitle };
GameOverChoice run_game_over(const logic::World& world);  // const: it only displays + reads input
}
```
- Show centered text "GAME OVER" and two selectable options: "CONTINUE" and "QUIT TO TITLE". **Use a d-pad cursor menu (PIN this scheme — do NOT use START/SELECT):** Up/Down moves a highlight/`>` cursor between the two options (default highlight = CONTINUE); **A or START confirms** the highlighted option. Render the cursor as a `>` prefix (or brighter text) on the selected line, redrawn when the selection changes. `engine::fade_in(16)` on entry, `engine::fade_out(16)` before returning the chosen `GameOverChoice`.
- No host test (Butano scene). Build the ROM to confirm it compiles.
- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): run_game_over scene (Continue / Quit to title)`.

### Task 2.2: death→lives→Game-Over flow + refill + persist (in `scene_dungeon.cpp`)

**Files:** Modify `src/game/scene_dungeon.cpp`; `include/game/scene_dungeon.h` (add a `DungeonResult` variant).

1. **DungeonResult:** add `QuitToTitle` → `enum class DungeonResult { Cleared, Quit, QuitToTitle, Restart };`. Add `GameOver` to the internal `RoomOutcome` enum (`ExitDungeon, Quit, Restart, GoToRoom, GameOver`).
2. **Death path** (the `if(health.is_empty())` block, ~747): BEFORE the existing respawn, decrement + check:
   ```cpp
   if(health.is_empty()){
       logic::lose_life(world);
       engine::write_world(world);          // persist the decremented count immediately
       // NOTE: this is the FIRST mid-dungeon save (the old code only wrote post-clear with
       // current_dungeon already reset to 0). So the save now holds current_dungeon = n.
       // VERIFY nothing at boot/hub depends on current_dungeon==0 (door_enterable uses
       // spronks_freed; the spronk-rescue `d` is re-set by main before each run_dungeon). It is
       // benign (overwritten on the next dungeon entry) — confirm, don't assume.
       if(world.lives == 0){
           return RoomOutcome{ RoomOutcome::GameOver };   // run_dungeon shows the Game Over scene
       }
       // else: the existing respawn (room entrance, full health, RESPAWN_IFRAMES grace, block reset)
       player.body.pos = spawn_pos; /* ...existing respawn body unchanged... */
   }
   ```
   (`#include "logic/world_state.h"` for `lose_life`; it's already included transitively — verify.)
3. **run_dungeon `RoomOutcome::GameOver` handling:** in the switch (~884), add:
   ```cpp
   case RoomOutcome::GameOver: {
       engine::fade_out(16);
       game::GameOverChoice c = game::run_game_over(world);
       logic::refill_lives(world);          // both choices refill to max
       engine::write_world(world);          // persist the refill (save never holds 0)
       if(c == game::GameOverChoice::QuitToTitle) return DungeonResult::QuitToTitle;
       // Continue: restart THIS dungeon from the start room, vitals refilled EXACTLY like Restart
       cur_room = dungeon.start_room; cur_entrance = 0;
       ps.health.cur = ps.health.max; ps.magic.cur = ps.magic.max;  // mirror the existing Restart case
       break;   // loop re-enters play_room at the start room
   }
   ```
   (Copy the existing `Restart` case's vitals lines verbatim so Continue and Restart refill identically — read the `Restart` case to confirm whether it refills magic to max; match it exactly.)
4. **Refill on clear:** in the `ExitDungeon` case (returns `Cleared`), BEFORE returning, `logic::refill_lives(world);` — the spronk was freed in play_room (so `max_lives` already grew), and `main.cpp` persists on Cleared. (Add `engine::write_world(world)` here too if you want the refill saved independently of main's write; main already writes on Cleared, which persists `world.lives`.)
5. **`#include "game/scene_game_over.h"`** in scene_dungeon.cpp.

**TESTS:** the flow is scene-level (Butano) — not host-unit-testable directly. The lives MATH is covered in Phase 1. Add a pure helper test only if you extract one (e.g. the death-boundary is already tested). Verify via ROM build + Phase 4 QA. Build the ROM.
- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): death decrements lives; 0 -> Game Over (Continue restarts dungeon / Quit to title); refill on clear+continue`.

### Task 2.3: main.cpp routing + boot clamp

**Files:** Modify `src/main.cpp`.

- After `read_world(world)` (and the fresh-game fallback), call `logic::clamp_lives_on_load(world);` so a loaded save never boots into an instant Game Over (and a fresh `World{}` already has `lives=STARTING_LIVES`).
- In the hub↔dungeon loop, handle the new result: after `run_dungeon`, `if(dr == DungeonResult::QuitToTitle){ game::run_title(world); }` then continue the loop (which re-enters `run_hub`). No extra `write_world` here — run_dungeon already refilled + persisted lives before returning QuitToTitle (Task 2.2). Keep the existing `Cleared → write_world` behavior. (`Quit` and `QuitToTitle` both return to the hub loop; `QuitToTitle` shows the title first.)
- Build the ROM.
- [ ] implement → `bash tools/build_rom.sh` → `ROM fixed!` → commit `feat(game): route Game-Over Quit-to-title via main; boot-clamp loaded lives`.

**After Phase 2:** review (≥3 rounds): death decrements + persists; 0→Game Over; Continue restarts at room 0 with lives=max + full health; Quit-to-title → title → reload; refill on clear; the save never persists 0 (refill before return); no infinite Game-Over loop; existing Cleared/Quit/Restart paths intact; D1–D7 still play. Update the banner ✅ SHIPPED.

---

## Phase 3 — HUD lives counter

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `include/engine/hud.h` + `src/engine/hud.cpp` (the health/magic pip rows; `Hud::update(health, magic)`), `include/logic/hud_math.h` + `test/test_hud_math.cpp` (the pure HUD-math + test pattern), and how `Hud::update` is called in `scene_dungeon.cpp` (and `scene_hub.cpp` if it shows the HUD).

### Task 3.1: lives indicator in the HUD

**Files:** Modify `include/engine/hud.h` + `src/engine/hud.cpp`; `include/logic/hud_math.h`; the `Hud::update` call sites (`scene_dungeon.cpp`, and `scene_hub.cpp` if it draws the HUD); Test: `test/test_hud_math.cpp`.

- Pure math (`hud_math.h`): `int lives_display_count(int lives, int cap)` returning `min(max(lives,0), cap)` with `LIVES_HUD_CAP = 12` (max climbs to 11). Host-test the clamp.
- Engine (`hud.cpp`/`hud.h`): **PIN the representation to a row of life-icon pips** — one small life glyph per current life, up to `LIVES_HUD_CAP`, exactly mirroring the existing health/magic pip rows (a pre-created `bn::vector<bn::sprite_ptr, LIVES_HUD_CAP>`, with `lives_display_count` of them set visible). This matches the pip-based HUD and avoids introducing a `sprite_text_generator`/number into the HUD. Use a distinct life glyph sprite — add a small placeholder via `make_placeholder_art.py` (a Laurel-head or simple life icon) if no existing sprite fits; position the row screen-fixed near the health/magic rows. Extend `Hud::update` to `Hud::update(const Meter& health, const Meter& magic, int lives)`; update the call sites (`scene_dungeon.cpp`, and `scene_hub.cpp` if it draws the HUD) to pass `world.lives`.
- TEST (`test_hud_math.cpp`): the lives display count is correct for 0..max and clamps at the cap.
- `bash tools/host_test.sh` green; `bash tools/build_rom.sh` → `ROM fixed!`.
- [ ] failing test → implement → host_test green → ROM → commit `feat(hud): lives counter`.

**After Phase 3:** review (≥3 rounds): the HUD shows the right count, updates on death/refill, fits the screen at max (11+), no `bn::` in the hud_math logic. Update the banner ✅ SHIPPED.

---

## Phase 4 — Emulator QA

**Execution Status:** ⬜ NOT STARTED

QA is manual (handed to the user). Build the QA ROM; a TEMP uncommitted `src/main.cpp` scaffold MAY grant abilities / free spronks to reach later content quickly (clearly commented `REVERT before final commit`) — but lives QA mostly works from a fresh save in D1. Verify:
- Dying decrements the HUD lives counter; respawn at the room entrance still works (M8 grace intact).
- At 0 lives → Game Over scene → **Continue** restarts the current dungeon at room 0 with lives refilled + full health, progress kept; **Quit to title** → title → START reloads the save (hub, progress kept).
- **Refill on dungeon clear:** clearing a dungeon (free spronk) refills lives to the new max.
- **Spronk grants +max:** after freeing a spronk, max lives is one higher (visible as a higher refill ceiling).
- **Persistence:** lives survive power-off (soft-reset the emulator / reload SRAM); a save with depleted lives never boots into an instant Game Over.
- Iterate fixes round by round (TDD any logic fix; tune `STARTING_LIVES` if the economy feels off). After QA passes: revert any scaffold, final full-branch review, then `superpowers:finishing-a-development-branch`.
- [ ] QA rounds → fixes → final review READY TO MERGE → merge.

**After Phase 4:** update the banner ✅ SHIPPED + the top-of-plan table; record QA-driven deviations.

---

## Self-review (against the spec)

- **Spec §2 (lives model)** → Phase 1 (World.lives, max_lives from spronks, helpers) + Phase 2 (death decrement, Game Over at 0, refill on clear/Continue). ✓
- **Spec §3 (Game Over scene + Continue flow)** → Phase 2 (run_game_over, RoomOutcome::GameOver, DungeonResult::QuitToTitle, main routing). ✓
- **Spec §4 (HUD)** → Phase 3. ✓
- **Spec §5 (save v4)** → Task 1.2 (v4 + migration + boot clamp). ✓
- **Spec §6 (new vs reused, purity)** → Phase 1 pure/host-tested; Phases 2–3 bn::; no new compiler symbol/pickup. ✓
- **Spec §8 (testing)** → Task 1.1/1.2 host tests (lives math, v4 round-trip + migration, corruption, boot clamp) + Phase 3 HUD math. TEST-4 covered. ✓
- **Type consistency:** `World.lives`, `STARTING_LIVES`, `max_lives()`, `refill_lives()`, `lose_life()`, `clamp_lives_on_load()` used consistently across 1.1→1.2→2.2→2.3; `GameOverChoice{Continue,QuitToTitle}`, `RoomOutcome::GameOver`, `DungeonResult::QuitToTitle`, `run_game_over` consistent across 2.1→2.2→2.3. ✓
- **No save corruption:** v4 sizeof 20 + checksum_v4 + every prior version migrates + boot clamp; corrupted SRAM → fresh game. ✓
