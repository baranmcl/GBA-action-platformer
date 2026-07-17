#include "game/scene_boss.h"

#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_items_king.h"

#include "logic/boss.h"
#include "logic/world_state.h"   // max_health_for
#include "logic/player.h"
#include "logic/spell.h"         // SpellState::ensure_valid
#include "logic/room_graph.h"    // find_entrance
#include "engine/level_loader.h" // load_level, LoadedLevel
#include "game/boss_fight.h"     // run_boss_fight, FightOutcome

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
}

// run_boss — the Nightmare King fight. A thin caller-side wrapper (Task 5.3) around the
// shared run_boss_fight (Task 5.2): loads the arena, spawns the player at the entrance,
// primes the King's entry vitals (health cap + a full magic refill + a valid selected
// spell — the extra bits run_boss_fight does NOT do, see boss_fight.h), then hands off to
// the shared loop with inline_game_over=true (King semantics: the Game-Over scene runs
// INLINE and QuitToTitle is a real exit; Victory ends the fight).
BossResult run_boss(const logic::DungeonData& arena, logic::World& world, logic::PlayerState& ps)
{
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    const logic::LevelData& level = *arena.rooms[arena.start_room];
    engine::LoadedLevel lvl = engine::load_level(level);
    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);

    // ---- player spawn (entrance id 0) ----
    logic::EntranceSpawn ent = logic::find_entrance(level, 0);
    const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };
    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;
    player.facing = ent.facing;

    // Entry vitals that are the CALLER's job (run_boss_fight only does health.cur = health.max):
    // sync the health cap for the current world state, refill magic to full (the King fight starts
    // fresh, unlike a room boss which carries magic in from the prior room), and make sure the
    // selected spell is still one the player owns.
    ps.health.max = logic::max_health_for(world);
    ps.magic.cur = ps.magic.max;
    ps.spell.ensure_valid(world);

    FightOutcome fo = run_boss_fight(level, logic::KING_DEF, bn::sprite_items::king, world, ps, lvl,
                                     cam, player, spawn_pos, ent, /*inline_game_over=*/true);
    switch(fo){
        case FightOutcome::Victory:     return BossResult::Victory;
        case FightOutcome::QuitToTitle: return BossResult::QuitToTitle;
        case FightOutcome::GameOver:    // inline_game_over=true never actually returns this; defensive.
        default:                        return BossResult::QuitToTitle;
    }
}
}
