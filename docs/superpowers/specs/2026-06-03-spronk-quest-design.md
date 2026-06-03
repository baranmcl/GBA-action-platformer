# Spronk Quest — Design Document

**Working title:** Spronk Quest
**Platform:** Game Boy Advance (GBA) — `.gba` ROM
**Targets:** mGBA emulator + real GBA hardware (via flashcart)
**Date:** 2026-06-03
**Status:** Design approved; first implementation plan (Milestone 1) pending.

---

## 1. Concept & Pillars

An original side-scrolling **action platformer** for the Game Boy Advance. **Laurel**, a
brave young **Goob** (a human-like folk), wields a **magic wand** to rescue the **8 spronks**
imprisoned across 8 dungeons and defeat the **Nightmare King** who took them.

Inspired by the Zelda "earn an item, open the world" loop, but deliberately **its own game** —
side-scrolling (not top-down), wand-based ranged magic (not a sword), and continuous **meters**
for health and magic (not discrete hearts).

**Design pillars:**

1. **Magic-driven action platforming** — jumping and ranged wand combat are the core verbs.
2. **Earn a power, open the world** — each dungeon grants one signature ability that unlocks
   new parts of a connected hub world.
3. **Systemic elements** — fire / ice / light spells interact with the world (burn vines,
   freeze water into platforms, reveal hidden platforms), creating emergent puzzles cheaply.
4. **Runs on real hardware** — built for mGBA *and* an actual GBA, validated on both.

---

## 2. Full-Game Vision (north star)

### Protagonist & core mechanics

- **Laurel** — a Goob wand-wielder.
- **Base attack:** a **free magic bolt** from the wand (short cooldown, always available — no
  resource cost). This guarantees combat is never gated behind a meter.
- **Health meter:** a continuous bar that depletes when Laurel takes damage and refills from
  health pickups. (NOT discrete hearts — a deliberate departure from Zelda.)
- **Magic meter:** a continuous bar that powers **elemental spells** and **refills from
  defeated enemies**. Encourages aggressive play (kill enemies → fuel spells) without
  micromanagement.
- The two meters are visually distinct (e.g. health = warm/red, magic = cool/blue).

### World structure

A central side-scrolling **hub overworld** connects to all 8 dungeon doors. The hub is
**lightly gated**: late areas are physically walled off until Laurel has the matching ability
(e.g. a wide chasm is impassable until Featherleap; a vine wall until Fire). Beating a dungeon
frees its spronk and grants its power, which opens new hub routes. This is the engine of
progression and difficulty pacing — content is gated by *capability*, not soft logic.

### The 8 dungeons & rewards

| # | Dungeon          | Theme                | Reward power            | Opens up                                   |
|---|------------------|----------------------|-------------------------|--------------------------------------------|
| 1 | Whispering Woods | tutorial forest      | **Featherleap** (double-jump) | high ledges, wide first gaps         |
| 2 | Ember Caverns    | volcanic caves       | **Fire spell** ⚡        | burns vine barriers, lights braziers, melts ice |
| 3 | Frost Hollow     | frozen grotto        | **Ice spell** ⚡         | freezes water / waterfalls into platforms  |
| 4 | Gale Cliffs      | windswept heights    | **Wind Cloak** (glide)  | long gaps, updraft shafts                  |
| 5 | Sunken Ruins     | flooded temple       | **Blink** (dash)        | narrow gaps, dash-through cracked walls    |
| 6 | Thornwild Marsh  | overgrown swamp      | **Vine Grapple** (swing/pull) | grapple points, pull heavy blocks    |
| 7 | Quaking Quarry   | crumbling mines      | **Stone spell** (ground-pound) | smashes cracked floors, slams switches |
| 8 | Gloom Spire      | approach to the King | **Light spell** ⚡       | reveals hidden platforms, dispels darkness — the King's bane |

⚡ = elemental spell; draws the magic meter. The other four (Featherleap, Glide, Dash, Grapple)
are traversal powers.

**Systemic element interactions** (cheap to implement — each spell sets a tile/entity state):
Fire melts Ice; Fire lights braziers (which may drain water); Ice freezes water into standable
platforms; Light reveals hidden platforms and is the Nightmare King's weakness.

### Spronks

Each of the 8 dungeons imprisons one spronk in a cage at its end. Freeing the spronk is the
dungeon's victory condition and grants Laurel that dungeon's power.

### Finale

Once all 8 spronks are freed, the path to the **Nightmare King** opens. He is defeated by
chaining gathered powers — **Light** to expose him, elemental spells to wound him.

---

## 3. Vertical Slice — Milestone 1 (FIRST implementation plan)

The first spec/plan builds a **thin vertical slice** that exercises every system end-to-end,
so the hard unknowns (does collision feel good? does it hold 60 fps on hardware? does the asset
pipeline work?) are surfaced while cheap to fix.

### Acceptance criteria — "a complete playable loop"

- [ ] Boots on **mGBA** and on **real GBA hardware** at a stable **60 fps**.
- [ ] **Platformer physics:** gravity, jump arc, run accel/decel, collision against solid tiles
      **and** one-way platforms. Tunable and host-tested.
- [ ] **Laurel:** walk, jump, fire **magic bolt**; **health meter** (take damage → deplete →
      death/respawn); **magic meter** shown on HUD (bolt is free; meter present for future spells).
- [ ] **One ability earned in-slice:** **Featherleap (double-jump)** — Laurel does NOT start
      with it; she gains it from the spronk.
- [ ] **One short hub segment** leading to the entrance of **Dungeon 1 (Whispering Woods)**:
      a few screens of platforming, **one enemy type**, and the caged **spronk**.
- [ ] Freeing the spronk grants Featherleap and opens a previously-unreachable exit (proves the
      "earn a power → gate opens" loop).
- [ ] **Title screen** + **SRAM save** that persists "spronk #1 freed / Featherleap owned".
- [ ] **Placeholder art** throughout, but a readable Laurel sprite, tileset, enemy, and spronk.

### Explicitly OUT of the slice

Dungeons 2–8, all elemental spells, the full-size overworld, music (SFX optional), the Nightmare
King, story/cutscenes, bosses. These are later milestones.

---

## 4. Technical Architecture

### Toolchain

- **devkitARM + Butano** (modern C++17 GBA engine; community-recommended high-level path).
- Output: `.gba`. Tested in **mGBA** (supports GDB stepping) and on **real GBA hardware**.
- Butano provides sprite/background/text/audio management, a no-heap/no-exceptions runtime
  (ETL-based), and a PNG+JSON asset import pipeline.

### Layering (the key decision — preserves host-testability inside C++)

```
src/
  logic/    Pure C++. ZERO Butano (`bn::`) types. Physics integrator, collision,
            combat/HP/magic meters, world-state (which spronks freed, abilities owned).
            >>> Host-testable with g++ in milliseconds. <<<
  engine/   Butano glue: sprite/animation manager, camera/scroll, input mapping,
            SRAM save, audio hooks. Reusable across all dungeons.
  game/     Scenes (title, play, win) + Dungeon 1 content. Scene state-machine
            pattern carried over from the prior `gameboygame` project.
```

**Hard rule:** Butano types must never leak into `src/logic/`. Gameplay math operates on plain
structs and fixed-point integers. This is what keeps the prior project's "test physics on the
host" superpower alive under Butano.

### Assets

- Authored as PNGs, imported via **Butano's asset pipeline** (graphics/audio + JSON specs).
- GBA constraints respected: 8×8 tiles, 16-color (4bpp) palettes, sprite size limits.
- Placeholder art now; final art is a **drop-in PNG replacement** later, not a rewrite.

### Content as data

- Dungeon/room layouts authored as **data** (Tiled `.tmx`/JSON or a simple text format),
  compiled into the ROM by a **Python build tool** — the proven `gameboygame` pattern.
- Goal: adding dungeons 2–8 is **content**, not engine code.

### Testing

- `test/` host suite (g++) covers everything in `src/logic/`: physics, collision, combat,
  meters, save/world-state. Runs without an emulator, in milliseconds.

---

## 5. Milestone Roadmap (decomposition)

Each milestone is its **own** spec → plan → implementation cycle. The design above is the north
star; only Milestone 1 is built first.

1. **M1 — Vertical Slice** *(first spec; Section 3)*
2. **M2 — World framework:** full hub overworld + gating system + dungeon-entry framework
3. **M3–M9 — Dungeons 2–8:** each adds one new ability/spell + enemies + a boss (repeatable)
4. **M10 — Nightmare King finale**
5. **M11 — Audio & polish:** music (Butano/Maxmod), SFX, game feel / juice
6. **M12 — Story & cutscenes**

---

## 6. Learnings Carried From `gameboygame` (prior shipped GB project)

The prior project was a **Game Boy** (DMG/GBC) ROM built with GBDK-2020/SDCC — different
hardware and toolchain — but these architectural lessons transfer:

- **Engine / game separation** (`src/engine/` vs `src/game/`) kept the codebase extensible.
  We extend this to a three-layer split (`logic/` / `engine/` / `game/`) for Butano.
- **Scene state machine** — one file per scene with a clean handoff mechanism.
- **Content-as-data pipeline** — game content authored as JSON/data, compiled to source by a
  **Python tool** at build time. Reused here for dungeon layouts.
- **Host-side unit tests** — pure logic tested with native gcc/g++, no emulator. The single
  biggest robustness win; preserved via the strict `logic/` layer boundary.
- **Emulator-vs-hardware discipline** — develop on mGBA but validate on real hardware; document
  emulator settings gotchas (sprite layer, palette, filtering).

---

## 7. Open Questions / Deferred Decisions

These do not block Milestone 1; revisit at the relevant milestone:

- **Story depth & tone** (M12): how much narrative/dialogue, intro/outro cutscenes, the
  Nightmare King's characterization, what the spronks *are*.
- **Final art direction** (post-slice): Laurel's definitive sprite design, palette identity,
  dungeon tilesets.
- **Audio** (M11): chiptune music scope, Butano/Maxmod integration, SFX set.
- **Dungeon/room data format** (M1/M2): confirm Tiled `.tmx` vs a bespoke text format during
  the M1 implementation plan.
- **Boss design** (M3+): per-dungeon boss vs spronk-cage-only; the slice uses cage-only.
