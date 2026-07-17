#pragma once
#include "logic/level_data.h"
#include "logic/player.h"
#include "game/room/room_ctx.h"

namespace game {

// Owns room-door + dungeon-exit ARCHWAY RENDERING (bg tiles only -- collision is untouched,
// doors/exit stay walkable) and the Up-press door-use resolution. Extracted verbatim from
// play_room (Task 6.1 of the maintainability remediation). play_room still owns RoomOutcome
// (its own control-flow return type) and the `return RoomOutcome{...}` statements -- this
// class only computes WHICH outcome an Up-press resolves to (room_door_at() + the
// target_room==-1 hub-exit sentinel check), mirroring the original inline code exactly.
class DoorsSystem {
public:
    void spawn(const logic::LevelData& level, Ctx& ctx);

    struct UpPressResult {
        enum Kind { None, ExitToHub, GoToRoom } kind = None;
        int target_room = 0;
        int target_entrance = 0;
    };
    // Call on bn::keypad::up_pressed(). Kind::None means no door was in range (play_room does
    // nothing that frame, exactly as the original `if(const RoomDoorSpawn* dr = ...)` falling
    // through did).
    UpPressResult on_up_pressed(const logic::LevelData& level, const logic::Player& player) const;
};
}
