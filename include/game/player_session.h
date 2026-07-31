#pragma once
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_vector.h"
#include "bn_optional.h"
#include "logic/player.h"
#include "logic/player_state.h"
#include "logic/world_state.h"
#include "logic/spell.h"
#include "logic/level_data.h"   // LevelData, MagicCrystalSpawn (CrystalStation::spawn)
namespace game {

// Screen-clamped follow camera (the 4x-duplicated lambda, verbatim semantics).
void set_clamped_cam(bn::camera_ptr& cam, int map_px_w, int map_px_h,
                     int level_w, int level_h, int cx, int cy);

struct SessionIntent { logic::InputFrame in; bool want_grapple = false; bool cast_spell = false; };

class PlayerSession {
public:
    PlayerSession(bn::camera_ptr& cam, logic::World& world, logic::PlayerState& ps, logic::Player& player);
    void sync_abilities();            // the 5-line world.has(...) block
    SessionIntent read_intent();      // read_input + spell cycle + grapple/cast decode; in.grapple_fire=false
                                      // (caller decides anchor vs pull, then sets it)
    void refresh_spell_icon();        // Fire/Ice/Grapple/Light icon swap-on-change
    logic::Vec2 muzzle() const;       // aim-adjusted muzzle (read_aim_dy)
    // Vine VFX: latched line to anchor, or the 10-frame miss animation. Call once per frame.
    void note_anchor_miss(int facing);
    void update_vine_vfx(int hw, int hh);
private:
    bn::camera_ptr& _cam; logic::World& _world; logic::PlayerState& _ps; logic::Player& _player;
    bn::sprite_ptr _spell_icon; logic::SpellId _last_icon = logic::SpellId::None;
    bn::vector<bn::sprite_ptr, 4> _vine_segs; int _miss_vine_t = 0; int _miss_vine_dir = 1;
};

// Respawning full-refill magic crystals (the 3x-duplicated M10 pattern).
// Handles ALL of level.magic_crystals (up to 8 — play_room's case); boss arenas have 1.
// Grounding differs between the two originals and MUST be preserved per caller:
//   TileCentre  — play_room's `mc.ty*8+8` placement (D8 rest-ledge visuals depend on it)
//   FloorBelow  — the boss loops' floor_row_below grounding (the M13 sank-into-floor fix)
class CrystalStation {
public:
    enum class Grounding { TileCentre, FloorBelow };
    // respawn_when_depleted preserves the two originals' DIFFERENT semantics:
    //   true  — boss loops: a collected crystal reappears once magic.cur < SPELL_COST (repeatable station)
    //   false — play_room: collected stays gone until reset() (death/attempt reset only)
    void spawn(const logic::LevelData& level, bn::camera_ptr& cam, const logic::Tilemap& map,
               int hw, int hh, Grounding g, bool respawn_when_depleted);
    void update(const logic::Body& player_body, logic::Meter& magic);
    void reset();                                                        // fight-restart / death path: all uncollected+visible
private:
    struct Crystal { logic::Body body; bn::optional<bn::sprite_ptr> sprite; bool collected = false; };
    bn::vector<Crystal, 8> _crystals;
    bool _respawn_when_depleted = false;
};
}
