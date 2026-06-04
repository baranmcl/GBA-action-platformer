#pragma once
#include "logic/world_state.h"
namespace game {
// Plays Dungeon 1. If `world.spronk1_freed` is already set (loaded from save), starts
// with the gate open and Featherleap owned. On reaching the exit, saves and returns.
void run_play(logic::World& world);
}
