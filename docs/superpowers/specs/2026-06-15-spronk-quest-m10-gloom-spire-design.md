# Spronk Quest — Milestone 10 (Gloom Spire + Light) Design

**Status:** Approved (brainstorming complete, 2026-06-15). Next: implementation plan.

**One-liner:** Dungeon 8 (Gloom Spire) — the final dungeon — introduces the **Light spell**: a cast that clears **DarkVeil** gates and, in a room-wide flash, **reveals hidden platforms** (solid + visible) for a few seconds before they fade. A dark vertical **ascent** climbed by timing Light casts against the fading platforms, managing magic via respawning **magic crystals**. Freeing the 8th spronk opens the path to the Nightmare King (the boss is the next milestone).

A **content + systems milestone** on the shipped M2 framework + M3–M9 systems (room-to-room, lives/Game-Over, latched shortcuts, no-soft-lock invariants). The roadmap (`docs/superpowers/specs/2026-06-03-spronk-quest-design.md`) pre-assigns **D8 = Gloom Spire / Light spell / "reveals hidden platforms, dispels darkness — the King's bane."**

---

## 1. Identity & arc

- **Theme:** Gloom Spire — a dark vertical tower; the climactic **ascent** (deliberate contrast to D7's descent). "Approach to the King."
- **Reward power:** the **Light spell** (Ability bit 7, `Light`, already in the enum). Earned from a mid-dungeon `F` shrine in room 0.
- **Arc position:** D8 of 8 — the LAST dungeon. Carried kit on arrival: Featherleap, Fire, Ice, Glide, Dash, Grapple, Stone. Freeing spronk 8 means **all 8 spronks freed** → "the path to the Nightmare King opens" (a teaser; the **King boss fight is the NEXT milestone**, out of scope here).
- **Hub:** Door 8 becomes enterable once **spronk 7 (D7 Quaking Quarry) is freed** (`door_enterable` gains `|| (n==8 && w.spronk_freed(7))`); `main.cpp` dispatches `n==8 → DUNGEON8_DUNGEON`.

## 2. The Light spell (cast: clears DarkVeil + reveals hidden platforms)

Light joins the existing spell system as a new `SpellId::Light`. Cycled with **L** (Fire→Ice→Grapple→Light), fired with **R**, **costs magic** (like Fire/Ice — `SpellCast.cost`). One R-press does two things:
1. **Fires a Light projectile** that **clears `DarkVeil` gates** on contact — via the EXISTING gate-clear path (`gate_cleared_by(DarkVeil)` → Light), once `SpellId::Light` is added and `spell_for_ability(Light)` returns it. (DarkVeil is `required=Light, is_geometry=false, bg_tile=12` — art added in M8.) Persists the latch if the gate has one.
2. **Emits a room-wide reveal flash:** all **hidden platforms** in the current room become **solid + visible** for `REVEAL_FRAMES` (~3–4 s, tunable), then fade back to non-solid + invisible. **Re-casting refreshes** the timer (costing more magic). So you light the room, climb as far as you can before the platforms fade, and re-cast (spending magic) to extend the route.
- **Wiring (mostly small):** add `Light` to `enum class SpellId`; thread it through `SpellState` (owns/has_any/cycle/refresh) keyed on `Ability::Light`; map it in `spell_for_ability`; add a Light projectile sprite + the cast branch in the scene (mirror Fire/Ice). The HUD spell icon shows a Light glyph.

## 3. Hidden-platform system (the new engine work)

A new spawn type — a hidden platform — that the Light flash toggles:
- **Spawn (`level_data.h`):** a `HiddenPlatformSpawn { int tx, ty, len; }` array on `LevelData` + a compiler symbol (e.g. `h`, verify unused). At room load the platform's tiles are **non-solid + invisible** (no collision, no bg/sprite shown).
- **Scene behavior:** a room-level `reveal_timer`. On a Light cast, set `reveal_timer = REVEAL_FRAMES` and make ALL hidden-platform tiles solid (`set_collision_tile(...,1)`) + visible (sprites/bg shown). Each frame decrement; at 0, set them non-solid + invisible again. Re-cast resets the timer. (Mirrors M8's loose-platform sprite/collision handling, but **Light-toggled + timed** rather than gravity-dropped.)
- **Pure logic (host-tested):** a tiny `RevealState`/timer helper — `on_cast()` sets the timer, `tick()` decrements, `revealed()` true while > 0 — with exact-frame tests (the decay boundary), like `StoneState`/`DashState`. The collision/sprite toggling is the scene's (Butano) part.

## 4. Magic crystal (the recharge — balanced, no magic soft-lock)

Light costs magic and platforms fade, so the player must be able to recharge — but not trivially:
- **A magic-crystal pickup:** on overlap, **fully refills the magic meter** (`magic.cur = magic.max`).
- **Respawning recharge station, NOT a one-time collectible:** it is NOT latched. It **resets to un-collected on death-respawn** (like pushable blocks reset, per the M8 death-fix) AND on room re-entry — so EVERY attempt (fresh or post-death) has it available. This prevents a magic soft-lock (you can always reach magic to cast) without being a latched one-shot that vanishes on retry.
- **Reachable without magic/Light** (base movement only) so it can never be gated behind the thing it powers.
- **Balance levers (tuned in QA — "don't make the ascent too easy"):** crystal **spacing** (how much climb per recharge), `REVEAL_FRAMES` (fade window), Light **magic cost per cast**, and magic max. The crystal guarantees solvability; the *challenge* is fade-timing + casting efficiently to cover the stretch between crystals.
- **New:** a `MagicCrystalSpawn { int tx, ty; }` + compiler symbol (e.g. `$`, verify unused) + a crystal sprite; scene overlap → refill + reset-on-respawn (mirror the heart-container spawn/overlap, but respawning + magic instead of one-time + HP).

## 5. Gloom Spire dungeon (3 rooms, ascent + branch + latched shortcut)

Room-to-room framework; rooms ≥30 wide; standard floor; two-way doors + the M9 hub-return `Q` door; the M9 lives system applies (deaths cost lives; freeing spronk 8 grants +1 max life + refill, per M9).
- **Room 0 — Spire Base (entry):** the Light `F` shrine (`{"pickups":[{"ability":"light"}]}`); a tutorial **timed-reveal climb** (cast Light → a short hidden-platform staircase appears → climb before it fades) with a **magic crystal** at its base; a **DarkVeil gate** (cast Light to clear) demonstrating gate-clearing; a magic-source enemy. Two-way doors to Room 1 + Room 2, hub-return `Q`.
- **Room 1 — branch (puzzle + shortcut):** a DarkVeil-gated section + a **latched shortcut** (a latched `CrackedFloor`/gate the player opens once, persisted). A carried-kit combo (e.g. Stone-pound or Grapple to reach a DarkVeil gate, then Light to clear it). Optional reward. Two-way return.
- **Room 2 — The Ascent (spronk):** the climactic vertical **timed-reveal climb** up the spire — light the room, climb the fading hidden platforms, re-cast (managing magic via crystals spaced up the climb) — with carried-kit combos interleaved (grapple to an anchor, glide a gap, freeze a hazard). The **spronk 8 (`C`) + exit (`E`)** at the top. Two-way return.
- **King teaser (out-of-scope stub):** freeing spronk 8 = all 8 freed. The dungeon completes normally (earn Light, exit). The actual "path to the King" / Door 9 / boss is the NEXT milestone — D8 does NOT build the King; at most a single line of on-screen text or a visibly-locked element noting the path has opened (optional, cheap).

## 6. What's new vs reused (three-layer purity preserved)

**New (pure logic):** `SpellId::Light` + `SpellState` threading; `spell_for_ability(Light)`; a `RevealState` timer helper; `HiddenPlatformSpawn` + `MagicCrystalSpawn` in `level_data.h`. **New (engine/scene):** the Light cast branch + projectile (clears DarkVeil) + the room reveal-flash toggling hidden-platform collision/visibility on a timer; the magic-crystal overlap→refill + reset-on-respawn; Light projectile sprite, hidden-platform sprite, crystal sprite, Light HUD icon; the death-respawn reset for crystals (extend the existing block-reset). **New (compiler):** symbols for hidden platform + magic crystal. **New (content):** the 3 Gloom Spire rooms + hub Door 8. **Reused:** the spell system (Fire/Ice pattern), the existing `DarkVeil` gate + gate-clear path, the room-to-room primitives, latch persistence, the M9 lives system, the no-soft-lock invariant harness (2-wide flood-fill), `F` shrine, the heart-container/loose-platform spawn+sprite patterns. **Three-layer rule:** all spell/timer/data logic that can be pure is host-tested; only `bn::` rendering/scene work lives in engine/game.

## 7. Out of scope (deferred)

The **Nightmare King boss fight + the path/Door-9 to him** (the next milestone — D8 only frees spronk 8 + earns Light, with at most a cheap teaser); a literal darkness/visibility overlay (hidden-platforms-only was chosen — no screen-darkening); Light as an aura/torch (it's a cast spell); music; non-placeholder art; retuning earlier dungeons to the Light economy.

## 8. Testing strategy

- **Pure-logic host tests:** `SpellId::Light` threads through `SpellState` (cycle reaches Light when owned; owns/has_any/refresh); `spell_for_ability(Light)==SpellId::Light` and `gate_cleared_by(DarkVeil)==SpellId::Light`; the `RevealState` timer (on_cast sets, tick decrements, `revealed()` true while >0, exact-frame decay boundary, re-cast resets); `level_data.h` round-trips `HiddenPlatformSpawn` + `MagicCrystalSpawn`.
- **Compiler unit tests:** the new hidden-platform + magic-crystal symbols parse + emit; levels without them still compile.
- **No-soft-lock invariant tests (2-wide player, `test_dungeon8_level.cpp`):** standard (shrine + spronk grounded; two-way doors; rooms ≥30 wide; reachability). Plus REQUIRED Light-specific ones, each fail-on-broken:
  - `d8_spronk_requires_light`: the spronk/exit (and the gated path) are NOT reachable by the base movement kit (treating hidden platforms as non-solid + DarkVeil gates as closed) — i.e. Light is genuinely required; and become reachable when hidden platforms are treated solid + DarkVeil open.
  - `d8_magic_source_before_each_light_beat`: a **magic crystal (or enemy) reachable by base movement before each Light-gated climb** (so you can always afford to cast — no magic soft-lock).
  - `d8_no_pit_traps` / `d8_no_one_way_traps`: the M8/M9 guards hold (a fall during a timed climb lands somewhere escapable; lives/START-reset backstop).
- **Build gates:** `bash tools/host_test.sh` green (N/N), `python tools/check_logic_purity.py` clean, `bash tools/build_rom.sh` → `ROM fixed!`. Manual emulator QA (the established round-based loop) — especially **balancing** the ascent: tune crystal spacing, `REVEAL_FRAMES`, and Light cost so it's challenging but not soft-lock-prone; verify the timed reveal, DarkVeil clearing, magic recharge, lives interaction, and spronk-8 completion.

## 9. Success criteria

- The Light spell clears DarkVeil gates and, per cast, reveals the room's hidden platforms (solid + visible) for a tunable window before they fade; re-casting refreshes (costing magic).
- Magic crystals fully refill magic, respawn each attempt, and are reachable without magic — no magic soft-lock; the ascent is challenging (fade-timing + casting efficiency) but solvable (invariants green, fail-on-broken).
- Gloom Spire (3 rooms) is completable end-to-end with the full kit + Light; freeing spronk 8 earns Light, completes the 8-dungeon spine, and teases the King (boss deferred).
- Hub Door 8 opens after D7; Light persists in the `abilities` bitmask; shortcut persists via latches — no SaveData format change. All host tests green, purity clean, ROM builds; no soft-locks.
