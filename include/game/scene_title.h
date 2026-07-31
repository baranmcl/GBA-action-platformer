#pragma once
#include "logic/world_state.h"
namespace game {
// `debug` is true if the player entered by holding SELECT while pressing START (I35 dev-tooling
// combo) rather than a plain START — main.cpp routes that into the debug selector instead of the
// hub. False is the normal case and leaves main.cpp's existing title->hub flow unchanged.
struct TitleResult { bool debug; };
// Shows the title; "CONTINUE" if a save exists. Returns when START is pressed (plain, or with
// SELECT held for the debug entry combo).
TitleResult run_title(const logic::World& world);
}
