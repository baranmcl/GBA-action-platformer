// Spronk Quest — entry point. Loads the save, then: Title -> Hub <-> Dungeon.
#include "bn_core.h"
#include "logic/world_state.h"
#include "logic/player_state.h"
#include "engine/save.h"
#include "game/scene_title.h"
#include "game/scene_hub.h"
#include "game/scene_dungeon.h"
#include "game/levels/dungeon1.h"
#include "game/levels/dungeon2.h"

int main()
{
    bn::core::init();

    logic::World world;
    if(!engine::read_world(world))
        world = logic::World{}; // fresh game on empty/corrupt SRAM

    game::run_title(world); // START -> enter the hub

    logic::PlayerState ps;  // health/magic persist across hub <-> dungeon for the whole session

    while(true)
    {
        game::HubResult hr = game::run_hub(world, ps); // returns when a door is entered
        int n = hr.enter_dungeon;                     // 1..8; M3 supports 1 and 2 (others locked)
        const logic::LevelData* lvl = nullptr;
        if(n == 1) lvl = &DUNGEON1_DATA;
        else if(n == 2) lvl = &DUNGEON2_DATA;
        else continue;                                // doors 3-8 not built

        world.current_dungeon = n;
        game::DungeonResult dr = game::run_dungeon(*lvl, world, ps);
        world.current_dungeon = 0;                    // back in the hub before saving
        if(dr == game::DungeonResult::Cleared)
            engine::write_world(world);               // persist earned spronk/ability
    }
}
