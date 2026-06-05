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
// Fixed-size pool of fire projectiles (no heap). SPLIT API so the scene can clear gates
// (which are solid tiles) via consume_hit BEFORE despawning projectiles on walls.
class FirePool {
public:
    FirePool(int map_px_w, int map_px_h, const bn::camera_ptr& cam);
    // Tick cooldown; cast if Fire selected + magic available; advance projectiles; despawn OFF-LEVEL only.
    void update_and_cast(bool cast_pressed, logic::SpellState& spell, logic::Meter& magic,
                         logic::Vec2 muzzle, int facing, const logic::Tilemap& map);
    // Despawn a projectile overlapping `target`, return true (scene applies the effect).
    bool consume_hit(const logic::Body& target);
    // Despawn any projectile overlapping a solid tile (walls) — call AFTER all consume_hit.
    void despawn_on_solid(const logic::Tilemap& map);
private:
    struct Shot {
        bool active = false;
        logic::Body body;
        logic::Vec2 vel{};
        bn::optional<bn::sprite_ptr> sprite;
    };
    static constexpr int N = 4;
    Shot _shots[N];
    logic::FireCast _caster;
    int _half_w_px;
    int _half_h_px;
    bn::camera_ptr _camera;
};
}
