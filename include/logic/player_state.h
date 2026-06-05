#pragma once
#include "logic/meters.h"
namespace logic {
// Runtime player vitals that persist across scenes within a session (hub <-> dungeons).
// Owned by main() and passed by reference into every scene, so health drained / magic
// banked in one room carries into the next. NOT saved to SRAM: a fresh boot starts here.
struct PlayerState {
    Meter health{ 100, 100 };
    Meter magic{ 0, 100 };
};
}
