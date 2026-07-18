#include "game/room/pickups_system.h"
#include "game/room/room_util.h"

#include "bn_sprite_items_shrine.h"
#include "bn_sprite_items_heart_container.h"
#include "bn_sprite_items_health_pickup.h"
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
        // Ground the 16x16 spronk sprite on the floor-scanned row below the authored cage tile
        // (IMPL-5), so its bottom rests on the floor surface for both flat rooms and ledge cages.
        // Sprite half-height is 8 px, so centre = floor_surface - 8.
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
        // Ground the shrine on the floor-scanned row below the authored tile (same pattern as the
        // cage/heart above), so a shrine authored above a deeper floor doesn't embed in it.
        int shrine_fr = floor_row_below(ctx.lvl.map, p.tx, p.ty);
        si.sprite->set_position(wx(p.tx * 8 + 8), wy(shrine_fr * 8 - 8));
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
        // Ground the 16x16 sprite on the first solid tile below the authored row (floor-scanned,
        // IMPL-5), like the cage/exit/room-doors, so its bottom rests on the floor surface
        // regardless of authored floor depth. Floor surface is at fr*8; sprite half-height is 8,
        // so centre = fr*8 - 8.
        int hc_fr = floor_row_below(ctx.lvl.map, hc.tx, hc.ty);
        hi.sprite->set_position(wx(hc.tx * 8 + 8), wy(hc_fr * 8 - 8));
    }

    // ---- magic crystals (M10 Light: full-magic-refill pickup; respawns each attempt, NOT latched) ----
    crystals.spawn(level, cam, ctx.lvl.map, hw, hh, game::CrystalStation::Grounding::TileCentre,
                   /*respawn_when_depleted=*/false);   // collected stays gone until reset() (death/attempt reset only)

    // ---- health pickups (M14: one-shot full-HP restore, placed in a boss's approach room; NOT
    //      persisted -- re-appears on room re-entry, same respawn-per-visit semantics as the
    //      magic crystal above, not the heart container's SRAM latch). ----
    for(int i = 0; i < level.health_pickup_count && i < 4; ++i){
        const logic::HealthPickupSpawn& hp = level.health_pickups[i];
        _health_pickups.push_back(HealthPickupInst{ hp.tx, hp.ty, tile_body(hp.tx, hp.ty, 6, 8), {}, false });
        HealthPickupInst& hpi = _health_pickups.back();
        hpi.sprite = bn::sprite_items::health_pickup.create_sprite(0, 0);
        hpi.sprite->set_camera(cam);
        // Ground the sprite on the floor-scanned row below the authored tile (same pattern as
        // the shrine/heart/crystal above), so it doesn't embed in a deeper floor.
        int hp_fr = floor_row_below(ctx.lvl.map, hp.tx, hp.ty);
        hpi.sprite->set_position(wx(hp.tx * 8 + 8), wy(hp_fr * 8 - 8));
    }
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

void PickupsSystem::update_health_pickups(Ctx& ctx)
{
    // ---- health pickups: collect on overlap -> full HP restore. One-shot per room instance
    //      (the `collected` flag is per-spawn, not world-persisted); no write_world call --
    //      re-entering the room re-spawns it, same as a magic crystal. ----
    for(HealthPickupInst& hpi : _health_pickups){
        if(hpi.collected) continue;
        if(logic::aabb_overlap(ctx.player.body, hpi.body)){
            ctx.ps.health.cur = ctx.ps.health.max;
            hpi.collected = true;
            if(hpi.sprite) hpi.sprite->set_visible(false);
        }
    }
}

bool PickupsSystem::check_spronk_and_exit(Ctx& ctx)
{
    logic::World& world = ctx.world;
    const int d = world.current_dungeon;

    // ---- spronk rescue (marks the dungeon cleared; abilities now come from F pickups) ----
    if(_has_cage){
        bool was = world.spronk_freed(d);
        // Grounded-only, mirroring the exit's on_ground gate (no head-bump cheese): a tall cage on
        // a ledge can otherwise be overlapped by jumping up underneath the platform. Normal grabs
        // are grounded anyway (you walk/stand onto the cage), so this only blocks the mid-air
        // from-below grab.
        if(ctx.player.body.on_ground)
            logic::try_free_spronk(ctx.player.body, _cage, world, d);
        if(world.spronk_freed(d) && !was){
            if(_spronk) _spronk->set_visible(false);
            // Freeing the spronk grants +1 max (via spronks_freed) AND +1 current life (not a full refill).
            logic::gain_life(world);
            ctx.ps.health.cur = ctx.ps.health.max;   // spronk rescue restores vitals (recovery deferred here from boss-defeat)
            ctx.ps.magic.cur  = ctx.ps.magic.max;
            engine::write_world(world);   // persist the new max + granted life immediately
        }
    }

    bool spronk_ok = !_has_cage || world.spronk_freed(d);
    // Must LAND on the exit (grounded), not bump it from underneath — clearing requires
    // standing on the platform, which matters for the gated vertical climb (no head-bump cheese).
    return _has_exit && spronk_ok && ctx.player.body.on_ground && logic::aabb_overlap(ctx.player.body, _exit);
}
}
