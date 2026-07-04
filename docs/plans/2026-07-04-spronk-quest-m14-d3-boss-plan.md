# M14 — D3 Frost Hollow Boss "Coldforge Twins" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the D3 (Frost Hollow) boss — the "Coldforge Twins", a two-headed fire/ice beast whose vulnerable element SHIFTS each wound so the player must cycle Fire↔Ice — on the M12/M13 boss framework, restructuring D3 from 1 room into boss room + spronk.

**Architecture:** Generalize M12's fixed-element `VulnMode::SpellExpose` into a *shifting* one via a new `BossState::cur_expose` (the current expose element) that `on_wound` flips between `BossDef::expose_spell` and a new `BossDef::expose_spell_alt`. Everything else reuses M13: `run_room_boss` (stationary — no pacing/rockfall/crystal for D3), `AttackPool` aimed + spiral, block-to-charge (extended to a second block spell so both Fire & Ice recharge magic), magic continuity, the 1→3-room restructure. The boss shows its state via a 4-frame sprite indexed by `(cur_expose, exposed)`.

**Tech Stack:** C++17, Butano 21.6.0, GBA (devkitARM). Host tests via `bash tools/host_test.sh`. ROM via `bash tools/build_rom.sh`. Purity guard `python tools/check_logic_purity.py`. Three-layer architecture (`include/logic`+`src/logic` pure C++ NO `bn::`; `src/engine` Butano glue; `src/game` scenes).

## Global Constraints

- **Three-layer purity (HARD):** NO `bn::` in `include/logic/`. Purity guard MUST stay green (`python tools/check_logic_purity.py` → `logic purity OK`).
- **King + D1 + D2 regression:** the new `BossDef` fields (`expose_spell_alt`, `block_spell2`) are the LAST fields, defaulted `SpellId::None`, so `KING_DEF` (8-field), `D1_DEF` (11-field), `D2_DEF` (13-field) aggregate inits are UNCHANGED. `BossState::cur_expose` initializes to `def->expose_spell` in `reset()`, so `on_expose_hit`/`resolve_damage` reading `cur_expose` is IDENTICAL to reading `def->expose_spell` for every non-shift boss. All existing `test_boss.cpp` assertions MUST stay UNCHANGED and green.
- **No save-format change:** boss not persisted (re-entry re-fights). `SaveData` untouched.
- **GBA:** integer fixed-point only, no `float`/`double` (implementation-pitfalls IMPL-2). Logic tests assert exact integers (testing-pitfalls TEST-2).
- **Art is a MANUAL step:** `python tools/make_placeholder_art.py` is NOT run by the build; run it explicitly and commit the `graphics/*` outputs. The ROM build + host tests glob `tools/levels/*.txt` (no hardcoded level list); generated `include/game/levels/*.h` are committed.
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
3. Follow TDD: failing test → run red → implement → run green → commit.

**Host-testable tasks** (touching `include/logic/`, `src/logic/`, `test/`) are TDD with `bash tools/host_test.sh`. **ROM-only tasks** (engine `bn::` glue, scenes, art) verify via `bash tools/build_rom.sh` (`ROM fixed!`) + the Phase-4 emulator QA; extract any host-testable logic into a logic task FIRST.

**BEFORE marking any task complete:**
1. Re-read tests against `docs/pitfalls/testing-pitfalls.md`; confirm edge/error paths.
2. Run `bash tools/host_test.sh` (green) AND, for `logic/` changes, `python tools/check_logic_purity.py` (`logic purity OK`).
3. If a test races/flakes/fails nondeterministically, fix with determinism (a fixed seed / explicit ticks) — **NOT** assertion removal/weakening. If you can't, STOP and raise. Commit subjects touching assertions say "add"/"strengthen"/"preserve".

**After completing each Phase:** review the batch from multiple perspectives — minimum 3 rounds (purity; King/D1/D2 regression; spec success criteria). Keep going until clean.

---

## Execution Status

**Overall:** Not started.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure-logic shifting-expose (cur_expose + BossDef fields + D3_DEF) | ⬜ Not started | — | host-tested |
| 2 — Coldforge 4-frame art | ⬜ Not started | — | ROM-built |
| 3 — Integration (resolve_damage cur_expose + run_room_boss sprite/frame/block2) | ⬜ Not started | — | ROM-built |
| 4 — D3 level restructure + invariants + QA | ⬜ Not started | — | host + emulator QA |

---

## File Structure

**Created:** `test/test_dungeon3_level.cpp`; `tools/levels/dungeon3_room0.txt`/`_room1.txt`/`_room2.txt` (+ `.json`); `include/game/levels/dungeon3_room0.h`/`_room1.h`/`_room2.h` (generated).

**Modified:** `include/logic/boss.h` (shifting-expose + D3_DEF); `test/test_boss.cpp` (D3 tests + regression); `include/engine/boss_attacks.h` (`resolve_damage` uses `cur_expose`); `src/game/scene_dungeon.cpp` (`run_room_boss`: D3 sprite, element-frame, block2); `tools/make_placeholder_art.py` (`gen_coldforge`); `tools/build_level.py` (`BOSS_SYMBOL['d3']`); `include/game/levels/dungeons.h` (`DUNGEON3_*` 3-room); `test/test_d1_boss_respawn.cpp` (D3 entrances). **Removed:** `tools/levels/dungeon3.txt`/`.json` (renamed), `include/game/levels/dungeon3.h` (orphaned).

---

## Phase 1 — Pure-logic shifting-expose (cur_expose + BossDef fields + D3_DEF)

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** the shifting element is the whole new mechanic and it's fully host-testable. Getting `cur_expose` to initialize to `def->expose_spell` is what keeps every existing boss byte-for-byte identical.

### Task 1.1: Shifting-expose logic + D3_DEF

**Files:**
- Modify: `include/logic/boss.h`
- Test: `test/test_boss.cpp`

**Interfaces:**
- Produces: `BossDef::expose_spell_alt`, `BossDef::block_spell2` (defaulted `SpellId::None`, LAST fields); `BossState::cur_expose` (current expose element) + shift logic; `logic::D3_DEF`, `D3_PHASES`, `D3_ATTACKS_P1/P2`. Consumed by Phase 3 (`resolve_damage`/`run_room_boss`) + Phase 4 (`build_level.py`).

Follow the Per-Task Protocol.

- [ ] **Step 1: Write the failing tests** — append to `test/test_boss.cpp` (do NOT modify existing tests):

```cpp
// --- M14 D3 Coldforge Twins (SpellExpose with a SHIFTING element: cycle Fire<->Ice each wound) ---
TEST(bossdef_d3_coldforge_fields){
    CHECK_EQ(D3_DEF.max_hp, 70);
    CHECK_EQ(D3_DEF.phase_count, 2);
    CHECK((int)D3_DEF.vuln == (int)VulnMode::SpellExpose);
    CHECK((int)D3_DEF.expose_spell == (int)SpellId::Fire);       // starts on the ICE head -> counter is Fire
    CHECK((int)D3_DEF.expose_spell_alt == (int)SpellId::Ice);    // shifts to Ice (fire head)
    CHECK((int)D3_DEF.locomotion == (int)Locomotion::Stationary);
    CHECK((int)D3_DEF.block_spell == (int)SpellId::Fire);
    CHECK((int)D3_DEF.block_spell2 == (int)SpellId::Ice);        // both elementals block+charge
    CHECK(D3_DEF.hit_iframes >= 60);                             // re-armor gates spam (with the forced cycle)
    CHECK_EQ(D3_DEF.phases[0].end_hp, 35);
    CHECK_EQ((int)D3_DEF.phases[0].attacks, (int)BOSS_ATK_AIMED);
    CHECK_EQ((int)D3_DEF.phases[1].attacks, (int)(BOSS_ATK_AIMED | BOSS_ATK_SPIRAL));
    CHECK(D3_DEF.phases[0].pattern.telegraph_frames >= SWITCH_BUDGET);
    CHECK(D3_DEF.phases[1].pattern.telegraph_frames >= SWITCH_BUDGET);
}
// THE shift invariant: only the CURRENT element exposes; a wound flips it; you must alternate.
TEST(d3_shift_expose_alternates){
    BossState b; b.reset(D3_DEF);
    CHECK((int)b.cur_expose == (int)SpellId::Fire);          // ice head active -> cast Fire
    b.on_expose_hit(SpellId::Ice);   CHECK(!b.exposed());   // wrong element: no expose
    b.on_expose_hit(SpellId::Fire);  CHECK(b.exposed());    // correct: exposes
    b.on_wound(D3_DEF.wound_dmg);
    CHECK((int)b.cur_expose == (int)SpellId::Ice);          // wound FLIPS the element
    CHECK(!b.exposed());
    for(int i=0;i<D3_DEF.hit_iframes;++i) b.tick();         // drain re-armor
    b.on_expose_hit(SpellId::Fire);  CHECK(!b.exposed());   // Fire no longer exposes
    b.on_expose_hit(SpellId::Ice);   CHECK(b.exposed());    // Ice now exposes
    b.on_wound(D3_DEF.wound_dmg);
    CHECK((int)b.cur_expose == (int)SpellId::Fire);         // flips back to Fire
}
TEST(d3_takes_seven_wounds_alternating){
    BossState b; b.reset(D3_DEF);
    int wounds=0;
    while(!b.defeated() && wounds<100){
        b.on_expose_hit(b.cur_expose);                     // cast the currently-needed element
        b.on_wound(D3_DEF.wound_dmg);
        for(int i=0;i<D3_DEF.hit_iframes;++i) b.tick();    // wait out re-armor
        ++wounds;
    }
    CHECK(b.defeated());
    CHECK_EQ(wounds, 7);                                    // 70/10
}
// REGRESSION: non-shift bosses keep a fixed element (alt==None -> cur_expose never moves).
TEST(king_d1_d2_no_shift_fields){
    CHECK((int)KING_DEF.expose_spell_alt == (int)SpellId::None);
    CHECK((int)D1_DEF.expose_spell_alt   == (int)SpellId::None);
    CHECK((int)D2_DEF.expose_spell_alt   == (int)SpellId::None);
    CHECK((int)KING_DEF.block_spell2 == (int)SpellId::None);
    CHECK((int)D1_DEF.block_spell2   == (int)SpellId::None);
    CHECK((int)D2_DEF.block_spell2   == (int)SpellId::None);
}
TEST(d2_cur_expose_does_not_shift){
    BossState b; b.reset(D2_DEF);
    CHECK((int)b.cur_expose == (int)SpellId::Fire);        // == expose_spell
    b.on_expose_hit(SpellId::Fire); CHECK(b.exposed());
    b.on_wound(D2_DEF.wound_dmg);
    CHECK((int)b.cur_expose == (int)SpellId::Fire);        // alt==None -> no shift
}
```

- [ ] **Step 2: Run red** — `bash tools/host_test.sh` → FAIL (`D3_DEF`/`cur_expose`/`expose_spell_alt`/`block_spell2` undefined).

- [ ] **Step 3: Implement in `include/logic/boss.h`.**

3a. Add two fields to the END of `struct BossDef` (after `block_spell`, line ~58), defaulted so existing inits are unchanged:

```cpp
    SpellId     block_spell = SpellId::None;            // M13: a player cast of this spell DESTROYS the
                                                       // boss's bolts on contact (None = dodge-only).
    SpellId     expose_spell_alt = SpellId::None;       // M14: if set, cur_expose SHIFTS between
                                                       // expose_spell and this on each wound (dual-spell boss).
    SpellId     block_spell2 = SpellId::None;           // M14: a SECOND spell that also blocks+charges
                                                       // (dual-element block; None = only block_spell blocks).
```

3b. Add the `cur_expose` member to `BossState` (after `attack_cycles`, line ~118):

```cpp
    int attack_cycles = 0;        // TiredWindow: completed attack cycles since the last tired window
    SpellId cur_expose = SpellId::None;   // M14: the CURRENTLY vulnerable element (== expose_spell for a
                                          // non-shift boss; flips on wound for a shift boss).
```

3c. Initialize `cur_expose` in `reset()`:

```cpp
    void reset(const BossDef& d){
        def = &d; hp = d.max_hp; phase = 0; phase_start_hp = d.max_hp;
        expose_timer = 0; attack_timer = 0; hit_iframes = 0; attack_cycles = 0;
        cur_expose = d.expose_spell;   // M14: current element starts at the def's expose spell
    }
```

3d. Change `on_expose_hit` to check `cur_expose` (was `def->expose_spell`):

```cpp
    void on_expose_hit(SpellId s){
        if(defeated() || hit_iframes > 0) return;          // can't re-expose during post-wound i-frames
        if(def->vuln != VulnMode::SpellExpose) return;     // AlwaysVulnerable: no expose mechanic
        if(s != cur_expose) return;                        // M14: must match the CURRENT element
        expose_timer = def->expose_frames;
    }
```

3e. In `on_wound`, flip `cur_expose` when the boss is a shift boss (after the `expose_timer = 0` line):

```cpp
    void on_wound(int dmg){
        if(defeated() || hit_iframes > 0 || !vulnerable()) return;
        hp -= dmg; if(hp < 0) hp = 0;
        advance_phase_for_hp();
        hit_iframes = def->hit_iframes;
        if(def->vuln != VulnMode::AlwaysVulnerable) expose_timer = 0;
        if(def->expose_spell_alt != SpellId::None)   // M14: shift the vulnerable element for the next opening
            cur_expose = (cur_expose == def->expose_spell) ? def->expose_spell_alt : def->expose_spell;
    }
```

3f. Add the D3 attack masks, phase table, and def AFTER `D2_DEF` (after line ~107). Field order is `max_hp, wound_dmg, hit_iframes, expose_frames, vuln, expose_spell, phases, phase_count, tired_after, intro, death, locomotion, block_spell, expose_spell_alt, block_spell2`:

```cpp
// --- D3 Frost Hollow boss: the "Coldforge Twins" (SpellExpose with a SHIFTING element, Stationary,
//     2 phases). A two-headed beast; the LIT head is the target. Counter the active head with the
//     OPPOSITE spell: ice head active -> cast Fire; fire head active -> cast Ice -> expose -> wound ->
//     the active head SWITCHES (cur_expose flips Fire<->Ice), forcing an L-cycle each wound. Both
//     Fire AND Ice block+charge magic (block_spell/block_spell2). Starts on the ice head (cast Fire). ---
inline constexpr uint8_t D3_ATTACKS_P1 = BOSS_ATK_AIMED;
inline constexpr uint8_t D3_ATTACKS_P2 = BOSS_ATK_AIMED | BOSS_ATK_SPIRAL;
inline constexpr BossPhaseDef D3_PHASES[2] = {
    { 35, { 80, 30, 40 }, D3_ATTACKS_P1 },   // P1 70->35 : aimed frost shards
    {  0, { 70, 30, 30 }, D3_ATTACKS_P2 },   // P2 35->0  : + the rotating spiral (icy shard-ring)
};
inline constexpr BossDef D3_DEF{
    70, 10, 70, 90, VulnMode::SpellExpose, SpellId::Fire, D3_PHASES, 2,
    /*tired_after=*/0,
    /*intro_line=*/"One of us always burns.",
    /*death_line=*/"Both heads... fall still...",
    /*locomotion=*/Locomotion::Stationary,
    /*block_spell=*/SpellId::Fire,
    /*expose_spell_alt=*/SpellId::Ice,
    /*block_spell2=*/SpellId::Ice
};
```

- [ ] **Step 4: Run green** — `bash tools/host_test.sh` → `N/N tests passed` (new D3 tests pass; ALL existing King/D1/D2 tests still pass). `python tools/check_logic_purity.py` → `logic purity OK`.

- [ ] **Step 5: Commit**

```bash
git add include/logic/boss.h test/test_boss.cpp
git commit -m "feat(logic): shifting-expose element (BossState::cur_expose + BossDef::expose_spell_alt/block_spell2) + D3_DEF Coldforge Twins; non-shift bosses unchanged (M14 P1.1)"
```

**After Phase 1:** 3-round review (purity; King/D1/D2 regression green; cur_expose==expose_spell for non-shift). Banner → ✅.

---

## Phase 2 — Coldforge 4-frame art

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** the boss must read its current element (ice/fire head) AND exposed/armored — a 4-frame sprite the Phase-3 frame logic indexes.

### Task 2.1: `gen_coldforge` placeholder art

**Files:**
- Modify: `tools/make_placeholder_art.py`

**Context:** `gen_slagshell` (a `draw_slagshell_frame(im, oy, exposed)` helper + a `gen_slagshell` writing a 32×(32*2) image, `{"type":"sprite","height":32}`, registered in `if __name__ == "__main__":`) is the template. The Coldforge Twins needs **4** stacked 32×32 frames (a 32×128 image), in this exact order (matched by Phase 3's `want_frame`):

| Frame | oy | Active head | State |
|---|---|---|---|
| 0 | 0   | ice (blue, lit), fire head dormant | armored |
| 1 | 32  | ice (blue, lit) | exposed (recoiling) |
| 2 | 64  | fire (red, lit), ice head dormant | armored |
| 3 | 96  | fire (red, lit) | exposed |

Follow the Per-Task Protocol. (Art has no host test; verify the generator runs + the ROM builds in Phase 3.)

- [ ] **Step 1: Add a `draw_coldforge_frame(im, oy, fire_active, exposed)` helper** modeled on `draw_slagshell_frame`: draw a two-headed beast body (a wide 32×32 mass with a LEFT head and a RIGHT head). Make one head LIT (bright) and the other DORMANT (dark) per `fire_active`: **fire head = red (pal 13) + gold-hot (pal 6)** (reuse Slagshell's molten palette); **ice head = the blue/cyan the ice art already uses** — inspect `make_placeholder_art.py`'s palette definition + `gen_light_proj`/`gen_magic_crystal`/`ice_proj` art for the existing blue/cyan indices and reuse them (do NOT invent a new palette entry). Add white glints (pal 15) on the lit head. When `exposed`, the LIT head cracks/brightens (its counter melted/cooled it) to clearly signal the wound window; the DORMANT head is dark/dim regardless. **Keep the two heads in the SAME positions across all four frames** (e.g. ice head always left, fire head always right) — only which head is lit/dormant + the exposed cracking changes; the player reads the element from the lit head's COLOUR, not its position. Use integer pixel ops only (`rect`, `px`) — no floats.

- [ ] **Step 2: Add `gen_coldforge`:**

```python
def gen_coldforge():
    """D3 Frost Hollow boss placeholder 32x(32*4) — FOUR 32x32 frames (M14) indexed by (active head,
    exposed): 0 ice-active armored, 1 ice-active exposed, 2 fire-active armored, 3 fire-active exposed.
    A two-headed beast: one ICE head (blue) + one FIRE head (red); the LIT head is the wound target."""
    im = new_img(32, 32 * 4)
    draw_coldforge_frame(im, 0,  fire_active=False, exposed=False)   # 0: ice head lit, armored
    draw_coldforge_frame(im, 32, fire_active=False, exposed=True)    # 1: ice head lit, exposed
    draw_coldforge_frame(im, 64, fire_active=True,  exposed=False)   # 2: fire head lit, armored
    draw_coldforge_frame(im, 96, fire_active=True,  exposed=True)    # 3: fire head lit, exposed
    write(im, "coldforge", {"type": "sprite", "height": 32})
```

- [ ] **Step 3: Register + run.** Add `gen_coldforge()` in the `if __name__ == "__main__":` block after `gen_slagshell()`. Run `python tools/make_placeholder_art.py` (NOT run by the build) → confirm it writes `graphics/coldforge.json` + `graphics/coldforge.bmp` without error.

- [ ] **Step 4: Commit**

```bash
git add tools/make_placeholder_art.py graphics/coldforge.*
git commit -m "art: Coldforge Twins 4-frame boss (ice/fire head x armored/exposed) (M14 P2.1)"
```

**After Phase 2:** 3-round review (4 frames, correct order, integer ops). Banner → ✅.

---

## Phase 3 — Integration (resolve_damage cur_expose + run_room_boss sprite/frame/block2)

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** wires the shift into the actual fight. ROM-built; behaviour confirmed in Phase-4 QA. Do Task 3.1 (engine) then 3.2 (scene) — they touch different files so aren't strictly order-dependent, but both are required for the shift to work end-to-end.

**Context (no code needed):** `run_room_boss` ALREADY handles the dual-spell input the Twins need — it reads `L` to `spell.cycle(world)` (Fire↔Ice) and `R` to cast the selected Fire/Ice (`cast_spell` + `spells.update_and_cast`), and `resolve_damage` already accepts a Fire OR Ice wound while vulnerable. So no new input/wound code — the shift is enabled purely by (3.1) `resolve_damage` reading `cur_expose` and (3.2) the frame/block edits.

### Task 3.1: `resolve_damage` uses the current element

**Files:**
- Modify: `include/engine/boss_attacks.h`

**Context:** `resolve_damage` (line ~258) currently hardcodes `b.def->expose_spell` for the expose check. For a shifting boss it must use `b.cur_expose`. For non-shift bosses `cur_expose == expose_spell`, so this is behaviourally identical (no King/D1/D2 change).

Follow the Per-Task Protocol. (Engine template — verified by the Phase-4 host tests exercising `on_expose_hit(cur_expose)` at the BossState level + the ROM build; the shift *logic* is already host-covered in Phase 1.)

- [ ] **Step 1: Change the expose check** — in `resolve_damage`, replace the two `b.def->expose_spell` uses with `b.cur_expose`:

```cpp
    // Light/Fire/Ice (the CURRENT expose element) exposes/refreshes (no-op if defeated/i-framed or
    // AlwaysVulnerable). cur_expose == expose_spell for a non-shift boss; shifts for a dual-spell boss.
    if(b.def->vuln == logic::VulnMode::SpellExpose &&
       spells.consume_hit(boss_body, b.cur_expose)){
        b.on_expose_hit(b.cur_expose);
    }
```

(The wound block below it is unchanged — `bolts`/`Fire`/`Ice` wound while `vulnerable()`.)

- [ ] **Step 2: Build** — `bash tools/build_rom.sh` → `ROM fixed!`. `bash tools/host_test.sh` still green (445+/N; no logic change). `python tools/check_logic_purity.py` → `logic purity OK`.

- [ ] **Step 3: Commit**

```bash
git add include/engine/boss_attacks.h
git commit -m "feat(engine): resolve_damage exposes on BossState::cur_expose (enables the shifting element; identical for non-shift bosses) (M14 P3.1)"
```

### Task 3.2: `run_room_boss` — D3 sprite, element-aware frame, dual block

**Files:**
- Modify: `src/game/scene_dungeon.cpp`

**Context:** three small edits to `run_room_boss`. **Line numbers are approximate — locate by the quoted anchor.** Add `#include "bn_sprite_items_coldforge.h"` with the other sprite includes.

Follow the Per-Task Protocol. (ROM-built; QA in Phase 4.)

- [ ] **Step 1: Boss-sprite selector** — add a D3 entry to `boss_sprite_for` (anchor: the function returning `slagshell` for `&D2_DEF`):

```cpp
static const bn::sprite_item& boss_sprite_for(const logic::BossDef* def){
    if(def == &logic::D2_DEF) return bn::sprite_items::slagshell;
    if(def == &logic::D3_DEF) return bn::sprite_items::coldforge;
    return bn::sprite_items::guardian;   // D1 (default)
}
```

- [ ] **Step 2: Element-aware frame swap** — find the frame line (anchor: `int want_frame = b.exposed() ? 1 : 0;`) and replace it so a shift boss uses frames 2–3 when the current element is the def's alt:

```cpp
        // 4-frame shift boss (D3): frames 0-1 = element A (expose_spell), 2-3 = element B
        // (expose_spell_alt). Non-shift bosses (expose_spell_alt==None) keep elem_base 0 -> unchanged
        // 2-frame behaviour. +1 within a pair = the exposed frame.
        int elem_base = (level.boss->expose_spell_alt != logic::SpellId::None
                         && b.cur_expose == level.boss->expose_spell_alt) ? 2 : 0;
        int want_frame = elem_base + (b.exposed() ? 1 : 0);
```

- [ ] **Step 3: Block with BOTH block spells** — find the block-defense block (anchor: `if(level.boss->block_spell != logic::SpellId::None){ ... block_with_spell ... magic.heal ... }`) and replace it with one that sums blocks from `block_spell` AND `block_spell2`:

```cpp
        // ---- defense + magic economy: block the boss's bolts with block_spell (and, for a dual-element
        //      boss, block_spell2) — each block RECHARGES magic. D3: both Fire and Ice block+charge, so
        //      whichever element you're holding to cycle also refuels you. block_spell(2)==None -> skipped
        //      (D1 dodge-only; D2 Fire-only). One cast = one use (a blocked shot can't also expose).
        {
            constexpr int BLOCK_MAGIC_CHARGE = 25;
            int blocked = 0;
            if(level.boss->block_spell  != logic::SpellId::None) blocked += attacks.block_with_spell(spells, level.boss->block_spell);
            if(level.boss->block_spell2 != logic::SpellId::None) blocked += attacks.block_with_spell(spells, level.boss->block_spell2);
            if(blocked) magic.heal(BLOCK_MAGIC_CHARGE * blocked);
        }
```

- [ ] **Step 4: Build** — `bash tools/build_rom.sh` → `ROM fixed!` (D1/D2 unaffected: `expose_spell_alt`/`block_spell2` are `None` for them → `elem_base` 0, only `block_spell` blocks).

- [ ] **Step 5: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): run_room_boss — Coldforge sprite + element-aware 4-frame swap + dual-element block-charge (Fire+Ice); D1/D2 unaffected (M14 P3.2)"
```

**After Phase 3:** 3-round review (D1/D2/King untouched — all new paths gated on the None-defaulted fields). Banner → ✅.

---

## Phase 4 — D3 level restructure + integration + invariants + QA

**Execution Status:** ⬜ NOT STARTED

**Why this matters:** wires the Twins into a real D3 and proves no soft-locks. Order: compiler key → level files → dungeons.h → invariants → QA.

### Task 4.1: `build_level.py` boss key for D3

**Files:**
- Modify: `tools/build_level.py`

Follow the Per-Task Protocol.

- [ ] **Step 1: Add `'d3'`** to the `BOSS_SYMBOL` dict:

```python
BOSS_SYMBOL = {
    'd1': 'logic::D1_DEF',
    'd2': 'logic::D2_DEF',
    'd3': 'logic::D3_DEF',
}
```

- [ ] **Step 2: Commit** (verified when Task 4.2 compiles room 1):

```bash
git add tools/build_level.py
git commit -m "feat(tools): build_level.py boss key d3 -> logic::D3_DEF (M14 P4.1)"
```

### Task 4.2: D3 level restructure (3 rooms)

**Files:**
- Rename: `tools/levels/dungeon3.txt` → `dungeon3_room0.txt`; `dungeon3.json` → `dungeon3_room0.json`
- Create: `dungeon3_room1.txt` + `.json` (boss arena); `dungeon3_room2.txt` + `.json` (spronk)

**Context:** mirror D2 (`tools/levels/dungeon2_room0/1/2.*`). Char legend: `@`=spawn, `F`=ability shrine (json pickups), `C`=cage, `E`=exit, `N`=entrance (json), `D`=room-door (json), `Q`=hub-return (no json), `#`=wall, `w`=water hazard, `o`=enemy, and the D3 gate chars (`V`,`X`,`I`, etc.). **D3's room 0 already HAS a `Q`** (unlike D2, where one was added) — keep it.

Follow the Per-Task Protocol.

- [ ] **Step 1: Room 0** — `git mv tools/levels/dungeon3.txt tools/levels/dungeon3_room0.txt` and `.json` likewise. Edit `dungeon3_room0.txt`: **keep all existing Frost Hollow puzzle content + `@` + the existing `Q` + the `F` Ice shrine + the gates (`V`/`X`/`I`/etc.) + the water (`w`) tiles + enemies (`o`) — modify ONLY the cage/exit.** Find the `C` (cage) and `E` (exit) at the right end of the content row and **delete both** (replace each with `.`); place an `N` then a `D` at that same spot (entrance id 1 + room-door to room 1). **The `N`/`D` MUST sit over SOLID floor, not a water gap** — the old `C`/`E` were over solid floor (the far-right `#` floor region), so keep the `N`/`D` at that column; a player returning from room 1 respawns here and must not land in water. Edit `dungeon3_room0.json` — keep `enemies`/`pickups` and ADD:

```json
  "entrances": [ { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 1, "target_entrance": 0 } ]
```

The existing `Q` (hub-return, ~col 6) needs NO json entry — it's a hardcoded `target_room:-1` door and does not consume a `room_doors` slot, so the single `room_doors` entry above maps to the new `D` (this is the same Q-then-D scan-order arrangement D2's room 0 uses).

- [ ] **Step 2: Room 1 (boss arena)** — create `tools/levels/dungeon3_room1.txt`: a FLAT-floor arena **~48 wide × ~19 tall** (model on `dungeon2_room1.txt`), **NO water/spikes, NO crystal, NO `@`/`C`/`E`**, with `N`(id0,left)+`D`(→room0) on the left and `N`(id1,right)+`D`(→room2) on the right. Solid `#` border; floor on the bottom two rows. `dungeon3_room1.json`:

```json
{ "tileset": "tiles",
  "boss": "d3",
  "entrances": [ { "id": 0, "facing": 1 }, { "id": 1, "facing": -1 } ],
  "room_doors": [ { "target_room": 0, "target_entrance": 1 },
                  { "target_room": 2, "target_entrance": 0 } ] }
```

- [ ] **Step 3: Room 2 (spronk)** — create `tools/levels/dungeon3_room2.txt`: model on `dungeon2_room2.txt` — `N`(id0,left)+`D`(→room1), the `C` cage grounded on the floor, the `E` exit grounded, no hazards. `dungeon3_room2.json`:

```json
{ "tileset": "tiles",
  "entrances": [ { "id": 0, "facing": 1 } ],
  "room_doors": [ { "target_room": 1, "target_entrance": 1 } ] }
```

- [ ] **Step 4: Compile + verify** — `bash tools/build_rom.sh` (regenerates headers; `ROM fixed!`). Confirm `include/game/levels/dungeon3_room0.h`/`_room1.h`/`_room2.h` are generated and `dungeon3_room1.h` contains `&logic::D3_DEF`. Verify all rows of each new `.txt` are equal width (the compiler requires rectangular maps).

- [ ] **Step 5: Commit** (the `git mv` staged the renames):

```bash
git add tools/levels/dungeon3_room0.* tools/levels/dungeon3_room1.* tools/levels/dungeon3_room2.* include/game/levels/dungeon3_room*.h
git commit -m "content: restructure D3 into Frost-Hollow puzzle(+Ice shrine) -> Coldforge Twins arena -> spronk (M14 P4.2)"
```

### Task 4.3: `dungeons.h` — D3 3-room dungeon

**Files:**
- Modify: `include/game/levels/dungeons.h`

Follow the Per-Task Protocol.

- [ ] **Step 1: Swap the D3 include + dungeon** — replace `#include "game/levels/dungeon3.h"` with the three room headers, and replace the 1-room `DUNGEON3_ROOMS`/`DUNGEON3_DUNGEON` with the 3-room form (mirror the D2 block):

```cpp
#include "game/levels/dungeon3_room0.h"
#include "game/levels/dungeon3_room1.h"
#include "game/levels/dungeon3_room2.h"
```

```cpp
// DUNGEON3 — Frost Hollow (M14 restructure): room 0 = the Ice-spell puzzle (+ '@' spawn, hub-return
// 'Q', water<->ice); room 1 = the Coldforge Twins arena (D3_DEF, shifting Fire/Ice expose — fought on
// entry; victory opens the onward door); room 2 = the spronk + exit.
inline constexpr const logic::LevelData* DUNGEON3_ROOMS[] = {
    &DUNGEON3_ROOM0_DATA, &DUNGEON3_ROOM1_DATA, &DUNGEON3_ROOM2_DATA };
inline constexpr logic::DungeonData DUNGEON3_DUNGEON{ DUNGEON3_ROOMS, 3, 0 };
```

- [ ] **Step 2: Remove the orphaned header** — `git rm include/game/levels/dungeon3.h` (no longer regenerated since `dungeon3.txt` was renamed; nothing includes it after Step 1).

- [ ] **Step 3: Build** — `bash tools/build_rom.sh` → `ROM fixed!`.

- [ ] **Step 4: Commit**

```bash
git add include/game/levels/dungeons.h
git commit -m "feat(game): DUNGEON3 is now a 3-room dungeon (puzzle/boss/spronk); drop orphaned dungeon3.h (M14 P4.3)"
```

### Task 4.4: D3 no-soft-lock + structural invariants

**Files:**
- Create: `test/test_dungeon3_level.cpp`
- Modify: `test/test_d1_boss_respawn.cpp`

**Context:** mirror `test/test_dungeon2_level.cpp` exactly. **Copy its `static` helpers** (`D2Grid`/`build_grid`/`snap_start`/`reachable`/`stands_at`/`reaches_forward_exit`/`room_start_x`/`room_start_y`/`d2_tile` + `CLIMB`/`PW`/`PH`) into `test_dungeon3_level.cpp`, renamed to `D3Grid`/`d3_tile` (keep them `static` — both files link into one binary, so non-static duplicates are an ODR link error). Room 0 is a water↔ice puzzle room: like D2's room 0, it is NOT fully flood-fill-traversable (gates/blocks/water need spell-solving the harness can't model), so assert the **hub-return `Q` is reachable + the onward door exists structurally** (the accepted D2 deviation), and rely on emulator QA for the puzzle path.

Follow the Per-Task Protocol. **Assertion rigor:** each invariant MUST FAIL on a broken layout — verify non-vacuity (temporarily wall off the onward door in `dungeon3_room1.txt` → the reachability test goes RED → revert). Do NOT weaken an invariant to pass.

- [ ] **Step 1: Write the tests** `test/test_dungeon3_level.cpp` (includes: `test_framework.h`, `game/levels/dungeons.h`, `game/levels/dungeon3_room0.h`/`_room1.h`/`_room2.h`, `<vector>`/`<queue>`/`<cstdio>`; the copied static helpers; then):

```cpp
TEST(d3_dungeon_table){
    CHECK_EQ(DUNGEON3_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON3_DUNGEON.start_room, 0);
}
TEST(d3_room1_is_boss_arena_no_crystal){
    const LevelData& L = DUNGEON3_ROOM1_DATA;
    CHECK(L.boss == &D3_DEF);
    CHECK_EQ((int)L.boss->phase_count, 2);
    CHECK(!L.has_cage); CHECK(!L.has_exit);
    CHECK_EQ(L.magic_crystal_count, 0);   // magic comes from block-to-charge (Fire+Ice), not a crystal
}
TEST(d3_only_room1_has_boss){
    CHECK(DUNGEON3_ROOM0_DATA.boss == nullptr);
    CHECK(DUNGEON3_ROOM2_DATA.boss == nullptr);
}
TEST(d3_room0_has_ice_shrine_no_cage){
    const LevelData& L = DUNGEON3_ROOM0_DATA;
    CHECK(!L.has_cage);
    bool ice=false; for(int i=0;i<L.pickup_count;++i) if(L.pickups[i].ability==Ability::Ice) ice=true;
    CHECK(ice);   // Ice earned BEFORE the boss (needed to counter the fire head)
}
TEST(d3_room2_has_cage_and_exit){
    const LevelData& L = DUNGEON3_ROOM2_DATA;
    CHECK(L.has_cage); CHECK(L.has_exit);
}
TEST(d3_room1_onward_door_reachable){   // fail-on-broken
    const LevelData& L = DUNGEON3_ROOM1_DATA;
    D3Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    bool onward=false;
    for(int i=0;i<L.room_door_count;++i)
        if(L.room_doors[i].target_room==2 && stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) onward=true;
    CHECK(onward);
}
TEST(d3_room2_spronk_and_exit_reachable){
    const LevelData& L = DUNGEON3_ROOM2_DATA;
    D3Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, room_start_x(L), room_start_y(L), CLIMB);
    CHECK(stands_at(L, seen, L.cage_tx, L.cage_ty));
    CHECK(stands_at(L, seen, L.exit_tx, L.exit_ty));
}
// Room 0 is a spell-gated puzzle (not flood-fill-traversable): assert the hub-return is reachable
// from spawn + the onward door exists (the accepted D2 deviation).
TEST(d3_room0_hub_return_reachable_and_onward_exists){
    const LevelData& L = DUNGEON3_ROOM0_DATA;
    D3Grid g = build_grid(L);
    std::vector<uint8_t> seen = reachable(g, L.spawn_tx, L.spawn_ty, CLIMB);
    bool hub=false, onward=false;
    for(int i=0;i<L.room_door_count;++i){
        if(L.room_doors[i].target_room==-1 && stands_at(L, seen, L.room_doors[i].tx, L.room_doors[i].ty)) hub=true;
        if(L.room_doors[i].target_room==1) onward=true;   // structural existence
    }
    CHECK(hub); CHECK(onward);
}
```

- [ ] **Step 2: Run red→green** — `bash tools/host_test.sh`. If an invariant is RED, the LEVEL is wrong (fix the `.txt`, recompile via `bash tools/build_rom.sh`, re-run) — do NOT weaken the test. Verify non-vacuity: temporarily wall off the onward door in `dungeon3_room1.txt`, recompile, confirm `d3_room1_onward_door_reachable` goes RED, then revert.

- [ ] **Step 3: Extend the respawn-settle test to D3** — in `test/test_d1_boss_respawn.cpp`, add the three D3 room includes and a `d3_all_entrances_settle_on_floor` TEST mirroring `d2_all_entrances_settle_on_floor`:

```cpp
TEST(d3_all_entrances_settle_on_floor){
    check_entrance_settles(DUNGEON3_ROOM0_DATA, "d3-room0");
    check_entrance_settles(DUNGEON3_ROOM1_DATA, "d3-room1");
    check_entrance_settles(DUNGEON3_ROOM2_DATA, "d3-room2");
}
```

Run green.

- [ ] **Step 4: Commit**

```bash
git add test/test_dungeon3_level.cpp test/test_d1_boss_respawn.cpp
git commit -m "test(d3): no-soft-lock + structural invariants (boss arena no-crystal, spronk/exit reachable, room0 hub-return) + D3 respawn-settle (M14 P4.4)"
```

### Task 4.5: ROM build + emulator QA (handed to user)

- [ ] **Step 1: Final build + host suite** — `bash tools/host_test.sh` (all green incl. unchanged King/D1/D2), `python tools/check_logic_purity.py` (`logic purity OK`), `bash tools/build_rom.sh` (`ROM fixed!`).

- [ ] **Step 2: Emulator QA checklist** (hand `SpronkQuest.gba` to the user):
  - D3 room 0: solve Frost Hollow, earn Ice, reach the onward door (you own Fire + Ice).
  - The **active head reads clearly** (ice=blue / fire=red, one lit); casting the **counter** spell (Fire vs the ice head, Ice vs the fire head) exposes it; a bolt/Fire/Ice lands one wound.
  - The **head switches on each wound** — you must cycle `L` to the other element; the telegraph gives time to switch.
  - **Blocking with either Fire or Ice recharges magic**; magic never hard-soft-locks (death-restart refills).
  - Victory → onward door → free the spronk → reach the exit → **D3 cleared & saved** (+1 max life).
  - **Regression:** the King (Door 9), D1 Guardian, D2 Slagshell play exactly as before.

- [ ] **Step 3:** On QA pass, flip the banners → ✅ and proceed to the final review + `superpowers:finishing-a-development-branch`.

**After Phase 4:** 3-round review against spec §8. Banner → ✅.

---

## Self-Review (author checklist — completed)

- **Spec coverage:** §2.1 shifting expose → 1.1 (`cur_expose` + `expose_spell_alt`) + 3.1 (`resolve_damage`). §2.2 anti-spam → reuses `hit_iframes` (70) + the forced cycle; telegraphs ≥ SWITCH_BUDGET (1.1). §2.3 stationary + aimed/spiral → 1.1 (D3_DEF) + reused `run_room_boss`. §2.4 block-with-either → 1.1 (`block_spell2`) + 3.2. §3 art/frame → 2.1 + 3.2 (element-aware frame). §4 rooms → 4.1/4.2/4.3. §5 no save change → asserted (no SaveData task). §6 purity → Global Constraints + per-task checks. §7 testing → 1.1 (shift invariant + regression) / 4.4 (level) / 4.5 QA.
- **Placeholder scan:** none — concrete code/values throughout (tuning marked where relevant).
- **Type consistency:** `cur_expose` / `expose_spell_alt` / `block_spell2` / `D3_DEF` / `boss_sprite_for` / `elem_base` names used identically across tasks; `resolve_damage` uses `b.cur_expose` matching the BossState member added in 1.1.
