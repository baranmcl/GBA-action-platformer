# M13 — D2 Ember Caverns Boss "Slagshell" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the D2 (Ember Caverns) boss — the pacing Magma Golem "Slagshell" (SpellExpose-with-Fire, a new jump→rockfall attack, long re-armor anti-spam) — on the M12 boss framework, restructuring D2 from 1 room into entry→boss→spronk.

**Architecture:** Reuse `VulnMode::SpellExpose` (expose spell = Fire) and the M12 `BossState`/`AttackPool`/`resolve_damage`/`play_room` boss-room handling unchanged. Add three data-described, reusable framework pieces — a `Locomotion` field (pacing), a `BOSS_ATK_ROCKFALL` attack, and a pure-logic `rockfall_columns` fairness picker — plus the engine glue (a 2nd rock `AttackPool`, a `RockfallEmitter` with ground telegraph, pacing + magic-crystal handling in `run_room_boss`). Anti-spam is purely a long `hit_iframes` in `D2_DEF` (no new code). D2 restructures into 3 rooms mirroring D1.

**Tech Stack:** C++17, Butano 21.6.0, GBA (devkitARM). Host tests via `bash tools/host_test.sh`. ROM via `bash tools/build_rom.sh`. Purity guard `python tools/check_logic_purity.py`. Three-layer architecture (`include/logic`+`src/logic` pure C++ NO `bn::`; `src/engine` Butano glue; `src/game` scenes).

## Global Constraints

- **Three-layer purity (HARD):** NO `bn::` types under `include/logic/` or `src/logic/`. `include/engine/boss_attacks.h` may include `logic/boss.h` but NOTHING from `game/`. The purity guard MUST stay green (`python tools/check_logic_purity.py` → `logic purity OK`).
- **King + D1 regression:** the framework additions MUST NOT change King or D1 behaviour. `test/test_boss.cpp`'s existing King + D1 assertions MUST stay UNCHANGED and green. The King uses `run_boss` (its own movement); D1 + D2 use `run_room_boss`.
- **No save-format change:** the boss is not persisted; re-entry re-fights (a `play_room` local). `SaveData` untouched.
- **GBA platform:** integer fixed-point only (no `float`/`double` in gameplay) — `docs/pitfalls/implementation-pitfalls.md` IMPL-2. Logic tests assert exact integers (testing-pitfalls TEST-2).
- **Green bars:** host tests end `N/N tests passed, 0 checks failed`; ROM ends `ROM fixed!`.

---

## Living Document Contract

This plan is a living document. Every executing agent MUST update it as
execution progresses, not only at completion.

- **On phase claim:** the executor MUST flip the banner to 🚧 IN PROGRESS
  with a claim timestamp (ISO 8601 UTC) and the active branch name. The
  banner MUST NOT include an expected-completion estimate — agents cannot
  reliably estimate their own wall-clock, and a fabricated duration
  becomes a stale anchor that misleads future readers. Followers
  encountering a 🚧 banner determine liveness by observable signals (PR
  existence, recent branch commits), not by arithmetic on expected times.
  See Step 5's stale-claim reclaim protocol.
- **On phase ship:** the executor MUST update that phase's **Execution
  Status** banner with the shipped commit SHA(s) and date. If a PR is
  open, the PR number and URL MUST appear in the top-of-plan Execution
  Status table.
- **On phase defer:** the executor MUST update the banner with ⏸ status
  AND a prose description of the unblock condition + a link to the
  likely-unblocker artifact (plan page, task, or PR whose own Execution
  Status banner will signal completion). Prose + link is durable across
  paraphrases and scope edits; exact-string coordination between agents
  is not.
- **On PR merge:** the executor MUST record the merge SHA in the banner
  + the top-of-plan Execution Status table.
- **On deviation from the written plan** (scope edits, structural
  refactors, dropped tasks, reordered phases): the executor MUST
  inline-document the deviation in the affected task AND summarize it
  in the top-of-plan Execution Status as a "Deviations" subsection.
  Deviation state MUST NOT live only in PR notes or status reports.
- **On discovery** (pre-existing drift surfaced during execution, new
  bugs found, architectural issues noted): the executor MUST add a
  "Discoveries" subsection at the top of the plan with pointers to the
  files/lines affected. Follow-up dispatches read this subsection to
  avoid duplicate discovery work.

The plan SHOULD reflect reality at the end of every session that touches
it. Anything worth putting in a status report to the user is worth
putting in the plan.

Rationale: `/writing-plans-enhanced` Step 5. Writing at ship time is
cheap; reconstruction by downstream readers is expensive, compounds
across dispatches, and fails silently when state is split across PR
notes and commit messages.

---

## Per-Task Protocol (applies to EVERY task)

**BEFORE starting any task:**
1. Invoke `superpowers:test-driven-development`.
2. Read `docs/pitfalls/testing-pitfalls.md` and `docs/pitfalls/implementation-pitfalls.md`.
3. Follow TDD: write the failing test → run it red → implement minimally → run it green → commit.

**Host-testable tasks** (anything touching `include/logic/`, `src/logic/`, `test/`) are TDD with `bash tools/host_test.sh`. **ROM-only tasks** (engine `bn::` glue, scenes, art) cannot be host-tested — their "test" is `bash tools/build_rom.sh` compiling clean (`ROM fixed!`) plus the named emulator-QA checks handed to the user in Phase 4; for these, write/extend any *host-testable* logic (e.g. `rockfall_columns`) FIRST in its own logic task so the behaviour is unit-covered before the glue consumes it.

**BEFORE marking any task complete:**
1. Re-read the tests against `docs/pitfalls/testing-pitfalls.md`; confirm error/edge paths are covered.
2. Run `bash tools/host_test.sh` (green) AND, for `logic/` changes, `python tools/check_logic_purity.py` (`logic purity OK`).
3. If a test races/flakes/fails nondeterministically, the fix is deterministic synchronization or a deterministic seed — **NOT** assertion removal/weakening. If you cannot make it pass deterministically, STOP and raise to the dispatcher. Commit subjects touching assertions say "add"/"strengthen"/"preserve" (never a vague "test fix").

**After completing each Phase (group of tasks):** review the batch from multiple perspectives — minimum 3 review rounds (purity, King/D1 regression, the spec's success criteria). If round 3 still finds issues, keep going until clean.

---

## Execution Status

**Overall:** Not started.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure-logic framework (Locomotion + ROCKFALL + D2_DEF + rockfall_columns) | ⬜ Not started | — | host-tested |
| 2 — Art + engine rockfall emitter | ⬜ Not started | — | ROM-built |
| 3 — `run_room_boss` integration (sprite, pacing, crystal, rockfall) | ⬜ Not started | — | ROM-built |
| 4 — D2 level restructure + invariants + QA | ⬜ Not started | — | host + emulator QA |

---

## File Structure

**Created:**
- `include/logic/rockfall.h` — pure `rockfall_columns` fairness picker.
- `test/test_rockfall.cpp` — host tests for the picker.
- `test/test_dungeon2_level.cpp` — D2 no-soft-lock + structural invariants.
- `tools/levels/dungeon2_room0.txt` / `_room1.txt` / `_room2.txt` (+ `.json` each) — the 3 D2 rooms.
- `include/game/levels/dungeon2_room0.h` / `_room1.h` / `_room2.h` — generated by the level compiler.

**Modified:**
- `include/logic/boss.h` — `Locomotion` enum, `BossDef::locomotion`, `BOSS_ATK_ROCKFALL`, `D2_*`.
- `test/test_boss.cpp` — D2_DEF regression + SpellExpose-anti-spam invariant + King/D1 locomotion regression.
- `include/engine/boss_attacks.h` + `src/engine/boss_attacks.cpp` — `RockfallEmitter`.
- `src/game/scene_dungeon.cpp` — `run_room_boss`: boss-sprite selector, pacing, magic crystal, rockfall.
- `tools/make_placeholder_art.py` — `gen_slagshell`, `gen_rock`, `gen_rock_marker`.
- `tools/build_level.py` — `BOSS_SYMBOL['d2']`.
- `include/game/levels/dungeons.h` — `DUNGEON2_*` 3-room.
- `tools/levels/dungeon2.txt` / `dungeon2.json` — renamed/removed (become `dungeon2_room0.*`).

---

## Phase 1 — Pure-logic framework (Locomotion + ROCKFALL + D2_DEF + rockfall_columns)

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** the data + the fairness picker are the host-testable heart. Getting `BossDef`'s new field placement right (at the END, defaulted) is what keeps King/D1 byte-for-byte identical.

### Task 1.1: Boss data — Locomotion, BOSS_ATK_ROCKFALL, D2_DEF

**Files:**
- Modify: `include/logic/boss.h`
- Test: `test/test_boss.cpp`

**Interfaces:**
- Produces: `logic::Locomotion` enum; `logic::BossDef::locomotion` (defaulted `Stationary`); `logic::BOSS_ATK_ROCKFALL` (`1 << 3`); `logic::D2_DEF`, `logic::D2_PHASES`, `logic::D2_ATTACKS_P1`/`_P2`. Consumed by Phase 3 (`run_room_boss`) and Phase 2 (engine reads the attack bit).

Follow the Per-Task Protocol.

- [ ] **Step 1: Write the failing tests** in `test/test_boss.cpp` (append; do NOT touch existing King/D1 tests):

```cpp
// --- M13 D2 Slagshell (SpellExpose+Fire, pacing, rockfall, long re-armor anti-spam) ---
TEST(bossdef_d2_slagshell_fields){
    CHECK_EQ(D2_DEF.max_hp, 70);
    CHECK_EQ(D2_DEF.wound_dmg, 10);
    CHECK_EQ(D2_DEF.phase_count, 2);
    CHECK((int)D2_DEF.vuln == (int)VulnMode::SpellExpose);
    CHECK((int)D2_DEF.expose_spell == (int)SpellId::Fire);     // Fire melts the crust
    CHECK((int)D2_DEF.locomotion == (int)Locomotion::Pacing);  // the moving boss
    CHECK(D2_DEF.hit_iframes >= 80);                           // LONG re-armor = anti-spam
    CHECK_EQ(D2_DEF.phases[0].end_hp, 35);                     // P1 -> P2 at half HP
    // both phases use AIMED|ROCKFALL; the phase boundary only escalates rockfall intensity
    CHECK_EQ((int)D2_DEF.phases[0].attacks, (int)(BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL));
    CHECK_EQ((int)D2_DEF.phases[1].attacks, (int)(BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL));
    CHECK(D2_DEF.phases[0].pattern.telegraph_frames >= SWITCH_BUDGET);
    CHECK(D2_DEF.phases[1].pattern.telegraph_frames >= SWITCH_BUDGET);
}
// THE anti-spam invariant: after a wound you CANNOT re-expose until hit_iframes drains, and the
// attack pattern resumes (not frozen) during that window. This is what stops Fire->bolt mashing.
TEST(d2_no_reexpose_during_rearm){
    BossState b; b.reset(D2_DEF);
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());        // Fire opens the window
    b.on_wound(D2_DEF.wound_dmg);                               // one wound...
    CHECK_EQ(b.hit_iframes, D2_DEF.hit_iframes);               // ...starts the long re-armor
    CHECK(!b.exposed());                                        // window closed by the wound
    b.on_expose_hit(SpellId::Fire);  CHECK(!b.exposed());       // Fire during re-armor: NO re-expose
    int t = b.attack_timer;
    b.tick();                                                   // pattern RESUMES during re-armor...
    CHECK(b.attack_timer == t + 1 || b.attack_timer == 0);     // (advances, or wraps the period) -> not frozen
    for(int i = 0; i < D2_DEF.hit_iframes; ++i) b.tick();      // drain the re-armor
    CHECK_EQ(b.hit_iframes, 0);
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());        // NOW Fire re-exposes
}
TEST(d2_takes_seven_wounds){
    BossState b; b.reset(D2_DEF);
    int wounds = 0;
    while(!b.defeated() && wounds < 100){
        b.on_expose_hit(SpellId::Fire);
        b.on_wound(D2_DEF.wound_dmg);
        for(int i = 0; i < D2_DEF.hit_iframes; ++i) b.tick();   // wait out re-armor between wounds
        ++wounds;
    }
    CHECK(b.defeated());
    CHECK_EQ(wounds, 7);                                        // 70 / 10
}
// Bolt/None never expose a SpellExpose boss (only the def's expose spell does).
TEST(d2_only_fire_exposes){
    BossState b; b.reset(D2_DEF);
    b.on_expose_hit(SpellId::Ice);   CHECK(!b.exposed());
    b.on_expose_hit(SpellId::Light); CHECK(!b.exposed());
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());
}
// REGRESSION: the new locomotion field must default Stationary for King + D1 (unchanged behaviour).
TEST(king_and_d1_are_stationary){
    CHECK((int)KING_DEF.locomotion == (int)Locomotion::Stationary);
    CHECK((int)D1_DEF.locomotion == (int)Locomotion::Stationary);
}
```

- [ ] **Step 2: Run to verify failure**

Run: `bash tools/host_test.sh`
Expected: FAIL — `D2_DEF` / `Locomotion` / `BOSS_ATK_ROCKFALL` undefined (compile error in `test_boss.cpp`).

- [ ] **Step 3: Implement in `include/logic/boss.h`**

3a. Add the `Locomotion` enum immediately AFTER the `VulnMode` enum (after line ~23):

```cpp
// --- Locomotion model. Stationary: the boss holds its placed position (D1; and the King, which does
//     its own teleport in run_boss). Pacing: a room boss walks the arena floor, reversing at the
//     walls (D2 Slagshell) — run_room_boss reads this. Movement pauses while EXPOSED (clean window). ---
enum class Locomotion : uint8_t { Stationary, Pacing };
```

3b. Add the rockfall attack bit AFTER `BOSS_ATK_FAN` (line ~30):

```cpp
inline constexpr uint8_t BOSS_ATK_ROCKFALL = 1 << 3;  // M13: jump -> rocks fall from the ceiling
```

3c. Add `locomotion` as the LAST field of `BossDef`, defaulted (so King's 8-field and D1's 11-field aggregate inits are unchanged — they keep `Stationary`). After the `death_line` field:

```cpp
    const char* death_line = nullptr;   // optional on-defeat dialogue (null = none)
    Locomotion  locomotion = Locomotion::Stationary;   // M13: Pacing = walks the floor (run_room_boss)
```

3d. Add the D2 attack masks, phase table, and def AFTER `D1_DEF` (after line ~76). Note `tired_after = 0` (SpellExpose ignores it) and `locomotion = Pacing` is field #12:

```cpp
// --- D2 Ember Caverns boss: the Magma Golem "Slagshell" (SpellExpose+Fire, Pacing, 2 phases).
//     Fire melts the crust -> exposed window -> bolt/Fire wounds. A wound triggers a LONG re-armor
//     (hit_iframes) during which Fire cannot re-expose and the boss keeps attacking -> the player must
//     DODGE to earn the next opening (anti-spam). Both phases run AIMED + ROCKFALL; the phase boundary
//     escalates rockfall intensity (run_room_boss reads b.phase for the rock count). ---
inline constexpr uint8_t D2_ATTACKS_P1 = BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL;
inline constexpr uint8_t D2_ATTACKS_P2 = BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL;
inline constexpr BossPhaseDef D2_PHASES[2] = {
    { 35, { 80, 30, 40 }, D2_ATTACKS_P1 },   // P1 70->35  (telegraph 80, active 30, recovery 40)
    {  0, { 70, 30, 30 }, D2_ATTACKS_P2 },   // P2 35->0   (escalated rockfall)
    // NOTE: attack_active_frames (30) MUST exceed RockfallEmitter::WARN_FRAMES (26) so the rock drop
    // lands inside the Active window (the emitter is ticked only during AttackStep::Active). Keep >= 28.
};
inline constexpr BossDef D2_DEF{
    70, 10, 90, 90, VulnMode::SpellExpose, SpellId::Fire, D2_PHASES, 2,
    /*tired_after=*/0,
    /*intro_line=*/"You'll cook in my caverns.",
    /*death_line=*/"The embers...fade...",
    /*locomotion=*/Locomotion::Pacing
};
```

- [ ] **Step 4: Run to verify green**

Run: `bash tools/host_test.sh` → expect `N/N tests passed, 0 checks failed` (new D2 tests pass; ALL existing King/D1 tests still pass).
Run: `python tools/check_logic_purity.py` → `logic purity OK`.

- [ ] **Step 5: Commit**

```bash
git add include/logic/boss.h test/test_boss.cpp
git commit -m "feat(logic): D2 Slagshell BossDef (SpellExpose+Fire, Pacing, ROCKFALL, long re-armor) + Locomotion field; King/D1 unchanged (M13 P1.1)"
```

### Task 1.2: `rockfall_columns` fairness picker (pure logic)

**Files:**
- Create: `include/logic/rockfall.h`
- Create: `test/test_rockfall.cpp`

**Interfaces:**
- Produces: `int logic::rockfall_columns(int player_tx, int arena_w_tiles, int count, int seed, int* out, int max_out)`. Consumed by Phase 2's `RockfallEmitter`.

**Context:** the rockfall drops rocks at chosen tile columns. Fairness = it must NEVER cover every interior column; a dodge lane (≥ `ROCKFALL_GAP_MIN` contiguous rock-free interior columns) MUST always exist, and one rock biases toward the player so standing still is punished. Pure + deterministic (no RNG — `Date`/`rand` unavailable and tests need determinism): `seed` (the scene passes a rolling rockfall counter) shifts the spread.

Follow the Per-Task Protocol.

- [ ] **Step 1: Write the failing test** `test/test_rockfall.cpp`:

```cpp
#include "test_framework.h"
#include "logic/rockfall.h"
using namespace logic;

static bool has_gap(const int* cols, int n, int w, int gap_min){
    // a rock-free run of >= gap_min interior columns [1, w-2] must exist
    int run = 0;
    for(int x = 1; x <= w - 2; ++x){
        bool rock = false; for(int i = 0; i < n; ++i) if(cols[i] == x) rock = true;
        run = rock ? 0 : run + 1;
        if(run >= gap_min) return true;
    }
    return false;
}

TEST(rockfall_count_bounds_distinct_sorted){
    int out[8];
    int n = rockfall_columns(/*player_tx=*/20, /*arena_w=*/48, /*count=*/5, /*seed=*/0, out, 8);
    CHECK_EQ(n, 5);
    for(int i = 0; i < n; ++i){ CHECK(out[i] >= 1); CHECK(out[i] <= 46); }   // inside the walls
    for(int i = 1; i < n; ++i){ CHECK(out[i] > out[i-1]); }                  // sorted, distinct
}
TEST(rockfall_always_leaves_a_dodge_lane){
    int out[8];
    for(int seed = 0; seed < 8; ++seed){
        for(int ptx = 1; ptx <= 46; ++ptx){
            int n = rockfall_columns(ptx, 48, 5, seed, out, 8);
            CHECK(has_gap(out, n, 48, ROCKFALL_GAP_MIN));   // never walls off the arena
        }
    }
}
TEST(rockfall_biases_one_rock_to_player){
    int out[8];
    int n = rockfall_columns(/*player_tx=*/12, 48, 3, /*seed=*/2, out, 8);
    bool near = false; for(int i = 0; i < n; ++i) if(out[i] >= 10 && out[i] <= 14) near = true;
    CHECK(near);   // a rock falls at/near the player's column
}
TEST(rockfall_seed_varies_layout){
    int a[8], bb[8];
    int na = rockfall_columns(20, 48, 5, 0, a, 8);
    int nb = rockfall_columns(20, 48, 5, 1, bb, 8);
    CHECK_EQ(na, nb);
    bool differ = false; for(int i = 0; i < na; ++i) if(a[i] != bb[i]) differ = true;
    CHECK(differ);   // different seed -> different spread (not a static pattern)
}
TEST(rockfall_clamps_count_to_max_out){
    int out[3];
    int n = rockfall_columns(20, 48, 5, 0, out, 3);
    CHECK_EQ(n, 3);   // never writes past max_out
}
```

- [ ] **Step 2: Run to verify failure**

Run: `bash tools/host_test.sh` → FAIL (`logic/rockfall.h` missing).

- [ ] **Step 3: Implement** `include/logic/rockfall.h`:

```cpp
#pragma once
namespace logic {
// Minimum rock-free contiguous interior columns the picker guarantees (a dodge lane).
inline constexpr int ROCKFALL_GAP_MIN = 4;

// Pick up to `count` distinct rock columns (tile x) in the interior [1, arena_w_tiles-2], written
// ASCENDING to out[0..n). One column biases to the player's column (clamped interior); the rest are
// spread by an even stride offset by `seed` so repeats vary. Deterministic, no RNG. GUARANTEES a
// rock-free run of >= ROCKFALL_GAP_MIN interior columns (never walls off the arena). Returns n =
// min(count, max_out, interior width). Caller passes seed = a rolling rockfall counter for variety.
inline int rockfall_columns(int player_tx, int arena_w_tiles, int count, int seed,
                            int* out, int max_out){
    const int lo = 1, hi = arena_w_tiles - 2;     // interior column range (inside the walls)
    const int interior = hi - lo + 1;
    if(interior <= 0 || count <= 0 || max_out <= 0) return 0;
    if(count > max_out) count = max_out;
    // Cap so a dodge lane always survives: never fill the interior past (width - GAP_MIN) columns.
    int cap = interior - ROCKFALL_GAP_MIN;
    if(cap < 1) cap = 1;
    if(count > cap) count = cap;

    int tmp[16]; int n = 0;
    auto add = [&](int x){
        if(x < lo) x = lo; if(x > hi) x = hi;
        for(int i = 0; i < n; ++i) if(tmp[i] == x) return;   // distinct
        if(n < 16) tmp[n++] = x;
    };
    // 1) the player-biased column.
    add(player_tx);
    // 2) spread the rest on an even stride, offset by seed, skipping collisions. The stride leaves
    //    big gaps for few rocks in a wide arena; the cap above is the hard fairness guarantee.
    int stride = interior / (count + 1); if(stride < 1) stride = 1;
    for(int k = 1; n < count && k <= interior * 2; ++k){
        int x = lo + ((k * stride + seed * 3) % interior);
        add(x);
    }
    // insertion sort ascending into out (n is tiny).
    for(int i = 0; i < n; ++i){
        int v = tmp[i], j = i - 1;
        while(j >= 0 && out[j] > v){ out[j+1] = out[j]; --j; }
        out[j+1] = v;
    }
    return n;
}
}
```

- [ ] **Step 4: Run to verify green**

Run: `bash tools/host_test.sh` → all green. Run: `python tools/check_logic_purity.py` → `logic purity OK`.

The picker is fully deterministic (no RNG), so tests either pass or fail consistently — never flaky. If `rockfall_seed_varies_layout` is RED because the two chosen seeds happen to produce identical sorted column sets at those exact inputs, the fix is to pick different inputs/seeds that demonstrably differ (a legitimate test-input choice) — **NOT** to delete or weaken the "different seed → different spread" assertion.

- [ ] **Step 5: Commit**

```bash
git add include/logic/rockfall.h test/test_rockfall.cpp
git commit -m "feat(logic): rockfall_columns fairness picker (player-biased, always leaves a dodge lane, deterministic) (M13 P1.2)"
```

**After Phase 1:** run the 3-round group review (purity; King/D1 regression green; D2_DEF + picker match spec §2). Update this phase's banner to ✅ with the commit SHAs.

---

## Phase 2 — Art + engine rockfall emitter

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** the rock/marker/slagshell sprites and the `RockfallEmitter` are what Phase 3 wires in. ROM-built (these touch `bn::`); the fairness logic they call is already host-covered (Task 1.2).

### Task 2.1: Placeholder art — Slagshell, rock, rock-marker

**Files:**
- Modify: `tools/make_placeholder_art.py`

**Context:** art is generated by Python into `graphics/*.bmp` + `graphics/*.json`; the build compiles them into `bn::sprite_items::<name>`. `gen_guardian` (the D1 2-frame boss) is the template for `gen_slagshell`; `gen_boss_bolt` (8×8) is the template for `gen_rock`/`gen_rock_marker`. Each generator is registered in the script's generator list and called when the script runs; the ROM build runs it.

Follow the Per-Task Protocol. (Art has no host test; the "test" is the sprite header appearing + the ROM building in Phase 3. Verify each new generator runs without error and writes a `.json`.)

- [ ] **Step 1: Add `gen_slagshell`** (2 stacked 32×32 frames: frame 0 dark cooled-lava crust = armored; frame 1 cracked, glowing molten = exposed). Model it on `gen_guardian`'s structure (a `draw_slagshell_frame(im, oy, exposed)` helper + a `gen_slagshell` that draws frame 0 normal, frame 1 exposed, then `write(im, "slagshell", {"type":"sprite","height":32})`). Use the shared 16-colour palette: dark rock (pal 11/1), crust grey (pal 2), molten orange/red (pal 13) + gold-hot (pal 6) for the exposed frame's glowing cracks, white glints (pal 15). Exposed frame = brighter cracks + glowing core so the player reads the wound window.

- [ ] **Step 2: Add `gen_rock`** (a single 8×8 chunky grey/brown boulder; the rock pool scales it 2×, so 8×8 → 16×16 on screen — matches `gen_boss_bolt`'s size convention):

```python
def gen_rock():
    # M13: a falling rock for Slagshell's rockfall. 8x8 (the rock AttackPool scales 2x -> 16x16).
    # Grey/brown boulder, distinct from the red boss_bolt so "falling debris" reads clearly.
    im = new_img(8, 8)
    rect(im, 1, 2, 6, 7, 2)                  # grey body
    rect(im, 2, 1, 5, 2, 10)                 # brown top
    px(im, 1, 2, 1); px(im, 6, 7, 1); px(im, 0, 4, 1); px(im, 7, 5, 1)  # dark edges
    px(im, 3, 3, 15); px(im, 5, 5, 11)       # highlight + crack
    write(im, "rock", {"type": "sprite"})
```

- [ ] **Step 3: Add `gen_rock_marker`** (an 8×8 ground telegraph — a red crack/shadow shown where a rock will land):

```python
def gen_rock_marker():
    # M13: ground telegraph for the rockfall — a red crack/shadow at the column a rock is about to
    # hit, shown during the warn delay so the drop is fair/dodgeable.
    im = new_img(8, 8)
    rect(im, 0, 6, 7, 7, 1)                  # dark shadow base
    px(im, 1, 5, 13); px(im, 3, 4, 13); px(im, 5, 5, 13); px(im, 6, 6, 13)  # red crack zig-zag
    px(im, 2, 6, 6); px(im, 4, 6, 6)         # gold-hot flecks
    write(im, "rock_marker", {"type": "sprite"})
```

- [ ] **Step 4: Register the three generators.** In `tools/make_placeholder_art.py`'s `if __name__ == "__main__":` block (~lines 571-588, the sequence of `gen_*()` calls), add `gen_slagshell()`, `gen_rock()`, `gen_rock_marker()` after the `gen_guardian()` call.

- [ ] **Step 5: Run the art generator EXPLICITLY.** It is **NOT** run by `tools/build_rom.sh` or `tools/host_test.sh` (those only run `build_level.py`). Run it directly:

Run: `python tools/make_placeholder_art.py`
Expected: it completes without error and writes `graphics/slagshell.json` + `graphics/slagshell.bmp`, `graphics/rock.json` + `.bmp`, `graphics/rock_marker.json` + `.bmp` (Butano compiles these into `bn::sprite_items::slagshell` / `::rock` / `::rock_marker` on the next ROM build). Then `bash tools/build_rom.sh` → `ROM fixed!` to confirm the sprites compile.

- [ ] **Step 6: Commit** (commit the generated art outputs — they're checked in, like the existing `graphics/*.json`):

```bash
git add tools/make_placeholder_art.py graphics/slagshell.* graphics/rock.* graphics/rock_marker.*
git commit -m "art: Slagshell (2-frame crust/molten) + falling rock + rock ground-marker (M13 P2.1)"
```

### Task 2.2: `RockfallEmitter` (engine)

**Files:**
- Modify: `include/engine/boss_attacks.h`, `src/engine/boss_attacks.cpp`

**Interfaces:**
- Consumes: `logic::rockfall_columns` (Task 1.2); `engine::AttackPool` (existing — the rock pool). 
- Produces: `engine::RockfallEmitter` with `begin(player_tx, arena_w_tiles, arena_h_tiles, count, seed)`, `tick(AttackPool& rocks)`, `clear()`. Consumed by Phase 3.

**Context:** mirrors `SpiralEmitter` (a small stateful emitter ticked during an Active window) but it (a) picks columns via the pure picker, (b) shows ground markers during a warn delay, (c) on warn-elapse launches rocks from the ceiling into the passed rock `AttackPool` (which already despawns on solid + applies contact damage). Markers are `bn::sprite_ptr` (engine). Screen mapping mirrors `AttackPool` (a world px maps to screen via `px - map_px/2`).

Follow the Per-Task Protocol. (Engine `bn::` code — not host-tested; the column logic is already host-covered. Verify by ROM build in Phase 3.)

- [ ] **Step 1: Declare `RockfallEmitter` in `include/engine/boss_attacks.h`** (after `SpiralEmitter`, ~line 134):

```cpp
// -----------------------------------------------------------------------------
// RockfallEmitter — Slagshell's jump-rockfall. begin() picks fair columns (logic::rockfall_columns)
// and shows a ground crack-marker at each for a warn delay; tick() drops rocks from the ceiling into
// the supplied rock AttackPool when the warn elapses (one drop per window). Markers are screen-fixed
// to the camera like the bolt VFX. clear() hides markers + resets (call on fight restart).
// -----------------------------------------------------------------------------
class RockfallEmitter {
public:
    static constexpr int MAXCOLS = 6;     // P2 uses up to 5; one slot of margin
    static constexpr int WARN_FRAMES = 26;// telegraph before rocks drop. INVARIANT: must be < the
                                          // rockfall phase's attack_active_frames (D2 = 30) so the
                                          // drop lands inside the Active window (tick runs only in Active).
    static constexpr int ROCK_VY = 3;     // downward px/frame

    RockfallEmitter(const bn::sprite_item& marker_item, const bn::camera_ptr& cam,
                    int map_px_w, int map_px_h);

    // Pick columns + show markers + start the warn timer. count = rocks this window (3 P1 / 5 P2);
    // seed = a rolling counter for variety. arena_*_tiles = level.w / level.h.
    void begin(int player_tx, int arena_w_tiles, int arena_h_tiles, int count, int seed);
    // Advance the warn timer; when it elapses, launch rocks from the ceiling into `rocks` (once) and
    // hide the markers. Call each frame while the rockfall attack is the live Active attack.
    void tick(AttackPool& rocks);
    void clear();
    // Cosmetic: the boss's "leap" height this frame (a triangle arc peaking mid-warn), 0 when
    // grounded/idle. run_room_boss subtracts it from the boss sprite's y so the rockfall reads as a
    // JUMP (the user-requested move). 0 outside a rockfall warn -> applying it always is a no-op then.
    int leap_offset() const;

private:
    bn::vector<bn::sprite_ptr, MAXCOLS> _markers;
    int _cols[MAXCOLS];
    int _col_count = 0;
    int _warn = 0;
    bool _dropped = false;
    int _arena_h_tiles = 0;
    int _half_w_px, _half_h_px;
};
```

Add `#include "logic/rockfall.h"` near the top of `boss_attacks.h` (it already includes `logic/boss.h`).

- [ ] **Step 2: Implement in `src/engine/boss_attacks.cpp`** (after `SpiralEmitter::tick`):

```cpp
// ---------------------------------------------------------------------------
// RockfallEmitter
// ---------------------------------------------------------------------------
RockfallEmitter::RockfallEmitter(const bn::sprite_item& marker_item, const bn::camera_ptr& cam,
                                 int map_px_w, int map_px_h)
    : _half_w_px(map_px_w / 2), _half_h_px(map_px_h / 2)
{
    for(int i = 0; i < MAXCOLS; ++i){
        _markers.push_back(marker_item.create_sprite(0, 0));
        _markers[i].set_camera(cam);
        _markers[i].set_visible(false);
    }
}

void RockfallEmitter::begin(int player_tx, int arena_w_tiles, int arena_h_tiles, int count, int seed){
    _arena_h_tiles = arena_h_tiles;
    _dropped = false;
    _warn = WARN_FRAMES;
    if(count > MAXCOLS) count = MAXCOLS;
    _col_count = logic::rockfall_columns(player_tx, arena_w_tiles, count, seed, _cols, MAXCOLS);
    // show a ground marker at each target column, resting on the floor surface (row h-2).
    const int floor_y = (arena_h_tiles - 2) * 8;
    for(int i = 0; i < MAXCOLS; ++i){
        bool on = i < _col_count;
        if(on) _markers[i].set_position(_cols[i] * 8 + 4 - _half_w_px, floor_y - _half_h_px);
        _markers[i].set_visible(on);
    }
}

void RockfallEmitter::tick(AttackPool& rocks){
    if(_dropped) return;
    if(--_warn > 0) return;
    // warn elapsed: drop one rock per column from the ceiling (row 1), hide the markers.
    for(int i = 0; i < _col_count; ++i)
        rocks.launch(i, _cols[i] * 8 + 4, 8 + 6, 0, ROCK_VY);   // ceiling, straight down
    for(int i = 0; i < MAXCOLS; ++i) _markers[i].set_visible(false);
    _dropped = true;
}

void RockfallEmitter::clear(){
    for(int i = 0; i < MAXCOLS; ++i) _markers[i].set_visible(false);
    _col_count = 0; _warn = 0; _dropped = false;
}

int RockfallEmitter::leap_offset() const {
    if(_dropped || _warn <= 0) return 0;
    int t = WARN_FRAMES - _warn;                 // frames since the leap began (0..WARN_FRAMES)
    int peak = WARN_FRAMES / 2;
    int d = (t < peak) ? t : (WARN_FRAMES - t);  // triangle: rise to the peak, fall back down
    return d;                                     // ~0..13 px (cosmetic)
}
```

- [ ] **Step 3: Verify it builds** (ROM): `bash tools/build_rom.sh` → `ROM fixed!` (the emitter compiles; it's exercised once Phase 3 wires it). Also `python tools/check_logic_purity.py` → `logic purity OK` (engine including logic is fine; ensure no `game/` include crept in).

- [ ] **Step 4: Commit**

```bash
git add include/engine/boss_attacks.h src/engine/boss_attacks.cpp
git commit -m "feat(engine): RockfallEmitter — fair-column ground telegraph + ceiling rock drop into a rock AttackPool (M13 P2.2)"
```

**After Phase 2:** 3-round review (purity; engine has no `game/` include; matches spec §2.5). Banner → ✅.

---

## Phase 3 — `run_room_boss` integration (sprite selector, pacing, magic crystal, rockfall)

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** this is where Slagshell comes alive. All ROM-built; behaviour confirmed in Phase 4 QA.

**CRITICAL — these four tasks all edit the SAME function (`run_room_boss` in `src/game/scene_dungeon.cpp`):** do them strictly in order, committing between. Every line number in this phase is **APPROXIMATE and drifts** as each task inserts code — **locate edit points by the quoted anchor code, NOT by line number.** `run_room_boss` already has these locals you'll reference: `wx`/`wy` (screen-map lambdas), `boss_cx()`/`boss_cy()` (boss centre), `boss_body`, `attacks` (the bolt `AttackPool`), `spiral`, `telegraph`, `next_attack_for_phase`, `atk_spawned_this_active`, `current_attack`, `restart_fight` (lambda), `cam`, `invuln`, `spell`/`magic`/`health`, and the file-scope helper `tile_body(tx,ty,hw,hh)`.

### Task 3.1: Per-boss sprite selector (Slagshell vs Guardian)

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

**Context:** `run_room_boss` hardcodes `bn::sprite_items::guardian` for the boss sprite + the frame swap (~lines 194, 322). D2 needs `slagshell`. Add a small game-layer selector (pointer-compare against the def symbols — `BossDef` is pure logic and can't name `bn::` items, so the mapping lives here).

Follow the Per-Task Protocol. (ROM-built; QA in Phase 4.)

- [ ] **Step 1: Add the selector** above `run_room_boss` (file-scope helper). Add `#include "bn_sprite_items_slagshell.h"` with the other sprite includes:

```cpp
// Map a boss def to its 2-frame sprite (frame 0 = normal/armored, frame 1 = exposed/vulnerable).
// Pointer-compare against the canonical def symbols (BossDef is pure logic -> can't name bn:: items).
// Extend per new per-dungeon boss.
static const bn::sprite_item& boss_sprite_for(const logic::BossDef* def){
    if(def == &logic::D2_DEF) return bn::sprite_items::slagshell;
    return bn::sprite_items::guardian;   // D1 (default)
}
```

- [ ] **Step 2: Use it** in `run_room_boss`. Replace the boss-sprite creation (`bn::sprite_items::guardian.create_sprite(...)`, ~line 194) with a captured reference:

```cpp
const bn::sprite_item& boss_item = boss_sprite_for(level.boss);
bn::sprite_ptr boss_spr = boss_item.create_sprite(0, 0);
```

and the frame-swap line (~line 322) `bn::sprite_items::guardian.tiles_item().create_tiles(want_frame)` → `boss_item.tiles_item().create_tiles(want_frame)`.

- [ ] **Step 3: Build** `bash tools/build_rom.sh` → `ROM fixed!` (D1 still uses guardian; D2 will use slagshell).

- [ ] **Step 4: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): per-boss sprite selector in run_room_boss (Slagshell for D2, Guardian for D1) (M13 P3.1)"
```

### Task 3.2: Pacing movement (data-gated)

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

**Context:** a `Locomotion::Pacing` boss walks the floor, reversing at the interior walls; movement PAUSES while exposed (the wound window stays clean). D1 (`Stationary`) is untouched. `boss_body` is placed by `place_boss()` (~line 190); add pacing state after it and the movement step in the main loop after `b.tick()`.

Follow the Per-Task Protocol.

- [ ] **Step 1: Add pacing state** after the `boss_cx`/`boss_cy` lambdas (~line 192):

```cpp
// M13 pacing: a Pacing boss walks the floor between the interior walls; Stationary bosses never move.
int pace_dir = 1;                                            // +1 right, -1 left
const int PACE_SPEED = 1;                                    // px/frame (slow; tunable in QA)
const int pace_min_cx = 8 + boss_body.half_w.to_int();              // just inside the left wall (col 1)
const int pace_max_cx = (level.w - 1) * 8 - boss_body.half_w.to_int(); // just inside the right wall
```

- [ ] **Step 2: Add the movement step** in the main loop immediately AFTER `b.tick();` (~line 315) and BEFORE the boss render:

```cpp
// Pacing (data-gated): move horizontally, reverse at the walls. Paused while EXPOSED so the clean
// wound window doesn't also require tracking a moving target (mirrors the frozen-attack invariant).
if(level.boss->locomotion == logic::Locomotion::Pacing && !b.exposed()){
    int cx = boss_cx() + pace_dir * PACE_SPEED;
    if(cx <= pace_min_cx){ cx = pace_min_cx; pace_dir = 1; }
    else if(cx >= pace_max_cx){ cx = pace_max_cx; pace_dir = -1; }
    boss_body.pos.x = fx(cx - boss_body.half_w.to_int());
}
```

- [ ] **Step 3: Build** `bash tools/build_rom.sh` → `ROM fixed!`.

- [ ] **Step 4: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): data-gated pacing in run_room_boss (Locomotion::Pacing walks the floor, paused while exposed) (M13 P3.2)"
```

### Task 3.3: Respawning magic crystal (port from the King)

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

**Context:** SpellExpose costs magic (Fire) — port the King's repeatable magic crystal (`scene_boss.cpp` lines 162-173 decl + 403-413 loop) so the player can recharge mid-fight and never magic-soft-lock. `run_room_boss` already refills magic on start/restart; this adds the in-arena station, driven by `level.magic_crystals`. Needs `#include "bn_sprite_items_magic_crystal.h"` (if not already present) and the `tile_body` helper (used elsewhere in the file).

Follow the Per-Task Protocol.

- [ ] **Step 1: Add the crystal declaration** after the boss setup (near the attack-pool setup, ~line 203). Mirror `scene_boss.cpp`'s block but use `run_room_boss`'s `wx`/`wy`:

```cpp
// Respawning magic crystal (M10/King pattern): full refill on touch; reappears once magic is spent
// below one cast, so a SpellExpose room boss (Fire to expose) can never magic-soft-lock.
bool crystal_collected = false;
bn::optional<bn::sprite_ptr> crystal_sprite;
int crystal_tx = 0, crystal_ty = 0;
if(level.magic_crystal_count > 0){
    const logic::MagicCrystalSpawn& mc = level.magic_crystals[0];
    crystal_tx = mc.tx; crystal_ty = mc.ty;
    crystal_sprite = bn::sprite_items::magic_crystal.create_sprite(0, 0);
    crystal_sprite->set_camera(cam);
    crystal_sprite->set_position(wx(mc.tx * 8 + 8), wy(mc.ty * 8 + 8));
}
logic::Body crystal_body = tile_body(crystal_tx, crystal_ty, 6, 8);
```

- [ ] **Step 2: Reset the crystal in `restart_fight`** — add to the `restart_fight` lambda (~line 277, alongside `attacks.clear()`):

```cpp
crystal_collected = false; if(crystal_sprite) crystal_sprite->set_visible(true);
```

- [ ] **Step 3: Add the per-frame crystal logic** in the main loop (near the projectile-pool updates, after `spells.update_and_cast`, ~line 377):

```cpp
// magic crystal: full refill on overlap; reappears once magic drops below one cast (repeatable).
if(level.magic_crystal_count > 0){
    if(!crystal_collected && logic::aabb_overlap(player.body, crystal_body)){
        magic.cur = magic.max; crystal_collected = true;
        if(crystal_sprite) crystal_sprite->set_visible(false);
    }
    if(crystal_collected && magic.cur < 10){
        crystal_collected = false; if(crystal_sprite) crystal_sprite->set_visible(true);
    }
}
```

- [ ] **Step 4: Build** `bash tools/build_rom.sh` → `ROM fixed!` (D1 has no crystal → the `magic_crystal_count > 0` guard makes this a no-op for D1).

- [ ] **Step 5: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): port respawning magic crystal into run_room_boss (SpellExpose room bosses can't magic-soft-lock) (M13 P3.3)"
```

### Task 3.4: Rockfall wiring (rock pool + emitter in the attack loop)

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

**Context:** add a SECOND `AttackPool` for rocks (the `rock` sprite) + a `RockfallEmitter` (the `rock_marker` sprite). In the Active step, when `current_attack == BOSS_ATK_ROCKFALL`, drive the emitter instead of `spawn_attack`; advance the rock pool each frame (contact damage); clear both on restart. Needs `#include "bn_sprite_items_rock.h"` and `#include "bn_sprite_items_rock_marker.h"`.

Follow the Per-Task Protocol.

- [ ] **Step 1: Add the rock pool + emitter + seed** near the existing `attacks`/`spiral` setup (~line 204):

```cpp
engine::AttackPool rocks(bn::sprite_items::rock, cam, lvl.view.map_px_w, lvl.view.map_px_h);
engine::RockfallEmitter rockfall(bn::sprite_items::rock_marker, cam,
                                 lvl.view.map_px_w, lvl.view.map_px_h);
int rockfall_seed = 0;   // rolling counter -> varied rock spreads
```

- [ ] **Step 2: Reset them in `restart_fight`** (~line 277, with `attacks.clear()`):

```cpp
rocks.clear(); rockfall.clear();
```

- [ ] **Step 3: Drive the emitter in the Active step.** Locate the `if(!atk_spawned_this_active){ ... }` block inside the `AttackStep::Active` branch. It currently reads (anchor):

```cpp
current_attack = next_attack_for_phase(attack_slot);   // lock + advance
int pcx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
int pcy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
spiral.begin(boss_cx(), pcx0);                      // sweep toward the player's side
if(current_attack != logic::BOSS_ATK_SPIRAL){      // <-- REPLACE ONLY THIS conditional + its body
    int spd = (b.phase == 0) ? 3 : 4;
    engine::spawn_attack(attacks, current_attack, boss_cx(), boss_cy(),
                         pcx0, pcy0, spd, b.phase, /*aim_full=*/true);
}
```

KEEP the `current_attack = ...`, `pcx0`/`pcy0`, and `spiral.begin(...)` lines unchanged. Replace ONLY the `if(current_attack != logic::BOSS_ATK_SPIRAL){...}` conditional with:

```cpp
if(current_attack == logic::BOSS_ATK_ROCKFALL){
    int player_tx = (player.body.pos.x.to_int() + player.body.half_w.to_int()) / 8;
    int rock_count = (b.phase == 0) ? 3 : 5;            // escalate in P2
    rockfall.begin(player_tx, level.w, level.h, rock_count, rockfall_seed++);
} else if(current_attack != logic::BOSS_ATK_SPIRAL){
    int spd = (b.phase == 0) ? 3 : 4;
    engine::spawn_attack(attacks, current_attack, boss_cx(), boss_cy(),
                         pcx0, pcy0, spd, b.phase, /*aim_full=*/true);
}
```

Then find the existing spiral-tick line (anchor `if(current_attack == logic::BOSS_ATK_SPIRAL) spiral.tick(attacks, boss_cx(), boss_cy());`) and add the rockfall tick immediately after it:

```cpp
if(current_attack == logic::BOSS_ATK_ROCKFALL) rockfall.tick(rocks);
```

Finally, hide any lingering ground markers when the window ends or an expose interrupts a mid-warn rockfall. Add `rockfall.clear();` in TWO places: (a) in the `if(b.exposed()){ attacks.clear(); ... }` branch (next to `attacks.clear()`), and (b) in the Recovery `else` branch (next to its `telegraph.hide()`). `rockfall.clear()` only hides the markers + resets the emitter — it does NOT touch the `rocks` pool, so already-falling rocks keep falling (the player still dodges them).

- [ ] **Step 4: Advance the rock pool** each frame, right after the existing `attacks.advance(...)` contact block (~line 367):

```cpp
bool rock_hit = rocks.advance(player.body, level.w * 8, level.h * 8,
                              invuln != 0 || player.dash.invincible(), lvl.map);
if(rock_hit){ health.damage(20); invuln = 45; }
```

Note (testing-pitfalls TEST-3, tunneling): `ROCK_VY = 3` px/frame < 8 (tile size), so a rock cannot tunnel through the 2-row floor in one step — `AttackPool::advance` checks the centre tile each frame. Do NOT raise `ROCK_VY` to ≥ 8 without adding substepping.

- [ ] **Step 5: Make the rockfall read as a JUMP (user-requested).** Find the boss sprite position line (set in Task 3.1 — anchor `boss_spr.set_position(wx(boss_cx()), wy(boss_cy()));`) and subtract the emitter's leap offset so Slagshell visibly leaps as it triggers the rockfall:

```cpp
boss_spr.set_position(wx(boss_cx()), wy(boss_cy()) - rockfall.leap_offset());
```

`leap_offset()` returns 0 except during a rockfall warn, so this is a no-op for aimed attacks / pacing.

- [ ] **Step 6: Build** `bash tools/build_rom.sh` → `ROM fixed!`.

- [ ] **Step 7: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): wire rockfall into run_room_boss (rock pool + emitter + leap; P1 3 rocks / P2 5; contact damage) (M13 P3.4)"
```

**After Phase 3:** 3-round review (D1 unaffected — pacing/crystal/rockfall all gated on def fields it doesn't set; the King via `run_boss` untouched; no `bn::` in logic). Banner → ✅.

---

## Phase 4 — D2 level restructure + integration + invariants + QA

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** wires Slagshell into a real D2 and proves no soft-locks. Order matters: compiler key → level files → dungeons.h → invariants → QA.

### Task 4.1: `build_level.py` boss key for D2

**Files:**
- Modify: `tools/build_level.py`

Follow the Per-Task Protocol.

- [ ] **Step 1: Add `'d2'`** to the `BOSS_SYMBOL` dict (~line 52):

```python
BOSS_SYMBOL = {
    'd1': 'logic::D1_DEF',
    'd2': 'logic::D2_DEF',
}
```

- [ ] **Step 2: Commit** (verified indirectly when Task 4.2 compiles the room):

```bash
git add tools/build_level.py
git commit -m "feat(tools): build_level.py boss key d2 -> logic::D2_DEF (M13 P4.1)"
```

### Task 4.2: D2 level restructure (3 rooms)

**Files:**
- Modify→rename: `tools/levels/dungeon2.txt` → `tools/levels/dungeon2_room0.txt`; `tools/levels/dungeon2.json` → `tools/levels/dungeon2_room0.json`
- Create: `tools/levels/dungeon2_room1.txt` + `.json` (boss arena); `tools/levels/dungeon2_room2.txt` + `.json` (spronk)

**Context:** mirror D1's structure (`tools/levels/dungeon1_room0/1/2.*`). Char legend: `@`=spawn, `F`=Fire shrine, `C`=cage, `E`=exit, `N`=entrance (JSON id/facing), `D`=room-door (JSON target), `Q`=hub-return door (no JSON), `$`=magic crystal (no JSON), `#`=wall, `s`=spikes. Room JSON keys: `boss`, `entrances`, `room_doors`, plus the existing puzzle keys for room 0.

Follow the Per-Task Protocol.

- [ ] **Step 1: Room 0** — `git mv tools/levels/dungeon2.txt tools/levels/dungeon2_room0.txt` and `git mv tools/levels/dungeon2.json tools/levels/dungeon2_room0.json`. Edit `dungeon2_room0.txt`: **keep all existing puzzle content + the `@` spawn + the `F` Fire shrine.** Then: (a) find the single `C` (cage) cell and the single `E` (exit) cell — **delete BOTH** (replace each with `.`); they move to room 2 (the dungeon now clears in room 2, not here). (b) On the floor near where the cage was (the far/right end), place an `N` then a `D` on adjacent floor cells (entrance id 1 + the room-door to room 1). (c) Near the `@` start, place a `Q` on a floor cell (the hub-return door). The current single-room D2 has NO entrances/doors, so all of `N`/`D`/`Q` are new. Edit `dungeon2_room0.json` — keep the existing `enemies`/`pickups`/`plates`/`buttons`/`braziers`/`brazier_groups` keys and ADD:

```json
  "entrances": [ { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 1, "target_entrance": 0 } ]
```

(The `Q` needs no JSON — it hardcodes `target_room:-1`. Order matters: the `D` is room_doors entry #0; the `Q` is auto-appended as a `-1` door and needs no entry.)

- [ ] **Step 2: Room 1 (boss arena)** — create `tools/levels/dungeon2_room1.txt`: a FLAT-floor arena (model on `dungeon1_room1.txt` but **no spikes**), **~48 wide × ~19 tall (match D1's boss arena)** for comfortable dodging room, with `N`(id0,left)+`D`(→room0) on the left, `N`(id1,right)+`D`(→room2) on the right, a `$` magic crystal on the floor between the entrance and centre (reachable from the entrance with a short walk), and NO `@`/`C`/`E`. Floor on the bottom two rows; solid `#` border all around. `dungeon2_room1.json`:

```json
{ "tileset": "tiles",
  "boss": "d2",
  "entrances": [ { "id": 0, "facing": 1 }, { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 0, "target_entrance": 1 },
                  { "target_room": 2, "target_entrance": 0 } ] }
```

- [ ] **Step 3: Room 2 (spronk)** — create `tools/levels/dungeon2_room2.txt`: model on `dungeon1_room2.txt` — `N`(id0,left)+`D`(→room1), the `C` cage grounded on the floor, the `E` exit grounded, no spikes. `dungeon2_room2.json`:

```json
{ "tileset": "tiles",
  "entrances": [ { "id": 0, "facing": 1 } ],
  "room_doors": [ { "target_room": 1, "target_entrance": 1 } ] }
```

- [ ] **Step 4: Compile + verify** — run `bash tools/build_rom.sh` (it regenerates level headers; `ROM fixed!`). Confirm `include/game/levels/dungeon2_room0.h` / `_room1.h` / `_room2.h` are generated and `dungeon2_room1.h` contains `&logic::D2_DEF`.

- [ ] **Step 5: Commit** (the `git mv` in Step 1 already staged the renames; just add the new room files + generated headers. The orphaned `include/game/levels/dungeon2.h` is removed in Task 4.3, where `dungeons.h` stops including it.):

```bash
git add tools/levels/dungeon2_room0.* tools/levels/dungeon2_room1.* tools/levels/dungeon2_room2.* include/game/levels/dungeon2_room*.h
git commit -m "content: restructure D2 into entry(+Fire shrine) -> Slagshell boss arena(+magic crystal) -> spronk rooms (M13 P4.2)"
```

### Task 4.3: `dungeons.h` — D2 3-room dungeon

**Files:**
- Modify: `include/game/levels/dungeons.h`

Follow the Per-Task Protocol.

- [ ] **Step 1: Replace the D2 include + dungeon** — swap `#include "game/levels/dungeon2.h"` for the three room headers, and replace the 1-room `DUNGEON2_ROOMS`/`DUNGEON2_DUNGEON` with the 3-room form (mirror the D1 block):

```cpp
#include "game/levels/dungeon2_room0.h"
#include "game/levels/dungeon2_room1.h"
#include "game/levels/dungeon2_room2.h"
```

```cpp
// DUNGEON2 — Ember Caverns (M13 restructure): room 0 = the Fire-spell puzzle (+ '@' spawn, hub-return
// 'Q'); room 1 = the Slagshell boss arena (D2_DEF, SpellExpose+Fire, pacing, rockfall — fought on
// entry; victory opens the onward door); room 2 = the spronk + exit.
inline constexpr const logic::LevelData* DUNGEON2_ROOMS[] = {
    &DUNGEON2_ROOM0_DATA, &DUNGEON2_ROOM1_DATA, &DUNGEON2_ROOM2_DATA };
inline constexpr logic::DungeonData DUNGEON2_DUNGEON{ DUNGEON2_ROOMS, 3, 0 };
```

- [ ] **Step 2: Delete the orphaned generated header.** `include/game/levels/dungeon2.h` (the old single-room header) is a committed file that is NO LONGER regenerated (its source `dungeon2.txt` was renamed in Task 4.2) and nothing includes it after Step 1. Remove it: `git rm include/game/levels/dungeon2.h`.

- [ ] **Step 3: Build** `bash tools/build_rom.sh` → `ROM fixed!` (confirms nothing still references `dungeon2.h`/`DUNGEON2_DATA`).

- [ ] **Step 4: Commit**

```bash
git add include/game/levels/dungeons.h
git commit -m "feat(game): DUNGEON2 is now a 3-room dungeon (entry/boss/spronk); drop orphaned dungeon2.h (M13 P4.3)"
```

### Task 4.4: D2 no-soft-lock + structural invariants

**Files:**
- Create: `test/test_dungeon2_level.cpp`

**Context:** mirror `test/test_dungeon1_level.cpp` (the flood-fill reachability harness with the 2-wide×4-tall body model + the double-jump-over-hazard "hop" edge). Reuse the same approach; assert against the COMPILED `DUNGEON2_ROOM*_DATA`.

Follow the Per-Task Protocol. **Assertion rigor:** every invariant MUST FAIL on a deliberately-broken layout — verify non-vacuity during authoring (e.g., temporarily wall off the onward door → test RED → revert). Do NOT weaken an invariant to pass.

- [ ] **Step 1: Write the tests** `test/test_dungeon2_level.cpp` (copy the `Grid`/`build_grid`/`snap_start`/`reachable`/`stands_at`/`reaches_forward_exit`/`room_start_x`/`room_start_y` helpers + the `CLIMB`/`PW`/`PH` constants from `test_dungeon1_level.cpp`, pointed at the `DUNGEON2_ROOM*` data; then the D2 assertions). **Keep every copied helper/constant `static`** (internal linkage) — `test_dungeon1_level.cpp` already defines identically-named symbols, and both are linked into the one host-test binary, so non-static duplicates would be an ODR/multiple-definition link error. `TEST(...)` names must be globally unique — all D2 tests below already use the `d2_` prefix.

```cpp
// (Include the same headers + copy the Grid/build_grid/snap_start/reachable/stands_at/
//  reaches_forward_exit/room_start_x/room_start_y/d_tile helpers from test_dungeon1_level.cpp,
//  pointed at the DUNGEON2_ROOM* data.)
TEST(d2_dungeon_table){
    CHECK_EQ(DUNGEON2_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON2_DUNGEON.start_room, 0);
}
TEST(d2_room1_is_boss_arena_with_crystal){
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    CHECK(L.boss == &D2_DEF);                 // the canonical D2 symbol
    CHECK_EQ((int)L.boss->phase_count, 2);
    CHECK(!L.has_cage); CHECK(!L.has_exit);
    CHECK(L.magic_crystal_count >= 1);        // SpellExpose can't magic-soft-lock
}
TEST(d2_only_room1_has_boss){
    CHECK(DUNGEON2_ROOM0_DATA.boss == nullptr);
    CHECK(DUNGEON2_ROOM2_DATA.boss == nullptr);
}
TEST(d2_room0_has_fire_shrine_no_cage){
    const LevelData& L = DUNGEON2_ROOM0_DATA;
    CHECK(!L.has_cage);
    bool fire = false; for(int i=0;i<L.pickup_count;++i) if(L.pickups[i].ability==Ability::Fire) fire=true;
    CHECK(fire);                              // Fire earned BEFORE the boss (needed to expose Slagshell)
}
TEST(d2_room2_has_cage_and_exit){
    const LevelData& L = DUNGEON2_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);
}
TEST(d2_room1_onward_door_reachable){     // boss arena traversable to room 2 (fail-on-broken)
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    bool onward = false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
}
TEST(d2_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON2_ROOM2_DATA;
    Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    CHECK(stands_at(L, seen, L.cage_tx, L.cage_ty));
    CHECK(stands_at(L, seen, L.exit_tx, L.exit_ty));
}
TEST(d2_room1_crystal_reachable){          // the magic-crystal safety net must be reachable
    const LevelData& L = DUNGEON2_ROOM1_DATA;
    Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    CHECK(L.magic_crystal_count >= 1);
    CHECK(stands_at(L, seen, L.magic_crystals[0].tx, L.magic_crystals[0].ty));
}
```

- [ ] **Step 2: Run red→green** — `bash tools/host_test.sh`. If an invariant is RED, the LEVEL is wrong (fix the `.txt`, recompile via `bash tools/build_rom.sh`, re-run) — do NOT weaken the test. Verify non-vacuity: temporarily wall off the onward door in `dungeon2_room1.txt`, recompile, confirm `d2_room1_onward_door_reachable` goes RED, then revert.

- [ ] **Step 3: Extend the respawn-settle test to D2.** In `test/test_d1_boss_respawn.cpp` add a `d2_all_entrances_settle_on_floor` TEST mirroring `d1_all_entrances_settle_on_floor` (drive `player.update`/`move_and_collide` against each `DUNGEON2_ROOM*_DATA` entrance; assert `on_ground && !buried && feet_row < h`). Run green.

- [ ] **Step 4: Commit**

```bash
git add test/test_dungeon2_level.cpp test/test_d1_boss_respawn.cpp
git commit -m "test(d2): no-soft-lock + structural invariants (boss arena, crystal reachable, spronk/exit reachable) + D2 respawn-settle (M13 P4.4)"
```

### Task 4.5: ROM build + emulator QA (handed to user)

**Files:** none (verification).

- [ ] **Step 1: Final build + full host suite** — `bash tools/host_test.sh` (all green incl. unchanged King/D1), `python tools/check_logic_purity.py` (`logic purity OK`), `bash tools/build_rom.sh` (`ROM fixed!`).

- [ ] **Step 2: Emulator QA checklist** (hand `SpronkQuest.gba` to the user):
  - D2 room 0: solve the puzzle, earn Fire, reach the onward door.
  - Slagshell **paces** the floor and is hittable; **Fire exposes** it (crust→molten frame) and a bolt/Fire lands one wound.
  - **Anti-spam:** after a wound the crust reforms — Fire does nothing for ~1.5s and the boss keeps attacking; you must dodge before re-exposing (no Fire→bolt mash-to-win).
  - **Rockfall:** crouch telegraph → ground crack-markers at ~3 columns (P2 ~5) → rocks fall, always a dodge gap; rocks deal contact damage + despawn on the floor; aimed fireballs reach the walls.
  - **Magic never soft-locks** (crystal refills; reachable).
  - Victory → onward door opens → free the spronk → reach the exit → **D2 cleared & saved** (+1 max life).
  - **Regression:** the King (Door 9) and D1 Guardian play exactly as before.

- [ ] **Step 3:** On QA pass, update the Execution Status banners → ✅ and proceed to the final review + `superpowers:finishing-a-development-branch`.

**After Phase 4:** 3-round review against spec §8 success criteria. Banner → ✅.

---

## Self-Review (author checklist — completed)

- **Spec coverage:** §2.1 SpellExpose+Fire → 1.1 (D2_DEF) + reused `resolve_damage`. §2.2 anti-spam → 1.1 (long `hit_iframes`) + `d2_no_reexpose_during_rearm`. §2.3 pacing → 1.1 (`Locomotion`) + 3.2. §2.4 phases/attacks → 1.1. §2.5 rockfall → 1.2 (picker) + 2.1 (art) + 2.2 (emitter) + 3.4 (wiring). §2.6 magic crystal → 3.3 + invariant in 4.4. §3 art/dialogue → 1.1 (lines) + 2.1 (slagshell) + 3.1 (selector). §4 rooms → 4.1/4.2/4.3. §5 no save change → no SaveData task (asserted). §6 purity → Global Constraints + per-task purity check. §7 testing → 1.1/1.2/4.4 + QA 4.5.
- **Placeholder scan:** none — every code step has concrete code; tuning values are concrete with "(tunable in QA)" noted.
- **Type consistency:** `Locomotion`/`BOSS_ATK_ROCKFALL`/`D2_DEF`/`rockfall_columns`/`RockfallEmitter`/`boss_sprite_for` names are used identically across tasks.
