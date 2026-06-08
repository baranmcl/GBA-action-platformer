#pragma once
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_camera_ptr.h"
#include "logic/spell.h"
#include "logic/meters.h"
#include "logic/physics.h"
#include "logic/tilemap.h"
#include "logic/vec2.h"
namespace engine {
// Fixed-size pool of spell projectiles (no heap). Each shot is tagged with the SpellId it was
// cast as (Fire or Ice). SPLIT API so the scene resolves typed effects (gate clear, freeze, melt,
// enemy hit) via consume_hit / consume_tile_hit BEFORE despawning leftovers on walls.
class SpellPool {
public:
    SpellPool(int map_px_w, int map_px_h, const bn::camera_ptr& cam);
    // Tick cooldown; cast the SELECTED spell if magic allows; advance projectiles; despawn OFF-LEVEL only.
    void update_and_cast(bool cast_pressed, logic::SpellState& spell, logic::Meter& magic,
                         logic::Vec2 muzzle, int facing, const logic::Tilemap& map);
    // Despawn a shot of `kind` overlapping `target`; return true (scene applies the effect).
    bool consume_hit(const logic::Body& target, logic::SpellId kind);
    // Despawn a shot of `spell_kind` whose column contains `target_kind` below it (chest-height
    // shot vs floor-level tile — the M3 brazier-height lesson); report the matched tile.
    bool consume_tile_hit(const logic::Tilemap& map, logic::TileKind target_kind,
                          logic::SpellId spell_kind, int& out_tx, int& out_ty);
    // Despawn any shot overlapping a solid tile (walls) — call AFTER all consume_hit/consume_tile_hit.
    void despawn_on_solid(const logic::Tilemap& map);
private:
    struct Shot {
        bool active = false;
        logic::SpellId kind = logic::SpellId::None;
        logic::Body body;
        logic::Vec2 vel{};
        bn::optional<bn::sprite_ptr> sprite;
    };
    static constexpr int N = 4;
    Shot _shots[N];
    logic::SpellCast _caster;
    int _half_w_px;
    int _half_h_px;
    bn::camera_ptr _camera;
};
}
