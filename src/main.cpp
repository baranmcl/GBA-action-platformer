// Spronk Quest — entry point. Loads the save, then alternates title <-> play.
#include "bn_core.h"
#include "logic/world_state.h"
#include "engine/save.h"
#include "game/scene_title.h"
#include "game/scene_play.h"

int main()
{
    bn::core::init();

    logic::World world;
    if(!engine::read_world(world))
        world = logic::World{}; // fresh game on empty/corrupt SRAM

    while(true)
    {
        game::run_title(world);
        game::run_play(world);
    }
}
