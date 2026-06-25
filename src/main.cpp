// Spronk Quest — entry point. Loads the save, then: Title -> Hub <-> Dungeon.
#include "bn_core.h"
#include "logic/world_state.h"
#include "logic/player_state.h"
#include "engine/save.h"
#include "game/scene_title.h"
#include "game/scene_hub.h"
#include "game/scene_dungeon.h"
#include "game/scene_boss.h"
#include "game/scene_victory.h"
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
        int n = hr.enter_dungeon;                     // 1..9; D1-D8 dungeons + D9 finale (boss)

        // Door 9 (finale) is NOT a normal dungeon: a 2-room approach hands off to the boss arena.
        if(n == 9)
        {
            world.current_dungeon = 9;
            game::DungeonResult ar = game::run_dungeon(DUNGEON9_APPROACH, world, ps);
            if(ar == game::DungeonResult::Cleared)
            {
                game::BossResult br = game::run_boss(DUNGEON9_ARENA, world, ps);
                if(br == game::BossResult::Victory)
                {
                    world.beaten = true;
                    engine::write_world(world);       // persist completion before the ending
                    game::run_victory(world);         // "THE END" screen
                }
                // Victory (post-ending) and QuitToTitle both fall through to the title.
                game::run_title(world);
            }
            else if(ar == game::DungeonResult::QuitToTitle)
            {
                game::run_title(world);
            }
            // A DungeonResult::Quit from the approach (hub-return 'Q' door) falls through to
            // the shared reset below and resumes the hub, same as the other dungeons.
            world.current_dungeon = 0;
            continue;
        }

        const logic::DungeonData* lvl = nullptr;
        if(n == 1) lvl = &DUNGEON1_DUNGEON;
        else if(n == 2) lvl = &DUNGEON2_DUNGEON;
        else if(n == 3) lvl = &DUNGEON3_DUNGEON;
        else if(n == 4) lvl = &DUNGEON4_DUNGEON;
        else if(n == 5) lvl = &DUNGEON5_DUNGEON;
        else if(n == 6) lvl = &DUNGEON6_DUNGEON;
        else if(n == 7) lvl = &DUNGEON7_DUNGEON;
        else if(n == 8) lvl = &DUNGEON8_DUNGEON;
        else continue;                                // out-of-range n (no such dungeon)

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
