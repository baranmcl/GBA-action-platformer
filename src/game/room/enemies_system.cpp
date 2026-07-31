#include "game/room/enemies_system.h"

#include "bn_sprite_items_enemy.h"
#include "bn_sprite_items_fire_enemy.h"

#include "logic/combat_rules.h"  // KILL_MAGIC_REFILL, try_hit
#include "logic/collision.h"     // aabb_overlap

namespace game {
namespace {
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
}

void EnemiesSystem::spawn(const logic::LevelData& level, Ctx& ctx)
{
    // ---- enemies: type decoded from param2 (I10 seam) ----
    for(int i = 0; i < level.enemy_count && i < 8; ++i){
        const logic::EntitySpawn& s = level.enemies[i];
        _enemies.push_back(EnemyInst{});
        EnemyInst& inst = _enemies.back();
        inst.e.body.half_w = fx(8); inst.e.body.half_h = fx(8);
        inst.e.body.pos = { fx(s.tx * 8), fx(s.ty * 8) };
        inst.e.left_bound = fx(s.param0 * 8); inst.e.right_bound = fx(s.param1 * 8);

        // The ONE place a future enemy type (flyer, shooter, ...) adds a row: set
        // fire_immune + pick the sprite per type. Behavior for the two existing
        // types is unchanged from the old `(s.param2 & 1)` ternary.
        switch(logic::enemy_type_from_params(s.param2)){
            case logic::EnemyType::PatrollerFireImmune:
                inst.e.fire_immune = true;
                inst.sprite = bn::sprite_items::fire_enemy.create_sprite(0, 0);
                break;
            case logic::EnemyType::Patroller:
            default:
                inst.e.fire_immune = false;
                inst.sprite = bn::sprite_items::enemy.create_sprite(0, 0);
                break;
        }
        inst.sprite->set_camera(ctx.cam);
    }
}

void EnemiesSystem::update(Ctx& ctx, engine::BoltPool& bolts, engine::SpellPool& spells)
{
    logic::Player& player = ctx.player;
    logic::Meter& health = ctx.ps.health;
    logic::Meter& magic  = ctx.ps.magic;
    const int hw = ctx.hw, hh = ctx.hh;

    // ---- enemies: patrol, render, bolt-kill(+magic), fire-kill(no magic unless immune), contact ----
    for(EnemyInst& inst : _enemies){
        inst.e.update(ctx.lvl.map);
        if(!inst.e.alive) continue;
        int ex = inst.e.body.pos.x.to_int() + inst.e.body.half_w.to_int();
        int ey = inst.e.body.pos.y.to_int() + inst.e.body.half_h.to_int();
        inst.sprite->set_position(ex - hw, ey - hh);
        if(player.stone.active() && logic::aabb_overlap(player.body, inst.e.body)){
            // M8: a pound CRUSHES any enemy on contact (including fire_immune), refilling magic
            // like a bolt-kill. Guarded by stone.active() (pound i-frames), parallel to dash i-frames.
            inst.e.kill(); magic.heal(logic::KILL_MAGIC_REFILL); inst.sprite->set_visible(false);
        } else if(bolts.consume_hit(inst.e.body)){
            inst.e.kill(); magic.heal(logic::KILL_MAGIC_REFILL); inst.sprite->set_visible(false);
        } else if(spells.consume_hit(inst.e.body, logic::SpellId::Fire)){
            if(!inst.e.fire_immune){ inst.e.kill(); inst.sprite->set_visible(false); } // no magic refill from fire
        } else {
            logic::try_hit(health, ctx.invuln, player.dash.invincible(), logic::aabb_overlap(player.body, inst.e.body));  // dash i-frames blink through contact
        }
    }
}
}
