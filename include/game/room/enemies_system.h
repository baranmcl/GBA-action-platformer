#pragma once
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "logic/level_data.h"
#include "logic/enemy.h"
#include "engine/bolts.h"
#include "engine/spell_pool.h"
#include "game/room/room_ctx.h"

namespace game {

// One patrolling enemy + its sprite. Public (not nested in EnemiesSystem) because play_room's
// grapple targeting (Task 6.3 of the maintainability remediation -- stays in play_room per the
// Phase 6 preamble) iterates this vector directly to pick a pull target and nudges it by
// mutating `e.body.pos` -- same seam as TriggersSystem's BlockTileXY (Task 6.2) and
// TerrainSystem's BlockInst (Task 6.3).
struct EnemyInst { logic::Enemy e; bn::optional<bn::sprite_ptr> sprite; };

// Owns the room's patrolling enemies. Extracted verbatim from play_room (Task 6.3 of the
// maintainability remediation).
class EnemiesSystem {
public:
    void spawn(const logic::LevelData& level, Ctx& ctx);

    // Per-frame, "enemies" slot (after braziers, before freeze/melt): patrol + render, then in
    // order -- pound-crush (player.stone.active(), kills even fire_immune enemies + magic
    // refill), bolt-kill (+magic refill), fire-kill (fire_immune respected, no refill), else
    // contact damage via logic::try_hit (dash i-frames blink through it). Reads/writes
    // ctx.ps.health/ctx.ps.magic/ctx.invuln directly (no separate health/magic/invuln params).
    void update(Ctx& ctx, engine::BoltPool& bolts, engine::SpellPool& spells);

    // Grapple-pull seam (Task 6.3): play_room's grapple targeting scans this vector directly --
    // the nearest-non-immune-alive-enemy-in-arc pick AND the one-tile nudge-toward-player both
    // stay in play_room exactly as before (arc/range test needs the player's facing + intent,
    // which aren't part of Ctx) -- only the storage moved. No separate try_enemy_pull(): since
    // EnemyInst is exposed directly, play_room's existing "compute dest tile, mutate
    // body.pos.x if non-solid" code works unchanged against `enemies_sys.enemies()` in place of
    // its old local `enemies` vector.
    bn::vector<EnemyInst, 8>& enemies(){ return _enemies; }

private:
    bn::vector<EnemyInst, 8> _enemies;
};
}
