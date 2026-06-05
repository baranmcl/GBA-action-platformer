# Spronk Quest — Milestone 3: Ember Caverns + Fire Spell — Design

**Platform:** Game Boy Advance (Butano / devkitARM)
**Date:** 2026-06-04
**Builds on:** M2 world framework (shipped). See `2026-06-03-spronk-quest-design.md` (north star),
`2026-06-04-spronk-quest-m2-world-framework-design.md`.
**Status:** Design — pending implementation plan.

---

## 1. Concept & Scope

M3 is the **first real content dungeon**: a mechanics-first, **puzzle-rich Ember Caverns** that
introduces the **Fire spell**. It establishes the content template for D3–D8. **No boss** (bosses
are a deliberate separate effort).

Three reusable framework firsts: a **spell system** (selected-spell + cast button), an
**ability-pickup entity** (mid-dungeon shrine), and a generic **trigger→target puzzle system**.

**Explicitly OUT of scope:** boss; audio/SFX (M11); the Ice→Water systemic combo (needs D3's Ice);
final art; story/dialogue.

**Success =** Title → Hub → enter Dungeon 2 → first half (lava platforming, a pushable-block
puzzle, a hidden-button gate) using existing kit → **Fire shrine grants Fire** → second half (vine +
ice gates, a brazier puzzle, fire enemies) using Fire → free the caged spronk → return to hub with
Fire owned + Dungeon 2 cleared → saved. The hub's D2 door becomes enterable (it was "locked" in M2);
a Fire-gated hub element (optional) opens.

---

## 2. Spell System + Fire Spell

### Input (selected-spell + cast button)
- **B** = free basic magic bolt (unchanged: infinite, weak, short cooldown).
- **R** = cast the **selected** spell, spending magic.
- **L** = cycle the selected spell. In M3 the only spell is Fire, so L is effectively a no-op, but
  the selection state + cycling wiring exist so D3 (Ice), D8 (Light), etc. each just register as
  another spell entry with no new input design.
- A small HUD indicator shows the selected spell (M3: always "Fire" once owned; nothing shown before).

### Fire behavior
- A **fire projectile**: larger and a bit slower than the bolt; **costs 25 magic** per cast.
- Magic refills **25 per enemy kill** (existing), so the economy is "kill → bank one Fire cast." The
  free bolt remains the always-available fallback, keeping the economy generous (a choice, not a tax).
- On hit, a fire projectile: **damages enemies** (more than the bolt — e.g. an instant kill on basic
  enemies), **burns vine gates**, **melts ice gates**, **lights braziers**.

### Pure logic
- `logic::FireCast` mirrors `BoltCaster`: holds cooldown; `try_cast(pressed, magic_meter, origin,
  facing, out)` returns false if `magic.spend(25)` fails or on cooldown, else spawns a `BoltSpawn`-like
  fire projectile and spends magic. **Host-tested:** insufficient magic → no cast, meter unchanged;
  sufficient → spends 25 + yields projectile; cooldown respected; facing respected.
- "What fire hits" reuses `logic::aabb_overlap`. Whether a hit clears a gate uses `gates.h` `can_pass`
  / the gate type's required ability (Fire).

---

## 3. Ability Pickup + Reward Generalization

- New entity **`AbilityPickup { tx, ty, ability }`** placed in level data (the "Fire shrine").
  Overlapping it grants the ability: sets the `world.abilities` bit + a visual cue (shrine lights up).
  Pure rule `try_take_pickup(player, pickup, world) -> bool` (idempotent), host-tested.
- **Generalize the reward model.** Player abilities are driven from `world.abilities`, not a lone
  `featherleap` bool. `Player::Abilities` gains `fire` (and is sourced each frame from `world.has(...)`).
  The M2 hardcode "spronk grants Featherleap" is replaced by: **freeing the spronk sets the
  `spronks_freed` bit (marks the dungeon cleared)**; **abilities come from pickups**.
- **D1 compatibility:** D1's Featherleap moves to an ability-pickup placed in Dungeon 1 (so D1 and D2
  share one model), OR D1 keeps a special-case grant. Decision in the plan; both keep D1 playable.

---

## 4. New Gameplay Systems

All decision logic is **pure C++ (host-tested)**; Butano only renders.

### Obstacle gates rendered + cleared (vine, ice)
- New gate tiles for `GateType::Vine` and `GateType::Ice` (the table already maps both to Fire). A
  fire-projectile hit on a gate the player can't yet pass clears it: collision tile → empty + the bg
  tile updates (reusing the M2 `set_level_tile` + collision-buffer pattern). Host-tested: Fire clears a
  Vine/Ice gate; the bolt does not.

### Lava hazard
- A damaging tile kind. Body overlap with a lava tile drains the **health meter** (existing `damage`
  + 45-tick i-frames). First in-dungeon use of health. Pure rule `lava_overlap(body, tilemap) -> bool`,
  host-tested.

### Fire enemy
- A patroller variant **immune to Fire** (a `bool fire_immune` on the enemy): Fire passes through /
  doesn't kill it, so the player must use the bolt or avoid it. One new flag; reuses the M1 enemy
  patrol/contact logic. Host-tested: fire-immune enemy survives a fire hit, dies to a bolt.

### Pushable block
- `logic::PushableBlock` (a `Body` + grid alignment). When Laurel walks into it horizontally, it
  slides one tile if the destination is clear, stops at walls, and can rest on a **pressure plate** or
  fall into a gap (gravity). Pure physics, host-tested: push into clear → moves; push into wall →
  blocked; on a plate → plate "pressed"; pushed over a gap → falls.

### Trigger → target puzzle system
- A generic puzzle machine: a `Trigger` becomes *active* by one of:
  - **brazier group** — all braziers in the group lit (Fire),
  - **pressure plate** — a pushable block (or the player) resting on it,
  - **hidden button** — a concealed switch pressed (stepped on / bolt- or fire-hit).
- An active trigger opens its **target** (a door/gate tile → removed). Pure resolution
  `trigger_active(trigger, state) -> bool` + `apply_targets(world/level)`, host-tested with truth
  tables (e.g. 3-of-3 braziers lit → active; 2-of-3 → not). Visual feedback only in M3 (target opens +
  a flash); the satisfying **sound is deferred to M11**.

---

## 5. Dungeon 2 Design (structure A — Fire mid-dungeon)

**Arc (Zelda-style):**
1. **Enter** → first half tests existing kit: double-jump platforming over **lava** pools, a
   **pushable-block** puzzle (shove a block onto a plate to open a gate), a **hidden button** that
   opens another gate.
2. **🔥 Fire shrine** (ability pickup) grants Fire.
3. **Second half** uses Fire: **vine** and **ice** gates blocking the path, a **brazier** puzzle
   (light all braziers → open a door), **fire enemies** mixed with basic enemies.
4. Free the **caged spronk** (marks cleared) → reach the **exit** → fade back to the hub.

**Length/feel:** a real dungeon — target **~8–12 screens, a few minutes**, difficulty escalating.
Authored as data: `tools/levels/dungeon2.txt` + `.json` via the M2 compiler, extended with new
symbols (see §6). Ember-cavern look (warm placeholder palette).

**Hub integration:** the M2 hub's **Dungeon 2 door becomes enterable** (was a "locked" marker).
Clearing D2 + owning Fire may open one Fire-gated hub element (optional, low priority).

---

## 6. Architecture, Content & Testing

### Code (three-layer, purity preserved)
- `src/logic/` (pure, host-tested): `fire_cast`, `pushable_block`, `trigger`/`target` resolution,
  `ability_pickup` grant, lava-damage rule, fire-clears-gate rule, expanded `Player::Abilities`.
- `src/engine/`: spell input (R cast / L cycle), fire-projectile sprite pool (reuse the `BoltPool`
  pattern), rendering for new tiles + entities (gate tiles, lava, brazier lit/unlit, block, plate,
  button, shrine), ability-pickup/HUD spell indicator.
- `src/game/`: extend the data-driven `scene_dungeon` to spawn/update the new entities (additive — it
  is already `LevelData`-parameterized); `dungeon2` content + hub door unlock.

### Content / level compiler (`tools/build_level.py`)
- New grid symbols + JSON params: `F` Fire shrine (ability pickup), `B` pushable block, `=` pressure
  plate, `?` hidden button, `*` brazier, `~` lava, `V` vine gate, `I` ice gate, plus `target`/`trigger`
  group wiring in the JSON sidecar. **Python unit tests** for each new symbol + validation.
- New placeholder art (Pillow): ember tileset, vine/ice gate tiles, lava, brazier (lit/unlit),
  pushable block, pressure plate, hidden button, Fire shrine, fire projectile, fire enemy.

### Testing
- **Host (g++):** fire-cast economy; pushable-block physics; trigger/target truth tables;
  ability-pickup grant; lava damage; fire-clears-gate vs bolt-doesn't; fire-immune enemy; plus the
  Python compiler tests for new symbols.
- **mGBA:** the full Dungeon 2 playthrough (each system on screen), hub D2-door unlock, save persists
  Fire + D2-cleared.
- **Real hardware:** carry the discipline.

---

## 7. Open Questions / Deferred

- Whether D1's Featherleap moves to a pickup or stays special-cased — decided in the plan (both keep
  D1 working).
- Exact Fire cost (25 assumed) / projectile speed — tune in implementation.
- Brazier/hidden-button "press" affordance (step-on vs bolt-hit) — pin in the plan.
- Boss, audio/SFX, Ice→Water combo, final art, story → later milestones.
