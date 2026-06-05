// Spronk Quest — entry point. Loads the save, then: Title -> Hub <-> Dungeon.
#include "bn_core.h"
#include "logic/world_state.h"
#include "engine/save.h"
#include "game/scene_title.h"
#include "game/scene_hub.h"
#include "game/scene_dungeon.h"
#include "game/levels/dungeon1.h"

int main()
{
    bn::core::init();

    logic::World world;
    if(!engine::read_world(world))
        world = logic::World{}; // fresh game on empty/corrupt SRAM

    game::run_title(world); // START -> enter the hub

    while(true)
    {
        game::HubResult hr = game::run_hub(world);   // returns when a door is entered
        int n = hr.enter_dungeon;                     // 1..8; M2 only ever 1 (others locked)
        if(n != 1) continue;

        world.current_dungeon = n;
        game::DungeonResult dr = game::run_dungeon(DUNGEON1_DATA, world);
        world.current_dungeon = 0;                    // back in the hub before saving
        if(dr == game::DungeonResult::Cleared)
            engine::write_world(world);               // persist earned spronk/ability
    }
}
