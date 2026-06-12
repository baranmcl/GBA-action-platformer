# M7 — Thornwild Marsh (Dungeon 6) + Vine Grapple Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build Dungeon 6 (Thornwild Marsh) and the Vine Grapple — a hookshot-pull that yanks Laurel to `GrapplePoint` anchor tiles (selected via the L-cycle, fired with R) plus one pull-block puzzle — as the first real multi-room dungeon on the room-to-room feature, and wire hub Door 6.

**Architecture:** Anchors are a new `TileKind::GrapplePoint` marker tile; the pull is a pure `GrappleState` in `Player::update` (mirrors Dash/wind, fully host-tested). Grapple joins the existing `SpellId` selection (L cycles Fire→Ice→Grapple; R fires the selected — grapple is free and auto-targets the nearest anchor). The dungeon is authored multi-room (branching + an SRAM-latched shortcut) using the shipped room-to-room primitives. Pull-blocks reuse `PushableBlock` slide logic driven from the scene.

**Tech Stack:** C++17 (three-layer: pure `logic/`, Butano `engine/`, scenes `game/`), Butano 21.6.0 (GBA), Python 3 level compiler (`tools/build_level.py`), host unit tests (`bash tools/host_test.sh`), ROM build (`bash tools/build_rom.sh`).

**Design spec:** `docs/superpowers/specs/2026-06-12-spronk-quest-m7-thornwild-marsh-design.md`

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

## Execution Status

**Overall:** 3/4 phases shipped (grapple logic + compiler + engine wiring done; 179/179 host + 33 Python tests green; ROM builds clean). Phase 4 (content + hub door + emulator QA) next.

| Phase | Status | Ship SHA(s) | Notes |
|---|---|---|---|
| 1 — Pure logic (anchors, spell-cycle, grapple, pull-block) | ✅ Shipped | `…a8e0117` (Tasks 1.1–1.6) | each spec+quality reviewed; 179/179 |
| 2 — Level compiler (anchor + pullable symbols) | ✅ Shipped | `3853e9d`, `34f23fb` | spec+quality reviewed; 33 Python tests |
| 3 — Engine/scene wiring (anchor art, vine VFX, R-branch, block-pull) | ✅ Shipped | `0851fb6`, `e16b7c0`, `fe58d36`, `45112fb` | Tasks 3.1–3.3; spec+quality reviewed; ROM builds clean |
| 4 — Content: Thornwild Marsh multi-room + hub Door 6 + manual QA | 🚧 In progress | — | claimed 2026-06-12, branch `feat/m7-thornwild-marsh`; needs ROM + emulator |

### Deviations
- _(none yet)_

### Discoveries
- **Placeholder art (track for a Phase 4 / future polish pass):** the grapple HUD icon reuses `bn::sprite_items::bolt` (cyan) instead of a dedicated vine/hook glyph (`scene_dungeon.cpp` `refresh_spell_icon`); the vine VFX during a pull is 4 small `bolt`-sprite dots lerped player→anchor, not a real rope (`scene_dungeon.cpp` vine block). Both are functional + clearly commented as placeholders. The anchor bg tile (index 25) IS real new art (green ring + crosshair via `make_placeholder_art.py`). Minor: the `gates.h` tile-map comment phrases "GrapplePoint kind 10" ambiguously (GrapplePoint is also a GateType); reword to "TileKind::GrapplePoint (=10)" during art polish.

---

## Conventions every task must follow

- **Three-layer rule** (`CLAUDE.md`): nothing under `include/logic/` or `src/logic/` may include or name a `bn::` type. Phases 1–2 stay host-testable. Phases 3–4 are the only `bn::` work.
- **Host tests:** `bash tools/host_test.sh` (NOT bare `make -C test`). A green run ends `N/N tests passed, 0 checks failed`. The script also runs `tools/check_logic_purity.py`, the Python compiler tests, and regenerates level headers before compiling.
- **ROM build (this Windows machine):** use `bash tools/build_rom.sh` (wraps the corrected devkitPro env; plain `make` fails because `DEVKITPRO` points at a non-existent path). A clean build prints `ROM fixed!` and produces `SpronkQuest.gba`.
- **Purity guard** before any commit touching logic: `python tools/check_logic_purity.py`.
- **Pitfalls:** read `docs/pitfalls/implementation-pitfalls.md` and `docs/pitfalls/testing-pitfalls.md` before each phase. Relevant traps: IMPL-2 (no float — fixed-point only; the grapple pull math MUST be integer fixed-point), IMPL-1 (no `bn::` in logic), TEST-2 (assert exact raw fixed values), TEST-1 (explicit per-tick stepping, no frame timing).
- **Room/camera constraint (from the room-to-room feature):** every authored room MUST be **≥30 tiles wide × ≥20 tall** (or the camera clamp degenerates and the player isn't framed) and use the **standard ~22-tall floor** (content row 18, floor row 20) so braziers are hittable. See `docs/plans/2026-06-11-room-to-room-dungeons-plan.md` Discoveries.

---

## Phase 1 — Pure logic: anchors, spell-cycle, grapple state, pull-block

**Execution Status:** ✅ SHIPPED on 2026-06-12 (Tasks 1.1–1.6; each spec + code-quality reviewed; 179/179 host tests green). Final commit `a8e0117`.

All pure C++17, host-tested. This is the testable crux of the milestone.

**BEFORE starting this phase:**
1. Invoke `superpowers:test-driven-development`.
2. Read `docs/pitfalls/testing-pitfalls.md` (TEST-2 exact fixed values; TEST-1 explicit dt) and `docs/pitfalls/implementation-pitfalls.md` (IMPL-2 no float).
Follow TDD: failing test → implement → green.

### Task 1.1: `TileKind::GrapplePoint` anchor tile

**Files:** Modify `include/logic/tilemap.h`. Test: `test/test_tilemap.cpp` (extend; create if absent).

- [ ] **Step 1: Write the failing test** (append to `test/test_tilemap.cpp`; if the file doesn't exist, create it with `#include "test_framework.h"` + `#include "logic/tilemap.h"` + `using namespace logic;`):

```cpp
TEST(grapple_point_tile_nonsolid){
    uint8_t cells[] = { (uint8_t)TileKind::GrapplePoint };
    Tilemap m{ 1, 1, cells };
    CHECK(m.at(0,0) == TileKind::GrapplePoint);
    CHECK(m.is_grapple_point(0,0));
    CHECK(!m.is_solid(0,0));     // anchors are non-solid (you pass through / latch to them)
    CHECK_EQ((int)TileKind::GrapplePoint, 10);   // next value after Spikes=9
}
```

- [ ] **Step 2: Run to verify it fails** — `bash tools/host_test.sh` — expect a compile failure (`GrapplePoint`/`is_grapple_point` undefined).

- [ ] **Step 3: Implement** in `include/logic/tilemap.h`: add `GrapplePoint=10` to the `TileKind` enum (after `Spikes=9`), update the enum's leading comment to mention it (non-solid latch anchor, M7), and add the query next to `is_spikes`:

```cpp
    bool is_grapple_point(int tx,int ty) const { return at(tx,ty)==TileKind::GrapplePoint; }
```

Do NOT add it to `is_solid` (it stays non-solid).

- [ ] **Step 4: Run to verify it passes** — `bash tools/host_test.sh` — expect green.

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/tilemap.h test/test_tilemap.cpp
git commit -m "feat(logic): add TileKind::GrapplePoint anchor tile (non-solid)"
```

### Task 1.2: `SpellId::Grapple` in the L-cycle

**Files:** Modify `include/logic/spell.h`. Test: `test/test_spell.cpp` (extend).

- [ ] **Step 1: Write the failing test** (append to `test/test_spell.cpp`):

```cpp
TEST(spell_cycle_includes_grapple){
    World w; w.grant(Ability::Fire); w.grant(Ability::Ice); w.grant(Ability::Grapple);
    SpellState s; s.refresh(w);
    CHECK(s.owns(w, SpellId::Grapple));
    // L cycles Fire -> Ice -> Grapple -> Fire among owned
    s.selected = SpellId::Fire;
    s.cycle(w); CHECK(s.selected == SpellId::Ice);
    s.cycle(w); CHECK(s.selected == SpellId::Grapple);
    s.cycle(w); CHECK(s.selected == SpellId::Fire);
}
TEST(grapple_not_owned_without_ability){
    World w; w.grant(Ability::Fire);
    SpellState s;
    CHECK(!s.owns(w, SpellId::Grapple));
    s.selected = SpellId::Fire; s.cycle(w);   // only Fire owned -> stays Fire
    CHECK(s.selected == SpellId::Fire);
}
```

- [ ] **Step 2: Run to verify it fails** — `bash tools/host_test.sh` — expect FAIL (`SpellId::Grapple` undefined; cycle excludes it).

- [ ] **Step 3: Implement** in `include/logic/spell.h`:
- Add `Grapple` to the enum: `enum class SpellId : uint8_t { None=0, Fire, Ice, Grapple };` (update the `// D5+: Light, Stone...` comment to note Grapple is the free traversal tool, not a magic spell).
- Extend `owns`: `(s==SpellId::Grapple && w.has(Ability::Grapple))`.
- Extend `has_any`: `|| owns(w,SpellId::Grapple)`.
- Extend `refresh`: after the Ice branch, `else if(owns(w,SpellId::Grapple)) selected = SpellId::Grapple;` (Fire/Ice still preferred as the default so existing dungeons behave the same).
- Extend `cycle` to rotate over all three owned in order Fire→Ice→Grapple:

```cpp
    void cycle(const World& w){
        if(!has_any(w)){ selected = SpellId::None; return; }
        SpellId order[3] = { SpellId::Fire, SpellId::Ice, SpellId::Grapple };
        int start = (selected==SpellId::Ice) ? 1 : (selected==SpellId::Grapple) ? 2 : 0;
        for(int i=1;i<=3;++i){ SpellId c = order[(start+i)%3]; if(owns(w,c)){ selected = c; return; } }
    }
```

> Grapple is in `SpellId` for the shared L-cycle/R-fire UI only; it is NOT a magic spell. The scene (Phase 3) branches on `selected==Grapple` to fire the grapple (no magic) vs cast Fire/Ice. `SpellCast` is never invoked for Grapple.

- [ ] **Step 4: Run to verify it passes** — `bash tools/host_test.sh`. The existing `spell.h` tests (Fire↔Ice cycle) must stay green — the 3-element cycle still rotates Fire→Ice→Fire when Grapple isn't owned.

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/spell.h test/test_spell.cpp
git commit -m "feat(logic): add SpellId::Grapple to the L-cycle (owned via Ability::Grapple)"
```

### Task 1.3: `nearest_grapple_anchor` scan (pure helper)

**Files:** Create `include/logic/grapple.h`. Test: `test/test_grapple.cpp` (create).

This is the targeting scan, split out as a pure free function so it's tested independently of the pull.

- [ ] **Step 1: Write the failing test** — create `test/test_grapple.cpp`:

```cpp
#include "test_framework.h"
#include "logic/grapple.h"
#include "logic/tilemap.h"
using namespace logic;

// Shared 12x9 all-Empty map with two GrapplePoint anchors on mid-row 4:
// (8,4) [ahead of a right-facing player] and (2,4) [behind]. Big enough for the 4-tile player.
static uint8_t G_CELLS[12*9];
static Tilemap make_map(){
    for(int i=0;i<12*9;++i) G_CELLS[i]=(uint8_t)TileKind::Empty;
    G_CELLS[4*12 + 8] = (uint8_t)TileKind::GrapplePoint;   // (8,4)
    G_CELLS[4*12 + 2] = (uint8_t)TileKind::GrapplePoint;   // (2,4)
    return Tilemap{ 12, 9, G_CELLS };
}

TEST(anchor_picks_nearest_ahead){
    Tilemap m = make_map();
    int ax, ay;
    // player at tile (5,4) facing right, range 6 -> ahead anchor (8,4) chosen (behind (2,4) excluded)
    CHECK(nearest_grapple_anchor(m, 5, 4, /*facing*/1, /*range*/6, ax, ay));
    CHECK_EQ(ax, 8); CHECK_EQ(ay, 4);
}
TEST(anchor_excludes_behind){
    Tilemap m = make_map();
    int ax, ay;
    // player at (10,4) facing right: (8,4) and (2,4) are both behind -> none targetable
    CHECK(!nearest_grapple_anchor(m, 10, 4, 1, 6, ax, ay));
}
TEST(anchor_respects_range){
    Tilemap m = make_map();
    int ax, ay;
    // player at (5,4) facing right, range 1 -> (8,4) is 3 away, out of range
    CHECK(!nearest_grapple_anchor(m, 5, 4, 1, 1, ax, ay));
}
TEST(anchor_above_is_targetable){
    uint8_t c[5*7]; for(int i=0;i<35;++i) c[i]=(uint8_t)TileKind::Empty;
    c[1*5 + 2] = (uint8_t)TileKind::GrapplePoint;   // (2,1) directly above (2,4)
    Tilemap m{ 5,7,c };
    int ax, ay;
    CHECK(nearest_grapple_anchor(m, 2, 4, /*facing*/1, 6, ax, ay));   // directly above -> targetable
    CHECK_EQ(ax,2); CHECK_EQ(ay,1);
}
```

- [ ] **Step 2: Run to verify it fails** — `bash tools/host_test.sh` — expect FAIL (`grapple.h` missing).

- [ ] **Step 3: Implement** `include/logic/grapple.h` (the scan only for this task; `GrappleState` added in 1.4):

```cpp
#pragma once
#include "logic/tilemap.h"
namespace logic {

// Find the nearest GrapplePoint anchor tile within Chebyshev `range` of the player tile,
// in the facing/up aim arc: an anchor is targetable unless it is strictly BEHIND the facing
// direction (anchors ahead, in the same column, or above are all allowed). Nearest by Manhattan
// distance. Returns false (out_* untouched meaningfully) if none.
inline bool nearest_grapple_anchor(const Tilemap& map, int ptx, int pty, int facing, int range,
                                   int& out_tx, int& out_ty){
    bool found = false; int best = 0;
    for(int ty = pty - range; ty <= pty + range; ++ty)
    for(int tx = ptx - range; tx <= ptx + range; ++tx){
        if(!map.is_grapple_point(tx, ty)) continue;
        int sx = (tx > ptx) - (tx < ptx);            // -1/0/+1 horizontal sign
        if(sx == -facing && tx != ptx) continue;     // exclude strictly-behind anchors
        int dist = (tx<ptx?ptx-tx:tx-ptx) + (ty<pty?pty-ty:ty-pty); // Manhattan
        if(!found || dist < best){ found = true; best = dist; out_tx = tx; out_ty = ty; }
    }
    return found;
}
}
```

- [ ] **Step 4: Run to verify it passes** — `bash tools/host_test.sh` — green.

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/grapple.h test/test_grapple.cpp
git commit -m "feat(logic): nearest_grapple_anchor scan (range + facing/up arc)"
```

### Task 1.4: `GrappleState` (latch + pull) + `Abilities::grapple` + `InputFrame.grapple_fire`

**Files:** Modify `include/logic/grapple.h` (add `GrappleState`), `include/logic/player.h` (`Abilities::grapple`, `InputFrame.grapple_fire`, a `GrappleState grapple` member). Test: `test/test_grapple.cpp` (extend).

The pull: while active, the player's velocity is set toward the anchor centre, per-axis clamped to `±GRAPPLE_VX`, so it converges (decelerates as it arrives) without sqrt. Player::update (Task 1.5) applies it + ends on arrival/wall.

- [ ] **Step 1: Write the failing test** (append to `test/test_grapple.cpp`):

```cpp
#include "logic/physics.h"   // Body
#include "logic/fixed.h"

// Body "at tile (tx,ty)" = top-left at (tx*8, ty*8). The 2x4-tile player's CENTRE is therefore
// at (tx*8+8, ty*8+16) = tile (tx+1, ty+2) — the latch scans from that centre tile.
static Body body_at_tile(int tx, int ty){
    Body b{}; b.half_w = Fixed::from_int(8); b.half_h = Fixed::from_int(16);
    b.pos = { Fixed::from_int(tx*8), Fixed::from_int(ty*8) };
    return b;
}

TEST(grapple_latches_only_with_ability_and_anchor){
    Tilemap m = make_map();                 // anchor at (8,4)
    Body b = body_at_tile(5,4);             // centre tile (6,6); anchor (8,4) is ahead + up, in range
    GrappleState g;
    CHECK(!g.latch(true, b, /*facing*/1, m, /*has*/false)); // no ability
    CHECK(!g.active());
    CHECK(g.latch(true, b, 1, m, /*has*/true));             // ability + anchor in arc -> latched
    CHECK(g.active());
}
TEST(grapple_no_latch_without_fire){
    Tilemap m = make_map(); Body b = body_at_tile(5,4); GrappleState g;
    CHECK(!g.latch(false, b, 1, m, true));   // not fired
    CHECK(!g.active());
}
TEST(grapple_pull_velocity_points_at_anchor){
    Tilemap m = make_map(); Body b = body_at_tile(5,4); GrappleState g;
    g.latch(true, b, 1, m, true);            // anchor (8,4) -> centre px (68,36); player centre (48,48)
    Vec2 v = g.pull_velocity(b);
    CHECK(v.x.raw > 0);                       // pulling right (anchor is right of player)
    CHECK(v.x <= GrappleState::GRAPPLE_VX);   // per-axis magnitude clamped to GRAPPLE_VX
}
TEST(grapple_ends_on_arrival){
    Tilemap m = make_map(); GrappleState g;
    Body b = body_at_tile(5,4);
    g.latch(true, b, 1, m, true);             // target centre = anchor (8,4) centre = (68,36)
    b.pos = { Fixed::from_int(60), Fixed::from_int(20) };  // body centre = (68,36) == anchor centre
    g.post(b, /*moved*/true);
    CHECK(!g.active());                        // arrived -> ended
}
TEST(grapple_ends_when_blocked){
    Tilemap m = make_map(); GrappleState g;
    Body b = body_at_tile(5,4);
    g.latch(true, b, 1, m, true);
    g.post(b, /*moved*/false);   // collision stopped the body before arrival
    CHECK(!g.active());
}
```

- [ ] **Step 2: Run to verify it fails** — `bash tools/host_test.sh` — expect FAIL (`GrappleState` undefined).

- [ ] **Step 3: Implement** — add `GrappleState` to `include/logic/grapple.h` (after the scan), `#include "logic/physics.h"` at the top:

```cpp
struct GrappleState {
    static constexpr int RANGE = 6;                          // tiles (feel-tunable)
    static constexpr Fixed GRAPPLE_VX = Fixed::from_int(5);  // ~5 px/frame per-axis pull (feel-tunable)
    bool pulling = false;
    int ax = 0, ay = 0;                                      // latched anchor tile

    bool active() const { return pulling; }

    // On a fire edge with the ability and an anchor in range, latch onto the nearest anchor.
    bool latch(bool fire, const Body& body, int facing, const Tilemap& map, bool has_grapple){
        if(!fire || pulling || !has_grapple) return false;
        int ptx = Tilemap::px_to_tile(body.pos.x + body.half_w);   // scan from the body CENTRE tile
        int pty = Tilemap::px_to_tile(body.pos.y + body.half_h);
        if(!nearest_grapple_anchor(map, ptx, pty, facing, RANGE, ax, ay)) return false;
        pulling = true; return true;
    }

    // Velocity toward the anchor tile centre, per-axis clamped to ±GRAPPLE_VX (converges near it).
    Vec2 pull_velocity(const Body& body) const {
        Fixed cx = body.pos.x + body.half_w, cy = body.pos.y + body.half_h;
        Fixed tx = Fixed::from_int(ax*8 + 4), ty = Fixed::from_int(ay*8 + 4);
        return { clamp_axis(tx - cx), clamp_axis(ty - cy) };
    }

    // End the pull when the body's CENTRE TILE reaches the anchor tile (tile-level arrival — robust
    // even when a floor prevents exact vertical alignment), or when blocked (the body didn't move).
    void post(const Body& body, bool moved){
        int ctx = Tilemap::px_to_tile(body.pos.x + body.half_w);
        int cty = Tilemap::px_to_tile(body.pos.y + body.half_h);
        if((ctx == ax && cty == ay) || !moved) pulling = false;
    }
private:
    static Fixed clamp_axis(Fixed d){
        if(d > GRAPPLE_VX) return GRAPPLE_VX;
        if(d < -GRAPPLE_VX) return -GRAPPLE_VX;     // Fixed has unary operator-
        return d;
    }
};
```

> IMPL-2: all integer fixed-point — no float. Verified in `include/logic/fixed.h`: `Fixed` has `from_int`/`from_raw` (both `constexpr`), `to_int`, `.raw`, full comparisons (`< > <= >= ==`), `+ - *` and unary `-` — so `static constexpr Fixed` and `-GRAPPLE_VX` are valid (a `static constexpr` member is implicitly inline in C++17; no out-of-class definition needed). `Vec2` is `{Fixed x, y}` (`include/logic/vec2.h`). Tile-based arrival avoids the floor-vs-overhead-anchor px-misalignment trap.

In `include/logic/player.h`: add `bool grapple = false;` to `struct Abilities`; add `bool grapple_fire = false;` to `struct InputFrame`; add `#include "logic/grapple.h"` and a `GrappleState grapple;` member to `struct Player` (next to `DashState dash;`).

- [ ] **Step 4: Run to verify it passes** — `bash tools/host_test.sh` — green.

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/grapple.h include/logic/player.h test/test_grapple.cpp
git commit -m "feat(logic): GrappleState latch+pull; Abilities::grapple; InputFrame.grapple_fire"
```

### Task 1.5: integrate the grapple pull into `Player::update`

**Files:** Modify `src/logic/player.cpp`. Test: `test/test_grapple.cpp` (extend with an integration test that steps `Player::update`).

- [ ] **Step 1: Write the failing test** (append to `test/test_grapple.cpp`):

```cpp
#include "logic/player.h"

TEST(player_update_pulls_toward_anchor){
    // 14x12 map: solid border + a floor at row 9; one anchor to the right at the player's height.
    uint8_t cells[14*12];
    for(int i=0;i<14*12;++i) cells[i]=(uint8_t)TileKind::Empty;
    for(int x=0;x<14;++x){ cells[0*14+x]=(uint8_t)TileKind::Solid; cells[11*14+x]=(uint8_t)TileKind::Solid; }
    for(int y=0;y<12;++y){ cells[y*14+0]=(uint8_t)TileKind::Solid; cells[y*14+13]=(uint8_t)TileKind::Solid; }
    for(int x=0;x<14;++x) cells[9*14+x]=(uint8_t)TileKind::Solid;          // floor row 9
    cells[7*14 + 9] = (uint8_t)TileKind::GrapplePoint;                     // anchor (9,7), right of player
    Tilemap m{ 14, 12, cells };

    Player p;
    p.body.half_w = Fixed::from_int(8); p.body.half_h = Fixed::from_int(16);
    p.body.pos = { Fixed::from_int(3*8), Fixed::from_int(5*8) };           // drop onto the floor
    p.facing = 1; p.abilities.grapple = true;

    InputFrame idle{};
    for(int i=0;i<20;++i) p.update(idle, m);                              // settle on the floor
    Fixed x0 = p.body.pos.x;
    InputFrame fire{}; fire.grapple_fire = true;
    p.update(fire, m);                                                    // latch + first pull step
    CHECK(p.body.pos.x > x0);                                             // moved right toward (9,7)
    InputFrame hold{};
    for(int i=0;i<40 && p.grapple.active(); ++i) p.update(hold, m);       // run pull to completion
    CHECK(!p.grapple.active());                                           // ended (centre tile reached anchor, or wall)
    CHECK(p.body.pos.x > x0);
}
```

- [ ] **Step 2: Run to verify it fails** — `bash tools/host_test.sh` — expect FAIL (grapple not integrated; player doesn't move on `grapple_fire`).

- [ ] **Step 3: Implement** in `src/logic/player.cpp` `Player::update`, AFTER the dash block (lines ~57-61) and BEFORE `move_and_collide` (line ~62):

```cpp
    // --- M7 vine grapple: latch onto an anchor and pull the body toward it; overrides
    // accel/friction/gravity/dash while active (applied last so it wins). ---
    grapple.latch(in.grapple_fire, body, facing, map, abilities.grapple);
    if(grapple.active()){
        body.vel = grapple.pull_velocity(body);
    }
    Vec2 grapple_prev = body.pos;
    move_and_collide(body, map);
    if(grapple.active()){
        bool moved = body.pos.x.raw != grapple_prev.x.raw || body.pos.y.raw != grapple_prev.y.raw;
        grapple.post(body, moved);
    }
```

Replace the existing bare `move_and_collide(body, map);` (line ~62) with the block above (it now includes the `move_and_collide` call). Do NOT call `move_and_collide` twice.

- [ ] **Step 4: Run to verify it passes** — `bash tools/host_test.sh` — green. Confirm the existing player/physics tests (jump, dash, wind, ice) stay green (grapple is inert when `grapple_fire` is false / `abilities.grapple` is false).

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add src/logic/player.cpp test/test_grapple.cpp
git commit -m "feat(logic): apply grapple pull in Player::update (override vel; end on arrival/wall)"
```

### Task 1.6: pull-block — `pullable` flag + reuse `PushableBlock::push`

**Files:** Modify `include/logic/level_data.h` (`BlockSpawn`), `include/logic/pushable_block.h` (a `pull` alias for clarity). Test: `test/test_pushable_block.cpp` (extend).

> DESIGN note: a "pull" is mechanically a one-tile slide TOWARD the player — identical to `push(dir)` with `dir = sign(player - block)`. We add a thin `pull(dir, map)` that delegates to the same slide (for call-site clarity and a focused test), and a `pullable` flag on the spawn so content marks which blocks the grapple can pull. The scene (Phase 3) computes the toward-player dir.

- [ ] **Step 1: Write the failing test** (append to `test/test_pushable_block.cpp`; if absent, create with the framework + `logic/pushable_block.h` includes):

```cpp
TEST(block_pull_slides_one_tile_toward_dir){
    uint8_t cells[5*3]; for(int i=0;i<15;++i) cells[i]=(uint8_t)logic::TileKind::Empty;
    // border-less open strip is fine; OOB is solid
    logic::Tilemap m{ 5,3, cells };
    logic::PushableBlock blk{ 3, 1 };
    CHECK(blk.pull(-1, m));     // pull left toward the player
    CHECK_EQ(blk.tx, 2);
    CHECK(blk.pull(-1, m));
    CHECK_EQ(blk.tx, 1);
}
TEST(block_pull_blocked_by_wall){
    uint8_t cells[3*3]; for(int i=0;i<9;++i) cells[i]=(uint8_t)logic::TileKind::Empty;
    cells[1*3 + 0] = (uint8_t)logic::TileKind::Solid;   // wall at (0,1)
    logic::Tilemap m{ 3,3, cells };
    logic::PushableBlock blk{ 1, 1 };
    CHECK(!blk.pull(-1, m));    // wall to the left -> no move
    CHECK_EQ(blk.tx, 1);
}
TEST(blockspawn_pullable_defaults_false){
    logic::BlockSpawn b{ 4, 5 };       // old 2-field literal still compiles
    CHECK(!b.pullable);
    logic::BlockSpawn b2{ 4, 5, true };
    CHECK(b2.pullable);
}
```

- [ ] **Step 2: Run to verify it fails** — `bash tools/host_test.sh` — FAIL (`pull`/`pullable` undefined).

- [ ] **Step 3: Implement**:
- `include/logic/pushable_block.h`: add a `pull` method (delegates to the same slide as `push`):

```cpp
    // Pull one tile toward `dir` (+1/-1, toward the grappling player) if the destination is non-solid.
    bool pull(int dir, const Tilemap& map){ return push(dir, map); }   // same slide, named for intent
```

- `include/logic/level_data.h`: add a TRAILING defaulted field to `BlockSpawn`: `struct BlockSpawn { int tx, ty; bool pullable = false; };` (trailing default keeps the generated `{0,0}` / `{tx,ty}` literals compiling — same trick as the room-to-room latch_id).

- [ ] **Step 4: Run to verify it passes** — `bash tools/host_test.sh` — green (existing block tests + generated `*_BLOCKS[]` literals still compile).

- [ ] **Step 5: Purity + commit**

```bash
python tools/check_logic_purity.py
git add include/logic/pushable_block.h include/logic/level_data.h test/test_pushable_block.cpp
git commit -m "feat(logic): PushableBlock::pull + BlockSpawn.pullable flag"
```

**BEFORE marking Phase 1 complete:**
1. Review tests vs `docs/pitfalls/testing-pitfalls.md` (TEST-2 exact fixed-point asserts; TEST-1 explicit per-frame stepping — the grapple integration test steps `update` frame by frame).
2. Verify edge cases: no-ability, no-anchor, behind-arc, out-of-range, arrival, wall-block, block-pull-blocked, pullable default.
3. `bash tools/host_test.sh` green; `python tools/check_logic_purity.py` clean.

**After completing Phase 1:** Review the batch from multiple perspectives. Minimum 3 review rounds. If round 3 still finds issues, keep going until clean.

---

## Phase 2 — Level compiler: anchor + pullable-block symbols

**Execution Status:** ✅ SHIPPED at `3853e9d`, `34f23fb` on 2026-06-12 (spec + code-quality reviewed; 33 Python compiler tests + 179/179 host tests green)

**BEFORE starting this phase:** invoke `superpowers:test-driven-development`; read `docs/pitfalls/implementation-pitfalls.md`. Compiler tests run via `cd tools && python -m unittest test_build_level.py` (or the whole `bash tools/host_test.sh`).

### Task 2.1: compile the `g` grapple-anchor tile and the `P` pullable-block symbol

**Files:** Modify `tools/build_level.py`. Test: `tools/test_build_level.py` (extend).

Read `tools/build_level.py` first (the `TILE` map, `CONTENT` set, the `B` block branch, `emit_header`). Current `BlockSpawn` emit is `{{{tx},{ty}}}`; it must now emit the `pullable` field.

- [ ] **Step 1: Write the failing tests** (append to `tools/test_build_level.py`):

```python
    def test_grapple_anchor_tile(self):
        txt = "#####\n#@g.#\n#####\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['tiles'][lvl['w']*1 + 2], 10)  # 'g' at (2,1) -> GrapplePoint=10 (non-solid)

    def test_pullable_block_symbol(self):
        txt = "######\n#@.P.#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['blocks'], [(3, 1, True)])     # 'P' -> pullable block (tx,ty,pullable)

    def test_plain_block_not_pullable(self):
        txt = "######\n#@.B.#\n######\n"
        lvl = compile_str(txt, {})
        self.assertEqual(lvl['blocks'], [(3, 1, False)])    # 'B' -> normal push block

    def test_emit_block_pullable_field(self):
        txt = "######\n#@.P.#\n######\n"
        lvl = compile_str(txt, {})
        hdr = build_level.emit_header(lvl, "TESTB")
        self.assertIn("{3,1,true}", hdr)                    # BlockSpawn emits the pullable bool
```

- [ ] **Step 2: Run to verify it fails** — `cd tools && python -m unittest test_build_level.py` — FAIL (unknown `g`/`P`; blocks are 2-tuples).

- [ ] **Step 3: Implement** in `tools/build_level.py`:
- Add `'g': 10` to the `TILE` map (grapple anchor — non-solid, like wind tiles) and document it in the docstring grid legend.
- Add `'P'` to the `CONTENT` set and the docstring (pullable block). Make `blocks` 3-tuples `(tx, ty, pullable)`: the existing `B` branch appends `(x, y, False)`; add a `P` branch appending `(x, y, True)`.
- Update `emit_header`'s `BLOCKS` emit to the 3-tuple with a C++ bool: `[f'{{{tx},{ty},{"true" if pull else "false"}}}' for (tx,ty,pull) in level['blocks']]` and dummy `'{0,0,false}'`.

- [ ] **Step 4: Fix the pre-existing block test** — making `blocks` 3-tuples breaks any existing assertion that expects 2-tuples (e.g. `test_block_plate_button_brazier` asserts `lvl['blocks'] == [(2, 1)]`). Update it to `[(2, 1, False)]`.

- [ ] **Step 5: Run to verify it passes** — `cd tools && python -m unittest test_build_level.py` — OK. Then `bash tools/host_test.sh` — the regenerated level headers (with 3-tuple blocks) must still compile against the Phase 1 `BlockSpawn`; expect `N/N tests passed, 0 checks failed`.

- [ ] **Step 6: Commit (regenerated headers included)**

```bash
git add tools/build_level.py tools/test_build_level.py include/game/levels/*.h
git commit -m "feat(tools): compile 'g' grapple-anchor tile + 'P' pullable block"
```

**BEFORE marking Phase 2 complete:** review vs testing-pitfalls; confirm the missing-pullable default and the `g` non-solid mapping; `bash tools/host_test.sh` green AND regenerated headers compile.

**After completing Phase 2:** Minimum 3 review rounds.

---

## Phase 3 — Engine/scene wiring (anchor art, vine VFX, R-branch, block-pull)

**Execution Status:** ✅ SHIPPED at `0851fb6`, `e16b7c0`, `fe58d36`, `45112fb` on 2026-06-12 (Tasks 3.1–3.3; spec + code-quality reviewed; ROM builds clean; 179/179 host tests). HUD icon + vine VFX are placeholders — see Discoveries.

The Butano layer — the only `bn::` phase. Not host-testable; verified by ROM compile + Phase 4 manual QA. Do NOT pull `bn::` into the logic layer to "test" the scene.

**BEFORE starting this phase:** invoke `superpowers:test-driven-development` (for the seams); read both pitfalls docs; re-read `src/game/scene_dungeon.cpp` (the spell/cast section ~252-254, the gate/brazier build, the block section ~187-195 + ~323-347) and `include/engine/level_view.cpp` (the kind→bg map ~31-33).

### Task 3.1: grapple-anchor bg tile + kind→bg map + HUD grapple icon

**Execution Status:** ✅ SHIPPED 2026-06-12, commit `0851fb6`.

**Files:** Modify `src/engine/level_view.cpp` (kind→bg), the tileset art (`graphics/tiles.bmp` is 25 tiles 0-24; a `GrapplePoint` anchor needs a tile — reuse an existing readable tile OR extend the tileset), `src/game/scene_dungeon.cpp` (HUD icon for selected Grapple). 

- [x] **Step 1: Anchor bg tile (MANDATORY mapping).** Added `(kind == 10) ? 25 :` to the kind→bg chain in `level_view.cpp`. Extended `graphics/tiles.bmp` from 25 to 26 tiles (200→208 px) via `tools/make_placeholder_art.py` — tile 25 is a green ring with white crosshair (distinct from all flame art). Updated `gates.h` tile-map comment to document index 25 = grapple-anchor.

- [x] **Step 2: HUD grapple icon.** Extended `refresh_spell_icon` lambda to handle `SpellId::Grapple` by showing `bn::sprite_items::bolt` (cyan bolt sprite — distinct from fire's orange and ice's cyan-white). Added `#include "bn_sprite_items_bolt.h"` to `scene_dungeon.cpp`. (Placeholder — a dedicated vine/hook sprite is a polish follow-up for Phase 4 QA.)

- [x] **Step 3: Build** `bash tools/build_rom.sh` — clean (`ROM fixed!`). Host tests: 179/179 green.

- [x] **Step 4: Commit** — `0851fb6`

> Art is the soft part here. If extending the tileset is heavy, reuse a readable existing tile for the anchor in this milestone and note it as a polish follow-up — do NOT block the mechanic on final art (the M6 design used placeholder art the same way).

### Task 3.2: scene R-branch (grapple vs cast) + `grapple_fire` + vine VFX

**Files:** Modify `src/game/scene_dungeon.cpp`.

- [ ] **Step 1: Wire ability + R branch (REQUIRES REORDERING the spell-intent read).** Currently the loop order is: `in = read_input()` (~241) → set abilities (~242-244) → `player.update(in, lvl.map)` (~245) → … → `si = read_spell_intent(); if(si.cycle) spell.cycle(world); spells.update_and_cast(si.cast, …)` (~252-254). Because `in.grapple_fire` depends on the CURRENT spell selection, you MUST **move the spell-intent read + cycle to BEFORE `player.update`**. New order:

```cpp
        logic::InputFrame in = engine::read_input();              // (~241, unchanged)
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.abilities.glide       = world.has(logic::Ability::Glide);
        player.abilities.dash        = world.has(logic::Ability::Dash);
        player.abilities.grapple     = world.has(logic::Ability::Grapple);   // NEW
        // Read spell intent + cycle FIRST so the selection is current for the grapple/cast branch:
        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);
        // R fires the SELECTED tool: Grapple -> the player's grapple pull (free); Fire/Ice -> cast.
        in.grapple_fire = si.cast && spell.selected == logic::SpellId::Grapple;
        bool cast_spell = si.cast && (spell.selected == logic::SpellId::Fire ||
                                      spell.selected == logic::SpellId::Ice);
        player.update(in, lvl.map);                               // consumes in.grapple_fire
        // ... (avatar.sync, bolts, etc. as before) ...
        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map); // real spells only
```

Delete the old `si = read_spell_intent(); if(si.cycle) spell.cycle(world);` lines from their old location (~252-253) — they moved up. The spell-pool call (~254) now takes `cast_spell` (not `si.cast`) so Grapple never spends magic or spawns a projectile. Keep `refresh_spell_icon()` and the rest of the loop body intact (just the cast value and the intent-read position change).

- [ ] **Step 2: Vine VFX.** While `player.grapple.active()`, draw a vine line/sprite from the player to the anchor (`player.grapple.tx_px/ty_px`). Use a stretched/tiled sprite or a series of small sprites along the line (no Butano line primitive). Keep it simple — a few segment sprites or one scaled sprite — and hide it when `!active()`. This is cosmetic; a minimal visible vine is sufficient for QA.

- [ ] **Step 3: Build** `bash tools/build_rom.sh` — clean.

- [ ] **Step 4: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): R fires grapple when selected (else cast); vine VFX during pull"
```

### Task 3.3: scene block-pull on grapple fire

**Files:** Modify `src/game/scene_dungeon.cpp`.

- [ ] **Step 1: Block-pull branch (refines the Task 3.2 R-branch — same code region).** The grapple has dual purpose: pull the player to an anchor OR pull a pullable block toward the player. Blocks are scene entities (collision cells), so resolve block-pull in the scene. Restructure the Task 3.2 branch so that when `si.cast && spell.selected==Grapple`: FIRST check each `BlockInst` whose `blk.pullable` is true and which is within `GrappleState::RANGE` tiles in the facing/up arc (reuse `logic::nearest_grapple_anchor`'s arc rule, or a small inline check) and roughly aligned; if one is found, pull it one tile toward the player (`bi.blk.pull(dir_toward_player, lvl.map)` + update the collision cells exactly as the push code does ~333-336) and set `in.grapple_fire = false` (the grapple is consumed by the block, not the player). Else `in.grapple_fire = true` for the player anchor-grapple. (So Task 3.2's `in.grapple_fire = si.cast && selected==Grapple` becomes the "else" of the block check.)

> Track `pullable` on `BlockInst` (add a `bool pullable` field, set from `level.blocks[i].pullable` at build ~187-195). The toward-player dir = `sign(player.body.pos.x - block px)`. Add a push/pull cooldown like the existing `push_cd` so one R press pulls one tile, not many.

- [ ] **Step 2: Build** `bash tools/build_rom.sh` — clean.

- [ ] **Step 3: Commit**

```bash
git add src/game/scene_dungeon.cpp
git commit -m "feat(game): grapple pulls a pullable block one tile toward the player"
```

**BEFORE marking Phase 3 complete:**
1. Confirm no `bn::` leaked into `include/logic/` (`python tools/check_logic_purity.py`).
2. `bash tools/build_rom.sh` clean; `bash tools/host_test.sh` still green (Phases 1-2 unaffected).
3. Re-read the R-branch: Fire/Ice still cast (magic) when selected; Grapple never spends magic; existing dungeons (no Grapple owned) are unaffected (selected is never Grapple).

**After completing Phase 3:** Minimum 3 review rounds (did the R-branch break casting? does block-pull mis-fire? is the vine hidden when inactive?).

---

## Phase 4 — Content: Thornwild Marsh multi-room + hub Door 6 + manual QA

**Execution Status:** 🚧 IN PROGRESS — Task 4.1 (3-room dungeon, `07485be`) + Task 4.2 (hub Door 6, `c768627`) SHIPPED + wiring-reviewed; 190/190 host tests; ROM builds clean. Task 4.3 (interactive emulator QA) handed to the user. QA reachability via a TEMP uncommitted `src/main.cpp` scaffold (grants carried kit + frees D1–D5 spronks so Door 6 is reachable + solvable); REVERT before the final commit.

**BEFORE starting this phase:** read `docs/pitfalls/implementation-pitfalls.md`; re-read the room-to-room authoring (the d6 rooms in `tools/levels/`, `include/game/levels/dungeons.h`, and the **room/camera constraint**: every room ≥30×20, standard ~22-tall floor).

### Task 4.1: author the Thornwild Marsh multi-room dungeon (replaces the d6 reference)

**Files:** Create `tools/levels/dungeon6_room0.txt`+`.json` … (4–5 rooms) + generated headers; modify `include/game/levels/dungeons.h` (`DUNGEON6_DUNGEON` → the real room set); remove the throwaway `d6_room0/1` reference rooms.

**Functional requirements** (exact tile layout finalized by the author in the emulator; the compiler enforces rectangular ≥30×20 rooms + solid border + standard floor):
- **4–5 rooms**, branching + a **latched shortcut**. Entry room (room 0) branches via two room-doors. One branch's grapple puzzle lights a **brazier-group** (`latch_id`) opening a **shortcut room-door back toward room 0** (persisted to SRAM). The other branch holds the **spronk (`C`) + exit (`E`)**, grapple-gated.
- **Grapple shrine** (`F` + `"ability":"grapple"`) early (room 0 or just past it), before any grapple-gated beat (soft-lock guard).
- At least: **≥2 grapple-anchor (`g`) traversal beats** (one cross-gap, one pull-up-to-ledge); the **one pull-block (`P`) puzzle** (pull a heavy block to bridge a gap you can't reach to push); **≥1 carried-kit combo beat** (e.g. freeze water with Ice then grapple, or glide→grapple); a **bolt-enemy (`o`) before any spell beat** (magic funding).
- Wire `DUNGEON6_DUNGEON` in `dungeons.h` to the real room pointers; delete the `d6_room0/1.*` files (`tools/levels/d6_room0.txt`/`.json`, `d6_room1.txt`/`.json`) + their generated `include/game/levels/d6_room*.h` + the old `DUNGEON6_ROOMS/DUNGEON6_DUNGEON` block, and any `test/test_d6*_level.cpp` if one exists (`git grep -l d6_room test/`), replacing with the new set.
- Compile each room (`python tools/build_level.py tools/levels/dungeon6_roomN.txt include/game/levels/dungeon6_roomN.h`); host tests regenerate them.
- Author conservatively per the room-to-room QA lessons: content on **row 18**, floor row 20, rooms ≥30×20; keep grapple anchors reachable; keep the pull-block puzzle death-recoverable.

- [ ] Build (`bash tools/build_rom.sh`) clean; host tests green (add a `test/test_dungeon6_level.cpp` mirroring the other `*_level.cpp`: room count, entrances/doors resolve, ≥1 latched shortcut, exactly one Grapple shrine + spronk + exit, ≥1 `g` anchor, ≥1 `P` pullable block, ≥1 carried-kit obstacle, every room ≥30×20 + solid border).
- [ ] Commit the content + the d6-reference removal.

### Task 4.2: hub Door 6

**Files:** Modify `tools/levels/hub.txt`+`.json` (add a door-6 archway), `src/game/scene_hub.cpp` (`door_enterable`: Door 6 enterable once D5's spronk is freed), `src/main.cpp` (map `n==6` → `&DUNGEON6_DUNGEON`).

- [ ] Add door `6` to `hub.txt` at a sensible plaza spot (regenerate `hub.h`); extend `door_enterable` so door 6 unlocks when `world.spronk_freed(5)` (mirror how door 5 unlocks on D4); map `n==6` in `main.cpp`'s dungeon dispatch. Build clean; host tests green (the hub level test may need the new door count).
- [ ] Commit.

### Task 4.3: manual QA on mGBA (the runtime verification)

Build the ROM (`bash tools/build_rom.sh`) and load `SpronkQuest.gba` in mGBA. With a **fresh save**, clear D1–D5 (or use a save that has — note: reaching D6 legitimately requires D5 cleared; for QA you MAY temporarily grant the prerequisites, but REVERT any such temp hack before the final commit, as in the room-to-room QA). Verify and record results:
1. Door 6 is **locked until D5's spronk is freed**, then enterable.
2. **Grapple shrine** grants Grapple; **L cycles** to Grapple (HUD icon shows it); **R fires** the grapple — auto-latches to the nearest anchor and **pulls Laurel across a gap** and **up to an overhead ledge**.
3. With Fire/Ice selected, **R still casts** (magic) — grapple never spends magic.
4. The **pull-block** puzzle: grapple a `P` block, it slides one tile toward you; solve the bridge.
5. The **branching + latched shortcut**: solve branch A's puzzle → shortcut opens; re-enter → still open; **power-cycle (keep save)** → still open (SRAM).
6. A **carried-kit combo beat** works (freeze→grapple or glide→grapple).
7. **Spronk → exit** clears the dungeon.
8. Record pass/fail per item; file any failure under Discoveries and fix before closing.

**BEFORE marking Phase 4 complete:** all QA items pass with evidence; `bash tools/host_test.sh` green; `bash tools/build_rom.sh` clean; any temp QA prerequisite-grant reverted (confirm `git status` clean of it).

**After completing Phase 4:** Minimum 3 review rounds.

---

## Final self-review (run before declaring the plan done)

- [ ] **Spec coverage:** grapple mechanic (P1.3–1.5), L-cycle control (P1.2 + P3.2), anchors as tiles (P1.1+P2+P3.1), pull-block (P1.6+P2+P3.3), multi-room dungeon (P4.1), hub Door 6 (P4.2), carried-kit + soft-lock guards (P4.1), testing (P1–P2 host + P4.3 manual), future track (recorded in the spec + memory, not built). ✅
- [ ] **Three-layer rule:** P1–P2 pure/Python; P3 the only `bn::` work; P3 forbids leaking `bn::` into logic. ✅
- [ ] **No float:** grapple pull math is integer fixed-point (P1.4 IMPL-2). ✅
- [ ] **Type consistency:** `GrappleState`/`nearest_grapple_anchor`/`grapple_fire`/`Abilities::grapple`/`SpellId::Grapple`/`GRAPPLE_VX`/`RANGE`/`BlockSpawn.pullable`/`PushableBlock::pull` used identically across tasks. ✅
- [ ] **Room constraint:** P4.1 reiterates ≥30×20 + standard floor (camera + brazier hit-body). ✅
