# Spronk Quest — Milestone 14 (D3 Frost Hollow Boss: "Coldforge Twins") Design

**Platform:** Game Boy Advance (Butano / devkitARM)
**Date:** 2026-07-04
**Builds on:** M12 boss framework (merge `c269831`) + M13 D2 boss "Slagshell" (merge `802a942`).
See `2026-06-24-spronk-quest-m12-boss-framework-design.md`,
`2026-06-26-spronk-quest-m13-d2-boss-design.md`, and the D3 dungeon spec
`2026-06-08-spronk-quest-m4-frost-hollow-design.md`.
**Status:** Design — approved, pending implementation plan.

---

## 1. Scope (decided)

M14 adds the **third per-dungeon boss**: the **Coldforge Twins** in D3 (Frost Hollow). It is the
first boss to exercise a **dual-spell counter** mechanic — a shifting vulnerable element that forces
the player to cycle **Fire ↔ Ice**, which is exactly D3's signature (D3 is the dungeon where the
player first wields two spells). It proves the framework stretches to a *shifting* SpellExpose.

**In scope:** one boss (`D3_DEF`), the framework's shifting-expose extension (data-described +
host-tested), an element-aware boss sprite, block-with-either-elemental magic recharge, placeholder
art, the D3 1→3-room restructure, and tests. Reuses everything else from M12/M13 (SpellExpose core,
`AttackPool`, aimed + spiral attacks, block-to-charge, magic continuity, `run_room_boss`,
`play_room` boss-room handling, the Game-Over/clear flow, the spronk rescue).

**Explicitly OUT of scope:** D4–D8 bosses; enlarging D3's puzzle rooms; audio; final art; new
locomotion (the Twins are grounded + stationary — no pacing/floating). **No save-format change** —
the boss is not persisted (re-entry re-fights), exactly like D1/D2.

---

## 2. Combat design

### 2.1 Vulnerability — dual-spell counter (shifting expose element)

The Coldforge Twins is a two-headed beast: an **ice head (blue)** and a **fire head (red)**. Exactly
**one head is active/lit** at any time — that head is the target. The beast is **armored
(invulnerable)** until the player casts the **opposite element** to the active head:

- **ice head active → cast Fire** (Fire melts the ice) → exposed window → bolt/Fire/Ice lands one wound.
- **fire head active → cast Ice** (Ice cools the fire) → exposed window → wound.

The exposing spell is the **counter** of the active head's element. On a wound the active head
**switches** (ice→fire or fire→ice), so the required spell flips and the player must **cycle `L`
(Fire ↔ Ice)** before the next opening. The player therefore alternates Fire → Ice → Fire … across
the fight.

The fight **starts with the ice head active** (so the first counter is Fire; `D3_DEF.expose_spell =
Fire`, `expose_spell_alt = Ice`, `cur_expose` starts at `expose_spell`). This generalizes M12's
`VulnMode::SpellExpose` from a *fixed* expose spell to a **shifting** one. While exposed the attack
pattern is frozen (existing "clean window" behaviour). The wound counts, the window closes, and the
head shifts — see §2.2.

### 2.2 Anti-spam — re-armor + forced spell-cycle

A wound triggers the standard re-armor (`hit_iframes`): the beast is **immune and un-exposable** while
its attack pattern **resumes** (the player must dodge). ADDITIONALLY, because the active head flips on
the wound, the required spell changes — the player cannot mash one spell. The per-phase telegraph is
**≥ `SWITCH_BUDGET` (60 frames)** so there is always time to cycle `L` to the new element before the
next opening (this is the existing framework invariant — it now also covers the mandatory cycle). Use
`hit_iframes ≈ 60–75` (tunable) — the forced cycle already adds a gate, so the re-armor need not be as
long as D2's 90.

### 2.3 Locomotion + attacks

- **Locomotion: Stationary** (`Locomotion::Stationary`, the default). The Twins are rooted at arena
  centre; the challenge is reading the active head, cycling spells, and dodging — not chasing a moving
  target. No new locomotion code.
- **~70 HP, 10/wound → ~7 wounds** (7 alternating Fire/Ice exposes). **2 phases:**
  - **P1 (70 → 35):** `BOSS_ATK_AIMED` — aimed frost shards.
  - **P2 (35 → 0):** `BOSS_ATK_AIMED | BOSS_ATK_SPIRAL` — adds the rotating spiral (an icy shard-ring).
- Both attacks are already supported by `run_room_boss` (the D2 wiring handles AIMED + SPIRAL);
  **no new attack code.** Shards reuse the red `boss_bolt` sprite (or an Ice-tinted reskin), travel
  wall-to-wall, and despawn on solid geometry. Contact with the beast's body hurts (~20 + i-frames).

### 2.4 Magic economy — block with either elemental

Magic carries in from the previous room (continuity, M13) — no full refill on entry/victory (health
still tops up). The player needs magic for BOTH Fire and Ice (cycling), so **blocking a boss bolt with
either Fire OR Ice recharges magic** (+25/block, per M13's block-to-charge; the free bolt never
blocks). Death-restart refills magic + health as the ultimate soft-lock fallback. **No magic crystal**
in the arena (consistent with D2). This requires extending the single-spell block (`BossDef::block_spell`)
to a **second block spell** (`block_spell2`) so both elementals block+charge.

---

## 3. Identity, art, dialogue

**The Coldforge Twins** — a grounded two-headed beast, one ice head (blue), one fire head (red). The
**active head is lit** (the other dormant) and is the exposed target. **4-frame placeholder sprite**,
indexed by (active element, exposed):

| Frame | Active head | State |
|---|---|---|
| 0 | ice (blue, lit) | armored |
| 1 | ice (blue, lit) | exposed (recoiling) |
| 2 | fire (red, lit) | armored |
| 3 | fire (red, lit) | exposed |

`run_room_boss` selects `want_frame = elem_base + (exposed ? 1 : 0)`, where `elem_base` is 2 when the
current expose element is the def's `expose_spell_alt`, else 0. D1/D2 (`expose_spell_alt == None`) keep
`elem_base == 0` → unchanged 2-frame behaviour. **Intro:** *"One of us always burns."* **Death:**
*"Both heads... fall still..."*

---

## 4. Level structure (D3: 1 room → 3 rooms; mirrors D1/D2)

D3 is currently a single Frost Hollow room (`dungeon3.txt`/`.json`). Restructured into three rooms:

- **Room 0 — Frost Hollow puzzle (existing content):** Fire-first half → the **Ice `F` shrine** →
  frozen second half (water↔ice, Water gate, dual-spell climax). The player earns Ice here, so both
  spells are ready for the boss. The **cage + exit are removed** (they move to room 2); a **room-door
  → room 1** and a hub-return **`Q`** are added. Existing water↔ice puzzle content preserved.
- **Room 1 — boss arena (new):** **flat floor, no static hazards** (D1/D2's lesson), the Twins placed
  centred, `N`+`D` on both sides (arrival/return + onward). No crystal. No `@`/`C`/`E`.
- **Room 2 — spronk chamber (new):** the caged spronk + the dungeon exit. Freeing the spronk + reaching
  the exit clears D3.

`build_level.py` gains `"boss":"d3"` → `&logic::D3_DEF`. `DUNGEON3_ROOMS` in `dungeons.h` becomes a
3-room array (start room 0). The orphaned generated `dungeon3.h` is removed (as `dungeon2.h` was).

---

## 5. Save / persistence

**No change.** The boss is not persisted; re-entering D3 re-fights the Twins (a `play_room` local, like
D1/D2). `SaveData` format is untouched.

---

## 6. What's new vs reused (three-layer purity preserved)

**Reused unchanged:** `VulnMode::SpellExpose`, `AttackPool` (aimed + spiral), `SpiralEmitter`,
`TelegraphCue`, `BossHpBar`, `resolve_damage`, `next_attack_for_phase`, the `play_room` boss-room
handling, `run_room_boss`'s pacing/crystal/rockfall/block infrastructure (D3 uses stationary + block,
no pacing/rockfall/crystal), magic continuity, death-restart, the spronk rescue.

**New, pure logic (`include/logic/boss.h`, host-tested):** `BossDef::expose_spell_alt` (default
`None`); `BossState::cur_expose` (the current expose element) with `reset` init + `on_wound` flip +
`on_expose_hit` checking `cur_expose`; `BossDef::block_spell2` (default `None`); `D3_DEF` + `D3_PHASES`.
No `bn::`.

**New, engine (`src/engine`, `bn::` allowed):** none required — `AttackPool::block_with_spell` already
exists; `run_room_boss` calls it for both `block_spell` and `block_spell2`. (If a cleaner block-set API
is warranted, keep it in `boss_attacks.h`, logic-only include.)

**New, game (`src/game/scene_dungeon.cpp`):** element-aware boss-frame selection in `run_room_boss`;
block with `block_spell2` in addition to `block_spell`; D3 boss-sprite selector entry.

**New, art/tools:** `gen_coldforge` (4-frame) in `make_placeholder_art.py`; `"boss":"d3"` in
`build_level.py`.

---

## 7. Testing strategy

**Host (pure logic — `tools/host_test.sh`):**
- `test_boss.cpp` `D3_DEF` regression: `max_hp == 70`, `phase_count == 2`, `vuln == SpellExpose`,
  `expose_spell`/`expose_spell_alt` are Fire/Ice (the two counter elements), `phases[0].end_hp == 35`,
  P1 mask `AIMED`, P2 mask `AIMED | SPIRAL`, both telegraphs ≥ `SWITCH_BUDGET`, `locomotion ==
  Stationary`, `block_spell`/`block_spell2` are Fire/Ice.
- **The shift invariant:** with `D3_DEF`, `cur_expose` starts at `expose_spell`; only the CURRENT
  element exposes (the other element + bolt do NOT); a wound flips `cur_expose` to the other element
  (Fire→Ice→Fire); no re-expose while `hit_iframes > 0`; ~7 wounds to defeat, alternating the required
  element each wound.
- **Regression:** King/D1/D2 defs keep `expose_spell_alt == None` and `block_spell2 == None`; their
  `cur_expose` never shifts (a wound leaves it equal to `expose_spell`). Existing King/D1/D2 tests stay
  UNCHANGED and green.

**Host (level invariants — new `test_dungeon3_level.cpp`, mirroring D2):**
- Structural: room 0 has the Ice shrine + no cage + a door to room 1; room 1 has `boss == &D3_DEF` +
  no cage/exit + **no crystal**; room 2 has the cage + exit; only room 1 has a boss; the 0↔1↔2 door
  graph + one hub-return resolve.
- No-soft-lock (fail-on-broken): from each entrance the onward door / cage / exit is reachable; each
  entrance settles on the floor (extend the respawn-settle test to D3's entrances). (Room 0 is a
  water↔ice puzzle room — like D2's room 0, its full traversal is not flood-fill-modelable; assert the
  hub-return is reachable + the onward door exists structurally, and rely on emulator QA for the puzzle
  path — the same accepted deviation as D2.)

**ROM + emulator QA (manual, handed to user):** the active head reads clearly (blue/red); casting the
COUNTER spell exposes + wounds; the head switches on each wound and you must cycle `L`; the telegraph
gives time to switch; blocking with either Fire/Ice recharges magic; magic never hard-soft-locks; D3
clears (spronk freed, exit reached); King/D1/D2 unregressed.

---

## 8. Success criteria

Title → Hub → enter D3 → solve Frost Hollow and earn Ice → enter the boss arena owning Fire + Ice →
defeat the Coldforge Twins by **countering the active head's element (cast the opposite spell) and
cycling Fire ↔ Ice as the head switches each wound** (not by mashing one spell) → onward door opens →
free the spronk → reach the exit → D3 cleared and saved (+1 max life). The King, D1 Guardian, and D2
Slagshell still play exactly as before. Host tests green (incl. new D3 + unchanged regression), purity
green, ROM builds.
