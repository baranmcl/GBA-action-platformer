# Spronk Quest — Milestone 11 (Nightmare King Finale) Design

**Status:** Approved (brainstorming complete, 2026-06-15). Next: implementation plan.

**One-liner:** The finale. Once all 8 spronks are freed, **Door 9** opens in the hub onto a short **2-room traversal approach** and then a fixed-arena **boss fight against the Nightmare King** — a "use everything" gauntlet of 3 escalating phases where you chain your traversal powers to open a damage window, hit the King with **Light** to expose him, and **wound** him with bolt + elemental spells. Defeating him plays a victory screen, persists a **game-beaten** flag (the game's first real win state), and the title screen reflects completion. The fight's pure-logic core (`BossState`) is written to generalize so a shared boss framework can be extracted later (M12).

This is a **systems + content milestone** on the shipped M2 framework + M3–M10 systems (room-to-room, lives/Game-Over, latched shortcuts, spells, the Light reveal/expose hook, magic crystals, no-soft-lock invariants). The roadmap (`docs/superpowers/specs/2026-06-03-spronk-quest-design.md` §2 Finale) pre-assigns the finale: "Once all 8 spronks are freed, the path to the Nightmare King opens. He is defeated by chaining gathered powers — Light to expose him, elemental spells to wound him."

---

## 1. Identity & arc

- **Encounter:** the Nightmare King — the game's final boss and **first boss of any kind** (combat to date is a single-HP ground patroller, `include/logic/enemy.h`). This milestone builds the **first boss machinery**.
- **Arc position:** post-D8. All 8 spronks freed + all 8 abilities owned (Featherleap, Fire, Ice, Glide, Dash, Grapple, Stone, Light). The finale is a celebration of the full kit — a "use everything one last time" gauntlet.
- **Win state:** the game currently has **no way to win**. This milestone adds the ending: a victory screen + a persistent completion flag, closing the critical path to "a finishable game."
- **Hub:** a new **Door 9** appears/opens once `spronk_count(world) == 8`; entering routes through the approach into the boss.

## 2. The fight — "use everything" gauntlet, 3 escalating phases

The free wand bolt always damages the King **only while he is EXPOSED**. Each phase the player must use a different slice of the carried kit to create the opening, then hit the King with **Light** (his bane) to EXPOSE him for a short vulnerable window, then **wound** him with bolt + Fire/Ice during that window. Phases escalate in attack intensity.

- **Phase 1 — Aerial:** the King hovers out of reach behind a shield, with a sweeping attack. Use **Grapple** (to an anchor) + **Glide** to line up and land a **Light** hit → **EXPOSED** (~window) → bolt/Fire/Ice wound. Shield returns; repeat until the P1 HP threshold.
- **Phase 2 — Ground:** the King grounds behind a guard, attacks faster. **Dash** through a gap + **Stone-pound** a weak platform to stagger/drop his guard, then **Light** → EXPOSED → wound.
- **Phase 3 — All-in:** a bullet-hell finish. **Featherleap** + **freeze a hazard into a foothold with Ice** to reach the exposing position amid escalating attacks, then **Light** → EXPOSED → wound to 0.

**Damage model (reuses existing hooks):** the King is a `logic::Body` with an HP counter. Damage registers through the SAME primitives every enemy/gate/brazier uses — `SpellPool::consume_hit(body, SpellId)` (Fire/Ice/Light) and `BoltPool::consume_hit(body)` (free bolt) — but only subtracts HP while `BossState` is in the EXPOSED window. A **Light** `consume_hit` triggers EXPOSE (mirrors the M10 Light-clears-DarkVeil path); bolt/Fire/Ice `consume_hit` during EXPOSE subtract HP.

**Magic economy (no magic soft-lock):** Light + elemental casts cost magic. A respawning **magic crystal** (the M10 `MagicCrystalSpawn` pickup — full refill, resets each attempt) sits in the arena, reachable by base movement, so the player can always afford the keystone Light cast. Same guarantee as Gloom Spire: solvability is guaranteed; the challenge is execution + timing, balanced in QA (expose-window length, HP thresholds, attack timings, magic cost).

## 3. Pure-logic core — `include/logic/boss.h` (host-tested; the M12 extraction seed)

The boss's state/decisions are pure C++ (no `bn::`), host-testable like `RevealState`/`StoneState`/`DashState`. Written to **generalize** so M12 lifts it into a shared framework — a *move*, not a rewrite.

```cpp
// The 2-room approach runs OUTSIDE BossState (separate room flow); BossState begins at P1 when
// run_boss() starts. EXPOSED is a phase overlay: on_light_hit() saves the current phase into
// exposed_return and sets phase=Exposed; when expose_timer hits 0, phase returns to exposed_return.
enum class BossPhase : uint8_t { P1=0, P2, P3, Exposed, Defeated };

struct BossState {
    int hp = 0;                 // integer HP; defeated at <= 0
    BossPhase phase = BossPhase::P1;
    BossPhase exposed_return = BossPhase::P1; // phase to resume when the expose window ends
    int phase_start_hp = 0;     // HP boundary at the active phase's entry (full-fight restart -> P1 full)
    int expose_timer = 0;       // >0 while EXPOSED (vulnerable); decremented by tick()
    int attack_timer = 0;       // drives the current phase's deterministic attack pattern

    // pure transitions (exact-frame, host-tested):
    void on_light_hit();        // enter EXPOSED (set expose_timer, remember exposed_return)
    void on_wound(int dmg);     // subtract hp ONLY while EXPOSED; cross thresholds -> advance_phase()
    void tick();                // decrement timers; expose_timer hits 0 -> return to exposed_return phase
    void on_player_death();     // FULL-FIGHT RESTART: phase=P1, hp=full, timers reset
    bool exposed() const;       // expose_timer > 0
    bool defeated() const;      // hp <= 0 (or phase==Defeated)
};
```

**Design discipline for M12-ready generalization:** generic naming (`BossPhase`, not `KingRage`); integer-only; attack patterns **data-described** (a small per-phase pattern table the scene reads), NOT a hand-unrolled `if`-ladder; all decisions in logic, only sprite/rendering in the scene. HP thresholds for P1→P2→P3 are named constants (single source of truth, shared by scene + tests).

## 4. Encounter structure & scene wiring (hybrid approach)

The novel/hard part (phase machine + boss combat) lives in a focused new scene; traversal reuses the existing framework.

- **Approach (2 rooms, no combat):** built with the existing room-to-room framework as a small "dungeon 9" — a "use everything one last time" runway (chain carried traversal powers to cross). **Intentionally minimal (framework/testing pass); may be expanded later** alongside the planned dungeon content-expansion work. The approach's exit hands off to the boss scene. **Not repeated on death.**
- **Boss arena (bespoke):** a new `run_boss()` scene (`src/game/scene_boss.cpp` + `include/game/scene_boss.h`) — a **fixed-camera single-screen arena**. Owns the King entity, renders/drives `BossState`, applies the M9 lives/Game-Over flow, and resolves victory.
- **Dispatch:** Hub Door 9 (enterable when `spronk_count==8`) → `main.cpp` runs the 2-room approach (run_dungeon-style) → on approach exit, calls `run_boss()` → on victory, the ending; on QuitToTitle, back to title.

**Scene result:** `run_boss()` returns a result enum (e.g. `BossResult { Victory, QuitToTitle }`) consumed by `main.cpp`, mirroring `DungeonResult`.

## 5. Death / lives / restart (M9 integration)

- **Health → 0 with lives > 0:** lose a life (M9 `lose_life`), **restart the whole fight at Phase 1, King at full HP**, vitals refilled — via `BossState::on_player_death()`. The 2-room approach is **not** repeated.
- **Lives == 0:** **Game Over** (reuse `run_game_over`) → **Continue** restarts the fight from Phase 1 (approach not repeated); **QuitToTitle** returns to the title. Both choices refill lives + vitals (the M9 empty-bar fix applies).

## 6. Victory + persisted completion (the win-state gap)

- **On King HP → 0:** a brief victory beat (King collapses) → a **"THE END — CONGRATULATIONS"** screen → set a persistent **`beaten`** flag → return to hub/title. The **title screen reflects completion** afterward (e.g. a "GAME COMPLETE" line / marker).
- **Save v5 (no `sizeof` change):** add a `bool beaten` to `World`, stored in one of the existing **padding bytes [17..19]** of the 20-byte `SaveData` (so `sizeof(SaveData)` stays 20). Bump `SAVE_VERSION` 4 → 5, extend the checksum (`checksum_v5`) to cover the new byte, and add a **v4→v5 migration** that defaults `beaten=false`. v1/v2/v3 migrations also default `beaten=false`.

## 7. What's new vs reused (three-layer purity preserved)

**New (pure logic):** `include/logic/boss.h` — `BossState` + `BossPhase` + HP thresholds + the per-phase attack-pattern table; `World.beaten` + save v5 (`checksum_v5`, migration). **New (engine/scene):** `run_boss()` arena scene (King entity render + attack-pattern playback + EXPOSE/wound resolution via the existing pools + M9 lives/Game-Over + victory); the victory/"THE END" screen; King + arena placeholder art. **New (content):** the 2-room approach + the boss-arena layout; hub Door 9. **Reused:** `SpellPool`/`BoltPool` `consume_hit` damage hooks, the M10 Light-hit hook (now EXPOSE instead of gate-clear) + `MagicCrystalSpawn`, the room-to-room framework (approach), the M9 lives/Game-Over/`run_game_over` flow, the no-soft-lock invariant harness (2-wide flood-fill), `run_title` (completion reflect), the SaveData migration pattern. **Three-layer rule:** all boss state/decisions + save logic are pure + host-tested; only `bn::` rendering/scene work lives in engine/game.

## 8. Out of scope (deferred)

- The **shared boss-framework extraction + per-dungeon bosses** (M12 — this milestone builds the King concretely with extraction-ready logic, but does NOT generalize yet).
- **Elaborate story/credits** ending (M12 Story & Cutscenes — the ending here is a simple win screen + persisted completion).
- **Expanding the 2-room approach** into a longer finale gauntlet (revisit during the dungeon content-expansion pass).
- Final (non-placeholder) art; music/SFX (M-audio).

## 9. Testing strategy

- **Pure-logic host tests (`test/test_boss.cpp`):** P1→P2→P3 HP thresholds (exact); `on_light_hit` enters EXPOSED + sets `exposed_return`; `on_wound` subtracts HP **only while EXPOSED** (no damage while shielded); `tick` expose-window decay (exact-frame boundary, `exposed()` true while >0); `on_player_death` resets to P1 full HP + clears timers; `defeated()` at hp<=0; attack-pattern table is deterministic (same phase+timer → same pattern step).
- **Save tests:** v5 round-trip (`beaten` true/false survives); **every** migration v1/v2/v3/v4 → v5 defaults `beaten=false` (TEST-4: cover corruption + every migration); `sizeof(SaveData)==20` unchanged; bad-checksum rejected.
- **No-soft-lock invariant tests (2-wide player):** the 2 approach rooms are traversable with the carried kit (flood-fill); the arena's expose-opening is reachable each phase with the intended ability-combo; the magic crystal is reachable by base movement (no magic soft-lock); full-fight-restart leaves no stranded state (arena re-entry safe). Each invariant must FAIL on a deliberately-broken layout (non-vacuity gate), per the M8–M10 discipline.
- **Build gates:** `bash tools/host_test.sh` green (N/N), `python tools/check_logic_purity.py` clean, `bash tools/build_rom.sh` → `ROM fixed!`. Manual emulator QA — **balance pass**: expose-window length, P1/P2/P3 HP thresholds, attack timings/density, Light + element magic cost vs crystal spacing, full-fight-restart feel; verify the lives/Game-Over interaction, victory screen, persisted completion + title reflect, and that all 8 abilities are genuinely exercised across the phases.

## 10. Success criteria

- Door 9 opens when all 8 spronks are freed; the 2-room approach is completable with the carried kit and hands off to the arena.
- The King fight requires chaining traversal powers to open a window, **Light** to expose, and bolt/elements to wound — across 3 escalating phases; he cannot be cheesed by bolt-spam while shielded.
- Death restarts the whole fight at P1 (approach not repeated); lives/Game-Over (M9) integrate correctly; no magic soft-lock; invariants green + fail-on-broken.
- Defeating the King plays a victory screen, persists `beaten` (save v5, no `sizeof` change, all migrations covered), and the title reflects completion — the game is **winnable end-to-end**.
- `BossState` is pure logic, host-tested, and written to generalize (named phases/thresholds, data-described attacks) so M12 can extract a shared boss framework without a rewrite.
- All host tests green, purity clean, ROM builds; no soft-locks.
