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
#include "game/scene_debug.h"
#include "game/levels/dungeons.h"

int main()
{
    bn::core::init();

    logic::World world;
    if(!engine::read_world(world))
        world = logic::World{}; // fresh game on empty/corrupt SRAM
    logic::clamp_lives_on_load(world); // never boot into an instant Game Over (fresh World already has lives=3)

    // Title -> normal hub flow, OR (SELECT held on START) the I35 debug dungeon/ability selector.
    // A debug launch runs with saving disabled and a synthetic, throwaway World (never `world`
    // itself) so it can never touch the real SRAM save; afterward we land back on the title with
    // the ORIGINAL `world` untouched and loop until the player enters normally. This is the ONLY
    // change to main's control flow — once TitleResult::debug is false, everything below is
    // byte-identical to the pre-I35 flow.
    for(;;)
    {
        game::TitleResult tr = game::run_title(world);
        if(!tr.debug) break;

        game::DebugLaunch dl = game::run_debug_select();
        if(dl.dungeon != 0)
        {
            engine::set_save_enabled(false); // linchpin: nothing below can reach SRAM until re-enabled
            logic::World dbg_world = dl.world;
            dbg_world.current_dungeon = dl.dungeon;
            logic::PlayerState dbg_ps;
            logic::sync_health_cap(dbg_ps, dbg_world);
            dbg_ps.health.cur = dbg_ps.health.max;

            if(dl.dungeon == 9) // finale: approach then boss arena, same routing as door 9 below
            {
                game::DungeonResult ar = game::run_dungeon(DUNGEON9_APPROACH, dbg_world, dbg_ps);
                if(ar == game::DungeonResult::Cleared)
                    game::run_boss(DUNGEON9_ARENA, dbg_world, dbg_ps); // result unused — debug session ends either way
            }
            else
            {
                const logic::DungeonData* lvl = DUNGEONS_BY_ID[dl.dungeon - 1];
                game::run_dungeon(*lvl, dbg_world, dbg_ps);
            }

            engine::set_save_enabled(true); // restore normal saving for the rest of the session
        }
        // Loop back to the title; dl.dungeon == 0 (cancelled) also lands here.
    }

    logic::PlayerState ps;  // health/magic persist across hub <-> dungeon for the whole session
    // Boot at FULL health for the player's CURRENT max. PlayerState defaults to the BASE 100, but a
    // returning player may have earned heart containers (max_health_for > 100); without this they'd
    // start with a partly-empty bar (cur=100 < max=150). ps is never saved, so a fresh boot always
    // starts full — per-session damage still carries across hub<->dungeon (no free heal on hub return).
    logic::sync_health_cap(ps, world);   // grow the cap for any already-collected heart containers
    ps.health.cur = ps.health.max;       // ...then always start a fresh boot at FULL health (unlike the
                                         // mid-session clamp-down sync_health_cap does at the other sites)

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
        if(n >= 1 && n <= 8) lvl = DUNGEONS_BY_ID[n - 1];
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
