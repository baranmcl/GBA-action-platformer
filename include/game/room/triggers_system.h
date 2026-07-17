#pragma once
#include "bn_vector.h"
#include "logic/level_data.h"
#include "logic/collision.h"
#include "logic/puzzle.h"
#include "engine/spell_pool.h"
#include "game/room/room_ctx.h"

namespace game {

// A block tile occupied THIS frame, for the plate trigger's "something is resting on me" check.
// TriggersSystem doesn't own blocks (Task 6.3's terrain_system will) -- play_room fills this in
// each frame from its own block list, mirroring the original inline `for(BlockInst& bi : blocks)`
// scan exactly (Task 6.2 extraction rules: a cross-system query the trigger family genuinely
// needs, kept minimal until Task 6.3 gives Ctx a real BlockSystem&).
struct BlockTileXY { int tx, ty; };

// Owns braziers (M3 Fire-lit obstacle/puzzle piece) and plate/button/brazier-group TRIGGERS (M6-M8
// puzzle switches that open a target gate column). Extracted verbatim from play_room (Task 6.2 of
// the maintainability remediation), with one behavior fix: the brazier hit-body rows now track the
// floor-scanned draw row instead of a hardcoded row 14 (I25) -- see spawn().
class TriggersSystem {
public:
    void spawn(const logic::LevelData& level, Ctx& ctx);

    // Per-frame, spell-resolution slot (gates -> braziers -> ...): a Fire shot lights an unlit brazier.
    void update_braziers(Ctx& ctx, engine::SpellPool& spells);
    // Per-frame, triggers slot (after blocks): plate/button/brazier-group -> open/close the target column.
    void update_triggers(Ctx& ctx, const bn::vector<BlockTileXY, 8>& block_tiles);

    // Task 6.3's pound-resolution calls this on the frame a Stone pound lands squarely on a heavy
    // plate's tile (impact_cx, impact_fy). Heavy plates are excluded from update_triggers (Task
    // 1.3 / I24) -- they trip ONLY on a pound, never on a footstep or a pushed block.
    void trip_heavy_plate_at(int impact_cx, int impact_fy, Ctx& ctx);

private:
    // src_tx/ty: plate or button tile to test; group: brazier group index (Braziers kind only);
    // latch_id: the brazier group's latch (Braziers kind only; -1/unused for Plate/Button --
    // cached here at spawn so update_triggers doesn't need a LevelData& to look it up).
    struct TriggerInst { logic::Trigger trig; int src_tx, src_ty, group, latch_id; bool applied = false; };
    struct BrazierInst { int tx, ty, group; logic::Body body; bool lit = false; int draw_ty = 0; };

    bn::vector<TriggerInst, 16> _triggers;
    bn::vector<BrazierInst, 16> _braziers;
    bn::vector<logic::PlateSpawn, 16> _heavy_plates;
    int _level_h = 0;
};
}
