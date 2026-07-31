#pragma once
#include "bn_camera_ptr.h"
#include "bn_sprite_item.h"

#include "logic/level_data.h"     // LevelData, EntranceSpawn
#include "logic/vec2.h"           // Vec2
#include "logic/boss.h"           // BossDef
#include "logic/world_state.h"    // World
#include "logic/player_state.h"   // PlayerState
#include "logic/player.h"         // Player
#include "engine/level_loader.h"  // LoadedLevel

// =============================================================================
// boss_fight — THE ONE shared, def-driven boss fight loop (Task 5.2). It folds
// the two near-clone ~350-line boss loops (scene_boss.cpp's run_boss / the King,
// and scene_dungeon.cpp's run_room_boss / D1-D3) into a single function whose
// per-boss variation is expressed ENTIRELY through logic::BossDef fields plus the
// one `inline_game_over` switch — NO `if(def == &SOMEDEF)` branches.
//
// The caller resolves everything scene-specific BEFORE calling:
//   * loads the level + sets the bg transparent colour,
//   * constructs `player` at the entrance and syncs ps.health.max,
//   * for the King (inline_game_over=true) refills magic + ps.spell.ensure_valid,
//     for a room boss (inline_game_over=false) leaves magic carried-in,
//   * passes the boss BODY sprite (scene_boss: bn::sprite_items::king;
//     scene_dungeon: boss_sprite_for(level.boss)).
//
// Game layer (bn:: allowed): renders the arena + boss + attacks and runs the
// per-frame loop. Deliberately UNCONSUMED this commit — Tasks 5.3/5.4 wire
// scene_boss / scene_dungeon into it and delete the two originals.
// =============================================================================

namespace game
{
// Three exits. Victory = boss defeated. GameOver = 0 lives in a ROOM boss (the
// dungeon flow owns the Game-Over scene). QuitToTitle = the King's INLINE
// Game-Over "Quit" choice (inline_game_over only).
enum class FightOutcome { Victory, GameOver, QuitToTitle };

// Run one full boss fight in the already-loaded arena `lvl` (camera `cam`).
//   def          — the WHOLE boss description (id / phases / block_mode / perches /
//                  dialogue). Sourced from KING_DEF (King) or *level.boss (room).
//   boss_sprite  — the boss body sprite item, resolved by the caller (see header note).
//   spawn_pos/ent— the player's entrance spawn (already found by the caller).
//   inline_game_over — true = King semantics (run run_game_over INLINE on 0 lives;
//                  Continue restarts, Quit returns QuitToTitle; never returns GameOver;
//                  soft fade-back on a lives-left restart; no exit-heal on Victory).
//                  false = room semantics (return GameOver on 0 lives; a lives-left
//                  restart replays the intro line; exit Victory at full HEALTH).
FightOutcome run_boss_fight(const logic::LevelData& level, const logic::BossDef& def,
                            const bn::sprite_item& boss_sprite, logic::World& world,
                            logic::PlayerState& ps, engine::LoadedLevel& lvl,
                            bn::camera_ptr& cam, logic::Player& player,
                            const logic::Vec2& spawn_pos, const logic::EntranceSpawn& ent,
                            bool inline_game_over);
}
