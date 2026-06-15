# M10 — Gloom Spire (Dungeon 8) + Light Spell Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the final dungeon (Gloom Spire) + the **Light spell**: a cast (L-cycle/R-fire, costs magic) that clears **DarkVeil** gates and, in a room-wide flash, reveals **hidden platforms** (solid+visible) for a few seconds before they fade. A dark vertical **ascent** climbed by timing Light casts against the fading platforms, recharging via respawning **magic crystals**. Freeing spronk 8 earns Light + completes the 8-dungeon spine.

**Architecture:** Light is a new `SpellId::Light` threaded through the existing spell system; it clears DarkVeil via the EXISTING gate-clear loop (`gate_cleared_by(DarkVeil)→Light` + `consume_hit`). The room-wide reveal is a pure `RevealState` timer driving the scene to toggle **hidden-platform** collision/visibility. **Magic crystals** are a respawning full-refill pickup (reset each attempt) so Light's magic cost never soft-locks. Dungeon 8 is a room-to-room ascent; freeing spronk 8 teases the King (boss = next milestone, OUT of scope). No save-format change (Light in the `abilities` bitmask; spronk 8 in `spronks_freed`; latches for shortcuts).

**Tech Stack:** C++17 (three-layer: pure `logic/`, Butano `engine/`, scenes `game/`), Butano 21.6.0 (GBA), Python 3 level compiler, host tests (`bash tools/host_test.sh`), ROM (`bash tools/build_rom.sh`).

**Design spec:** `docs/superpowers/specs/2026-06-15-spronk-quest-m10-gloom-spire-design.md`

**Branch:** create `feat/m10-gloom-spire` off `main` before Task 1.1.

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

**Overall:** Not started.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure logic: SpellId::Light + RevealState + spawns | ⬜ Not started | — | — |
| 2 — Level compiler: hidden-platform + magic-crystal symbols | ⬜ Not started | — | — |
| 3 — Engine/scene: Light cast (DarkVeil + reveal) + crystal + sprites | ⬜ Not started | — | — |
| 4 — Content: Gloom Spire 3 rooms + hub Door 8 + no-soft-lock invariants + QA | ⬜ Not started | — | — |

### Deviations
- _(none yet)_

### Discoveries
- _(none yet)_

---

## Conventions every task must follow

- **Three-layer rule** (`CLAUDE.md`): nothing under `include/logic/` or `src/logic/` may include or name a `bn::` type. Phases 1–2 host-testable; Phases 3–4 are the `bn::` work.
- **Host tests:** `bash tools/host_test.sh` (NOT bare `make -C test`). Green ends `N/N tests passed, 0 checks failed`. Also runs the purity guard + Python compiler tests + regenerates level headers.
- **ROM build:** `bash tools/build_rom.sh` (plain `make` fails — `DEVKITPRO` misconfigured). Clean build prints `ROM fixed!`.
- **Purity guard** before any logic commit: `python tools/check_logic_purity.py`.
- **Pitfalls:** read `docs/pitfalls/implementation-pitfalls.md` + `docs/pitfalls/testing-pitfalls.md`. Relevant: IMPL-1 (no `bn::` in logic), IMPL-2 (integer-only — `RevealState` is an int timer), TEST-1 (explicit per-tick stepping — test `RevealState` by ticking, not timing), TEST-2 (assert exact ints).
- **TDD mandate (every code task):**
  ```
  BEFORE starting work:
  1. Invoke superpowers:test-driven-development
  2. Read docs/pitfalls/testing-pitfalls.md
  Follow TDD: write failing test → implement → verify green.

  BEFORE marking this task complete:
  1. Review tests against docs/pitfalls/testing-pitfalls.md
  2. Verify coverage (error paths? edge cases?)
  3. Run tests and confirm green
  ```
- **After each phase:** review the batch from multiple perspectives, minimum 3 rounds; continue until clean.
- **Single committer:** one implementer per task; commit before the next. No parallel implementers on overlapping files.

---

## Phase 1 — Pure logic: SpellId::Light + SpellState threading + RevealState + spawns

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `include/logic/spell.h` (`SpellId` enum + `SpellState` owns/has_any/cycle/refresh/ensure_valid — all hardcode Fire/Ice/Grapple), `include/logic/gates.h` (`GateType::DarkVeil` = `{Ability::Light, false, 12}`; `spell_for_ability` maps only Fire/Ice; `gate_cleared_by`), `include/logic/world_state.h` (`Ability::Light`=7), `include/logic/level_data.h` (the `LoosePlatformSpawn`/`HeartContainerSpawn` + nullable-array+count pattern), `include/logic/dash.h` (timer-state style), and `test/test_spell.cpp` + `test/test_gates.cpp` for test style.

### Task 1.1: Add `SpellId::Light` + thread through `SpellState` + `spell_for_ability`

**Files:** `include/logic/spell.h`, `include/logic/gates.h`; Test: `test/test_spell.cpp`, `test/test_gates.cpp`.
- `spell.h`: `enum class SpellId : uint8_t { None=0, Fire, Ice, Grapple, Light };`. Extend `SpellState`:
  - `owns`: add `|| (s==SpellId::Light && w.has(Ability::Light))`.
  - `has_any`: add `|| owns(w,SpellId::Light)`.
  - `refresh`: append `else if(owns(w,SpellId::Light)) selected = SpellId::Light;` AFTER the Grapple branch (Fire/Ice/Grapple preferred first; Light last).
  - `cycle`: extend to 4 — `SpellId order[4] = { Fire, Ice, Grapple, Light };`, `int start = (selected==Ice)?1 : (selected==Grapple)?2 : (selected==Light)?3 : 0;`, loop `for(int i=1;i<=4;++i){ SpellId c = order[(start+i)%4]; if(owns(w,c)){ selected=c; return; } }`.
  - `ensure_valid` needs NO change (it calls owns/refresh).
- `gates.h` `spell_for_ability`: add `if(a==Ability::Light) return SpellId::Light;` (so `gate_cleared_by(DarkVeil)` returns `SpellId::Light`).
- **TESTS:** owning Light → `cycle` reaches Light (Fire→Ice→Grapple→Light→Fire); `owns`/`has_any`/`refresh` include Light; a Light-only world refreshes to Light; `spell_for_ability(Ability::Light)==SpellId::Light`; `gate_cleared_by(GateType::DarkVeil)==SpellId::Light` (and stays None for geometry gates). Keep existing Fire/Ice/Grapple spell tests green.
- [ ] failing tests → implement → `bash tools/host_test.sh` green → purity clean → commit `feat(logic): SpellId::Light threaded through SpellState + spell_for_ability (clears DarkVeil)`.

### Task 1.2: `RevealState` timer (host-tested)

**Files:** Create `include/logic/reveal.h`; Test: create `test/test_reveal.cpp`. Mirror `DashState` (pure, integer, no includes needed).
```cpp
#pragma once
namespace logic {
// M10 Light reveal: a room-wide timer. A Light cast (re)starts it; while >0 the room's hidden
// platforms are solid+visible (the scene toggles collision/sprites). Pure logic, integer-only.
struct RevealState {
    static constexpr int REVEAL_FRAMES = 200;  // ~3.3s at 60fps (feel-tunable in QA)
    int timer = 0;
    void on_cast(){ timer = REVEAL_FRAMES; }          // a Light cast lights the room (refreshes)
    void tick(){ if(timer > 0) --timer; }             // call once per frame
    bool revealed() const { return timer > 0; }
};
}
```
- **TESTS:** default not revealed; `on_cast` → revealed, timer==REVEAL_FRAMES; `tick` decrements; `revealed()` true while >0 and false at 0; exact-frame decay boundary (REVEAL_FRAMES ticks → still revealed at the last, not revealed after the (REVEAL_FRAMES+1)th); `on_cast` mid-decay resets to REVEAL_FRAMES.
- [ ] failing tests → implement → host_test green → purity clean → commit `feat(logic): RevealState timer for the Light room-reveal`.

### Task 1.3: `HiddenPlatformSpawn` + `MagicCrystalSpawn` (level-data, host-tested)

**Files:** `include/logic/level_data.h`; Test: `test/test_level_data.cpp`.
- Add `struct HiddenPlatformSpawn { int tx, ty, len; };` + `const HiddenPlatformSpawn* hidden_platforms = nullptr; int hidden_platform_count = 0;` to `LevelData`.
- Add `struct MagicCrystalSpawn { int tx, ty; };` + `const MagicCrystalSpawn* magic_crystals = nullptr; int magic_crystal_count = 0;` to `LevelData`.
- Mirror the existing nullable-array+count fields (e.g. `loose_platforms`, `heart_containers`). Defaults null/0 so existing levels are unaffected.
- **TESTS:** construct a `LevelData` with a hidden platform (tx,ty,len) + a magic crystal (tx,ty); read them back; assert defaults (null/0) when omitted.
- [ ] failing tests → implement → host_test green → purity clean → commit `feat(logic): level-data spawns for hidden platforms + magic crystals`.

**After Phase 1:** review (≥3 rounds): Light threads through every SpellState method + the cycle order; `gate_cleared_by(DarkVeil)==Light`; RevealState decay boundary exact; spawn defaults preserve existing levels; no `bn::`; no float. Update banner ✅ SHIPPED.

---

## Phase 2 — Level compiler: hidden-platform + magic-crystal symbols

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `tools/build_level.py` (the `TILE`/`CONTENT` symbol sets, the per-symbol loop, the JSON sidecar mapping; study how `:`/loose-platforms with a JSON `len` and `$`-style content symbols are parsed + how `heart_containers`/`loose_platforms` arrays are emitted into `*_DATA`) and `tools/test_build_level.py`. **Header-regen churn:** adding `LevelData` arrays changes `emit_header`, regenerating EVERY level header — commit the regenerated `*.h` in a `chore(levels): regenerate headers` commit (as M8 did), so the tree stays clean.

### Task 2.1: hidden-platform `h` + magic-crystal symbols

**Files:** `tools/build_level.py`, `tools/test_build_level.py`.
- **Hidden platform:** add UNUSED symbol **`h`** (verify against TILE+CONTENT) → `HiddenPlatformSpawn(tx,ty,len)` where `len` comes from a JSON `"hidden_platforms":[{"len":N}]` list (default 1, scan-order indexed like `loose_platforms`). Emit `*_HIDDEN_PLATFORMS[]` + wire `hidden_platforms`/`hidden_platform_count` into `*_DATA`.
- **Magic crystal:** add UNUSED symbol (suggested **`$`**, verify) → `MagicCrystalSpawn(tx,ty)` (no JSON needed). Emit `*_MAGIC_CRYSTALS[]` + wire `magic_crystals`/`magic_crystal_count`.
- Both follow the `loose_platforms`/`heart_containers` emit pattern exactly. Levels without them: null/0.
- **TESTS** (`test_build_level.py`): `h` + `{"hidden_platforms":[{"len":3}]}` → a hidden platform (tx,ty,3); `h` without JSON → len 1; `$` → a magic crystal at the right tile; levels without them compile.
- Run `bash tools/host_test.sh` (regenerates + compiles all levels — expect green); commit the symbol change, then the header-regen chore commit.
- [ ] failing tests → implement → unittest + host_test green → commit `feat(compiler): hidden-platform 'h' + magic-crystal '$' symbols` (+ `chore(levels): regenerate headers for hidden-platform + magic-crystal fields`).

**After Phase 2:** review (≥3 rounds): symbols don't collide; emit order matches `LevelData` field order; existing levels still compile + pass. Update banner ✅ SHIPPED.

---

## Phase 3 — Engine/scene: Light cast (DarkVeil + reveal) + crystal + sprites

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read `docs/pitfalls/implementation-pitfalls.md`. READ in `src/game/scene_dungeon.cpp`: the spell-icon (`~182-193`, `refresh_spell_icon` branches Fire/Ice/Grapple), `cast_spell` (`~487`: `si.cast && (Fire||Ice)`), `spells.update_and_cast(...)` (`~610`), the gate-clear loop (`~614-617`: `gate_cleared_by` + `consume_hit` + `open_column` + `persist_latch`), the loose-platform spawn + per-frame handling (`~320-324`, `~578-603`: `set_collision_tile`+sprite toggling), the heart-container spawn+overlap (`~267-290`, collect block) + its reset-on-respawn (the death block's block-reset, M8), and the death/respawn block (`~747`). Read `src/engine/spell_pool.cpp` (`update_and_cast` creates the projectile sprite, branching Ice vs Fire — `~create_sprite`). Read `include/engine/spell_pool.h`. Do NOT modify `src/logic/`/`include/logic/` (Phases 1–2 frozen).

### Task 3.1: Light projectile + cast + DarkVeil clearing + HUD icon
**Files:** `src/engine/spell_pool.cpp`, `src/game/scene_dungeon.cpp`, `tools/make_placeholder_art.py` (+ `graphics/light_proj.*`, a Light HUD glyph if separate).
- **Light projectile sprite:** add a `graphics/light_proj.bmp/.json` (a bright/white-yellow bolt, distinct from fire/ice) via `make_placeholder_art.py`. In `spell_pool.cpp` `update_and_cast`, extend the sprite-creation branch: `selected==Light → bn::sprite_items::light_proj`.
- **Cast:** in `scene_dungeon.cpp`, extend `cast_spell` to `si.cast && (Fire||Ice||Light)` so a Light cast fires a projectile (spends magic via `update_and_cast`).
- **DarkVeil clearing:** NO new code in the gate-clear loop — it already does `clears = gate_cleared_by(gi.spawn.type); if(consume_hit(gi.body, clears))...`. With `gate_cleared_by(DarkVeil)==Light` (Phase 1) and the Light shot tagged `SpellId::Light`, a Light shot hitting a DarkVeil gate clears it automatically. VERIFY by reading the loop (no change expected; confirm the DarkVeil GateInst is built + `consume_hit` matches the Light tag).
- **HUD icon:** in `refresh_spell_icon`, add `else if(spell.selected == logic::SpellId::Light) spell_icon.set_item(bn::sprite_items::light_proj);` (or a dedicated light icon).
- Build ROM. Commit: `feat(game): Light spell — cast a Light projectile that clears DarkVeil gates + HUD icon`.

### Task 3.2: room-wide timed reveal of hidden platforms
**Files:** `src/game/scene_dungeon.cpp`.
- Spawn hidden platforms (mirror `LoosePlatformInst`): for each `level.hidden_platforms[i]`, create `len` tiles at `(tx..tx+len-1, ty)` as **NON-solid + invisible** (do NOT `set_collision_tile(...,1)` at spawn; create sprites but `set_visible(false)`). Track in a `bn::vector<HiddenPlatformInst,…>`.
- Add a `logic::RevealState reveal;` to the room loop. **Detect a successful Light cast this frame** and call `reveal.on_cast()`. **PIN this approach:** make `SpellPool::update_and_cast` return whether it fired this frame (a `bool`, or the cast `SpellId`) — `try_cast` already returns success internally, so plumb it out — and call `reveal.on_cast()` when the returned/fired spell is `Light`. (Do NOT use a `magic.cur` before/after delta to infer the cast: the magic-crystal refill (Task 3.3) can change `magic.cur` the same frame and would corrupt the inference. A returned fired-flag is order-independent and unambiguous.)
- Each frame: `reveal.tick();`. When `reveal.revealed()` and the platforms are currently hidden → make them solid (`set_collision_tile(...,1)`) + sprites visible. When `!reveal.revealed()` and currently shown → make them non-solid (`set_collision_tile(...,0)`) + sprites invisible. (Toggle on the edge; don't thrash every frame.)
- **Player-safety note:** when platforms revert to non-solid, a player standing on one falls — that's intended (the M9 lives/respawn + START-reset handle it; the Phase-4 invariants keep it escapable). Do NOT special-case the player.
- Build ROM. Commit: `feat(game): Light reveals hidden platforms (solid+visible) for a timed window, refreshed per cast`.

### Task 3.3: magic crystal (respawning full-refill) + reset-on-respawn
**Files:** `src/game/scene_dungeon.cpp`, `tools/make_placeholder_art.py` (+ `graphics/magic_crystal.*`).
- Add a `graphics/magic_crystal.bmp/.json` (a cyan/blue crystal, distinct). Spawn each `level.magic_crystals[i]` as a `MagicCrystalInst{tx,ty,collected=false,sprite}` (sprite grounded/placed at the tile like the heart-container sprite). 
- On per-frame overlap (`aabb_overlap(player.body, crystal tile_body)`) && `!collected` → `magic.cur = magic.max;` set `collected=true`, hide sprite. (`magic` is `ps.magic`.)
- **Reset each attempt (NOT latched):** in the death/respawn block (where pushable blocks reset, M8), also reset crystals → `collected=false`, sprite visible. (Crystals also naturally respawn on room-load since they're rebuilt from `level.magic_crystals` each `play_room`.) This guarantees a crystal is available every attempt → no magic soft-lock.
- Build ROM. Commit: `feat(game): magic crystal — full magic refill, respawns each attempt (no magic soft-lock)`.

**After Phase 3:** review (≥3 rounds): Light fires + clears DarkVeil; the reveal toggles hidden-platform collision+visibility on the timer (and reverts); a Light cast refreshes the timer; crystals refill magic + reset on respawn; the Light shot is tagged Light (so `consume_hit` clears DarkVeil); D1–D7 unregressed (they have no hidden platforms/crystals/Light — empty vectors, and no DarkVeil reachable-by-Light since those players lack Light). `bash tools/build_rom.sh` → `ROM fixed!`; `bash tools/host_test.sh` green. Update banner ✅ SHIPPED.

---

## Phase 4 — Content: Gloom Spire 3 rooms + hub Door 8 + no-soft-lock invariants + QA

**Execution Status:** ⬜ NOT STARTED

**BEFORE starting:** read the D7 room files (`tools/levels/dungeon7_room*.txt/.json`) + `include/game/levels/dungeons.h` (`DUNGEON7_DUNGEON`) as the template, and `test/test_dungeon7_level.cpp` for the invariant harness (the 2-wide flood-fill, `build_grid_no_pound`/`freeze_water` style helpers). Honor the room/camera constraint (≥30 wide × 22 tall, standard floor, two-way doors + the M9 hub-return `Q` door).

### Task 4.1: author the 3 Gloom Spire rooms
**Files:** Create `tools/levels/dungeon8_room{0,1,2}.txt`+`.json` → `include/game/levels/dungeon8_room{0,1,2}.h`; add `DUNGEON8_DUNGEON` to `dungeons.h` (`{ DUNGEON8_ROOMS, 3, 0 }`, mirroring `DUNGEON7_DUNGEON`).
- **Room 0 — Spire Base:** Light `F` shrine (`{"pickups":[{"ability":"light"}]}`); a tutorial timed-reveal climb — a short hidden-platform staircase `h` you cast Light to reveal + climb before it fades — with a **magic crystal `$`** at its base; a **DarkVeil gate `G`** (`{"gates":[{"type":"dark_veil"}]}`) cast Light to clear; a magic-source enemy `o`. Two-way doors to Room 1 + Room 2, hub-return `Q`. **Shrine placement:** put the Light `F` shrine base-reachable AND on the path BEFORE the Light-gated beats (the forward progression — the hidden-platform climb / DarkVeil gate — gates the way deeper), so the player necessarily earns Light before needing it. The two-way doors are the backtrack safety (a player who wanders into Room 1/2 early can return for the shrine). This split is exactly what `d8_light_shrine_reachable_without_light` (shrine base-reachable) + `d8_spronk_requires_light` (spronk needs Light) machine-check.
- **Room 1 — branch:** a DarkVeil-gated section + a **latched shortcut** (a latched `CrackedFloor`/gate, persisted). One carried-kit combo (Stone-pound or Grapple to reach a DarkVeil gate, then Light to clear). Two-way return.
- **Room 2 — The Ascent (spronk):** the climactic vertical timed-reveal climb — hidden-platform `h` sequences up the spire, **magic crystals `$` spaced up the climb**, interleaved carried-kit beats (grapple anchor `g`, glide gap, freeze hazard). The **spronk `C` + exit `E`** at the top. Two-way return.
- Each room: exactly one `@`, ≥30×22, solid border. Compile each header + add to dungeons.h. `bash tools/host_test.sh` (compiles).
- [ ] author → compile → host_test green → commit `feat(content): Gloom Spire — 3-room Dungeon 8 (Light ascent)`.

### Task 4.2: hub Door 8 + main dispatch
**Files:** `src/game/scene_hub.cpp` (`door_enterable`), `src/main.cpp`, `tools/levels/hub.txt`+`include/game/levels/hub.h`, `test/test_hub_level.cpp`.
- `door_enterable`: add `|| (n == 8 && w.spronk_freed(7))`.
- `main.cpp`: add `else if(n == 8) lvl = &DUNGEON8_DUNGEON;` (and remove the now-obsolete `else continue; // door 8 not built` comment/branch — door 8 IS built now; keep the `else continue` for safety on any out-of-range n).
- `hub.txt`: the M8 hub reorg placed exactly **7** evenly-spaced doors (1–7) — there is **no Door 8**. Add it by **RE-SPACING all 8 doors evenly** across the plaza (recompute the pitch for 8, widening the hub if needed to keep ≥30 wide + camera-valid + grounded 2-wide archways). Recompile `hub.h`. **Update `test_hub_level.cpp`** — the even-spacing/door-count invariant (currently expects 7 at a fixed pitch) must be updated to 8. **Verify the M9 spawn-at-exited-door logic still works** with the new door positions (it looks up the `DoorSpawn` for `ps.last_dungeon` by dungeon number, so moving doors is fine — just confirm Door 8's spawn lands correctly).
- `bash tools/build_rom.sh` → `ROM fixed!`. Commit: `feat(game): hub Door 8 → Gloom Spire (opens after D7 spronk)`.

### Task 4.3: level + no-soft-lock invariants (2-wide player)
**Files:** Create `test/test_dungeon8_level.cpp` (reuse the D7 flood-fill harness pattern). Assert against the compiled `DUNGEON8_ROOM*` data.
Standard (D7 style): shrine + spronk grounded; two-way doors with co-located return entrances; rooms ≥30×22 + border; reachability.
**No-soft-lock invariants (REQUIRED; each must FAIL on a broken layout, then restore):**
- `d8_light_shrine_reachable_without_light`: the Light `F` shrine is in the **base-reachable** set (hidden platforms non-solid, DarkVeil closed) — so the player can EARN Light before any Light-gated beat. (Without this the dungeon is unsolvable: Light-gated content before you can get Light. Same class as D6's grapple-shrine-before-grapple-beats.)
- `d8_spronk_requires_light`: with the BASE movement kit AND hidden platforms treated NON-solid + DarkVeil gates CLOSED, the spronk/exit (and the gated path BEYOND the shrine) are NOT reachable (Light is genuinely required); they BECOME reachable when hidden platforms are treated solid + DarkVeil open. (Add a `reveal_hidden`/`open_darkveil` grid variant to the flood-fill, paralleling D7's `freeze_water`/`open_heavy_gate`.)
- `d8_magic_crystal_before_each_light_beat`: for each Light-gated climb (each hidden-platform region), a **magic crystal `$` is reachable by base movement before it** — a crystal is the GUARANTEED combat-free full refill (an enemy is only a magic source if you stop to kill it, so it does NOT satisfy the no-magic-soft-lock guarantee). Assert a crystal is in the base-reachable set leading into each hidden-platform region.
- `d8_no_pit_traps` / `d8_no_one_way_traps` / `d8_reveal_climb_over_safe_ground`: the M8/M9 guards hold — no region is enter-but-not-leave except the intended terminal. AND for each hidden-platform climb, the **fall-zone directly below it is NON-hazard safe ground** (Lava/Water/Spikes-free) so a fade-fall is a *retry* (fall back to a safe ledge/floor + re-cast), not death-by-hazard. (A fade-fall onto safe ground + the M9 lives/respawn is the intended failure mode; a fade-fall into lava is not.) Assert by scanning the columns under each `h` region down to the first solid and checking it isn't a hazard.
- Keep the grounded-shrine/spronk + two-way-door checks. Reuse ONE flood-fill helper.
```
BEFORE marking complete: each soft-lock invariant MUST be able to FAIL on a deliberately-broken
layout (verify by temporarily breaking a room, seeing red, restoring). A vacuous always-pass
"soft-lock test" is worse than none. If an invariant can't be expressed structurally, STOP and raise.
```
- [ ] write invariants → verify each FAILS on a broken layout → `bash tools/host_test.sh` green → commit `test(content): D8 level + no-soft-lock invariants (requires_light, magic-source-before-light-beat)`.

### Task 4.4: emulator QA + balance
- Build the QA ROM; a TEMP uncommitted `src/main.cpp` scaffold MAY grant the full kit + free spronks 1–7 to reach Door 8 quickly (clearly commented `REVERT before final commit`). Light itself is earned from the room-0 shrine.
- Verify: Light cast clears DarkVeil gates; a cast reveals hidden platforms (solid+visible) for the window, then they fade; re-casting refreshes (costs magic); magic crystals fully refill + respawn each attempt; the timed-reveal ascent is completable; freeing spronk 8 earns Light + completes the dungeon (+ the King teaser if added). Probe for soft-locks (fall during a climb → respawn escapable; can't get stuck out of magic).
- **Balance the ascent ("not too easy"):** tune `RevealState::REVEAL_FRAMES`, crystal spacing, and the Light magic cost / magic max so it's challenging but solvable. Iterate round by round (TDD any logic fix).
- After QA passes: revert the scaffold, final full-branch review, then `superpowers:finishing-a-development-branch`.
- [ ] QA rounds → fixes/balance → scaffold reverted → final review READY TO MERGE → merge.

**After Phase 4:** update the banner ✅ SHIPPED + the top-of-plan table; record QA-driven deviations (esp. balance tuning).

---

## Self-review (against the spec)

- **Spec §2 (Light spell)** → Phase 1.1 (SpellId::Light + spell_for_ability) + Phase 3.1 (cast + projectile + DarkVeil clear + icon). ✓
- **Spec §3 (hidden platforms)** → Phase 1.2 (RevealState) + 1.3 (spawn) + 2.1 (symbol) + 3.2 (timed reveal toggle). ✓
- **Spec §4 (magic crystal)** → Phase 1.3 (spawn) + 2.1 (symbol) + 3.3 (refill + reset-on-respawn). ✓
- **Spec §5 (3-room ascent + hub Door 8)** → Phase 4.1 + 4.2. ✓
- **Spec §6 (new vs reused, purity)** → Phases 1–2 pure/host-tested; 3–4 bn::. ✓
- **Spec §7 (King out of scope)** → Phase 4.1 (teaser stub only; no boss). ✓
- **Spec §8 (testing)** → 1.1/1.2/1.3 host tests + 2.1 compiler tests + 4.3 no-soft-lock invariants (requires_light, magic-source). ✓
- **No save change:** Light in `abilities` bit 7; spronk 8 in `spronks_freed`; latches for shortcut — no SaveData edit. ✓
- **Type consistency:** `SpellId::Light`, `RevealState`/`REVEAL_FRAMES`/`on_cast`/`tick`/`revealed`, `HiddenPlatformSpawn{tx,ty,len}`, `MagicCrystalSpawn{tx,ty}` used consistently across 1.x→2.1→3.x→4.x. ✓
