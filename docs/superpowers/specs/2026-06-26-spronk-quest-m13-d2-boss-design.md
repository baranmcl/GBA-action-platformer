# Spronk Quest — Milestone 13 (D2 Ember Caverns Boss: "Slagshell") Design

**Platform:** Game Boy Advance (Butano / devkitARM)
**Date:** 2026-06-26
**Builds on:** M12 boss framework (shipped, merge `c269831`). See
`2026-06-24-spronk-quest-m12-boss-framework-design.md` (framework) and
`docs/plans/2026-06-24-spronk-quest-m12-boss-framework-plan.md`.
**Status:** Design — approved, pending implementation plan.

---

## 1. Scope (decided)

M13 adds the **second per-dungeon boss**: the **Magma Golem "Slagshell"** in D2 (Ember Caverns).
It is the first boss to exercise the M12 framework's **SpellExpose** vuln model in a *room* boss, the
first **moving** room boss, and it introduces a brand-new **rockfall** attack. It proves the framework
generalizes to a substantially different fight than the D1 Guardian (TiredWindow, stationary).

**In scope:** one boss (`D2_DEF`), a new `BOSS_ATK_ROCKFALL` attack + a `Locomotion::Pacing` movement
mode (both data-described, reusable by future bosses), the supporting engine code (rock projectile
pool, ground telegraph, pacing, magic-crystal handling in `run_room_boss`), placeholder art, the D2
1→3-room restructure, and tests.

**Explicitly OUT of scope:** D3–D8 bosses; making D2's existing puzzle rooms *larger* (a separately
deferred goal — the boss arena is kept flat/simple so it slots into a bigger D2 later); audio; final
art; story beyond the two dialogue lines. **No save-format change** — the boss is not persisted
(re-entry re-fights), exactly as D1.

---

## 2. Combat design

### 2.1 Vulnerability — SpellExpose with Fire

Slagshell is encased in a hardened **cooled-lava crust** and is **invulnerable** until the player
hits it with a **Fire** cast, which melts the crust and **exposes the molten core** for
`expose_frames ≈ 90` (~1.5s). While exposed the attack pattern is **frozen** (the framework's existing
"clean damage window" behaviour — `BossState::tick`), so the player never dodges and switches spells on
the same clock. During the window a **bolt OR Fire** lands exactly **one wound** (`wound_dmg = 10`).

This reuses `VulnMode::SpellExpose` with `expose_spell = SpellId::Fire` — **no new vuln model**.
`engine::resolve_damage` already drives SpellExpose generically off `def->expose_spell`, and
`run_room_boss` already swaps to the exposed sprite frame on `b.exposed()`.

### 2.2 Anti-spam — long re-armor after each wound

To prevent a mindless "Fire → bolt, repeat" win, a wound makes the crust **reform**: Slagshell becomes
**immune AND un-exposable** for `hit_iframes ≈ 90` (~1.5s), during which its attack pattern **resumes**
(the expose window freezes attacks; the post-wound window does not — see `BossState::tick`/`on_wound`).
The player must therefore **dodge** Slagshell's attacks (pacing approach + fireballs + rockfall) to earn
the next opening, instead of re-Firing immediately.

This is the same mechanism the King uses (teleport-away-after-a-wound forces a re-engage gap), expressed
for a *pacing* boss as a longer `hit_iframes`. It is purely a `D2_DEF` tuning value — **no new
framework code**. (D1 = 30, King = 45; Slagshell ≈ 90, tunable in QA.)

### 2.3 Movement — paces the floor (`Locomotion::Pacing`)

Slagshell **walks left/right along the arena floor**, reversing at the walls, at a slow speed (~0.5–1
px/frame, tunable). It is large and slow enough to remain a fair Fire/bolt target, but the player must
**lead shots and reposition** (it deals contact damage, ~20 + i-frames, like D1/King — fight from
range). Movement pauses while EXPOSED (the frozen-window invariant) so the wound window stays clean.

New framework: a `Locomotion` enum on `BossDef` (`Stationary` for D1; `Pacing` for D2). The pacing
integration lives in `run_room_boss` (scene-side, data-gated on `def->locomotion`), updating
`boss_body.pos.x` each non-frozen frame and bouncing off the arena walls. D1 (`Stationary`) is
unaffected. The King uses `run_boss` (its own teleport movement) and is untouched.

### 2.4 Attacks — 2 phases; rockfall is the signature

- **Phase 1 (hp 70 → 35):** `BOSS_ATK_AIMED` (aimed fireball) + `BOSS_ATK_ROCKFALL` (jump-rockfall).
- **Phase 2 (hp 35 → 0):** `BOSS_ATK_AIMED` + `BOSS_ATK_ROCKFALL` with **more/faster rocks**.

Both phases share the same attack **bitmask** (`AIMED | ROCKFALL`); the phase boundary exists to
**escalate the rockfall intensity** (column count + fall speed), which the rockfall emitter reads from
`b.phase` — see §2.5. So "Phase 2" is the same two attacks at higher rockfall pressure, not a new
attack set.

The **spiral is intentionally not used** (it was an earlier candidate; the rockfall replaces it as the
distinctive D2 move). Aimed fireballs reuse the existing aimed variant and the red `boss_bolt` sprite,
travel wall-to-wall, and despawn on solid geometry (the M12-QA-fixed behaviour). Attacks rotate over
the phase's bitmask via the existing `next_attack_for_phase` cycler in `run_room_boss`.

### 2.5 The rockfall attack (`BOSS_ATK_ROCKFALL`)

The signature jump move, dodgeable by design:

1. **Telegraph step:** Slagshell crouches; the telegraph cue pulses (existing `TelegraphCue`, colour
   assigned for rockfall).
2. **Active step:** it **leaps** (a quick up/down hop), and **ground crack/shadow markers** appear at
   the target columns — **3 columns in P1, ~4–5 in P2** — with one marker biased toward the player's
   current x and the rest spread, **always leaving at least one gap wide enough to stand in**. After a
   short warn delay (~20–30 frames) the markers convert to falling rocks.
3. Rocks spawn at the **ceiling** at the marked columns and **fall** (downward velocity / gravity),
   deal **contact damage** to the player on overlap (same per-hit tuning as bolts), and **despawn on
   the floor** (solid tile) — reusing the projectile-advance + solid-despawn logic.
4. **Recovery step:** markers cleared, Slagshell resumes pacing.

New engine code: a **second projectile pool for rocks** (the existing `AttackPool` constructed with a
**rock** sprite — `AttackPool` already despawns on solid + applies contact damage), a small
**rockfall emitter** that picks columns + drives the warn→drop timing + the ground markers, and the
`run_room_boss` wiring to advance the rock pool alongside the bolt pool. New art: `gen_rock` and a
crack/shadow marker (or reuse a simple existing sprite for the marker).

### 2.6 Magic economy + safety net

Player enters at full magic (100, set by `run_room_boss`). Fire costs **10**; a Fire-wound refills
**+25** (`resolve_damage` `magic_heal`). Bolt is free but does not refill. To remove any chance of a
magic soft-lock (cast Fire to nothing, then be unable to expose), the boss room contains a
**respawning magic crystal** (full refill on touch, reset un-collected each attempt — the M10/King
pattern). This requires porting the King's ~15-line magic-crystal handling from `run_boss` into
`run_room_boss` (a `level.magic_crystal_count > 0` block). Dying also refills magic via the existing
fight-restart.

---

## 3. Identity, art, dialogue

**Slagshell**, a hulking magma golem. **2-frame placeholder sprite** (the `run_room_boss` frame swap on
`b.exposed()` drives it): frame 0 = dark rocky crust (armored), frame 1 = cracked/glowing molten core
(exposed). **Intro:** *"You'll cook in my caverns."* **Death:** *"The embers...fade..."* Fireballs
reuse the red `boss_bolt`. Falling rocks use the new `rock` sprite.

---

## 4. Level structure (D2: 1 room → 3 rooms; mirrors D1)

D2 is currently a single large puzzle room (`dungeon2.txt`/`.json`). Restructured into three rooms
(the D1 entry→boss→spronk shape), keeping the existing puzzle content intact as room 0:

- **Room 0 — Ember Caverns puzzle (existing content):** lava platforming, pushable block, button +
  brazier gates, fire enemies, and the **Fire shrine** (Fire must be earned *before* the boss room, so
  the player has the expose tool). The **cage is removed**; a **room-door is added at the far end →
  room 1**. The hub-return door ('Q') stays here.
- **Room 1 — boss arena (new):** **flat floor, no static hazards in the first cut** (D1's hard-won
  lesson — a platform creates unhittable camp spots and blocks shots; and the pacing + rockfall already
  supply the challenge, so dodging rocks *and* floor hazards at once risks being unfair). Slagshell is
  placed centred, with a **respawning magic crystal**. Entrance from room 0; onward door to room 2 that
  opens on victory. (QA may add a couple of ≤-double-jump-width jump-over lava patches — the D1-spikes
  role — only if the fight proves too easy.)
- **Room 2 — spronk chamber (new):** the caged spronk + the dungeon exit. Freeing the spronk and
  reaching the exit clears D2 (the existing clear/spronk-free flow; the spronk grants the usual +1
  max-life on rescue).

`build_level.py` gains `"boss": "d2"` → `&logic::D2_DEF` (explicit symbol map, as for `d1`). The boss
room's `LevelData.boss` is non-null; `play_room` runs `run_room_boss` on entry, victory opens the
onward door, Game-Over reuses the dungeon's existing flow. `DUNGEON2_ROOMS` in `dungeons.h` becomes a
3-room array (start room 0).

---

## 5. Save / persistence

**No change.** The boss is not persisted; re-entering D2 re-fights Slagshell (a per-`play_room` local,
exactly like D1). The only persisted latch remains the spronk-freed bit (existing). `SaveData` format
is untouched.

---

## 6. What's new vs reused (three-layer purity preserved)

**Reused unchanged:** `VulnMode::SpellExpose`, `BossState` (expose/wound/i-frames/phase-index/tick),
`resolve_damage`, `AttackPool` (for both bolts and rocks), `TelegraphCue`, `BossHpBar`,
`next_attack_for_phase`, the `play_room` boss-room handling, the Game-Over/clear flow, the spronk
rescue.

**New, pure logic (`include/logic/boss.h`, host-tested):** `enum class Locomotion { Stationary,
Pacing }` + a `BossDef::locomotion` field; `BOSS_ATK_ROCKFALL = 1 << 3`; `D2_PHASES` + `D2_DEF`. No
`bn::` — purity guard must stay green.

**New, engine (`src/engine` / `include/engine`, `bn::` allowed):** the rockfall emitter (column pick +
warn/drop timing + ground markers) and rock-pool wiring; pacing integration; magic-crystal handling in
`run_room_boss`. `engine/boss_attacks.h` may include `logic/boss.h` only (no `game/`).

**New, game (`src/game`):** `run_room_boss` gains the pacing update, the rockfall step handling, the
rock pool, and the magic crystal; D2 level data restructured.

**New, art/tools:** `gen_slagshell` (2-frame) + `gen_rock` + crack-marker in `make_placeholder_art.py`;
`"boss":"d2"` in `build_level.py`.

---

## 7. Testing strategy

**Host (pure logic — `tools/host_test.sh`):**
- `test_boss.cpp` `D2_DEF` regression: `max_hp == 70`, `phase_count == 2`, `vuln == SpellExpose`,
  `expose_spell == Fire`, `locomotion == Pacing`, `hit_iframes` is the long re-armor value, P1 mask ==
  `AIMED | ROCKFALL`, P2 mask == `AIMED | ROCKFALL`, phase-1 `end_hp == 35`.
- BossState behaviour with `D2_DEF`: Fire exposes (bolt/none do not); a wound sets the long
  `hit_iframes` and clears `expose_timer`; **no re-expose while `hit_iframes > 0`** (the anti-spam
  invariant); 7 wounds to defeat; phase flips at hp ≤ 35.
- Keep the King + D1 regression tests **unchanged and green** (framework additions must not alter them).

**Host (level invariants — new `test_dungeon2_level.cpp`, mirroring `test_dungeon1_level.cpp`):**
- Structural: room 0 has the Fire shrine + no cage + a door to room 1; room 1 has `boss == &D2_DEF` +
  no cage/exit + **a magic crystal** (SpellExpose can't soft-lock on magic); room 2 has the cage + exit;
  only room 1 has a boss; the 0↔1↔2 door graph + one hub-return resolve.
- No-soft-lock (fail-on-broken, like D1): from each entrance the onward door / cage / exit is reachable
  with bolt + double-jump + Fire, and entrances settle on the floor (extend the D1 respawn-settle test
  to D2's entrances).

**ROM + emulator QA (manual, handed to user):** Slagshell paces and is hittable; Fire exposes and the
crust reforms (no Fire→bolt spam — must dodge between wounds); rockfall telegraphs fairly and always
leaves a dodge gap; bolts/rocks reach the floor/walls; magic never soft-locks; D2 clears (spronk freed,
exit reached); the King and D1 are unregressed.

---

## 8. Success criteria

Title → Hub → enter D2 → solve the Ember Caverns puzzle and earn Fire → enter the boss arena → defeat
Slagshell by **using Fire to expose it and dodging between wounds** (not by spamming) → onward door
opens → free the spronk → reach the exit → D2 cleared and saved (spronk freed, +1 max life). The King
and the D1 Guardian still play exactly as before. Host tests green (incl. new D2 + unchanged
King/D1 regression), purity green, ROM builds.
