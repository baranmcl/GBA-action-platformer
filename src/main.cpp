// Phase-2 temporary harness: Title -> data-driven Dungeon 1, looping. Verifies the
// compiled Dungeon 1 plays identically to M1. The plaza hub + full loop arrive in Phase 4.
#include "bn_core.h"
#include "logic/world_state.h"
#include "engine/save.h"
#include "game/scene_title.h"
#include "game/scene_dungeon.h"
#include "game/levels/dungeon1.h"

int main()
{
    bn::core::init();

    logic::World world;
    if(!engine::read_world(world))
        world = logic::World{};

    while(true)
    {
        game::run_title(world);
        world.current_dungeon = 1;               // REQUIRED before run_dungeon (free_spronk(d))
        game::run_dungeon(DUNGEON1_DATA, world);
        world.current_dungeon = 0;
    }
}
