# Spronk Quest — Milestone 7 (Thornwild Marsh + Vine Grapple) Design

**Status:** Approved (brainstorming complete, 2026-06-12). Next: implementation plan.

**One-liner:** Dungeon 6 (Thornwild Marsh) introduces the **Vine Grapple** — a hookshot-pull that
yanks Laurel to grapple-point anchors (and pulls one heavy block) — and is the **first real
multi-room dungeon**, built on the room-to-room feature (merged 2026-06-12): a branching swamp with
a SRAM-latched shortcut.

A **content milestone** snapping into the shipped M2 framework + M3–M6 systems + the room-to-room
feature, following the per-dungeon recipe: a new ability from a mid-dungeon `F` shrine, carried-kit
combo beats, a spronk, a hub-door unlock. The roadmap
(`docs/superpowers/specs/2026-06-03-spronk-quest-design.md`) pre-assigns
**D6 = Thornwild Marsh / Vine Grapple (swing/pull) / "grapple points, pull heavy blocks."**

---

## 1. Identity & arc

- **Theme:** Thornwild Marsh — an overgrown swamp.
- **Ability earned:** **Vine Grapple** — a **traversal** ability (NO magic for the pull), the fourth
  sibling of Featherleap/Glide/Dash plus a light puzzle facet (pull one heavy block).
  `Ability::Grapple` is already scaffolded in `world_state.h`. Granted by an `F` shrine partway
  through.
- **Carried kit:** the player owns Featherleap + Fire + Ice + Glide + Dash (D1–D5). D6 reuses them.
- **First real multi-room dungeon:** 4–5 rooms, **branching + a latched shortcut**, exercising the
  room-to-room primitives (entrances, room-doors, brazier-group latch persisted to SRAM).
- **Arc:**
  1. **First beats — carried powers** in a swamp setting (a Fire/Ice/Glide/Dash beat or two) on the
     way to the **Grapple shrine**.
  2. **Grapple headlines:** latch across chasms and up to overhead ledges; a **pull-block** puzzle;
     a **branch** whose grapple puzzle opens a **persisted latched shortcut**; carried-kit **combo
     beats** (glide→grapple, freeze→grapple).
  3. **Spronk → exit.** Hub **Door 6** enterable once D5's spronk is freed.

## 2. The Vine Grapple mechanic (pure `Player::update`, host-tested)

Architecture follows the proven Dash/wind pattern: anchors are **map tiles** and the pull is a
self-contained pure state machine, so the whole mechanic is host-testable and `bn::`-free.

- **Anchors as tiles:** new `TileKind::GrapplePoint` (next value after `Spikes=9`), a **non-solid
  marker tile** the pure logic can scan for (mirrors `Updraft`/`WindLeft`/`Spikes`). The existing
  `GateType::GrapplePoint` "geometry gate" is retired (wrong model for an active hookshot).
- **Selection via the L-cycle (no R arbitration):** Grapple joins the existing
  `SpellId` selection (`SpellId::Grapple`), **owned** when `world.has(Ability::Grapple)`. **L
  cycles** Fire → Ice → Grapple among owned; the HUD shows the selected tool. **R fires the
  selected tool:** when Grapple is selected, R triggers the grapple pull (costs **no magic**); when
  Fire/Ice is selected, R casts the spell (magic) as today. Selection-based firing means there is
  **no ambiguity** with casting and **no content constraint** on where anchors sit.
- **Auto-target + pull:** on R with Grapple selected, scan for the **nearest** `GrapplePoint` tile
  within `GRAPPLE_RANGE` (~6 tiles) in the facing/up aim arc; if found, **pull the player in a
  straight line toward the anchor** at a high `GRAPPLE_VX` for the pull duration, **stopping at the
  anchor tile or on wall collision** (reuse the existing swept collision). Reaches **wide gaps**
  (beyond jump+dash) and **overhead ledges** (pull up). No anchor in range → nothing happens (R is
  consumed as a no-op; the player simply isn't pulled).
- **Gated by `abilities.grapple`** — inert until the shrine is collected.
- **No new engine input plumbing:** R (`cast`) and L (`cycle`) already exist
  (`engine/spell_input.h`); the grapple reuses them. Aim is auto-target (no directional input).

New pure types: `Abilities::grapple` (bool); `grapple.h` `struct GrappleState` (the latch+pull
state machine, exposing `active()`, the pull velocity/target, range/aim scan); `SpellId::Grapple`
threaded through `SpellState::owns/has_any/cycle/refresh`.

## 3. Grapple beats + carried-kit combos (authoring, reused systems)

- **Cross a chasm** wider than jump+dash by latching to an anchor across it.
- **Pull up** to a high ledge by latching to an overhead anchor.
- **Glide → grapple:** jump → glide to drift near an anchor mid-air → grapple to finish a long reach.
- **Freeze → grapple:** freeze a water span (Ice) for footing, then grapple off an anchor beyond it.

No new code — these place existing Fire/Ice/Glide/Dash obstacles next to grapple anchors.

## 4. The pull-block beat (bounded — ONE puzzle)

Extend `PushableBlock` (M3) with a **pullable** flag. With Grapple selected, grappling a pullable
block **pulls it one tile toward the player** (the reverse of the existing push), updating its
collision cell. **Exactly one** authored puzzle: pull a heavy block off a far ledge (or out of an
alcove) to **bridge a gap you cannot reach to push from the other side**. Not a general system —
one beat that teaches "grapple moves objects too."

## 5. Multi-room structure (the showcase)

**4–5 rooms, branching + a latched shortcut**, authored with the room-to-room compiler symbols
(`N` entrances, `D` room-doors) and a latched brazier-group:

- **Room 0 (entry/hub):** a small marsh atrium that **branches** to two room-doors (left/right).
- **Branch A:** a grapple-traversal room whose puzzle **lights a brazier-group** (`latch_id`) that
  opens a **shortcut door back toward Room 0** — persisted to SRAM, so on re-entry the shortcut is
  already open (the round-trip is one-way after solving). Contains the **pull-block** beat.
- **Branch B:** a grapple-gated room holding the **spronk** and the **exit**, reachable only after
  the Grapple shrine (and using the shortcut from Branch A to traverse efficiently).
- **Grapple shrine** lives early (Room 0 or just past it) so all later rooms can assume Grapple.

This **replaces the throwaway D6 reference rooms** (`tools/levels/d6_room0/1` + their headers)
authored during the room-to-room QA with the real Thornwild Marsh content (rename to
`dungeon6_*` for consistency with D1–D5 naming, or keep `d6_*` — implementation detail). The
`DUNGEON6_DUNGEON` table in `include/game/levels/dungeons.h` grows to the real room set.

**Hub wiring:** make **Door 6** reachable — extend `door_enterable` (Door 6 enterable once D5's
spronk is freed), add a door-6 archway to `tools/levels/hub.txt` (+ its JSON), and map dungeon
number 6 → `DUNGEON6_DUNGEON` in `src/main.cpp`. (The room-to-room plan explicitly deferred Door 6;
M7 owns it.)

## 6. What's new vs reused (three-layer purity preserved)

**New pure `logic/` (host-tested):**
- `Abilities::grapple` (bool).
- `grapple.h` — `struct GrappleState`: nearest-anchor scan within range/aim, latch, straight-line
  pull at `GRAPPLE_VX`, stop-at-anchor / wall-stop, gated by `abilities.grapple`.
- `tilemap.h` — `TileKind::GrapplePoint` (non-solid) + `is_grapple_point()`.
- `spell.h` — `SpellId::Grapple` threaded through `owns/has_any/cycle/refresh` (owned via
  `Ability::Grapple`).
- `pushable_block.h` — a `pull(dir, map)` (reverse of `push`) + a pullable flag on the spawn.
- `Player::update` — integrate the grapple pull (override velocity toward the anchor while pulling;
  stop on collision); expose state for the scene's vine VFX.

**New engine:**
- Grapple-anchor bg tile + the **vine VFX** (a line/sprite drawn from the player to the anchor
  during the pull); kind→bg map entry for `GrapplePoint`; HUD icon for the selected Grapple.

**Scene (additive, `scene_dungeon.cpp`):**
- `player.abilities.grapple = world.has(Ability::Grapple)`; on R, branch on `spell.selected`
  (Grapple → grapple pull; Fire/Ice → existing cast); render the vine during a pull; handle the
  pull-block (grapple a pullable block → `pull` it one tile + update collision).

**New content:**
- Compiler symbols (`tools/build_level.py`) for the GrapplePoint anchor tile and the pullable
  block; the Grapple shrine is `F` + `"ability":"grapple"` (already supported by `ABILITY_ENUM`).
- `tools/levels/dungeon6` authored as the **multi-room** Thornwild Marsh set (replacing the
  throwaway reference rooms); `DUNGEON6_DUNGEON` room table; hub Door 6.

**Reused as-is:** the **room-to-room feature** (entrances, room-doors, latches, `run_dungeon`/
`play_room`, floor-aware door/exit/brazier render), the whole carried kit (Featherleap/Fire/Ice/
Glide/Dash), the gate framework, the spell pool + `SpellCast`, the ability-shrine grant,
spronk/exit, persistent vitals, fades, the 64×128 big-map background + camera clamp.

## 7. Soft-lock guards

- The grapple is **free** (no consumable) and **auto-targets**, so you can't waste it or miss.
- The **Grapple shrine is mandatory** before any grapple-gated beat (no anchor-only path before it).
- **Carried-kit invariant:** Door 6 requires D5 cleared → D4 → … → D2, so Featherleap + Fire + Ice +
  Glide + Dash are all owned; every carried-power beat is solvable. No new Fire/Ice/etc. shrine in
  D6 (only Grapple).
- A **bolt-enemy (magic) before any Fire/Ice beat** so spells are affordable.
- The **latched shortcut** only ever *opens* a path (never removes one) — it cannot strand you.
- The **pull-block** puzzle has a death-reset (the M5 block-reset on respawn) so a block pulled into
  a dead corner is recoverable; **START** resets the current room.
- **Room/camera constraint (from the room-to-room work):** every room is **≥30×20 tiles** (or the
  camera clamp degenerates) and uses the **standard ~22-tall floor** (so braziers are hittable).

## 8. Out of scope (deferred)

Audio; boss / Nightmare King; mid-level checkpoints; final art; new enemy AI; the **pendulum
swing-grapple** (hookshot-pull only); a **full pull-block system** (one beat only); D7 (Stone) and
D8 (Light).

### Tracked future track — expand existing dungeons D1–D5 into multi-room layouts

A deliberate, separate future effort (also saved to project memory): **revisit and expand the
existing dungeons D1–D5** (authored single-room before the room-to-room engine existed) into richer
**multi-room** layouts with more content and level-design elements. Each existing-dungeon retrofit
is its **own** spec → plan → build cycle, scheduled after the 8-dungeon spine progresses (D7/D8 +
the Nightmare King). M7 (this milestone) is the first dungeon built multi-room from the start and
serves as the reference pattern for those retrofits. Mind the room-to-room engine constraints:
rooms ≥30×20 tiles (camera) and the standard ~22-tall floor (brazier hit-body).

## 9. Testing strategy

Pure host tests (`bash tools/host_test.sh`):
- `GrappleState`: latches only with the ability AND an anchor in range; pulls toward the anchor at
  `GRAPPLE_VX`; stops at the anchor / on a wall; respects `GRAPPLE_RANGE`; no anchor → no pull.
- `TileKind::GrapplePoint`: non-solid; `is_grapple_point` true; kind→bg mapping.
- `SpellState` with Grapple: `owns` via `Ability::Grapple`; L-cycle rotates Fire→Ice→Grapple among
  owned; `refresh` picks a sensible default.
- `PushableBlock::pull`: pulls one tile toward the player; blocked by a wall; updates the cell.
- `dungeon6` level data: room count + `start_room`; entrances/room-doors resolve cross-room; ≥1
  latched shortcut (brazier-group `latch_id`); exactly one Grapple shrine + spronk + exit; ≥1
  GrapplePoint anchor; ≥1 pullable block; ≥1 carried-power obstacle (combo theme); every room
  ≥30×20 with a solid border.

Logic-purity guard stays clean. Then **mGBA** verifies: grapple feel (L-select, R-fire, auto-latch
across a gap and up to a ledge), the pull-block puzzle, the branching + the persisted latched
shortcut (re-enter → still open; power-cycle → still open), the carried-kit combo beats, the spronk
→ exit, and Door 6 enterable only after D5.

## 10. Success criteria

- Grapple earned from the shrine; **L cycles to Grapple, R fires it** (free, auto-targets the
  nearest anchor), pulling Laurel across a wide gap and up to an overhead ledge.
- **One pull-block puzzle** solves (grapple-pull a heavy block to bridge a gap).
- The dungeon is a **branching multi-room** with a **persisted latched shortcut** (SRAM).
- At least one **combo beat** chains a carried power with the grapple.
- Spronk freed → exit clears → **Door 6** was enterable only after D5.
- All new logic host-tested; three-layer purity preserved; mGBA-verified end-to-end.
