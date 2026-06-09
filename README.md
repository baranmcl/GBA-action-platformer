# Spronk Quest

An original **Game Boy Advance** action platformer. Laurel, a wand-wielding Goob, must rescue
the 8 spronks from 8 dungeons and defeat the Nightmare King.

This repo is built in milestones. It currently contains **Milestone 6 — Sunken Ruins + Blink
(Dash)**: a flooded fifth dungeon that combines every earned power, on top of M1–M5.

## What's playable

**M6 — Sunken Ruins (Dungeon 5) + Dash**
- **Blink / Dash** — a traversal ability: **double-tap a direction** for a fast horizontal burst
  (~5 tiles), in the air or on the ground, with **i-frames** — one air-dash per jump (recharges
  when you land).
- **Dash through spikes** unharmed (the i-frames), and **smash cracked walls** by dashing into them.
- A **combo dungeon**: the first half threads **Fire, Featherleap, Ice, and Glide** together
  (vine gate, climb, fire-wall gate, updraft); the second half is dash-only (spikes, cracked wall,
  an **air-dash + glide** gap).
- **Level reset:** stuck (e.g. out of magic before a gate)? Press **START** to restart the dungeon
  with full health/magic.

**M5 — Gale Cliffs (Dungeon 4) + Glide**
- The **Wind Cloak / Glide** — a traversal ability: after jumping, **hold A** to fall slowly with
  air control.
- **Updraft shafts** lift you up — but **only while gliding** (dive through otherwise).
- **Wind-gust zones** push you sideways (the first directional force on Laurel).
- A genuinely **tall vertical climb** (the engine now supports levels up to 128 tiles tall).
- **Slippery ice:** frozen-water platforms (from D3 on) are now slick — you slide instead of
  stopping dead.

**M4 — Frost Hollow (Dungeon 3) + Ice spell**
- The **Ice spell** ⚡ (cast with R) — and since you carry **Fire** in from D2, **L now cycles
  Fire↔Ice** (the HUD shows the selected spell).
- **Reversible water↔ice:** cast **Ice** at a water gap to **freeze it into a bridge**; cast
  **Fire** at the bridge to **melt it back**.
- **Elemental gates:** Fire burns vines and melts an ice wall; **Ice extinguishes a wall of fire**.
- **Water** is a damaging hazard; the freeze bridge is the clean way across (with a ceiling so you
  can't just jump over).

**M3 — Ember Caverns (Dungeon 2) + Fire spell**
- A **spell system**: free wand **bolt** (B), plus a selectable **Fire** spell you **cast** (R)
  by spending magic (L cycles spells — one for now).
- **Fire** burns **vine** gates, melts **ice** gates, and lights **braziers** — but bounces off
  **fire-immune** enemies (use the bolt on those).
- **Puzzles:** push a **crate** onto a **pressure plate** (held open by the block), find a hidden
  **button**, and light a **brazier** group to open the way.
- **Lava** hazard (damage on contact).
- A mid-dungeon **Fire shrine** grants the spell partway through — a no-Fire first half, then a
  Fire-powered second half.
- **Health + magic persist** across the hub and dungeons.

**M2 — World framework:** data-driven levels, a **plaza hub** with ability-gated dungeon doors,
typed gates, and v1→v2 save migration.

**M1 — Core loop:** run, jump, wand bolt, the earn-a-power loop (free the spronk), a patrolling
enemy, HUD meters + death/respawn, and a **title screen** with **SRAM save / continue**.
Abilities now come from **`F` shrines** (e.g. D1's Featherleap), not from rescuing the spronk.

## Controls

| Button | GBA | Default mGBA key |
|---|---|---|
| Move | D-pad | Arrow keys |
| Jump / double-jump | A | X |
| Glide (hold after jumping) | A (held) | X (held) |
| Dash / Blink | double-tap ←/→ | double-tap Arrow |
| Fire wand bolt | B | Z |
| Cast selected spell (Fire/Ice) | R | S |
| Cycle spell | L | A |
| Enter door (in hub) | Up | Up |
| Reset level (in a dungeon) | START | Enter |
| Start game (title) | START | Enter |

## Play it

Open `SpronkQuest.gba` in an accurate GBA emulator — **[mGBA](https://mgba.io)** recommended
(developed against mGBA 0.9.3). Or flash it to a GBA flashcart (EZ-Flash, EverDrive) for real
hardware. SRAM save persists across power cycles.

## Build from source

Requires **devkitPro / devkitARM**, **Butano** (vendored as a submodule), and **Python 3 + Pillow**.

```bash
git submodule update --init           # fetch Butano 21.6.0
python tools/make_placeholder_art.py  # (re)generate placeholder art
bash tools/build_rom.sh               # build -> SpronkQuest.gba
bash tools/host_test.sh               # run the host-side logic tests (149 tests)
```

> **Windows note:** the ROM builds through devkitPro's bundled MSYS2; host tests use the mingw64
> compiler. `tools/build_rom.sh` and `tools/host_test.sh` set the right environment automatically
> (see `CLAUDE.md` for the gory details of why).

## Architecture

A strict three-layer split keeps gameplay logic testable off-hardware:

```
include/logic/ + src/logic/   pure C++17, NO Butano types — host-unit-tested with g++
src/engine/                   Butano glue (sprites, bg, camera, HUD, SRAM, input)
src/game/                     scenes (title, hub, dungeon) + level content
```

`tools/check_logic_purity.py` fails the build if any `bn::` type leaks into the logic layer.
Physics, collision, combat, meters, and the save format are all validated by fast host tests
**before** the ROM is ever built.

## Project status

**M6 Sunken Ruins + Dash: feature-complete, mGBA-verified.** Adds **Blink / Dash** — a double-tap
i-frame air-dash that blinks through spikes and smashes cracked walls — plus a `Spikes` hazard, a
combo **Dungeon 5** that uses the whole carried kit (Fire/Featherleap/Ice/Glide) alongside the dash,
and a **START level-reset** safety net for every dungeon. See `docs/acceptance-m6.md`.

**M5 Gale Cliffs + Glide: feature-complete, mGBA-verified.** Adds the **Wind Cloak / Glide**
traversal ability and a tile-based **wind kit** (glide, updraft shafts, gust zones), a tall
**vertically-scrolling Dungeon 4**, an engine bump to **128-tile-tall levels** (Butano big map),
**slippery ice**, and a tighter global jump. See `docs/acceptance-m5.md`.

**M4 Frost Hollow + Ice spell:** two typed spells (Fire+Ice, `L` cycles), reversible **water↔ice**
terrain (Ice freezes water into bridges, Fire melts them back), a water hazard, an Ice-extinguished
**fire-wall** gate, and **Dungeon 3** (`docs/acceptance-m4.md`).

**M3 Ember Caverns + Fire spell:** a spell system, mid-dungeon ability shrine, trigger→target
puzzles (plates/buttons/braziers), pushable blocks, lava, fire-immune enemies, and Dungeon 2.
Abilities come from `F` shrines; health/magic persist across the hub (`docs/acceptance-m3.md`).

**M2 world framework:** data-driven levels, the **plaza hub** with ability-gated doors, typed
gates, and v1→v2 save migration (`docs/acceptance-m2.md`).

**M1 vertical slice:** shipped (`docs/acceptance-m1.md`).

Real-hardware verification pending. Next milestones: Dungeons 5–8 + their abilities (each a
content milestone snapping into the M2 framework). See
`docs/superpowers/specs/2026-06-03-spronk-quest-design.md` and the per-milestone plans in
`docs/plans/`.

### Level authoring

Levels live in `tools/levels/<name>.txt` (ASCII tile grid) + `<name>.json` (metadata). Symbols:
`#` solid, `.` empty, `^` one-way, `~` lava, `w` water, `@` spawn, `C` caged spronk, `E` exit,
`o` enemy, `u` updraft, `<`/`>` wind-left/right, `G` gate, `V` vine gate, `I` ice gate, `X`
fire-wall gate, `W` water gate, `F` ability shrine, `B` pushable block, `=` pressure plate, `?`
hidden button, `*` brazier, `1`–`8` dungeon doors. Levels can be up to 64 wide × 128 tall. The
JSON sidecar wires enemy patrols, pickup abilities, and trigger→target links. `tools/build_level.py` compiles them to
`include/game/levels/<name>.h` (both `host_test.sh` and `build_rom.sh` regenerate these
automatically).
