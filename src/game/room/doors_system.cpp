#include "game/room/doors_system.h"
#include "game/room/room_util.h"

#include "logic/room_graph.h"   // room_door_at
#include "logic/tile_ids.h"
#include "engine/level_view.h"  // set_level_tile

namespace game {

void DoorsSystem::spawn(const logic::LevelData& level, Ctx& ctx)
{
    engine::LoadedLevel& lvl = ctx.lvl;

    // ---- room-doors (bg tile 5 open-door; 2-wide x 4-tall archway grounded on the floor,
    //      matching the hub's archway). Floor-scanned so a row-18-authored door reaches the
    //      floor (row 20) instead of floating. Collision unchanged (door stays walkable). ----
    for(int i = 0; i < level.room_door_count && i < 8; ++i){
        const logic::RoomDoorSpawn& rd = level.room_doors[i];
        int fr = floor_row_below(lvl.map, rd.tx, rd.ty);
        // target_room == -1 is the exit-to-hub door: render with a DISTINCT bg tile (26, hub portal)
        // so it reads differently from a normal room-door (tile 5) and the dungeon goal/exit (tile 6).
        // (13 is lava, so 26 is the next free strip slot — see make_placeholder_art.py gen_tiles.)
        int door_bg = (rd.target_room == -1) ? logic::tiles::HUB_PORTAL : logic::tiles::DOOR_OPEN;
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx)
            engine::set_level_tile(lvl.view, rd.tx + dx, fr - 1 - dy, door_bg);
    }
    // ---- exit marker (bg tile 6 door-locked = distinct closed door = dungeon goal;
    //      same 2-wide x 4-tall grounded archway. room-doors use tile 5 so they're distinct.
    //      Exit collision body untouched. ----
    if(level.has_exit){
        int fr = floor_row_below(lvl.map, level.exit_tx, level.exit_ty);
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx)
            engine::set_level_tile(lvl.view, level.exit_tx + dx, fr - 1 - dy, logic::tiles::DOOR_LOCKED);
    }
}

DoorsSystem::UpPressResult DoorsSystem::on_up_pressed(const logic::LevelData& level, const logic::Player& player) const
{
    if(const logic::RoomDoorSpawn* dr = logic::room_door_at(level, player.body)){
        // target_room == -1 is the sentinel "exit-to-hub" door: a diegetic Up-press
        // equivalent of SELECT=quit. play_room returns Quit (NOT Cleared) so the dungeon
        // stays re-enterable, mirroring the original inline check exactly.
        if(dr->target_room == -1) return UpPressResult{ UpPressResult::ExitToHub };
        return UpPressResult{ UpPressResult::GoToRoom, dr->target_room, dr->target_entrance };
    }
    return UpPressResult{};
}
}
