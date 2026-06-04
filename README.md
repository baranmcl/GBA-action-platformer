# Spronk Quest

An original **Game Boy Advance** action platformer. Laurel, a wand-wielding Goob, must rescue
the 8 spronks from 8 dungeons and defeat the Nightmare King.

This repo currently contains **Milestone 1 — the vertical slice**: a complete, playable loop of
Dungeon 1 that proves every system end-to-end.

## What's playable (M1)

- Run, jump, and a wand **magic bolt** (ranged attack).
- **Earn-a-power loop:** free the caged spronk → gain **Featherleap** (double-jump) + the gate
  dissolves → reach the double-jump-only exit.
- A patrolling **enemy**: kill it with the wand (refills magic), or take contact damage.
- **Health + magic meters** (HUD), death/respawn.
- **Title screen** with **SRAM save / continue**.

## Controls

| Button | GBA | Default mGBA key |
|---|---|---|
| Move | D-pad | Arrow keys |
| Jump / double-jump | A | X |
| Fire wand bolt | B | Z |
| Start game | START | Enter |

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
bash tools/host_test.sh               # run the host-side logic tests (52 tests)
```

> **Windows note:** the ROM builds through devkitPro's bundled MSYS2; host tests use the mingw64
> compiler. `tools/build_rom.sh` and `tools/host_test.sh` set the right environment automatically
> (see `CLAUDE.md` for the gory details of why).

## Architecture

A strict three-layer split keeps gameplay logic testable off-hardware:

```
include/logic/ + src/logic/   pure C++17, NO Butano types — host-unit-tested with g++
src/engine/                   Butano glue (sprites, bg, camera, HUD, SRAM, input)
src/game/                     scenes (title, play) + Dungeon 1 content
```

`tools/check_logic_purity.py` fails the build if any `bn::` type leaks into the logic layer.
Physics, collision, combat, meters, and the save format are all validated by fast host tests
**before** the ROM is ever built.

## Project status

**M1 vertical slice: feature-complete, mGBA-verified.** Real-hardware verification pending (see
`docs/acceptance-m1.md`). Next milestones: the full hub overworld and Dungeons 2–8 (see
`docs/superpowers/specs/2026-06-03-spronk-quest-design.md` and
`docs/plans/2026-06-03-spronk-quest-m1-vertical-slice.md`).
