// Spronk Quest — entry point. Loads the save, then: Title -> Hub <-> Dungeon.
#include "bn_core.h"
#include "logic/world_state.h"
#include "logic/player_state.h"
#include "engine/save.h"
#include "game/scene_title.h"
#include "game/scene_hub.h"
#include "game/scene_dungeon.h"
#include "game/levels/dungeons.h"

int main()
{
    bn::core::init();

    logic::World world;
    if(!engine::read_world(world))
        world = logic::World{}; // fresh game on empty/corrupt SRAM
    logic::clamp_lives_on_load(world); // never boot into an instant Game Over (fresh World already has lives=3)

    game::run_title(world); // START -> enter the hub

    logic::PlayerState ps;  // health/magic persist across hub <-> dungeon for the whole session

    while(true)
    {
        game::HubResult hr = game::run_hub(world, ps); // returns when a door is entered
        int n = hr.enter_dungeon;                     // 1..8; dungeons 1-6 built (M2-M7), 7-8 not yet
        const logic::DungeonData* lvl = nullptr;
        if(n == 1) lvl = &DUNGEON1_DUNGEON;
        else if(n == 2) lvl = &DUNGEON2_DUNGEON;
        else if(n == 3) lvl = &DUNGEON3_DUNGEON;
        else if(n == 4) lvl = &DUNGEON4_DUNGEON;
        else if(n == 5) lvl = &DUNGEON5_DUNGEON;
        else if(n == 6) lvl = &DUNGEON6_DUNGEON;
        else if(n == 7) lvl = &DUNGEON7_DUNGEON;
        else continue;                                // door 8 not built

        world.current_dungeon = n;
        game::DungeonResult dr = game::run_dungeon(*lvl, world, ps);
        world.current_dungeon = 0;                    // back in the hub before saving
        if(dr == game::DungeonResult::Cleared)
            engine::write_world(world);               // persist earned spronk/ability
        else if(dr == game::DungeonResult::QuitToTitle)
            game::run_title(world);                   // Game Over -> Quit to title; run_dungeon already
                                                      // refilled + persisted lives, so no write here.
        // loop re-enters run_hub either way (Quit/QuitToTitle both resume the hub)
    }
}
