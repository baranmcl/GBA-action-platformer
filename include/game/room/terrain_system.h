#pragma once
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "logic/level_data.h"
#include "logic/pushable_block.h"
#include "logic/reveal.h"
#include "game/room/room_ctx.h"
#include "game/room/gates_system.h"
#include "game/room/triggers_system.h"

namespace game {

// One pushable block instance + its sprite. Public (not nested in TerrainSystem) because
// play_room's grapple targeting (Task 6.3 of the maintainability remediation -- stays in
// play_room per the Phase 6 preamble) iterates this vector directly to pick a pull target and
// mutates it via PushableBlock::pull(...) + the matching set_collision_tile calls -- same seam
// as EnemiesSystem's EnemyInst and TriggersSystem's BlockTileXY (Task 6.2).
struct BlockInst {
    logic::PushableBlock blk;
    bn::optional<bn::sprite_ptr> sprite;
    bool pullable = false;
    // Authored spawn tile, cached at spawn() time (Task 6.3): TerrainSystem doesn't keep a
    // LevelData& around after spawn() returns, so reset_blocks() (the death handler's "put every
    // block back where it started" step, formerly `level.blocks[i].tx/ty`) needs its own copy of
    // the original position instead of re-reading LevelData.
    int spawn_tx = 0, spawn_ty = 0;
};

// M8 Stone: a breakable solid boulder (NOT pushable). Solid tile + sprite; removed on a pound.
struct BoulderInst { int tx, ty; bn::optional<bn::sprite_ptr> sprite; bool broken = false; };

// M8 Stone: a horizontal run of `len` tiles, suspended, that DROPS straight down (drop-to-rest,
// no momentum) when a pound's shockwave lands within Chebyshev distance <=6. Collision tiles +
// sprites.
struct LoosePlatformInst {
    int tx, ty, len, cur_ty;
    bool falling = false, fallen = false;
    bn::vector<bn::sprite_ptr, 8> sprites;   // one per tile in the run
};

// M10 Light: a horizontal run of `len` tiles that is NON-solid + invisible until a Light cast
// reveals it (RevealState window) -- then solid + visible; reverts when the timer expires.
struct HiddenPlatformInst {
    int tx, ty, len;
    bool shown = false;
    bn::vector<bn::sprite_ptr, 8> sprites;   // one per tile in the run
};

// Owns pushable blocks, M8 Stone boulders + loose platforms, and M10 Light hidden platforms --
// plus the cross-system M8 Stone pound-RESOLUTION sequence (Task 6.3 of the maintainability
// remediation). The pound is ONE event touching THREE families: cracked floors live in
// GatesSystem and heavy plates live in TriggersSystem (both Task 6.2), so resolve_pound() is the
// cross-system entry point that calls into both of them, in the exact original order, alongside
// this system's own boulder-break + loose-platform-shockwave steps. Extracted verbatim from
// play_room; the grapple-pull targeting (blocks AND enemies) and the pound VFX (dust
// sprite/camera shake -- play_room-local state, not terrain data) stay in play_room -- see
// resolve_pound()'s doc comment for the exact split.
class TerrainSystem {
public:
    void spawn(const logic::LevelData& level, Ctx& ctx);

    // Result of a pound-resolution attempt: `landed` is false on any frame the pound didn't just
    // land (mirrors `player.stone.just_landed()`, checked inside resolve_pound); `cx`/`floor` are
    // the impact tile (the same impact_cx/impact_floor values the original inline handler
    // computed) -- play_room uses them to position its own dust-sprite VFX exactly as before.
    struct PoundImpact { bool landed = false; int cx = 0, floor = 0; };

    // Task 6.3's cross-system pound handler, called once per frame at the pound-resolution slot
    // (right after player.update()/avatar.sync(), before the loose-platform fall-step). Internally
    // gates on ctx.player.stone.just_landed() (returns PoundImpact{} on any other frame), then
    // resolves in this EXACT order (unchanged from the original inline `if(player.stone.
    // just_landed())` handler):
    //   1. gates.break_cracked_run_at(impact_cx, impact_floor, ctx) -- may re-arm
    //      ctx.player.stone.start() to chain the plunge through stacked cracked floors.
    //   2. triggers.trip_heavy_plate_at(impact_cx, impact_fy, ctx).
    //   3. boulder break (this system's own _boulders -- unchanged logic).
    //   4. loose-platform shockwave arm (this system's own _loose_platforms -- unchanged logic;
    //      the actual fall-to-rest STEPPING happens next frame-slot, in update_loose_platforms).
    // VFX split: the original handler also set up a dust sprite + camera-shake timers using
    // play_room-local objects (`pound_dust`, `pound_vfx_t`, `pound_shake_t`) that are not part of
    // Ctx and have no gameplay effect -- purely cosmetic, fire-and-forget timers. Rather than
    // thread those play_room locals into this system, play_room reads the returned PoundImpact
    // and, if `landed`, sets up its own VFX using the SAME impact_cx/impact_floor this method
    // computed. This only changes WHEN the VFX trigger executes relative to the four resolution
    // steps above (after, instead of interleaved before step 1) -- behavior-identical, since the
    // VFX never influences resolution and the impact tile is fixed for the whole frame.
    PoundImpact resolve_pound(Ctx& ctx, GatesSystem& gates, TriggersSystem& triggers);

    // Per-frame, "loose platforms" slot (immediately after pound resolution, every frame
    // regardless of whether a pound just landed): drop-to-rest stepping for any platform whose
    // `falling` flag is set (solid-grid test only).
    void update_loose_platforms(Ctx& ctx);

    // Per-frame, "Light reveal" slot (right after the spell-cast resolution that produces the
    // fired SpellId). play_room passes `light_cast = (fired == logic::SpellId::Light)` -- the
    // signaling decision for Task 6.3: TerrainSystem doesn't otherwise see the spell-cast result,
    // and pulling logic::SpellId into Ctx for this one bool isn't worth it, so play_room computes
    // the bool and passes it in. Advances the owned RevealState (on_cast() if light_cast, then
    // always tick()) and toggles hidden-platform collision/visibility on the timer EDGE only.
    void update_hidden_platforms(Ctx& ctx, bool light_cast);

    // Per-frame, "blocks" slot (after hazards, before triggers): push detection + gravity + sprite
    // sync. `want_left`/`want_right` are the held-direction bits from play_room's SessionIntent
    // (not part of Ctx, so passed explicitly). Owns its push cooldown (`_push_cd`) internally --
    // this WAS play_room-local `push_cd`. NOTE `grapple_pull_cd` stays in play_room (it's the
    // shared cooldown for BOTH the block-pull and enemy-pull grapple targeting, which both stay
    // in play_room per the Phase 6 preamble) -- only `push_cd` (push-specific) moved in here.
    void update_blocks(Ctx& ctx, bool want_left, bool want_right);

    // Death-handler seam (Task 6.3): play_room's respawn block calls this instead of its old
    // inline "put every block back where it started" loop (a block shoved into a dead corner is
    // recoverable by dying).
    void reset_blocks(Ctx& ctx);

    // Grapple-pull seam (Task 6.3): play_room's grapple targeting scans this vector directly --
    // the range/arc test AND the first-pullable-in-spawn-order pick both stay in play_room
    // (needs the player's facing + intent, not part of Ctx) -- only the storage moved. No
    // separate try_block_pull(): since BlockInst is exposed directly, play_room's existing
    // "pull() + set_collision_tile" code works unchanged against `terrain.blocks()` in place of
    // its old local `blocks` vector.
    bn::vector<BlockInst, 8>& blocks(){ return _blocks; }

private:
    bn::vector<BlockInst, 8> _blocks;
    bn::vector<BoulderInst, 8> _boulders;
    bn::vector<LoosePlatformInst, 8> _loose_platforms;
    bn::vector<HiddenPlatformInst, 8> _hidden_platforms;
    logic::RevealState _reveal;   // room-wide Light reveal timer (a Light cast (re)starts it)
    int _push_cd = 0;
};
}
