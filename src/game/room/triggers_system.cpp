#include "game/room/triggers_system.h"
#include "game/room/room_util.h"

#include "logic/tile_ids.h"
#include "engine/level_loader.h"  // set_collision_tile
#include "engine/level_view.h"    // set_level_tile

namespace game {
namespace {
    // Same helper as the identical anonymous-namespace copy in gates_system.cpp -- see that
    // file's comment (and room_util.h's header note) for why these two aren't in room_util.h.
    void fill_column(engine::LevelView& view, int tx, int level_h, int bg){
        for(int ty = 1; ty < level_h - 2; ++ty) for(int dx = 0; dx < 2; ++dx){
            engine::set_collision_tile(tx + dx, ty, 1);
            engine::set_level_tile(view, tx + dx, ty, bg);
        }
    }
    void open_column(engine::LevelView& view, int tx, int level_h){
        for(int ty = 1; ty < level_h - 2; ++ty) for(int dx = 0; dx < 2; ++dx){
            engine::set_collision_tile(tx + dx, ty, 0);
            engine::set_level_tile(view, tx + dx, ty, 0);
        }
    }
}

void TriggersSystem::spawn(const logic::LevelData& level, Ctx& ctx)
{
    _level_h = level.h;
    engine::LoadedLevel& lvl = ctx.lvl;
    logic::World& world = ctx.world;

    // ---- braziers (bg tile 14 unlit; Body for fire-hit) ----
    for(int i = 0; i < level.brazier_count && i < 16; ++i){
        const logic::BrazierSpawn& b = level.braziers[i];
        // Grounded on the floor (fr-1), floor-scanned (IMPL-5) so a ledge-authored brazier still
        // grounds correctly; the hit-body rows (draw_ty-5..draw_ty) are derived from the same
        // floor-scanned row rather than a hardcoded offset, for the same reason (I25).
        int draw_ty = floor_row_below(lvl.map, b.tx, b.ty) - 1;
        _braziers.push_back(BrazierInst{ b.tx, b.ty, b.group, tile_body(b.tx, draw_ty - 5, 6, 24), false, draw_ty });
        engine::set_level_tile(lvl.view, b.tx, draw_ty, logic::tiles::BRAZIER_UNLIT);
    }

    // ---- plates (tile 17) / buttons (tile 18) + triggers ----
    for(int i = 0; i < level.plate_count && i < 16; ++i){
        const logic::PlateSpawn& p = level.plates[i];
        engine::set_level_tile(lvl.view, p.tx, p.ty, logic::tiles::PLATE);
        // M8: a HEAVY plate trips ONLY on a Stone pound (trip_heavy_plate_at, called from the
        // pound resolution, NOT here). Collect it for that lookup and skip it from the normal
        // step/block trigger loop so it never trips on a footstep or pushed block. If a latched
        // heavy plate's gate was already smashed on a prior visit, re-open it on room load
        // (mirrors the gate latch restore idiom in GatesSystem::spawn).
        if(p.heavy){
            _heavy_plates.push_back(p);
            if(p.latch_id >= 0 && world.latched(p.latch_id)) open_column(lvl.view, p.target_tx, level.h);
            continue;
        }
        logic::Trigger t = logic::Trigger::plate(); t.target_tx = p.target_tx; t.target_ty = p.target_ty;
        _triggers.push_back(TriggerInst{ t, p.tx, p.ty, -1, -1, false });
    }
    for(int i = 0; i < level.button_count && i < 16; ++i){
        const logic::ButtonSpawn& b = level.buttons[i];
        engine::set_level_tile(lvl.view, b.tx, b.ty, logic::tiles::BUTTON);
        logic::Trigger t = logic::Trigger::button(); t.target_tx = b.target_tx; t.target_ty = b.target_ty;
        _triggers.push_back(TriggerInst{ t, b.tx, b.ty, -1, -1, false });
    }
    for(int g = 0; g < level.brazier_group_count && g < 16; ++g){
        const logic::BrazierGroupSpawn& bg = level.brazier_groups[g];
        logic::Trigger t = logic::Trigger::braziers(bg.total); t.target_tx = bg.target_tx; t.target_ty = bg.target_ty;
        bool latched_open = (bg.latch_id >= 0) && world.latched(bg.latch_id);
        _triggers.push_back(TriggerInst{ t, 0, 0, g, bg.latch_id, latched_open });
        if(latched_open) open_column(lvl.view, bg.target_tx, level.h);
    }
}

void TriggersSystem::update_braziers(Ctx& ctx, engine::SpellPool& spells)
{
    engine::LoadedLevel& lvl = ctx.lvl;
    for(BrazierInst& bi : _braziers){
        if(!bi.lit && spells.consume_hit(bi.body, logic::SpellId::Fire)){  // only Fire lights braziers
            bi.lit = true;
            engine::set_level_tile(lvl.view, bi.tx, bi.draw_ty, logic::tiles::BRAZIER_LIT);
        }
    }
}

void TriggersSystem::update_triggers(Ctx& ctx, const bn::vector<BlockTileXY, 8>& block_tiles)
{
    engine::LoadedLevel& lvl = ctx.lvl;
    logic::World& world = ctx.world;
    logic::Player& player = ctx.player;

    for(TriggerInst& ti : _triggers){
        switch(ti.trig.kind){
            case logic::TriggerKind::Plate: {
                // Pressed by the player OR a block, but only when SQUARELY on the plate --
                // the player's horizontal centre must be over the plate column AND the player
                // must be grounded on the plate's row (not merely overlapping the edge while
                // standing next to/above it; that loose AABB caused the gate to flicker). The
                // gate is held open only WHILE pressed, so the player can step on it (it opens)
                // but can't pass alone -- only the block, left resting on it, holds it open.
                int pcx = logic::Tilemap::px_to_tile(player.body.pos.x + player.body.half_w);                       // centre col
                int pfy = logic::Tilemap::px_to_tile(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1)); // feet row
                bool on = player.body.on_ground && pcx == ti.src_tx && pfy == ti.src_ty;
                for(const BlockTileXY& bt : block_tiles) if(bt.tx == ti.src_tx && bt.ty == ti.src_ty) on = true;
                ti.trig.pressed = on;
                if(on && !ti.applied){ ti.applied = true;  open_column(lvl.view, ti.trig.target_tx, _level_h); }
                else if(!on && ti.applied){ ti.applied = false; fill_column(lvl.view, ti.trig.target_tx, _level_h, 1); }
                break; }
            case logic::TriggerKind::Button: {
                if(logic::aabb_overlap(player.body, tile_body(ti.src_tx, ti.src_ty, 4, 4))) ti.trig.pressed = true; // latch
                if(!ti.applied && ti.trig.active()){ ti.applied = true; open_column(lvl.view, ti.trig.target_tx, _level_h); }
                break; }
            case logic::TriggerKind::Braziers: {
                int n = 0; for(BrazierInst& bi : _braziers) if(bi.group == ti.group && bi.lit) ++n;
                ti.trig.lit = n;
                if(!ti.applied && ti.trig.active()){
                    ti.applied = true; open_column(lvl.view, ti.trig.target_tx, _level_h); // latch
                    persist_latch(world, ti.latch_id);
                }
                break; }
        }
    }
}

void TriggersSystem::trip_heavy_plate_at(int impact_cx, int impact_fy, Ctx& ctx)
{
    engine::LoadedLevel& lvl = ctx.lvl;
    logic::World& world = ctx.world;

    // Heavy switch: a heavy plate trips ONLY on a pound. Fire its gate target (open_column) when
    // the player's feet/centre land on the plate tile. (Normal plates are handled in
    // update_triggers, which excludes heavy plates so they never trip on a step/block.)
    for(const logic::PlateSpawn& p : _heavy_plates){
        if(impact_cx == p.tx && impact_fy == p.ty){
            open_column(lvl.view, p.target_tx, _level_h);
            persist_latch(world, p.latch_id);
        }
    }
}
}
