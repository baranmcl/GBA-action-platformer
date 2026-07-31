#include "game/room/gates_system.h"
#include "game/room/room_util.h"

#include "logic/gates.h"
#include "logic/tile_ids.h"
#include "engine/level_loader.h"  // set_collision_tile
#include "engine/level_view.h"    // set_level_tile

namespace game {
namespace {
    // A gate/wall is a FULL-height 2-wide column (rows 1..floor-1) at column tx, so it can't be
    // double-jumped over -- you must clear it. Clearing opens the whole column. (Same helper as
    // the identical anonymous-namespace copy in triggers_system.cpp -- see room_util.h's header
    // note re: small-helper duplication across the room-system units; these two need an
    // engine::LevelView&, which room_util.h deliberately does NOT pull in, to stay host-test-safe.)
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

void GatesSystem::spawn(const logic::LevelData& level, Ctx& ctx)
{
    _level_h = level.h;
    engine::LoadedLevel& lvl = ctx.lvl;
    logic::World& world = ctx.world;

    // M8: CrackedFloor is a horizontal FLOOR, NOT a full-column vertical wall -- it is SKIPPED
    // below and collected into _cracked_floors (made solid as a single floor tile, not a column).
    for(int i = 0; i < level.gate_count && i < 24; ++i){
        const logic::GateSpawn& g = level.gates[i];
        if(g.type == logic::GateType::CrackedFloor){
            // A cracked floor is a single SOLID floor tile the player walks on; only a pound
            // breaks it. The compiler emits content symbols on collision tile 0, so make it solid
            // here + render bg 11. If already latched-open (smashed on a prior visit and
            // persisted), leave it broken/empty.
            bool latched_open = (g.latch_id >= 0) && world.latched(g.latch_id);
            _cracked_floors.push_back(CrackedFloorInst{ g.tx, g.ty, g.latch_id, latched_open });
            if(!latched_open){
                engine::set_collision_tile(g.tx, g.ty, 1);
                engine::set_level_tile(lvl.view, g.tx, g.ty, logic::gate_info(logic::GateType::CrackedFloor).bg_tile); // 11
            }
            continue;
        }
        logic::Body gb; gb.half_w = fx(8); gb.half_h = fx((level.h - 3) * 4); // full-column body
        gb.pos = { fx(g.tx * 8), fx(8) };
        _gates.push_back(GateInst{ g, gb, false });
        GateInst& gi = _gates.back();
        const logic::GateInfo& info = logic::gate_info(g.type);
        bool passable = info.is_geometry && logic::can_pass(g.type, world.abilities);
        // latched_open: a shortcut opened on a prior visit, persisted in SRAM -- re-open it on room load.
        bool latched_open = (g.latch_id >= 0) && world.latched(g.latch_id);
        if(passable || latched_open){ gi.open = true; }              // geometry gate owned OR latch set -> open
        else { fill_column(lvl.view, g.tx, level.h, info.bg_tile); } // closed -> full-height vine/ice wall
    }
}

void GatesSystem::update(Ctx& ctx, engine::SpellPool& spells)
{
    engine::LoadedLevel& lvl = ctx.lvl;
    logic::World& world = ctx.world;
    logic::Player& player = ctx.player;

    for(GateInst& gi : _gates){
        logic::SpellId clears = logic::gate_cleared_by(gi.spawn.type);
        if(!gi.open && clears != logic::SpellId::None && spells.consume_hit(gi.body, clears)){
            gi.open = true;
            open_column(lvl.view, gi.spawn.tx, _level_h);
            persist_latch(world, gi.spawn.latch_id);
        }
        // M6: cracked walls aren't spell-cleared (gate_cleared_by==None); a dashing body smashes them on contact.
        if(!gi.open && gi.spawn.type == logic::GateType::CrackedWall
           && player.dash.active() && logic::aabb_overlap(player.body, gi.body)){
            gi.open = true;
            open_column(lvl.view, gi.spawn.tx, _level_h);
            persist_latch(world, gi.spawn.latch_id);
        }
    }
}

bool GatesSystem::break_cracked_run_at(int impact_cx, int impact_floor, Ctx& ctx)
{
    engine::LoadedLevel& lvl = ctx.lvl;
    logic::World& world = ctx.world;

    for(CrackedFloorInst& cf : _cracked_floors){
        if(cf.broken || cf.tx != impact_cx || cf.ty != impact_floor) continue;
        auto break_tile = [&](CrackedFloorInst& q){
            q.broken = true;
            engine::set_collision_tile(q.tx, q.ty, 0);
            engine::set_level_tile(lvl.view, q.tx, q.ty, logic::tiles::BLANK);
            persist_latch(world, q.latch_id);
        };
        auto cracked_at = [&](int x)->CrackedFloorInst*{
            for(auto& q : _cracked_floors)
                if(!q.broken && q.ty == cf.ty && q.tx == x) return &q;
            return nullptr;
        };
        for(int x = impact_cx; CrackedFloorInst* q = cracked_at(x); --x) break_tile(*q);
        for(int x = impact_cx + 1; CrackedFloorInst* q = cracked_at(x); ++x) break_tile(*q);
        return true;
    }
    return false;
}
}
