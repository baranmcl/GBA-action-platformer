#pragma once
#include "bn_vector.h"
#include "logic/level_data.h"
#include "logic/collision.h"
#include "logic/player.h"
#include "engine/spell_pool.h"
#include "game/room/room_ctx.h"

namespace game {

// Owns geometry/obstacle GATES (Gap/GrapplePoint/Vine/Ice/Water/CrackedWall/DarkVeil/FireWall --
// see logic::GateType) and M8 Stone CRACKED FLOORS (a horizontal floor a pound smashes through,
// NOT a vertical wall gate). Extracted verbatim from play_room (Task 6.2 of the maintainability
// remediation), with one behavior fix: the cracked-floor pound-break is now a 2-direction walk
// from the impact tile instead of an O(n^3) contiguity rescan (I27) -- see break_cracked_run_at.
class GatesSystem {
public:
    void spawn(const logic::LevelData& level, Ctx& ctx);

    // Per-frame, spell-resolution slot (gates -> braziers -> enemies -> ...): spell-clear (a
    // Fire/Ice/Light shot opens the matching obstacle gate) + dash-smash (a dashing body opens a
    // CrackedWall gate on contact -- CrackedWall has no clearing spell, gate_cleared_by==None).
    void update(Ctx& ctx, engine::SpellPool& spells);

    // Task 6.3's pound-resolution calls this on the frame a Stone pound lands on an unbroken
    // cracked-floor tile at (impact_cx, impact_floor). Breaks the maximal contiguous run of
    // cracked-floor tiles at that row via a 2-direction walk from the impact tile (I27 fix --
    // semantically identical to the old triple-nested contiguity scan: a contiguous walk from the
    // impact tile visits exactly the maximal contiguous span). Returns whether anything was
    // smashed -- the caller re-arms `player.stone.start()` to chain the plunge through stacked
    // cracked floors, exactly as before.
    bool break_cracked_run_at(int impact_cx, int impact_floor, Ctx& ctx);

private:
    struct GateInst { logic::GateSpawn spawn; logic::Body body; bool open = false; };
    // M8 Stone: a cracked FLOOR (NOT a vertical wall gate). Solid until a pound smashes it.
    struct CrackedFloorInst { int tx, ty, latch_id; bool broken = false; };

    bn::vector<GateInst, 24> _gates;
    bn::vector<CrackedFloorInst, 16> _cracked_floors;
    int _level_h = 0;
};
}
