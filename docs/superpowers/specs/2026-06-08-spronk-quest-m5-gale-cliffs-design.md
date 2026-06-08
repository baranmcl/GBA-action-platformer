# Spronk Quest — Milestone 5 (Gale Cliffs + Glide) Design

**Status:** Approved (brainstorming complete, 2026-06-08). Next: implementation plan.

**One-liner:** Dungeon 4 (Gale Cliffs) is the game's first **vertical-ascent** dungeon, introducing
the **Wind Cloak / Glide** traversal ability and a tile-based **wind kit** — updraft shafts that
lift you while gliding, and gust zones that push you sideways.

A **content milestone** snapping into the shipped M2 framework + M3/M4 systems, following the
per-dungeon recipe: a new ability from a mid-dungeon `F` shrine, a spronk, a hub door unlock. The
roadmap (`docs/superpowers/specs/2026-06-03-spronk-quest-design.md`) pre-assigns D4 = Gale Cliffs /
Wind Cloak (Glide) / "long gaps, updraft shafts".

---

## 1. Identity & arc

- **Theme:** Gale Cliffs — windswept vertical heights.
- **Ability earned:** **Glide (Wind Cloak)** — a **traversal** ability (NO magic cost, NO
  projectile), the sibling of Featherleap. `Ability::Glide` is already scaffolded in
  `world_state.h`. Granted by an `F` shrine partway up.
- **Spells carried:** the player owns Fire + Ice (from D2/D3). D4 weaves in **one Fire beat** (a
  vine gate) as continuity; otherwise it is a pure Glide dungeon.
- **Arc (the proven earn-a-power pattern, but vertical):**
  1. **Lower climb** — ascend with **Featherleap** (double-jump between ledges); a **vine gate**
     (Fire) blocks one route. Reach the Glide shrine.
  2. **Glide shrine** (~mid-height) — earn the Wind Cloak.
  3. **Upper cliffs** — gliding + updrafts + gusts are *required* to ascend.
  4. **Spronk → exit** at the top.

## 2. The wind kit — three tile-based forces (all resolved in pure `Player::update`)

`Player::update(InputFrame, Tilemap)` already integrates horizontal accel/friction, jump/double-
jump, gravity, and collision (`src/logic/player.cpp`). The wind kit adds three rules there, each
driven by a held input and/or a tile the player overlaps — all host-testable with a small tilemap.

- **Glide (hold A):** a new `InputFrame.glide_held` (the A button **held**, distinct from the
  edge-triggered `jump_pressed`). While airborne, falling, glide held, and `abilities.glide`,
  **cap the downward velocity** at a gentle glide terminal (slow float) with full air control.
- **Updraft shaft (`TileKind::Updraft`):** a non-solid tile. While **gliding** over/in an updraft
  tile (`updraft_overlap(body, map)`), set the vertical velocity to a **rising** value — you go
  UP. If NOT gliding, the tile does nothing (you fall straight through). This makes Glide the key
  to every ascent.
- **Wind gust (`TileKind::WindLeft` / `TileKind::WindRight`):** non-solid tiles that add a
  **horizontal push** (`wind_push(body, map)` returns a signed accel) whenever the player overlaps
  them — strongest-felt mid-glide. The first directional force on the player in the game.

New `TileKind` values: `Updraft`, `WindLeft`, `WindRight` (after `IcePlatform=5`). All are
**non-solid** (the player passes through and is pushed). `is_solid` is unchanged.

## 3. Vertical level shape (the one real engine risk)

D4 is **vertically oriented** — taller than the 20-tile screen height, so the camera scrolls up as
the player climbs. This is a new level shape (D1–D3 were ~one screen tall). **The plan's FIRST task
verifies the camera + background handle a map taller than one screen** (a proof spike) before any
content is authored. Exact dimensions are chosen then, against the GBA regular-BG size cap.

The camera already centres on the player with clamping (`scene_dungeon`), so vertical scrolling
should "just work" — but it is unproven and MUST be verified before authoring.

## 4. Layout (bottom → top)

Beat sequence (exact coordinates finalized during authoring):
1. **Bottom (spawn).**
2. **Lower climb (Featherleap):** double-jump between ledges; a **vine gate** (Fire) blocks one
   route — burn it. An enemy or two on ledges (magic for the vine beat + flavor).
3. **Glide shrine** — mid-height ledge.
4. **Updraft shaft #1:** glide in → lifted up → higher ledge (teaches glide-rides-updraft).
5. **Gust gap:** glide across a gap while a wind gust shoves you sideways.
6. **Upper climb:** taller updraft + gust combination — the skill climax.
7. **Top: spronk → exit.**

## 5. Hazard / consequence

Falling is **non-deadly**: you drop to a lower ledge and re-climb. Intermediate ledges catch long
falls so a slip doesn't reset to the bottom. No new damage-hazard tile. Death (enemy contact only,
rare) respawns at the bottom — acceptable because falls aren't lethal.

## 6. What's new vs reused (three-layer purity preserved)

**New pure `logic/` (host-tested):**
- `Abilities::glide` (bool); `InputFrame.glide_held` (bool).
- `Player::update` glide/updraft/gust rules (gated by `abilities.glide` for glide+updraft; gusts
  push regardless of ability).
- `TileKind::Updraft` / `WindLeft` / `WindRight` (non-solid); `updraft_overlap(body, map)` and
  `wind_push(body, map)→Fixed` helpers (the `*_overlap` pattern from `hazard.h`).

**New engine:**
- Read `bn::keypad::a_held()` into `InputFrame.glide_held`.
- kind→bg mapping + placeholder art for updraft + wind tiles (wind streaks).
- Verify vertical camera/scroll for a taller map.
- (Optional polish, MAY skip) a "cloak open" avatar tint/sprite while gliding.

**New content:**
- Compiler symbols for updraft + wind-left + wind-right tiles (Glide shrine is `F` +
  `"ability":"glide"`, already supported by `ABILITY_ENUM`).
- `tools/levels/dungeon4.txt` + `dungeon4.json` → `DUNGEON4_DATA`.

**Reused as-is:** Featherleap (`player.cpp`), Fire (one vine gate), the ability-shrine grant,
spronk/exit, persistent `PlayerState`, fades, the additive `scene_dungeon`, the hub. **Hub:** Door
4 enterable once D3's spronk is freed (extend `door_enterable`).

## 7. Magic economy

Glide costs no magic. Only the single vine beat needs Fire (10 magic), funded by a nearby enemy
bolt-kill. Magic is otherwise irrelevant in D4.

## 8. Soft-lock guards

- No mandatory updraft before the Glide shrine.
- **Fire-owned invariant:** Door 4 requires D3 cleared → D2 cleared → Fire owned; the vine beat is
  always solvable. No Fire shrine in D4 (only Glide).
- Gusts never push the player into an inescapable spot; every fall lands on a re-climbable ledge.

## 9. Out of scope (deferred)

Audio; boss / Nightmare King; new enemy AI (reuse the basic patrol enemy); mid-level checkpoints;
final art; real-hardware flash. Glide does not interact with water/ice terrain (no glide-over-
freeze combo — that was an alternative that lost to the pure-Glide focus, beyond the one vine beat).

## 10. Testing strategy

Pure host tests (`bash tools/host_test.sh`): glide caps fall speed only when held AND owned;
updraft lifts only while gliding (and not otherwise, and not without the ability); `wind_push`
applies the correct signed horizontal force per tile; `updraft_overlap` detection; and `dungeon4`
level data (dims, border, exactly one Glide shrine + spronk + exit, ≥1 updraft + ≥1 wind tile + the
vine gate). Logic-purity guard stays clean. Then **mGBA** verifies the vertical scroll (proven
first), glide feel, updraft ride, gust push, the vine beat, and the full climb.

## 11. Success criteria

- Glide earned from the shrine; hold-A slows the fall with air control.
- Updraft shafts lift **only while gliding**; gusts push sideways.
- The upper cliffs are impassable without Glide; the vine beat needs Fire.
- Vertical scrolling works (the key risk, verified first).
- Spronk freed → exit clears → Door 4 was enterable only after D3.
- All new logic host-tested; three-layer purity preserved; mGBA-verified end-to-end.
