# Spronk Quest — Milestone 8 (Quaking Quarry + Stone ground-pound) Design

**Status:** Approved (brainstorming complete, 2026-06-13). Next: implementation plan.

**One-liner:** Dungeon 7 (Quaking Quarry) introduces the **Stone ground-pound** — a downward slam
(Down + A in the air) that smashes cracked floors to descend, trips heavy switches, crushes armored
enemies, breaks boulders, and shakes loose platforms — built on the room-to-room framework as a
vertical descent through crumbling mines with a SRAM-latched shortcut.

A **content milestone** snapping into the shipped M2 framework + M3–M7 systems + the room-to-room
feature, following the per-dungeon recipe: a new ability from a mid-dungeon `F` shrine, carried-kit
combo beats, a spronk, a hub-door unlock. The roadmap
(`docs/superpowers/specs/2026-06-03-spronk-quest-design.md`) pre-assigns
**D7 = Quaking Quarry / Stone spell (ground-pound) / "smashes cracked floors, slams switches."**

---

## 1. Identity & arc

- **Theme:** Quaking Quarry — crumbling mines/quarry; the dungeon descends (you pound *downward*
  toward the spronk at the bottom).
- **Reward power:** the **Stone ground-pound** (Ability bit 6, `Stone`, already in the enum). Earned
  from a mid-dungeon `F` shrine in room 0 (the established recipe).
- **Arc position:** D7 of 8. Freeing spronk 7 opens hub Door 8 (Gloom Spire / Light, the finale —
  M9, not built here). Carried kit on arrival: Featherleap, Fire, Ice, Glide, Dash, Grapple.
- **Hub:** Door 7 becomes enterable once **spronk 6 (D6 Thornwild Marsh) is freed**
  (`door_enterable` gains `|| (n==7 && w.spronk_freed(6))`).

## 2. The Stone ground-pound mechanic (pure `Player::update`, host-tested)

A new pure-logic `StoneState` (in `include/logic/stone.h`), mirroring `DashState`/`GrappleState`,
fed once per frame from `Player::update`. NO `bn::` types; integer fixed-point only.

- **Trigger:** while **airborne** (`!body.on_ground`) and `abilities.stone`, pressing **Down + A**
  (`in.down` held + `in.jump_pressed`) starts a pound. This **overrides the Featherleap air-jump**
  (when Down is held, a jump-press pounds instead of double-jumping). Requiring A (not Down alone)
  makes it deliberate.
- **Pound physics:** sets a high downward velocity (`POUND_VY`, faster than the normal fall terminal),
  **locks horizontal velocity to 0** (no air control during the pound), and grants **i-frames**
  (`invincible()`, like Dash) until landing. Applied in `Player::update` AFTER gravity/glide and
  with precedence over the air-jump; grapple still wins if simultaneously active (consistent ordering).
- **Landing:** when the pound body becomes grounded on a solid, non-cracked tile, the pound ends and
  `StoneState` raises a one-frame **`just_landed`** signal (with the impact tile position) that the
  scene reads to resolve interactions. State then resets; the charge is available again next airtime.
- **Pure-logic exposure:** `pounding`, `active()`, `invincible()`, `just_landed()`, and the impact
  position. Fully host-tested (trigger, fast-fall, i-frames, just_landed edge, Down+A overrides
  double-jump, no pound while grounded, no pound without the ability).

## 3. Stone interactions + carried-kit combos (scene-resolved at impact)

The pound itself is pure logic; the **scene resolves interactions** at the `just_landed` impact
point against level entities — exactly how the scene already resolves Dash→CrackedWall, spell→gate,
bolt→enemy. Five interactions:

1. **CrackedFloor smash** *(core descent)* — reuses the **existing `CrackedFloor` gate type**
   (sibling of `CrackedWall`). Pounding onto a cracked-floor tile shatters it and the player
   **keeps falling** into the area below (a pound only *stops* on solid, non-cracked ground). The
   vertical mirror of Dash→CrackedWall. May persist via a latch when it opens a permanent route.
2. **Heavy switch** — a **pound-only** pressure-plate variant (a "heavy" flag on the plate spawn;
   a normal step or pushed block does NOT trip it). A pound landing on it fires its target
   (`open_column` a gate / drop a loose platform). Reuses the plate→target wiring + latch persistence.
3. **Crush enemies** — a pound landing on (or i-frame-passing through) an enemy kills it, **including
   fire-immune/armored foes** the bolt and Fire can't kill; refills magic like a bolt-kill. Reuses
   the Dash-style i-frame contact path.
4. **Break boulders** — a **boulder block** (new breakable block variant, quarry rubble) shatters
   when pounded from above, clearing a path/footing. Distinct from the pushable crate (`B`/`P`).
5. **Shake-loose platforms** *(the one new engine system — built last, kept simple)* — the impact
   emits a **shockwave**; suspended **loose platforms** within range fall **straight down until they
   hit solid** (drop-to-rest, no physics), forming a bridge/step or opening a drop. New falling-terrain
   handling in the scene + collision-grid update on rest.

**Carried-kit combos** (Quaking Quarry threads Stone with earned powers): pound through a cracked
floor INTO a shaft you must **grapple** back up or **glide**; **freeze** water *below* a cracked
floor so you don't pound into a hazard; contrast **Dash** (cracked WALLS, horizontal) with **Stone**
(cracked FLOORS, vertical) in one area; crush a **fire-immune** enemy a Fire-only player couldn't.

## 4. Multi-room structure (Quaking Quarry — 3 rooms, descent + branch + latched shortcut)

Room-to-room framework; rooms ≥30×20, standard ~22-tall floor (content row 18, floor row 20);
**two-way doors** with co-located return entrances (the D6 lesson); branching + a latched shortcut.

- **Room 0 — Quarry Mouth (entry):** the **Stone `F` shrine** (earn the pound), a tutorial
  **cracked floor** to pound through, a first **heavy switch**, a magic-source enemy. Two-way doors
  branch to Room 1 (puzzle) and Room 2 (descent).
- **Room 1 — Cut & Fill (puzzle branch):** **break a boulder** blocking the way; **pound a heavy
  switch** that drops a **shake-loose platform** into a bridge; a heavy switch opens the
  **latched shortcut** (persisted to SRAM). Optional reward: a **heart container** (the M7 system,
  latch-persisted) behind a Stone-only pocket. Two-way return.
- **Room 2 — The Deep Shaft (descent + spronk):** pound through **stacked cracked floors** to
  descend; **crush an armored/fire-immune enemy** mid-shaft (forces the pound); one carried-kit
  combo (freeze water below a cracked floor, or grapple back up). The **spronk (`C`) + exit (`E`)**
  at the bottom. Two-way return.

## 5. What's new vs reused (three-layer purity preserved)

**New (pure logic):** `StoneState` (`include/logic/stone.h`); a `heavy` flag on the plate spawn; a
**boulder** block variant + a **loose platform** spawn in `level_data.h`; an `in.down` input field.
**New (engine/scene):** pound input wiring + impact resolution (the 5 interactions); a Stone HUD/VFX
cue (pound dust/impact — placeholder); falling-terrain handling for loose platforms; a Stone shrine
art reuse. **New (compiler):** symbols for boulder, heavy switch, loose platform (CrackedFloor gate
already exists). **New (content):** the 3 Quaking Quarry rooms + hub Door 7. **Reused:** room-to-room
primitives, gates/`CrackedFloor`, pressure-plate→target, enemies + i-frame crush (from Dash),
pushable-block scene sync, latch persistence, heart container, `F` shrine, fixed-point physics.
**Three-layer rule preserved:** all interaction *rules* that can be pure are host-tested in logic;
only `bn::` rendering/sprite work lives in engine/scene.

## 6. Soft-lock guards (FIRST-CLASS — escalated for irreversible descent)

Cracked-floor descents are **irreversible** (you can't pound back *up*), and Stone manipulates the
world (breaks boulders, drops platforms, trips switches) — so the soft-lock surface is larger than
any prior dungeon. Guards (machine-checked where possible — see §8):

- **Irreversible-descent forward-path:** every cracked-floor drop lands somewhere with a **reachable
  forward exit** (next door / beat / spronk). Never drop the player into a sealed pit.
- **Manipulable-object safety:** no **boulder-break**, **loose-platform-drop**, or **heavy-switch**
  outcome may remove the *only* path — each interaction's resulting geometry must preserve a forward
  route (the M7 pull-block "can't be stranded / can't fall off" lesson, generalized).
- **Resource sufficiency:** any **magic-consuming** beat (Fire/Ice) has a **reachable magic source
  before it**, or is solvable with always-available powers (Stone/Dash/Featherleap/Glide/Grapple).
- **Pit-escape:** a pound that lands in a hole leaves a way out (the hole is jumpable, or has a
  forward exit) — a pound never traps the player.
- **Backstops:** every room stays exitable via its **two-way door**, and **START resets the room**
  with full health/magic (the shipped anti-soft-lock). These are backstops, not excuses — the
  intended path must be completable without them.

## 7. Out of scope (deferred)

Boss fights; D8/Light/Nightmare King (M9+); Stone interactions beyond the five above (e.g. Stone as
a thrown projectile, stone-block conjuring, stone armor); music; non-placeholder art; the tracked
future tracks (expand D1–D5 to multi-room; powers/controls menu; dungeon/progression debug selector;
the shared player-ability controller refactor). Loose-platform physics stays the bounded
**drop-to-rest** (no momentum/bounce).

## 8. Testing strategy

- **`StoneState` host tests** (pure logic, like `test_dash.cpp`): pound triggers on Down+A while
  airborne with the ability; does NOT trigger grounded / without the ability / on Down-alone; fast
  downward velocity + zeroed horizontal during the pound; i-frames active until landing;
  `just_landed` fires exactly one frame on solid-ground impact; **Down+A overrides the Featherleap
  air-jump**; charge refreshes after landing.
- **Pure interaction-rule tests:** the pound-only "heavy" plate trips on a pound but not a step/block;
  boulder break rules; cracked-floor "shatter-and-keep-falling vs stop-on-solid" predicate.
- **Level-invariant host tests** (`test_dungeon7_level.cpp`, on the compiled room data): shrine +
  spronk grounded on the content row; two-way doors with co-located return entrances; every
  cracked-floor reachable; the descent solvable; rooms ≥30×20.
- **No-soft-lock invariant tests (per the §6 guards — explicitly required):**
  - **`d7_every_cracked_floor_drop_has_forward_exit`** — for each cracked-floor tile, the landing
    area below has a reachable door/exit/next-beat (you can't pound into a sealed pit).
  - **`d7_manipulable_objects_cannot_strand`** — boulder/loose-platform/heavy-switch outcomes
    preserve a forward path (geometry asserted, like M7's `d6_room1_block_cannot_fall_off`).
  - **`d7_magic_beats_have_a_source_before_them`** — any Fire/Ice-gated beat has a reachable magic
    source earlier, or an always-available alternative.
  - **`d7_no_pound_pit_traps`** — every place a pound can land is escapable (jumpable hole or forward
    exit).
- **Compiler unit tests** (`tools/test_build_level.py`): the new boulder / heavy-switch / loose-
  platform symbols parse + emit correctly; levels without them still compile.
- **Build gates:** `bash tools/host_test.sh` green (N/N), `python tools/check_logic_purity.py` clean,
  `bash tools/build_rom.sh` → `ROM fixed!`. Manual emulator QA per the established round-based loop.

## 9. Success criteria

- The Stone ground-pound (Down + A airborne) smashes cracked floors to descend, trips heavy switches,
  crushes armored/fire-immune enemies, breaks boulders, and shakes loose platforms into place.
- Quaking Quarry (3 rooms) is completable end-to-end — descend, solve the branch, free spronk 7 — with
  the carried kit, **no soft-locks** (the §6 guards hold; the §8 invariants are green).
- Hub Door 7 opens after D6; Stone persists in the `abilities` bitmask; shortcut + optional container
  persist via latches — **no SaveData format change**.
- All host tests green, purity clean, ROM builds, full-branch review READY TO MERGE; QA scaffold (if
  used) reverted before the final commit.
