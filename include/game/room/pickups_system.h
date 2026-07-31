#pragma once
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "logic/level_data.h"
#include "logic/ability_pickup.h"
#include "logic/collision.h"
#include "game/player_session.h"   // CrystalStation
#include "game/room/room_ctx.h"

namespace game {

// Owns the pickup/reward entity families that don't interact with combat or terrain:
// ability shrines, heart containers, magic crystals (via the existing CrystalStation from
// Phase 4), health pickups (M14: pre-boss full-HP restore, one-shot per room visit), the
// cage/spronk rescue, and the dungeon exit. Extracted verbatim from play_room (Task 6.1 of
// the maintainability remediation) -- spawn()/update_*() bodies are unchanged copies of the
// original scene_dungeon.cpp code; only the call SITES in play_room changed (they now go
// through this object instead of inline blocks).
//
// Per-frame call order (must match play_room's documented order exactly): play_room calls
// update_shrines(), then update_hearts_and_crystals() (hearts + the crystal-collect check
// happen back-to-back in the original code) then update_health_pickups() (M14: same slot),
// then -- after PlayerSession::refresh_spell_icon() -- check_spronk_and_exit(). update() is
// split into these methods (rather than one) because the shrine/heart/crystal/spronk/exit
// checks are NOT contiguous in the original per-frame loop (PlayerSession's icon refresh sits
// between crystals and the spronk/exit check), so a single update() could not occupy all of
// its original slots at once.
class PickupsSystem {
public:
    void spawn(const logic::LevelData& level, Ctx& ctx);

    void update_shrines(Ctx& ctx);
    void update_hearts_and_crystals(Ctx& ctx);
    // M14: health pickups (pre-boss full-HP restore, one-shot per room visit; NOT persisted).
    // Called at the same order slot as update_hearts_and_crystals (right after it).
    void update_health_pickups(Ctx& ctx);
    // Returns true the frame the room's exit condition is met (spronk freed if the room has a
    // cage, AND the player is grounded on the exit tile) -- play_room turns that into
    // RoomOutcome::ExitDungeon. Takes no LevelData -- has_cage/has_exit were captured in spawn().
    bool check_spronk_and_exit(Ctx& ctx);

    // Public: play_room's death/respawn block (not moved this task) calls crystals.reset()
    // directly, exactly as it called the local `crystals` variable before this extraction.
    game::CrystalStation crystals;

private:
    struct ShrineInst { logic::AbilityPickup pk; logic::Body body; bn::optional<bn::sprite_ptr> sprite; };
    struct HeartInst  { logic::HeartContainerSpawn hc; logic::Body body; bn::optional<bn::sprite_ptr> sprite; bool collected = false; };
    // M14: a one-shot full-HP restore pickup, placed in the boss-approach room. NOT persisted
    // (unlike heart containers) -- `collected` resets every time the room is spawned, mirroring
    // the magic-crystal respawn-per-attempt semantics rather than the heart's SRAM latch.
    struct HealthPickupInst { int tx, ty; logic::Body body; bn::optional<bn::sprite_ptr> sprite; bool collected = false; };

    bn::vector<ShrineInst, 4> _shrines;
    bn::vector<HeartInst, 4> _hearts;
    bn::vector<HealthPickupInst, 4> _health_pickups;

    logic::Body _cage;
    bn::optional<bn::sprite_ptr> _spronk;
    bool _has_cage = false;

    logic::Body _exit;
    bool _has_exit = false;
};
}
