# Milestone 3 — Ember Caverns + Fire Spell Acceptance

Maps each Section from the M3 design to its verification. Status as of 2026-06-07
(branch `m3-ember-caverns`). Gameplay items marked **✅ mGBA** were playtested interactively.

| # | Criterion (design) | Status | Evidence |
|---|---|---|---|
| 1 | Spell system: B free bolt / R cast selected spell / L cycle | ✅ mGBA | `logic/spell.h` (`FireCast`, `SpellState`) + `engine/spell_input.h`; `test_spell.cpp` |
| 2 | Fire spell: costs magic, damages enemies, clears vine/ice gates, lights braziers | ✅ mGBA | `engine/fire_pool.h` split cast/hit/despawn API; ordered resolution in `scene_dungeon` |
| 3 | Magic economy: refill on bolt-kill only (NOT fire-kill); Fire is spendable | ✅ mGBA | `enemy` bolt-kill `magic.heal(25)`; fire-kill grants none; Fire cost **10** (rebalanced) |
| 4 | Mid-dungeon ability pickup (Fire shrine), generalized reward model | ✅ mGBA | `logic/ability_pickup.h`; D2 Fire shrine **and** D1 Featherleap shrine; grant is data-driven |
| 5 | Trigger→target puzzle framework (braziers / pressure-plate / hidden-button) | ✅ mGBA | `logic/puzzle.h` (`Trigger`); `TriggerInst` in scene maps source→target column |
| 6 | Pushable-block physics (push + gravity), scene-driven rate-limited push | ✅ mGBA | `logic/pushable_block.h`; `test_pushable_block.cpp`; push_cd rate-limit in scene |
| 7 | Lava hazard (health damage on overlap) | ✅ mGBA | `logic/hazard.h` `lava_overlap`; `TileKind::Lava`; 20 dmg + 45 i-frames |
| 8 | Fire-immune enemy variant (survives Fire, dies to bolt) | ✅ mGBA | `enemy.fire_immune` flag; D2 red enemy; `param2` bit0 in compiler/`EntitySpawn` |
| 9 | Vine + ice gates: render + Fire-clear | ✅ mGBA | `logic/fire_effect.h` `fire_clears_gate`; full-height columns filled/cleared in scene |
| 10 | Dungeon 2 (Ember Caverns) authored as data, Zelda-style arc | ✅ mGBA | `tools/levels/dungeon2.txt`+`.json` → `DUNGEON2_DATA`; `test_dungeon2_level.cpp` |
| 11 | Hub Dungeon-2 door becomes enterable after D1 cleared | ✅ mGBA | `scene_hub` `door_enterable()` (D2 enterable when `spronk_freed(1)`) |
| 12 | New placeholder art (tiles + sprites), distinct read | ✅ mGBA | `make_placeholder_art.py`: grey-bowl brazier, X-crate block, red button, fire/shrine |
| 13 | Additive `scene_dungeon` integration; three-layer purity preserved | ✅ | logic layer pure (`check_logic_purity.py` clean); 104/104 host |
| — | Persistent health/magic across hub↔dungeon (added per playtest) | ✅ mGBA | `logic/player_state.h` threaded through both scenes; hub renders the HUD |
| — | Real-hardware verification (flashcart) | ⏳ pending | inherited from M1/M2 — flash + confirm Ember Caverns on real GBA |

## Reward-model change (M3 Phase 4.3)
Abilities are no longer handed out by rescuing a spronk. **D1's Featherleap is now an `F`
shrine** on the floor near spawn (reachable without double-jump); the hardcoded
`world.grant(Featherleap)` on the spronk-free path was removed. Spronk rescue still gates
clearing the dungeon. Verified on a **fresh save**: shrine grants Featherleap → exit reachable
→ D2 unlocks. Continued saves that already own the ability start the shrine hidden.

## Notable fixes during M3 (see plan Discoveries)
- **Brazier hit-height:** a chest-height horizontal Fire shot flew over one-tile brazier bodies
  on the floor → braziers never lit → spronk soft-locked. Fixed with a tall (rows 14–19)
  invisible hit-body decoupled from the floor visual.
- **Plate over-trigger:** Laurel's 16px body AABB-overlapped the 8px plate while beside/above it
  (false opens + flicker). Fixed with a centre-point + `on_ground` sensor.
- **Hold-to-open plate:** the gate stays open only WHILE the plate is pressed, so the block (left
  resting on it) is required — the player can't be in two places at once.
- **Full-height gates:** player-sized 2×4 door regions were double-jumpable; all gates/walls are
  now full-height 2-wide columns, opened fully on clear.
- **Fire economy:** cost 25→10 so D2's 5 Fire obstacles are fundable by its 4 enemies.

## Deviations from plan
- **D2 size:** design called for "~8–12 screens"; the GBA 64×32-tile BG cap makes that impossible
  (~2 screens). D2 is a **dense 64×22 single-screen-tall gauntlet** threading all systems
  left→right. Acceptable for the framework-proving goal.

## Deferred (later milestones)
- **Audio** (victory sting on puzzle solve, SFX) — explicitly out of M3 scope.
- **Boss / Nightmare King** — not in M3.
- **Art polish** — placeholders read clearly but aren't final.
- **Real-hardware flash** — carry the M1/M2 discipline.

## Host test suite
`bash tools/host_test.sh` → **104/104 C++ tests + Python compiler tests, 0 failures**
(adds spell/FireCast, pushable block, puzzle trigger, ability pickup, lava hazard, fire-immune,
fire-clears-gate, dungeon2 level + D1 Featherleap-pickup on top of the M2 suite).
