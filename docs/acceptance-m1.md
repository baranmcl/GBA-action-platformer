# Milestone 1 — Vertical Slice Acceptance

Maps each Section-3 acceptance criterion from the design doc to its verification.
Status as of 2026-06-04 (branch `m1-vertical-slice`).

| # | Criterion (design §3) | Status | Evidence |
|---|---|---|---|
| 1 | Boots in mGBA at stable 60fps | ✅ mGBA | Boots to title; play scene runs smoothly in mGBA 0.9.3 |
| 2 | Platformer physics: gravity, jump, run, solid + one-way collision | ✅ | Host tests (physics/collision/player) + mGBA playtest |
| 3 | Laurel: walk, jump, magic bolt | ✅ | mGBA: D-pad run, A jump, B wand bolt |
| 4 | Health meter + magic meter HUD | ✅ | HUD pip rows (red health / cyan magic); host-tested `bar_pixels` |
| 5 | Featherleap earned (NOT pre-owned) | ✅ | Double-jump fails before rescue, works after — verified in mGBA |
| 6 | Short hub + Dungeon 1, one enemy, caged spronk | ✅ | 48-wide Dungeon 1 with patrolling enemy + caged spronk |
| 7 | Freeing spronk grants Featherleap + opens a gated exit | ✅ | Rescue dissolves full-height gate (collision+visual) + grants double-jump; exit reachable only after |
| 8 | Title screen + SRAM save | ✅ | Title (CONTINUE when saved); win writes checksummed SRAM; persists across reset (Ctrl+R) |
| 9 | Placeholder art | ✅ | Pillow-generated sprites + bg tiles (drop-in replaceable) |
| 10 | Three-layer architecture, no `bn::` in logic/ | ✅ | `check_logic_purity.py` guard green; 52/52 host tests |
| — | **Real-hardware verification (flashcart)** | ⏳ **pending** | Requires flashing `SpronkQuest.gba` to a GBA flashcart (user action) — see README |

## Combat / feel notes
- Jump tuned to a playtested midpoint (gravity raw 46, ~5.5-tile single jump).
- Enemy: bolt-kill refills magic; contact damages health with 45-tick i-frames (blink); empty health respawns at start.

## Host test suite
`bash tools/host_test.sh` → **52/52 tests passed, 0 checks failed** (fixed-point, vec2, tilemap,
physics, collision incl. one-way + anti-tunneling, player double-jump gating, meters, bolt,
world-state/SRAM POD, hud math, dungeon1 rescue, enemy patrol).

## Remaining for full M1 sign-off
- [ ] Flash to a real GBA via flashcart; confirm boot, 60fps feel, controls, sprite layer, and
      SRAM persistence across a true power cycle. Record any emulator-vs-hardware differences here.
