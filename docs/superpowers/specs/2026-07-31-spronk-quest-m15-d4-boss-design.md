# Spronk Quest — Milestone 15 (D4 Gale Cliffs Boss: "Galewing") Design

**Platform:** Game Boy Advance (Butano / devkitARM)
**Date:** 2026-07-31
**Builds on:** the UNIFIED boss framework (health-review remediation Phase 5, merged to `main`
`35d1b2c`). One data-driven `game::run_boss_fight` (`src/game/boss_fight.cpp`) serves every boss;
`BossDef` (`include/logic/boss.h`) is the single description. See
`2026-06-26-spronk-quest-m13-d2-boss-design.md` (the closest precedent — the last 1→3-room
restructure) and `docs/plans/2026-07-16-maintainability-health-review-remediation-plan.md` Phase 5.
**Status:** Design — approved, pending implementation plan.

---

## 1. Scope (decided)

M15 adds the **fourth per-dungeon boss**: **"Galewing"** in D4 (Gale Cliffs), and restructures D4
from a single room into three. It is the first **airborne** boss, the first fight whose arena cannot
be traversed without the host dungeon's own ability, and the first use of the `AlwaysVulnerable`
vuln model on a room boss.

**In scope:** one boss (`D4_DEF`), a new `Locomotion::Hovering` movement mode, three additive
`BossDef` fields (`hover_row`, `move_vel_raw`, `aim_horizontal`), the four supporting edits in
`run_boss_fight`, placeholder art, the D4 1→3-room restructure, and tests.

**Explicitly OUT of scope:** D5–D8 bosses (each is its own milestone); enlarging D4's existing
ascent shaft beyond re-terminating its top; audio; final art; story beyond the dialogue lines.
**No save-format change** — `World.boss_defeats` bit 3 is already reserved for D4
(`include/logic/world_state.h:18`, `bit (d-1)` for `d in 1..8`).

**Boss name:** "Galewing" is a placeholder-grade choice, used consistently for the `BossId` value,
the sprite asset, and the dialogue. Renaming it later is a mechanical find-and-replace across those
three sites plus this spec; nothing in the design depends on it.

---

## 2. Why this fight is shaped the way it is

Three facts about the existing code determine the entire design. They were verified against `main`
before the design was fixed, and any of them changing invalidates it.

**2.1 There are no diagonal shots.** `engine::read_aim_dy()` (`src/engine/input.cpp:15`) returns a
muzzle **offset** — `-14` (up held), `+8` (down held), `0` otherwise — which
`PlayerSession::muzzle()` (`src/game/player_session.cpp:111`) adds to the spawn point. The bolt then
travels **horizontally**. A boss more than ~14 px above the player's centre therefore cannot be shot
from the ground *at all*.

This is what makes `AlwaysVulnerable` viable without feeling degenerate: **altitude is the gate.**
No expose state, no tired window, no element cycling — the state machine that other bosses encode in
`VulnMode` is replaced here by the player's physical position in the room.

**2.2 Updrafts require Glide.** `src/logic/player.cpp:49`:

```cpp
if(abilities.glide && in.glide_held && !body.on_ground){
    if(updraft_overlap(body, map)) body.vel.y = UPDRAFT_VY;   // -3 px/frame
    else if(body.vel.y > GLIDE_VY) body.vel.y = GLIDE_VY;
}
```

Updraft overrides glide overrides gravity, and the whole block is gated on the Glide ability *and*
the button being held. An updraft-based arena is therefore impassable without D4's own reward. That
is a tighter ability↔boss coupling than D2 (Fire used *on* the boss) or D3 (Fire/Ice used *on* the
boss): here the ability is what gets the player into weapons range in the first place.

Horizontal wind (`'<'` / `'>'`, `logic::wind_dir`) pushes **regardless of ability**, so wind tiles
remain usable as a universal hazard or assist.

**2.3 A trap in the current aim derivation.** `src/game/boss_fight.cpp:179` reads:

```cpp
const bool aim_full = (def.perch_count == 0);
```

A *perched* boss (only the King today) fires horizontal bolts at its own height; a perch-less boss
aims a velocity vector at the player. A hovering, perch-less boss inherits the behavior it needs —
but by accident, from an unrelated field. §5 replaces the derivation with an explicit def field.

---

## 3. Combat design

### 3.1 The governing rule, and the number behind it

**No standing surface exists within 11 tiles below the boss's hover line.** The player can never
reach boss altitude from a ledge — not by standing, and not by jumping — so landing a hit always
requires the updraft-and-glide loop.

11 is derived, not guessed. From a standing surface the player's maximum bolt reach upward is:

| Term | Value | Source |
|------|-------|--------|
| Featherleap double jump | 2 × 3.5 = **7.0 tiles** | `JUMP_VY = -812` raw → "single jump ~3.5 tiles" (`src/logic/player.cpp:8`). The player owns Featherleap from D1, so both jumps are always available. |
| Up-aimed muzzle offset | 14 px = **1.75 tiles** | `read_aim_dy()` (`src/engine/input.cpp:16`) |
| Boss hitbox half-height | 16 px = **2.0 tiles** | `boss_body.half_h = fx(16)` (`src/game/boss_fight.cpp:91`) |
| **Total** | **10.75 → 11** | |

If emulator QA shows a ledge still permits a jump-shot, the constant is raised in one place (§7) and
the arena rows move with it.

**The mechanics enforce commitment on their own, too.** An updraft cannot be used to *park* at boss
altitude: holding glide inside a column keeps the player rising at 3 px/frame until the ceiling, and
releasing glide drops them. Boss altitude is therefore always a transient the player passes through —
either rising fast, or gliding down through it at `GLIDE_VY` (1 px/frame), which is the intended
firing window and the fight's core skill expression.

### 3.2 Arena geometry and the loop

32 × 24 tiles. Floor at row 22 (`run_boss_fight` grounds non-hovering bodies at `(level.h - 2) * 8`).
Hover line at **row 5**; three ledges at **rows 16–19**, all ≥ 11 tiles clear of it. Updraft columns
run from the floor to ~row 4. The 17-tile floor-to-boss span fits the 20-tile-tall viewport, so a
player standing on the floor can see the boss and read its drift before committing.

```
 row  0  ################################
      1  #..............................#
      5  #.......~~~ BOSS drifts ~~~....#  <- hover line (rows 4-6 hitbox)
      8  #.....u..................u.....#  |
     12  #.....u..................u.....#  |  open glide corridor
     15  #.....u..................u.....#  |  (no surfaces: 11-tile rule)
     16  #..___u.......______.....u___..#  <- ledges (rows 16-19)
     19  #.....u..................u.....#
     22  #.@.ND u ..............u ..ND..#  <- floor
     23  ################################
```

1. Hold A inside an updraft column to rise at 3 px/frame.
2. Break out of the column at altitude and glide horizontally toward the drifting boss.
3. Fire one or two horizontal bolts while gliding down through its altitude band.
4. Fall back as it repositions; re-read the drift; climb again.

The descent and the re-climb are **not rest periods**: aimed bolts track the player downward, and
rockfall pressures the floor and the updraft columns.

### 3.3 Vulnerability — `AlwaysVulnerable`

Any bolt wounds it. No expose window, no `expose_spell`, no `expose_frames`. `hit_iframes = 60`
(one second) is the only rate limit, and it exists to stop a single lucky glide line from deleting a
whole phase.

Consequence for rendering: `boss_has_frames` in `run_boss_fight` swaps sprite frames **on expose
only**. An AlwaysVulnerable boss never exposes, so the sprite is deliberately **single-frame** —
extra frames would be dead weight, exactly as they are for the King.

### 3.4 Movement — `Locomotion::Hovering`

A horizontal drift at a fixed altitude, ignoring gravity and the floor. Mechanically it is `Pacing`
with a different Y origin — the same sub-pixel `Fixed` accumulation, the same reverse-at-the-walls
bounds — which is why §5 widens the existing pacing branch instead of adding a parallel one.

Drift, rather than the King's teleport, is a deliberate game-feel decision. The player's approach is
a slow, committed glide; a target that warps mid-approach invalidates the exact skill the fight
tests, and would read as arbitrary. Teleport suits the King because there the player is grounded and
nimble and the *boss* is avoiding being a sitting duck. Here the roles are reversed.

Drift speed is data (`move_vel_raw`), defaulting to today's hardcoded 0.5 px/frame so D2's pacing is
bit-identical, with D4 tuned upward from there at the emulator.

### 3.5 Attacks — 2 phases

| Phase | `end_hp` | Attacks | `proj_speed` | `rock_count` |
|-------|----------|---------|--------------|--------------|
| P1 | 40 | `AIMED \| ROCKFALL` | 2 | 3 |
| P2 | 0  | `AIMED \| ROCKFALL \| FAN` | 3 | 5 |

`max_hp` 80, `wound_dmg` 10 → eight successful approaches to kill. The rockfall numbers are D2's
shipped, QA-tuned values used as a starting point.

**Aimed bolts** are the answer to "what threatens a grounded player" — they track downward through
the climb (enabled by `aim_horizontal = false`, §5).

**Rockfall reskinned as wind-flung debris** is reused rather than replaced by a new gust attack: it
already drops from the ceiling on a fair ground telegraph with a guaranteed dodge lane
(`logic::rockfall_columns`), it is tuned, and its threat shape — a *downward* attack — is precisely
what pressures a player who is grounded or riding a column.

**Known QA risk:** rockfall makes the boss leap before the rocks drop (`rockfall.leap_offset()`). On
a hovering boss that becomes a wing-beat bob rather than a jump. The expectation is that it reads
well; it is the first thing to verify at the emulator, and the fallback is to zero the offset for
`Hovering`.

### 3.6 Magic economy — none needed

`BlockMode::None` (dodge-only). The basic bolt is free, Glide is a movement `Ability` rather than a
`SpellId`, and wounding requires no spell — so the fight has **no magic dependency at all**. No
crystal in the arena, and no block-to-charge economy. This is the first arena that needs neither.

---

## 4. Level structure (D4: 1 room → 3 rooms; mirrors D2/D3)

D4 today is a single 30×56 vertical shaft: `@` spawn and `Q` hub-return at the bottom, the Glide
shrine partway up, one patroller, an updraft column and wind-push tiles carrying the player upward
past spike banks, and the spronk cage `C` + exit `E` at the top.

| Room | Name | Content |
|------|------|---------|
| 0 | **The Ascent** | The existing shaft, kept as-is except at the top: `@`, `Q`, the Glide shrine, the patroller, updrafts, wind tiles, spike banks. The top `C` + `E` are **replaced** by a room-door to the arena plus a return entrance. |
| 1 | **The Eyrie** (new) | The boss arena, per §3.2: 32×24, floor row 22, hover line row 5, two updraft columns, three ledges at rows 16–19. `"boss": "d4"`; fought on entry; victory opens the onward door. |
| 2 | **Windward Perch** (new) | The spronk cage + exit, relocated from the old shaft top. Small and calm — the exhale after the fight, as in D2/D3 room 2. |

The restructure is **additive**: room 0 keeps its entire existing gauntlet and is merely
re-terminated. Entrances and room-doors follow the D3 pattern (`dungeon3_room1.json`): the arena
carries entrance `id 0` (arrival from room 0, facing +1) and `id 1` (return from room 2, facing -1),
with room-doors targeting rooms 0 and 2.

---

## 5. What changes in code

### 5.1 `include/logic/boss.h` (additive only)

- `Locomotion` gains `Hovering` — **appended**, so `Stationary`/`Pacing` keep their values.
- `BossId` gains `D4Galewing` — appended.
- Three new `BossDef` fields, appended **after `phase_lines[3]`**. This placement is mandatory:
  every existing def uses positional aggregate init, so a field inserted mid-struct would silently
  reassign King/D1/D2/D3 values.
  - `int hover_row = 0` — hover line in tile coords (used when `locomotion == Hovering`).
  - `int move_vel_raw = 128` — drift/pace speed as data. 128 = today's hardcoded 0.5 px/frame, so
    D2's pacing is unchanged.
  - `bool aim_horizontal = false` — replaces the `perch_count == 0` derivation (§2.3).
    `KING_DEF` sets it `true` explicitly; every other def defaults to aimed-at-player, which is
    what D1/D2/D3 already do.
- `D4_PHASES` + `D4_DEF`.

### 5.2 `src/game/boss_fight.cpp` (four edits)

1. **Placement:** a `place_hover()` beside `place_boss()`, positioning at `hover_row` rather than
   the floor row.
2. **Drift:** widen the existing pacing branch (`:265`) to `Pacing || Hovering` — identical X math,
   different Y origin — reading `move_vel_raw` instead of the local `PACE_VEL`.
3. **Aim:** `aim_full` reads `!def.aim_horizontal` (`:179`).
4. **`restart_fight` re-places a hovering boss**, exactly as it re-places a pacing one (`:203`).
   This is the M13 QA lesson written down: a moving boss that is not reset on death-restart
   respawns the player pinned against it.

### 5.3 Content pipeline

- `BOSS_SYMBOL['d4'] = 'logic::D4_DEF'` in `tools/build_level.py` (the map holds fully-qualified
  symbols; an unknown boss name is a build-time `LevelError`).
- `boss_sprite_for` in `src/game/scene_dungeon.cpp` gains a `BossId::D4Galewing` row. The switch's
  `BN_ERROR` default means an unmapped id fails loudly at fight start.
- `tools/levels/manifest.json`: `"4": ["dungeon4"]` → `["dungeon4_room0", "dungeon4_room1",
  "dungeon4_room2"]`. The rename changes the generated symbol to `DUNGEON4_ROOM0_DATA`, which is what
  forces the test rewrite in §7.
- `include/game/levels/dungeons.h`: three includes + `DUNGEON4_ROOMS[3]`, `DungeonData{..., 3, 0}`.
- Art: a single-frame `galewing` placeholder via `tools/make_placeholder_art.py` (draw fn + gen fn +
  `__main__` registration), committed as `graphics/galewing.bmp` + `.json`. Manual step — the build
  does not run it.

### 5.4 Deliberately untouched

- **Save format** — `boss_defeats` bit 3 already reserved; no v7, no migration.
- **`src/main.cpp`** and **hub door-4 gating** — `DUNGEONS_BY_ID[3]` already points at
  `DUNGEON4_DUNGEON`; it merely gains rooms.
- **Three-layer purity** — the drift math stays scene-side in `boss_fight.cpp`, matching `Pacing`.
  Splitting one of the two movement modes across layers would be worse than keeping both together.
  `python tools/check_logic_purity.py` must stay green.

---

## 6. What's new vs reused

**New:** `Locomotion::Hovering`; three `BossDef` fields; `D4_DEF`; the arena and spronk rooms; the
`galewing` sprite; the hover-line content invariant test.

**Reused unchanged:** `run_boss_fight` in its entirety (four localized edits, no restructuring);
`BOSS_ATK_AIMED` / `FAN` / `ROCKFALL` and their emitters; `AttackPool`; `BossHpBar`; the
`AlwaysVulnerable` path in `BossState`; the updraft/glide/wind tile kit from M5; the entrance /
room-door / boss-on-entry wiring from D2/D3; `boss_defeats` persistence from save v6.

The marginal cost of this boss is one movement mode plus content — which is the payoff the Phase 5
unification was built to deliver.

---

## 7. Testing strategy

**Host tests** (`bash tools/host_test.sh`, must end `N/N tests passed, 0 checks failed`):

- **`test/test_dungeon4_level.cpp` — rewritten** onto the shared `test/level_harness.h` for three
  rooms. The existing `d4_cage_exit_vertical` assertion (cage and exit in the same room) becomes
  false by construction and moves to room 2. Room 0 keeps its ascent assertions (vertical, border
  solid, Glide shrine, updraft + wind present, hub-return door) re-pointed at `DUNGEON4_ROOM0_DATA`.
  Reachability: spawn → shrine → top room-door (room 0); entrances → onward door (room 1); entrance →
  cage + exit (room 2).
- **The governing rule as a test:** in the arena room, assert that no standing surface exists within
  **11 tiles** below `D4_DEF.hover_row` (§3.1's derivation), with 11 held in a single named constant
  beside the test so QA can raise it in one edit. This is pure content data, so a future arena tweak
  cannot quietly turn the fight into a stand-and-plink without going red.
- **`test/test_boss.cpp`:** the existing def-invariant loop picks up `D4_DEF` for free. Add
  D4-specific assertions — an `AlwaysVulnerable` boss never opens an expose window; phase thresholds
  descend; `hover_row > 0` when `locomotion == Hovering`.
- **Regression:** every existing `test_boss.cpp` assertion for King/D1/D2/D3 must stay **unchanged**
  and green. The appended-fields rule (§5.1) plus `aim_horizontal`'s default are what guarantee it;
  if any existing assertion needs editing, the field placement is wrong.
- `test_all_dungeons.cpp`'s min-room-size check covers the new rooms automatically.

**Gates:** `python tools/check_logic_purity.py` → `logic purity OK`; `python
tools/validate_dungeons.py` (sprite budget for the new rooms); `bash tools/build_rom.sh` → `ROM
fixed!` with zero warnings.

**mGBA QA (human-only — agents build the ROM, they cannot play it):**

- The full loop: climb an updraft, glide, land a hit, fall, repeat to a kill.
- **The 11-tile margin holds in practice:** stand on the highest ledge, double-jump, fire up-aimed —
  the bolt must miss. This is the empirical check on §3.1's arithmetic.
- Rockfall's leap-offset read on a hovering boss (§3.5's known risk).
- Aimed bolts actually track a grounded player (the §2.3 fix).
- Death-restart re-places the boss at its hover line and does not pin the player (§5.2.4).
- Defeated-boss skip: kill it, exit to hub, re-enter → no re-fight, onward door open.
- D4 room 0 still plays as before after re-termination; room 2 cage frees the spronk.
- King / D1 / D2 / D3 fights unchanged — the `aim_horizontal` and `move_vel_raw` defaults are the
  claim under test.

---

## 8. Success criteria

1. D4 is a three-room dungeon: ascent → boss arena → spronk, reachable from hub door 4.
2. Galewing hovers, drifts, and cannot be hit from any standing surface in the arena.
3. The fight is winnable using Glide + updrafts + the free bolt, with no magic spent.
4. King, D1, D2, and D3 fights are behaviorally identical to `main` at `35d1b2c`.
5. Host tests green with strictly more invariants than before; purity guard green; ROM builds clean.
6. No save-format change; an existing v6 save loads and plays with D4's new rooms.
