# Spronk Quest — Milestone 6 (Sunken Ruins + Dash) Design

**Status:** Approved (brainstorming complete, 2026-06-08). Next: implementation plan.

**One-liner:** Dungeon 5 (Sunken Ruins) introduces **Blink (Dash)** — a double-tap i-frame air-dash
that smashes cracked walls and blinks through hazards — and is the first **combo dungeon** that
draws on the whole accumulated kit (Featherleap + Fire + Ice + Glide + Dash).

A **content milestone** snapping into the shipped M2 framework + M3/M4/M5 systems, following the
per-dungeon recipe: a new ability from a mid-dungeon `F` shrine, obstacle gates, a spronk, a hub
door unlock. The roadmap (`docs/superpowers/specs/2026-06-03-spronk-quest-design.md`) pre-assigns
D5 = Sunken Ruins / Blink (Dash) / "narrow gaps, dash-through cracked walls".

---

## 1. Identity & arc

- **Theme:** Sunken Ruins — a sprawling flooded temple.
- **Ability earned:** **Blink (Dash)** — a **traversal** ability (NO magic, NO projectile), the
  third sibling of Featherleap/Glide. `Ability::Dash` is already scaffolded in `world_state.h`.
  Granted by an `F` shrine partway through.
- **Carried kit:** the player owns Featherleap + Fire + Ice + Glide (D1–D4). D5 is the first dungeon
  to use them **together** with the new dash.
- **Arc:**
  1. **First half — carried powers** in a flooded-temple setting (Featherleap climbs, a Fire
     vine/brazier, an Ice freeze, a Glide gap/updraft). Reach the **Dash shrine**.
  2. **Second half — Dash headlines:** spike-corridor i-frame blinks, cracked-wall smashes, air-dash
     gaps, and **combo beats** chaining powers.
  3. **Spronk → exit.**

## 2. The Dash mechanic (pure `Player::update`, host-tested)

`Player::update(InputFrame, Tilemap)` already integrates horizontal accel/friction (incl. the M5
slippery-ice branch), jump/double-jump, the wind kit, gravity, and collision. Dash adds a
self-contained `DashState`:

- **Input:** double-tap **left or left** / **right or right** within a short window (~12 frames:
  press → release → press). Detected from the `left`/`right` HELD flags by tracking press edges —
  **no new engine input plumbing** (the InputFrame already carries `left`/`right`).
- **Dash burst:** sets horizontal velocity to a high `DASH_VX` (~6 px/frame) in the dashed
  direction for `DASH_FRAMES` (~7 → ~5 tiles), and **zeroes vertical velocity** for the duration
  (a clean horizontal "blink"). Overrides the normal accel/friction while active.
- **Air + ground:** works whether grounded or airborne (extends a jump/glide horizontally).
- **i-frames:** the player is **invincible** for the dash's duration (a small window).
- **One per ground-touch:** a dash charge, refreshed on landing (mirrors `air_jumps_left`). No
  spamming; you get exactly one dash between ground touches.
- **Gated by `abilities.dash`:** does nothing until the shrine is collected.

New `Abilities::dash` (bool); new `InputFrame` fields are **not** required (double-tap is derived
from `left`/`right`). `DashState` exposes `invincible()` (for the scene's damage guard) and
`active()` / `dir()` (for the cracked-wall smash + the dash burst).

## 3. Dash interactions

- **Cracked-wall gates** (`GateType::CrackedWall → Ability::Dash`, already in `gates.h`): rendered
  as a full-height wall (like vine/ice gates). It clears when the **dashing player's body overlaps
  it** (not via a projectile) — the scene opens the gate column on contact-while-dashing, and the
  dash carries the player through.
- **`TileKind::Spikes`** (new, non-solid, damaging): submerged spike/current corridors. Normally
  damages on overlap (like lava/water); **i-frames during a dash skip the damage**, so a corridor
  too wide to jump is crossed by timing the dash through it.
- **Wide gaps:** jump + air-dash to cross.

## 4. Combo beats (all reused systems — authoring only)

- **Freeze + dash:** freeze a water span (Ice) into an ice floor, then dash across it — the M5
  slippery ice carries the dash further.
- **Air-dash + glide:** jump → air-dash → glide to clear an extra-long flooded chasm.
- **Burn + smash:** burn a vine (Fire) to expose a cracked wall, then dash through it.
- **Updraft + dash:** ride an updraft (Glide) up to a high cracked wall, dash through to a hidden
  room.

These need NO new code — they place the existing Fire/Ice/Glide/Featherleap obstacles next to dash
beats.

## 5. What's new vs reused (three-layer purity preserved)

**New pure `logic/` (host-tested):**
- `Abilities::dash` (bool).
- `dash.h` — `struct DashState` (double-tap detection state machine, dash timer, i-frame window,
  recharge-on-land), reading `left`/`right` held + `on_ground` + `abilities.dash`.
- `tilemap.h` — `TileKind::Spikes` (next value after `WindRight=8`); non-solid; `is_spikes()`.
- `hazard.h` — include Spikes in `hazard_overlap` (so it damages like lava/water).
- `Player::update` — apply the dash (override horizontal vel + zero vy while active); recharge the
  dash on landing.

**New engine:**
- Spike bg tile + cracked-wall bg tile art (`make_placeholder_art.py`); kind→bg map for Spikes.
- (Optional polish, MAY skip) a dash trail / the existing damage-blink reused for i-frames.

**Scene (additive):**
- `scene_dungeon.cpp`: `player.abilities.dash = world.has(Ability::Dash)`; skip hazard/contact
  damage while `dash.invincible()`; open a `CrackedWall` gate when the dashing player overlaps it.

**New content:**
- Compiler symbols for the spike tile + the CrackedWall gate (the Dash shrine is `F` +
  `"ability":"dash"`, already supported by `ABILITY_ENUM`).
- `tools/levels/dungeon5.txt` + `dungeon5.json` → `DUNGEON5_DATA`.

**Reused as-is:** Featherleap/Fire/Ice/Glide (the whole carried kit), the gate framework, the
spell pool, the ability-shrine grant, spronk/exit, persistent vitals, fades, the additive
`scene_dungeon`, the 64×128 big-map background + camera clamp. **Hub:** Door 5 enterable once D4's
spronk is freed (extend `door_enterable`).

## 6. Hazard / consequence

Spikes + deep water damage (i-frames let you dash through). Death respawns at the entrance (reused;
no new checkpoint system). Pushable blocks (if used) reset on death (M5 fix). Mis-dashing into a
hazard without i-frames is a setback, not a lock.

## 7. Soft-lock guards

- Dash **recharges on landing** — never stranded mid-air without it.
- The **Dash shrine is mandatory** before any dash-only obstacle (no dash-gate or spike corridor on
  the path to the shrine).
- **Carried-power invariant:** Door 5 requires D4 cleared → D3 → D2, so Fire + Ice + Glide +
  Featherleap are all owned; every carried-power beat is solvable. No new Fire/Ice/Glide shrine in
  D5 (only Dash).
- A **bolt-enemy (magic) before any Fire/Ice beat** so spells are affordable.
- Spike corridors are ≤ one dash (~5 tiles) wide so a single i-frame dash clears them.

## 8. Out of scope (deferred)

Audio; boss / Nightmare King; mid-level checkpoints; final art; new enemy AI; a swim/underwater
movement system (deep water is just a damaging hazard, not a swimmable medium).

## 9. Testing strategy

Pure host tests (`bash tools/host_test.sh`): `DashState` (double-tap triggers a dash only with the
ability; dash sets `DASH_VX`/zeroes vy for `DASH_FRAMES`; `invincible()` true during the dash;
recharge only on landing; no double-dash mid-air); `TileKind::Spikes` hazard overlap; and the
`dungeon5` level data (dims, border, exactly one Dash shrine + spronk + exit, ≥1 spike tile, ≥1
CrackedWall gate, and ≥1 carried-power obstacle proving the combo theme). Logic-purity guard stays
clean. Then **mGBA** verifies the dash feel, the i-frame blink-through, cracked-wall smashes, the
combo beats, and the full carried-kit run.

## 10. Success criteria

- Dash earned from the shrine; double-tap blinks ~5 tiles, air + ground, one per ground-touch.
- i-frames let you dash **through** a spike/water hazard unharmed; cracked walls **smash** on a dash.
- The second half is impassable without Dash; the first half uses the carried kit.
- At least one **combo beat** chains a carried power with the dash.
- Spronk freed → exit clears → Door 5 was enterable only after D4.
- All new logic host-tested; three-layer purity preserved; mGBA-verified end-to-end.
