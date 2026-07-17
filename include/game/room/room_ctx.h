#pragma once
#include "bn_camera_ptr.h"
#include "logic/world_state.h"
#include "logic/player.h"
#include "logic/player_state.h"
#include "engine/level_loader.h"
namespace game {

// Shared per-room refs threaded from play_room into the room systems (Tasks 6.1-6.4 of the
// maintainability remediation). Each task adds ONLY the fields its own systems need — this
// keeps the struct's growth legible instead of front-loading every field a later task might
// want. Task 6.1 (doors/exit/cage/pickups) needs: world/player/ps (health+magic+spell live on
// PlayerState)/lvl (map+bg view)/cam/hw/hh. Later tasks (gates/triggers/enemies/terrain) are
// expected to add fields such as `int& invuln` and cross-system refs (e.g. `GatesSystem&`)
// here as needed — do not add them speculatively before a system actually reads them.
struct Ctx {
    logic::World& world;
    logic::Player& player;
    logic::PlayerState& ps;
    engine::LoadedLevel& lvl;
    bn::camera_ptr& cam;
    int hw;
    int hh;
};
}
