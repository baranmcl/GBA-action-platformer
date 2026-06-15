#pragma once
#include "logic/world_state.h"
namespace game {
// Game Over scene. Shows "GAME OVER" + a two-option d-pad cursor menu
// (CONTINUE / QUIT TO TITLE). Up/Down moves the highlight (default = Continue);
// A or START confirms. const: it only displays + reads input.
enum class GameOverChoice { Continue, QuitToTitle };
GameOverChoice run_game_over(const logic::World& world);
}
