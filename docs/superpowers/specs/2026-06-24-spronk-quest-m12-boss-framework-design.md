# Spronk Quest — Milestone 12 (Boss Framework + D1 Boss) Design

**Status:** Approved (brainstorming complete, 2026-06-24). Next: implementation plan.

**One-liner:** Extract a **data-described boss framework** from the M11 Nightmare King — a reusable pure-logic `BossDef`/`BossState` core plus an engine attack library + render/damage helpers — refactor the King onto it (host-tested, no behaviour change), and prove the abstraction with **one new per-dungeon boss**: a simple **Whispering Woods Guardian** (D1) fought in a dedicated boss room before the spronk. Later milestones add the remaining dungeon bosses.

This is the deferred M12 from the [[boss-fight-scope]] decision: M11 built the King concretely with a generalize-ready core; M12 does the **extraction** (a *move*, not a rewrite) and validates it at both ends of the range — the complex finale (King) and a simple early boss (D1).

---

## 1. Scope (decided)

- **Framework extraction** — generalize `include/logic/boss.h` into a data-described core (`BossDef` + generalized `BossState`); extract the King's scene-side combat primitives into a reusable engine attack library + helpers.
- **King refactor** — move the King onto the framework core (becomes a `KING_DEF`), keeping its idiosyncrasies (teleport, dialogue) local to its scene. Guarded by the existing host tests.
- **One new boss** — a per-dungeon boss for **D1 Whispering Woods** (`AlwaysVulnerable`, 2 phases), fought in a **dedicated boss room** before the spronk room.
- **Out of scope (deferred):** bosses for D2–D8 (later milestones); generalizing the King's teleport/dialogue into the framework; new save format; audio; final art.

## 2. Framework data model (pure logic — host-tested)

The King-specific `BossState` (fixed `P1/P2/P3` enum, global `KING_MAX_HP`/`PHASE_PATTERNS`) becomes **data-described**. Boss-specific numbers move into a `BossDef`; `BossState` holds runtime state + a pointer to its def.

```cpp
enum class VulnMode : uint8_t { SpellExpose, AlwaysVulnerable };

// PhasePattern (telegraph/active/recovery) is reused from the existing boss.h.
struct BossPhaseDef {
    int end_hp;            // this phase is active while hp > end_hp; the LAST phase has end_hp = 0
    PhasePattern pattern;  // telegraph/active/recovery frames (telegraph MUST be >= SWITCH_BUDGET)
    uint8_t attacks;       // bitmask selecting attacks from the engine attack library (see §3)
};

struct BossDef {
    int max_hp;
    int wound_dmg;
    int hit_iframes;
    int expose_frames;            // used only when vuln == SpellExpose
    VulnMode vuln;
    logic::SpellId expose_spell;  // used only when vuln == SpellExpose (King = Light)
    const BossPhaseDef* phases;   // variable length
    int phase_count;
};

struct BossState {
    const BossDef* def = nullptr;
    int hp = 0;
    int phase = 0;            // INDEX 0..phase_count-1 (no fixed enum -> any phase count)
    int phase_start_hp = 0;
    int expose_timer = 0;    // >0 while exposed (SpellExpose only)
    int attack_timer = 0;    // drives the active phase's PhasePattern
    int hit_iframes = 0;     // >0 = just wounded: immune; (SpellExpose) not re-exposable

    void reset(const BossDef& d);          // bind def + full reset to phase 0 / max_hp
    bool exposed() const;                  // expose_timer > 0
    bool defeated() const;                 // hp <= 0
    bool vulnerable() const;               // AlwaysVulnerable || exposed()
    void on_expose_hit(logic::SpellId s);  // SpellExpose: expose if s == def->expose_spell
    void on_wound(int dmg);                // lands iff vulnerable() && !defeated() && hit_iframes==0
    void tick();                           // decay timers; advance attack pattern (frozen while exposed)
    void on_player_death();                // full-fight restart (reset to phase 0)
    AttackStep current_step() const;       // Telegraph/Active/Recovery from the active phase pattern
    int active_phase() const;              // phase index used for rendering/attack selection
};
```

**Vulnerability model (the key axis):**
- **`SpellExpose`** (King): `on_expose_hit(Light)` sets `expose_timer = def->expose_frames` (the phase-overlay). `on_wound` lands only while `exposed()`; a wound ends the expose window + sets `hit_iframes` (one wound per Light — today's behaviour exactly). The attack pattern freezes while exposed.
- **`AlwaysVulnerable`** (D1): no expose gate — `on_wound` lands whenever `hit_iframes == 0`; the boss keeps attacking (no stun/freeze window). `expose_timer`/`expose_spell` unused. `hit_iframes` still applies (a short post-hit invuln so the boss can't be instakilled by rapid bolts).

**Phase machine:** unchanged logic, generalized to read `def->phases[]`: `tick` advances `attack_timer` over the active phase's `PhasePattern` period (frozen while exposed); `on_wound` crosses `end_hp` thresholds to advance `phase`; `phase_period`/`current_step` index `def->phases[active_phase()].pattern`. The `SWITCH_BUDGET` invariant (every telegraph + expose window >= SWITCH_BUDGET) is asserted **per-def** in tests.

**The King becomes `KING_DEF`** — a `BossDef{ max_hp=90, wound_dmg=10, hit_iframes=45, expose_frames=75, vuln=SpellExpose, expose_spell=Light, phases=KING_PHASES(3) }` reproducing the shipped numbers. **D1 becomes `D1_DEF`** — `{ max_hp≈60, wound_dmg=10, hit_iframes≈30, vuln=AlwaysVulnerable, phases=D1_PHASES(2) }`.

## 3. Engine attack library + render helpers (reusable)

The King's scene-side combat pieces (currently inline in `scene_boss.cpp`) extract into reusable **engine** modules so a new boss is mostly data:

- **Attack library** (`include/engine/boss_attacks.h` + `src/engine/boss_attacks.cpp`): the primitives lifted from the King — **aimed bolt**, **spiral (one arm toward the player)**, **fan**, plus the **telegraph charge-orb** — each a function that spawns into a shared attack pool given the boss + player positions and a speed. A phase's `attacks` bitmask selects which fire during its Active step.
- **Shared render/resolve helpers**: the **HP-pip bar**, the **telegraph cue**, the **attack/projectile pool**, the **block defense** (player bolt/Fire/Ice destroy boss projectiles; the expose spell is exempt), and the **damage resolution** (expose/wound/i-frames) — all driven by `BossState`.
- **Three-layer rule preserved:** all of this is `engine/` (Butano allowed); the decision logic stays in pure `logic/` `BossState`.

## 4. King refactor (proves reuse; the riskiest piece — host-tested)

`scene_boss.cpp` (`run_boss`) refactors to drive `KING_DEF` through the shared core + attack library + helpers. **The King's idiosyncrasies stay local to its scene:** teleport-between-perches and the intro/phase/death **dialogue** are NOT pushed into the framework (per the [[boss-fight-scope]] direction). The pure-logic tests in `test/test_boss.cpp` re-point to `KING_DEF`; they are the regression guard that the King's combat behaviour is byte-for-byte unchanged. Emulator QA confirms the King still plays identically.

## 5. D1 boss — the Whispering Woods Guardian

A simple first boss showcasing the framework at the **opposite end** from the King. Placeholder identity: a large forest guardian (e.g. a treant / forest beast); name + sprite are placeholder, drop-in-replaceable later.

- **Def:** `AlwaysVulnerable`, **2 phases**, `max_hp ≈ 60` (≈6 bolt hits), modest telegraphs (all >= SWITCH_BUDGET).
- **Kit available at D1:** the player has ONLY the **bolt + double-jump (Featherleap)** — so the fight is pure **dodge-and-shoot**: read the telegraph, dodge with movement/double-jump, fire bolts. No spell-expose, no blocking-spell required (the block helper is King-relevant; D1 relies on dodging).
- **Attacks (from the library, telegraphed):**
  - **Lunge / ground shockwave** → jump over it.
  - **Lobbed projectiles** → sidestep / dodge.
  - **Phase 2:** faster telegraphs and/or both attacks in rotation (escalation).
- **No expose gate:** the player just shoots it; the HP-pip bar shows progress; `hit_iframes` paces the damage so it can get attacks off between hits.

## 6. Integration + flow

- **Boss room:** `LevelData` gains an optional `const logic::BossDef* boss` field (null = ordinary room). D1's room graph gains a **dedicated boss room** placed **before** a spronk room: D1 becomes `entry content → boss room → spronk room`.
- **Driver:** when the player enters a boss room, `scene_dungeon` runs a **boss fight** for that room by composing the framework pieces (`BossState` + attack library + render/damage helpers); on **victory**, the room's onward door/exit opens → the player proceeds to the spronk room and frees the spronk as today.
- **Two thin consumers, one framework (Approach B):** there is NOT a single monolithic boss-scene function. The King's `run_boss` (dispatched from `main.cpp` Door 9) and the dungeon boss-room handling in `scene_dungeon` are two thin consumers that each compose the SAME shared framework (logic core + attack library + helpers). The King's consumer adds its local extras (teleport, dialogue); the D1 consumer is minimal. This is what keeps the King's idiosyncrasies out of the framework while sharing the heavy lifting.
- **Death/lives:** the M9 lives/Game-Over flow + full-fight restart apply (reuse the framework's lives handling). The boss room is escapable (a death costs a life and restarts the fight; lives==0 → Game Over).

## 7. Save / persistence

**No save-format change.** The boss is **not** persisted — re-entering the boss room **re-fights** the boss (mirrors the King's full-fight restart). Only the existing **spronk-freed latch** records dungeon progress (set when the spronk is freed *after* the boss). So a player who beats the boss but dies before the spronk re-fights the boss on re-entry — acceptable and consistent with the no-mid-fight-save model. SaveData v5 (from M11) is unchanged.

## 8. What's new vs reused (three-layer purity preserved)

**New (pure logic):** `VulnMode`, `BossDef`, `BossPhaseDef`, generalized `BossState` (def-driven, phase-as-index); `KING_DEF` + `D1_DEF` + their phase tables. **New (engine):** `boss_attacks` library + the shared HP-bar/telegraph/pool/block/damage helpers (extracted from the King's scene). **New (content):** the D1 boss room + spronk room + the Guardian placeholder sprite; `LevelData.boss` field + compiler support. **Refactored:** `scene_boss.cpp` (King → framework, idiosyncrasies local); `test_boss.cpp` (→ `KING_DEF` + new `D1_DEF` cases); `scene_dungeon.cpp` (boss-room handling). **Reused:** the existing phase-machine logic, `PhasePattern`/`SWITCH_BUDGET`, the M9 lives/Game-Over flow, the shot-aim (`read_aim_dy`) + pause, the room-to-room framework, the spronk-cage/exit flow, the no-soft-lock invariant harness. **Three-layer rule:** all boss state/decisions are pure + host-tested; only `bn::` rendering/scene work lives in engine/game.

## 9. Testing strategy

- **Pure-logic host tests (`test/test_boss.cpp`, extended):**
  - **King regression:** `KING_DEF`-driven `BossState` reproduces the shipped behaviour — expose-on-Light, one-wound-per-expose, hit-iframes, phase thresholds, defeat, full-fight restart, attack-step timing (the existing tests, re-pointed to `KING_DEF`).
  - **D1 / AlwaysVulnerable:** `on_wound` lands without any expose; `hit_iframes` caps the wound rate; phase advances by HP across **2** phases; no expose timer involved; defeat at hp<=0.
  - **Generalization:** variable `phase_count` (2 vs 3) drives `current_step`/`phase_period`/threshold-advance correctly; `reset(def)` binds + initializes.
  - **Invariant:** every def's telegraph windows + (SpellExpose) expose window are `>= SWITCH_BUDGET` — asserted per-def, fail-on-broken.
- **No-soft-lock invariants** (D1 level test, 2-wide flood-fill): the boss room + new spronk room reachable with the D1 kit (bolt + double-jump); the boss room escapable on death; the spronk reachable after the boss; fail-on-broken.
- **Build gates:** `bash tools/host_test.sh` green (N/N), `python tools/check_logic_purity.py` clean, `bash tools/build_rom.sh` → `ROM fixed!`.
- **Emulator QA:** the D1 boss is fair + winnable with only bolt + double-jump; the **King is unregressed** (plays identically post-refactor); the boss-room → spronk flow works; lives/Game-Over integrate.

## 10. Success criteria

- A data-described boss framework exists: `BossDef`/`BossState` (pure, host-tested) + an engine attack library + shared helpers; adding a boss is mostly **data** (a def + phase table + arena) plus a thin scene hook.
- The **King is refactored onto the framework** with **no behaviour change** (host tests + QA confirm).
- A **D1 boss** (`AlwaysVulnerable`, 2 phases) is fought in a dedicated boss room before the spronk, beatable with bolt + double-jump, fair + telegraphed.
- The framework spans both ends — spell-expose finale (King) and always-vulnerable early boss (D1) — proving the abstraction without rewriting the King.
- No save-format change; D1–D8 + the King + the hub unregressed; host tests green, purity clean, ROM builds; no soft-locks.
