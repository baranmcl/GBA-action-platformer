#include "game/room/terrain_system.h"
#include "game/room/room_util.h"

#include "bn_sprite_items_block.h"

#include "logic/tilemap.h"       // Tilemap::px_to_tile
#include "logic/stone_impact.h"  // loose_platform_in_shockwave
#include "engine/level_loader.h" // set_collision_tile

namespace game {

void TerrainSystem::spawn(const logic::LevelData& level, Ctx& ctx)
{
    const int hw = ctx.hw, hh = ctx.hh;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };
    bn::camera_ptr& cam = ctx.cam;

    // ---- pushable blocks (solid collision cell + 8x8 sprite) ----
    for(int i = 0; i < level.block_count && i < 8; ++i){
        const logic::BlockSpawn& b = level.blocks[i];
        _blocks.push_back(BlockInst{ logic::PushableBlock{ b.tx, b.ty }, {}, b.pullable });
        BlockInst& bi = _blocks.back();
        bi.spawn_tx = b.tx; bi.spawn_ty = b.ty;   // cached for reset_blocks (see BlockInst comment)
        engine::set_collision_tile(b.tx, b.ty, 1);       // block is solid; bg stays blank, sprite shows it
        bi.sprite = bn::sprite_items::block.create_sprite(0, 0);
        bi.sprite->set_camera(cam);
    }

    // ---- boulders (M8 Stone: breakable solid; like a block but NOT pushable; pound removes it) ----
    for(int i = 0; i < level.boulder_count && i < 8; ++i){
        const logic::BoulderSpawn& b = level.boulders[i];
        _boulders.push_back(BoulderInst{ b.tx, b.ty, {}, false });
        BoulderInst& bo = _boulders.back();
        engine::set_collision_tile(b.tx, b.ty, 1);                // solid; bg stays blank, sprite shows it
        bo.sprite = bn::sprite_items::block.create_sprite(0, 0);  // placeholder art (reuse block)
        bo.sprite->set_camera(cam);
        bo.sprite->set_position(wx(b.tx * 8 + 4), wy(b.ty * 8 + 4));
    }

    // ---- loose platforms (M8 Stone: drop straight down on a nearby pound shockwave) ----
    for(int i = 0; i < level.loose_platform_count && i < 8; ++i){
        const logic::LoosePlatformSpawn& lp = level.loose_platforms[i];
        _loose_platforms.push_back(LoosePlatformInst{ lp.tx, lp.ty, lp.len, lp.ty, false, false, {} });
        LoosePlatformInst& li = _loose_platforms.back();
        for(int dx = 0; dx < lp.len && dx < 8; ++dx){
            engine::set_collision_tile(lp.tx + dx, lp.ty, 1);     // solid run; bg blank, sprites show it
            li.sprites.push_back(bn::sprite_items::block.create_sprite(0, 0));  // placeholder art
            li.sprites.back().set_camera(cam);
            li.sprites.back().set_position(wx((lp.tx + dx) * 8 + 4), wy(lp.ty * 8 + 4));
        }
    }

    // ---- hidden platforms (M10 Light: NON-solid + invisible at spawn; a Light cast reveals them
    //      solid+visible for the RevealState window, then they revert). Mirrors loose platforms but
    //      we do NOT make them solid here and the sprites start hidden. ----
    for(int i = 0; i < level.hidden_platform_count && i < 8; ++i){
        const logic::HiddenPlatformSpawn& hp = level.hidden_platforms[i];
        _hidden_platforms.push_back(HiddenPlatformInst{ hp.tx, hp.ty, hp.len, false, {} });
        HiddenPlatformInst& hi2 = _hidden_platforms.back();
        for(int dx = 0; dx < hp.len && dx < 8; ++dx){
            // NON-solid + invisible until revealed (do NOT set_collision_tile here).
            hi2.sprites.push_back(bn::sprite_items::block.create_sprite(0, 0));  // placeholder art (reuse block)
            hi2.sprites.back().set_camera(cam);
            hi2.sprites.back().set_position(wx((hp.tx + dx) * 8 + 4), wy(hp.ty * 8 + 4));
            hi2.sprites.back().set_visible(false);
        }
    }
}

TerrainSystem::PoundImpact TerrainSystem::resolve_pound(Ctx& ctx, GatesSystem& gates, TriggersSystem& triggers)
{
    logic::Player& player = ctx.player;
    if(!player.stone.just_landed()) return PoundImpact{};

    int impact_cx = logic::Tilemap::px_to_tile(player.body.pos.x + player.body.half_w);                  // centre column
    // Two distinct rows (the collision resolver leaves the body resting just ABOVE the floor):
    //  - impact_fy: the body's lowest OCCUPIED tile (matches the plate-trip convention); a
    //    plate/heavy-plate marker is a non-solid tile the body stands ON, so we match this row.
    //  - impact_floor: the SOLID tile directly under the feet (matches the on_ground probe);
    //    cracked floors + boulders are SOLID tiles the player lands ON TOP of, so they live at
    //    this row, not impact_fy.
    int impact_fy    = logic::Tilemap::px_to_tile(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1));
    int impact_floor = logic::Tilemap::px_to_tile(player.body.pos.y + player.body.half_h + player.body.half_h);

    // 1. CrackedFloor smash + continue the plunge. The landed tile is solid; if it is an unbroken
    //    cracked floor, break the WHOLE contiguous cracked-floor run at that row (I27: a 2-direction
    //    walk from the impact tile -- game::GatesSystem::break_cracked_run_at) and RE-ARM the pound
    //    so the next frame plunges into the area below. Re-arm ONLY on a cracked tile, so one pound
    //    chains through STACKED cracked floors and naturally ends on the first non-cracked solid.
    bool smashed = gates.break_cracked_run_at(impact_cx, impact_floor, ctx);
    if(smashed) player.stone.start();   // re-arm: plunge through to the next floor below

    // 2. Heavy switch: a heavy plate trips ONLY on a pound (game::TriggersSystem::trip_heavy_plate_at).
    //    Fires its gate target when the player's feet/centre land on the plate tile. (Normal plates
    //    are handled in the trigger loop, which SKIPS heavy plates so they never trip on a step/block.)
    triggers.trip_heavy_plate_at(impact_cx, impact_fy, ctx);

    // 3. Boulder break: if a boulder is the tile directly below the player's feet (or the landed
    //    tile itself), remove it so the path clears. (Boulders rebuild on room re-entry -- fine.)
    for(BoulderInst& bo : _boulders){
        if(bo.broken) continue;
        // The boulder the player landed ON TOP of is the solid tile under the feet (impact_floor).
        bool below = (bo.tx == impact_cx && bo.ty == impact_floor);
        if(below){
            bo.broken = true;
            engine::set_collision_tile(bo.tx, bo.ty, 0);
            if(bo.sprite) bo.sprite->set_visible(false);
        }
    }

    // 4. Loose-platform shockwave: any not-yet-falling loose platform whose run is within
    //    Chebyshev distance <=6 of the impact begins falling (drop-to-rest; see update_loose_platforms).
    for(LoosePlatformInst& li : _loose_platforms){
        if(li.falling || li.fallen) continue;
        if(logic::loose_platform_in_shockwave(li.tx, li.cur_ty, li.len, impact_cx, impact_floor))
            li.falling = true;
    }

    return PoundImpact{ true, impact_cx, impact_floor };
}

void TerrainSystem::update_loose_platforms(Ctx& ctx)
{
    const int hw = ctx.hw, hh = ctx.hh;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };
    engine::LoadedLevel& lvl = ctx.lvl;

    // ---- loose platforms: drop-to-rest one tile/frame while falling (solid-grid test only) ----
    // The fall test considers ONLY the collision grid; the player is not a collision tile, so a
    // platform never rests on the player (content guarantees the player isn't under a dropping run).
    for(LoosePlatformInst& li : _loose_platforms){
        if(!li.falling) continue;
        bool clear_below = true;
        for(int dx = 0; dx < li.len; ++dx)
            if(lvl.map.is_solid(li.tx + dx, li.cur_ty + 1)){ clear_below = false; break; }
        if(clear_below){
            for(int dx = 0; dx < li.len; ++dx){
                engine::set_collision_tile(li.tx + dx, li.cur_ty, 0);       // clear old row
                engine::set_collision_tile(li.tx + dx, li.cur_ty + 1, 1);   // set new row solid
            }
            ++li.cur_ty;
            for(int dx = 0; dx < (int)li.sprites.size(); ++dx)
                li.sprites[dx].set_position(wx((li.tx + dx) * 8 + 4), wy(li.cur_ty * 8 + 4));
        } else {
            li.falling = false; li.fallen = true;   // rest; drop-to-rest, no bounce
        }
    }
}

void TerrainSystem::update_hidden_platforms(Ctx& /*ctx*/, bool light_cast)
{
    // ctx is unused here: the hidden-platform toggle only touches this system's own sprites +
    // collision tiles, keyed off the RevealState timer -- no player/world/lvl ref needed. The
    // parameter stays (rather than an overload without it) so this method's signature matches the
    // other per-frame room-system methods' Ctx&-first convention.
    // ---- M10 Light reveal: a Light cast that ACTUALLY fired (re)starts the room-wide window
    //      (play_room detects this via the returned fired-spell, NOT a magic delta -- the
    //      crystal refill mutates magic.cur the same frame and would corrupt a before/after
    //      inference -- and passes it in as `light_cast`). Then tick; toggle hidden-platform
    //      collision+visibility on the timer EDGE (don't rewrite every frame).
    if(light_cast) _reveal.on_cast();
    _reveal.tick();
    bool want_shown = _reveal.revealed();
    for(HiddenPlatformInst& hp : _hidden_platforms){
        if(want_shown == hp.shown) continue;            // edge only
        for(int dx = 0; dx < hp.len && dx < 8; ++dx)
            engine::set_collision_tile(hp.tx + dx, hp.ty, want_shown ? 1 : 0);
        for(int dx = 0; dx < (int)hp.sprites.size(); ++dx)
            hp.sprites[dx].set_visible(want_shown);
        hp.shown = want_shown;
    }
}

void TerrainSystem::update_blocks(Ctx& ctx, bool want_left, bool want_right)
{
    const int hw = ctx.hw, hh = ctx.hh;
    logic::Player& player = ctx.player;
    engine::LoadedLevel& lvl = ctx.lvl;

    // ---- pushable blocks: push detection, gravity, sprite ----
    if(_push_cd > 0) --_push_cd;
    for(BlockInst& bi : _blocks){
        // push when grounded, holding a dir, and the tile in front of the player == this block
        if(_push_cd == 0 && player.body.on_ground && (want_left || want_right)){
            int dir = want_right ? 1 : -1;
            int lead_px = want_right ? player.body.pos.x.to_int() + 16 : player.body.pos.x.to_int() - 1;
            int feet_ty = logic::Tilemap::px_to_tile(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1));
            if(logic::Tilemap::px_to_tile(fx(lead_px)) == bi.blk.tx && feet_ty == bi.blk.ty){
                int oldx = bi.blk.tx;
                if(bi.blk.push(dir, lvl.map)){
                    engine::set_collision_tile(oldx, bi.blk.ty, 0);
                    engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);
                    _push_cd = 8;
                }
            }
        }
        int oldy = bi.blk.ty;
        if(bi.blk.apply_gravity_step(lvl.map)){
            engine::set_collision_tile(bi.blk.tx, oldy, 0);
            engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);
        }
        int bx = bi.blk.tx * 8 + 4, by = bi.blk.ty * 8 + 4;
        bi.sprite->set_position(bx - hw, by - hh);
    }
}

void TerrainSystem::reset_blocks(Ctx& ctx)
{
    const int hw = ctx.hw, hh = ctx.hh;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };

    // Reset pushable blocks to their authored start so a block shoved into a dead corner
    // (a soft-lock) is recoverable by dying. (Plates re-evaluate next frame; latched
    // button/brazier gates stay solved.)
    for(BlockInst& bi : _blocks){
        engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 0);          // clear where it ended up
        bi.blk.tx = bi.spawn_tx; bi.blk.ty = bi.spawn_ty;
        engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);          // solid at the start cell
        if(bi.sprite) bi.sprite->set_position(wx(bi.blk.tx * 8 + 4), wy(bi.blk.ty * 8 + 4));
    }
}
}
