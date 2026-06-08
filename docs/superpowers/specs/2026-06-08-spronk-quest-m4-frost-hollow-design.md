# Spronk Quest — Milestone 4 (Frost Hollow + Ice Spell) Design

**Status:** Approved (brainstorming complete, 2026-06-08). Next: implementation plan.

**One-liner:** Dungeon 3 (Frost Hollow) introduces the **Ice spell** and reversible
**water↔ice terrain**, and is the first dungeon where Laurel wields **two** spells — `L`
finally cycles **Fire ↔ Ice**.

This is a **content milestone** that snaps into the shipped M2 world framework and M3 systems.
It follows the per-dungeon recipe: a new ability earned from a mid-dungeon `F` shrine, a themed
obstacle gate, a spronk to free, and a hub door that unlocks. The roadmap
(`docs/superpowers/specs/2026-06-03-spronk-quest-design.md`) pre-assigns D3 = Frost Hollow / Ice
spell / "freezes water into platforms".

---

## 1. Identity & arc

- **Theme:** Frost Hollow — a frozen grotto.
- **Ability earned:** **Ice spell** ⚡ (elemental; draws the magic meter; cast with `R`, same as
  Fire). Granted by an Ice `F` shrine partway through.
- **Spells in play:** Laurel walks in already owning **Fire** (from D2). After the Ice shrine she
  owns both, so `L` **cycles Fire ↔ Ice** (the cycle was a no-op in M3; M4 makes it real).
- **Arc (mirrors D2's proven pacing):**
  1. **Fire-first half** — bolt + Fire on Fire-flavored obstacles (vine gate, brazier), reach the
     Ice shrine. A teaser water pool is visible but uncrossable here (foreshadowing).
  2. **Ice shrine** (~midway) — earn Ice.
  3. **Frozen second half** — freeze water to cross, clear a Water gate, then a **dual-spell
     climax** that forces cycling Fire/Ice.
  4. **Spronk → exit.**

## 2. Headline mechanic — reversible water ↔ ice

- **Water** is a new **damaging hazard** tile (contact damage + i-frames, exactly like D2's
  lava). Deep/wide water spans are the gaps.
- Casting **Ice** at a water tile **freezes** it into a **solid ice platform** (collision becomes
  solid; bg shows ice).
- Casting **Fire** at an ice platform **melts** it back to water (collision becomes empty/hazard;
  bg shows water).
- The transition is **fully reversible** and both spells act on the **same** tile — this is the
  dual-spell puzzle engine.

**Reversibility is a system property, not a single scripted puzzle.** Fire melts *any* ice tile;
Ice freezes *any* water tile. Players can experiment; the critical path simply sequences both
spells. This is also the soft-lock cure (see §6).

## 3. Layout — Dungeon 3 (dense 64×22 gauntlet)

Same GBA 64×32-tile BG-cap reality as D2: D3 is one screen-tall, packed left→right. Exact tile
coordinates are finalized during authoring; this is the beat sequence.

**Fire-first half (cols ~3–29):**
- Spawn → basic **enemy** (magic source) → **vine gate** (Fire) → **brazier** beat (Fire) →
  **Ice shrine** (~col 30). A teaser **water pool** sits visible but uncrossable (no Ice yet).

**Frozen second half (cols ~30–62):**
- **Ice shrine** → earn Ice.
- **Teach-the-freeze:** a **wide water gap — wider than a double-jump** — with damaging water, so
  the player **must** freeze a stepping-stone platform mid-gap to cross. A magic source sits right
  before it.
- **Water gate** (frozen waterfall) — Ice clears it; the "new power opens the path" beat.
  `GateType::Water → Ability::Ice` already exists in `logic/gates.h`'s gate table.
- **Dual-spell climax (cols ~46–56):** freeze a water gap (Ice) → cross → an **Ice-wall gate**
  that melts only with **Fire** → cycle to Fire → a final brazier-or-water combo.
- **Spronk** (~col 60) → **exit** (~col 61).

## 4. What's new vs reused (three-layer purity preserved)

**New pure `logic/` (host-tested before the ROM builds):**
- `TileKind::Water` (next value after `Lava`) + a damaging-hazard rule reusing the `lava_overlap`
  pattern (generalize to a hazard-overlap that covers lava + water, or a parallel `water_overlap`).
- **Spell system generalized from one spell to two:** today `FireCast` + the fire pool assume a
  single effect. Generalize to a **typed `SpellCast`** (carries its `SpellId`) emitting
  **projectiles tagged Fire or Ice**; the scene resolves a hit by the projectile's type (Fire →
  burn/melt/light; Ice → freeze/clear-Water-gate). `SpellState::cycle(World)` rotates through the
  **owned** spells (Fire→Ice→Fire; no-op if only one owned); `selected` gains `SpellId::Ice`.
- **freeze/melt tile transition** — pure predicates (e.g. `ice_freezes(TileKind)` /
  `fire_melts(TileKind)`) returning the resulting tile; the scene mutates the collision+bg grid
  (mirrors D2's `fire_clears_gate` + `set_collision_tile`/`set_level_tile`).

**Reuses the existing gate framework:**
- **Water gate** = `GateType::Water`, already mapped to `Ability::Ice`. Ice projectile clears it,
  symmetric to Fire clearing Vine/Ice gates in D2.

**New `engine/`:**
- **Ice projectile art** (cyan orb) + the projectile pool carries a type tag.
- **Spell HUD icon** reflects the selected spell (Fire vs Ice icon).
- New bg tiles: **water** (animated blue, damaging) and **frozen ice platform** (may reuse the
  existing ice tile or a new one); **Water-gate** waterfall visual.

**New content (`tools/build_level.py` + level):**
- New ASCII symbols for **water** and the **Water gate**; the **Ice shrine** is just `F` with
  `"ability":"ice"` in the JSON (already supported by `ABILITY_ENUM`). Exact letters chosen in the
  plan, avoiding collision with existing symbols (`#.^~@CEoGVIFB=?*1-8`).
- `tools/levels/dungeon3.txt` + `dungeon3.json` → `DUNGEON3_DATA`.

**Reused as-is:** R/L spell input, magic economy (bolt-kill refills; spells spend), ability-shrine
grant, spronk/exit, persistent `PlayerState` vitals, fades, the additive `scene_dungeon`, and the
hub. **Hub:** Door 3 becomes enterable once D2's spronk is freed (extend `door_enterable`).

## 5. Magic economy

Both Fire and Ice cost the same (10 magic, the M3-tuned value) and draw the same meter, refilled
only by **bolt-kills** (not spell-kills). D3 must supply enough enemies/magic for its mandatory
casts on the critical path (the D2 lesson). Gap widths and cast counts are tuned in mGBA.

## 6. Soft-lock guards (design constraints, written here so the plan honors them)

1. **Water damages, doesn't instakill**, and pits are escapable by jumping — contact is a setback;
   death respawns at spawn (reused behavior, no new checkpoint system).
2. Wide gaps force a freeze, but a **magic source sits before every mandatory cast**.
3. Water is always **re-freezable** and ice always **re-meltable** → no melt-and-strand; there is
   no terminal state, only "cast again."
4. No puzzle may require melting the *only* platform with no water left to re-freeze.

## 7. Out of scope (deferred)

- **Audio** (victory sting, SFX) — whole-game audio milestone later.
- **Boss / Nightmare King.**
- **Mid-level checkpoints** — respawn-at-spawn is reused.
- **Brand-new enemy AI** — reuse the basic bolt-fodder enemy (+ optionally the D2 fire-immune red
  for variety). Slippery-ice-floor momentum physics is explicitly **not** in M4 (was an alternative
  that lost to the dual-spell focus).
- **Final art** — placeholders only.
- **Real-hardware flash** — inherited, deferred.

## 8. Testing strategy

Pure host tests (`bash tools/host_test.sh`) added for: `TileKind::Water` hazard overlap; the spell
generalization (Fire + Ice cast cost/cooldown, `cycle()` rotating owned spells, `selected`
correctness); freeze/melt tile transitions; Ice-clears-Water-gate; and `dungeon3` level data
(dims, border, exactly one each of shrine/spronk/exit, ≥1 water + Water gate + a Fire obstacle).
Logic-purity guard (`check_logic_purity.py`) stays clean. Then a full **mGBA playthrough** verifies
the arc, the freeze/melt feel, the dual-spell cycling, and the soft-lock guards.

## 9. Success criteria

- Ice spell earned from the shrine; `L` cycles Fire↔Ice; the HUD shows the selected spell.
- Ice freezes water into a standable platform; Fire melts it back; reversible.
- A wide water gap is impassable without freezing; the Water gate opens only with Ice.
- The dual-spell climax requires cycling both spells.
- Spronk freed → exit clears → hub Door 3 was enterable after D2, and D3 saves on clear.
- All new logic host-tested; three-layer purity preserved; mGBA-verified end-to-end.
