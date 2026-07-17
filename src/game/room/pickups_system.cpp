#include "game/room/pickups_system.h"
#include "game/room/room_util.h"

#include "bn_sprite_items_shrine.h"
#include "bn_sprite_items_heart_container.h"
#include "bn_sprite_items_spronk.h"

#include "logic/world_state.h"    // max_health_for, spronk_freed/free_spronk, refill_lives
#include "logic/spronk_rescue.h"  // try_free_spronk
#include "logic/spell.h"          // SpellState::ensure_valid
#include "engine/save.h"          // write_world

namespace game {

void PickupsSystem::spawn(const logic::LevelData& level, Ctx& ctx)
{
    const int hw = ctx.hw, hh = ctx.hh;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };
    bn::camera_ptr& cam = ctx.cam;
    logic::World& world = ctx.world;
    const int d = world.current_dungeon;

    // ---- cage / spronk ----
    _has_cage = level.has_cage;
    if(level.has_cage){
        _cage = tile_body(level.cage_tx, level.cage_ty, 8, 12);
        _spronk = bn::sprite_items::spronk.create_sprite(0, 0);
        _spronk->set_camera(cam);
        // Ground the 16x16 spronk sprite on the FIRST SOLID/one-way tile below the authored cage
        // row, so its BOTTOM rests on the floor surface (not embedded in it).  Sprite half-height
        // is 8 px, so centre = floor_surface - 8.  For the D1-D7 convention (cage_ty=18, floor at
        // row 20): floor_row_below(…,18)==20 → wy(20*8 - 8) == wy(cage.pos.y.to_int()+8) — no
        // visible regression.  For ledge cages (D8+ Room 2) the sprite now rests on the ledge
        // instead of being embedded in it.
        int cage_fr = floor_row_below(ctx.lvl.map, level.cage_tx, level.cage_ty);
        _spronk->set_position(wx(level.cage_tx * 8 + 8), wy(cage_fr * 8 - 8));
        _spronk->set_visible(!world.spronk_freed(d));
    }
    _has_exit = level.has_exit;
    if(level.has_exit) _exit = tile_body(level.exit_tx, level.exit_ty, 12, 12);

    // ---- ability shrines ----
    for(int i = 0; i < level.pickup_count && i < 4; ++i){
        const logic::AbilityPickup& p = level.pickups[i];
        _shrines.push_back(ShrineInst{ p, tile_body(p.tx, p.ty, 6, 8), {} });
        ShrineInst& si = _shrines.back();
        si.sprite = bn::sprite_items::shrine.create_sprite(0, 0);
        si.sprite->set_camera(cam);
        si.sprite->set_position(wx(p.tx * 8 + 8), wy(p.ty * 8 + 8));
        si.sprite->set_visible(!world.has(p.ability));   // already taken on a continued game
    }

    // ---- heart containers (permanent max-HP upgrade pickup) ----
    // Spawn only if NOT already collected (persisted in latches bits [24..31]); a collected
    // one stays hidden forever. Body matches the tile-sized shrine pickup; sprite grounded on
    // the content row exactly like the shrine (centre at tile-centre + 8).
    for(int i = 0; i < level.heart_container_count && i < 4; ++i){
        const logic::HeartContainerSpawn& hc = level.heart_containers[i];
        if(world.heart_container_collected(hc.id)) continue;  // already taken -> never show it
        _hearts.push_back(HeartInst{ hc, tile_body(hc.tx, hc.ty, 6, 8), {}, false });
        HeartInst& hi = _hearts.back();
        hi.sprite = bn::sprite_items::heart_container.create_sprite(0, 0);
        hi.sprite->set_camera(cam);
        // Ground the 16x16 sprite on the FIRST SOLID tile below the authored row (like the
        // cage/exit/room-doors), so its BOTTOM rests on the floor surface. M7 hard-coded a
        // tile-centre position assuming a row-18 floor-2-below layout; in D7's tight alcove the
        // floor is only 1 row below, so that sank the sprite INTO the platform. Floor surface is
        // at fr*8; sprite half-height is 8, so centre = fr*8 - 8.
        int hc_fr = floor_row_below(ctx.lvl.map, hc.tx, hc.ty);
        hi.sprite->set_position(wx(hc.tx * 8 + 8), wy(hc_fr * 8 - 8));
    }

    // ---- magic crystals (M10 Light: full-magic-refill pickup; respawns each attempt, NOT latched) ----
    crystals.spawn(level, cam, ctx.lvl.map, hw, hh, game::CrystalStation::Grounding::TileCentre,
                   /*respawn_when_depleted=*/false);   // collected stays gone until reset() (death/attempt reset only)
}

void PickupsSystem::update_shrines(Ctx& ctx)
{
    logic::World& world = ctx.world;
    logic::SpellState& spell = ctx.ps.spell;
    for(ShrineInst& si2 : _shrines){
        if(!world.has(si2.pk.ability) && logic::aabb_overlap(ctx.player.body, si2.body)){
            world.grant(si2.pk.ability);
            spell.ensure_valid(world);    // auto-select the new ability ONLY if nothing valid was selected; never clobber a cycled choice
            engine::write_world(world);   // persist the grant NOW — quit+power-cycle must not un-earn it (I33)
            if(si2.sprite) si2.sprite->set_visible(false);
        }
    }
}

void PickupsSystem::update_hearts_and_crystals(Ctx& ctx)
{
    logic::World& world = ctx.world;
    logic::Meter& health = ctx.ps.health;
    // ---- heart containers: collect on overlap -> grow max HP + refill to full, persist. ----
    for(HeartInst& hi : _hearts){
        if(hi.collected) continue;
        if(logic::aabb_overlap(ctx.player.body, hi.body)){
            world.collect_heart_container(hi.hc.id);
            health.max = logic::max_health_for(world);   // grow the cap (+25 per container)
            health.cur = health.max;                     // and refill to full — the payoff moment
            engine::write_world(world);                  // persist immediately (same path as latches)
            hi.collected = true;
            if(hi.sprite) hi.sprite->set_visible(false);
        }
    }

    // ---- magic crystals: collect on overlap -> full magic refill. NOT latched (resets each
    //      attempt below) so a Light beat never soft-locks on empty magic. ----
    crystals.update(ctx.player.body, ctx.ps.magic);
}

bool PickupsSystem::check_spronk_and_exit(Ctx& ctx)
{
    logic::World& world = ctx.world;
    const int d = world.current_dungeon;

    // ---- spronk rescue (marks the dungeon cleared; abilities now come from F pickups) ----
    if(_has_cage){
        bool was = world.spronk_freed(d);
        logic::try_free_spronk(ctx.player.body, _cage, world, d);
        if(world.spronk_freed(d) && !was){
            if(_spronk) _spronk->set_visible(false);
            logic::refill_lives(world);   // freeing the spronk grants +1 max (via spronks_freed) AND refills NOW (on pickup, not on exit)
            engine::write_world(world);   // persist the new max + refilled lives immediately
        }
    }

    bool spronk_ok = !_has_cage || world.spronk_freed(d);
    // Must LAND on the exit (grounded), not bump it from underneath — clearing requires
    // standing on the platform, which matters for the gated vertical climb (no head-bump cheese).
    return _has_exit && spronk_ok && ctx.player.body.on_ground && logic::aabb_overlap(ctx.player.body, _exit);
}
}
