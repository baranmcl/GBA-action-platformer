#pragma once
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_camera_ptr.h"
#include "logic/bolt.h"
#include "logic/physics.h"
#include "logic/tilemap.h"
#include "logic/vec2.h"
namespace engine {
// Fixed-size pool of wand bolts (no heap). One update() per frame: ticks the cooldown,
// fires on input, advances active bolts, despawns on solid tiles / off-level.
class BoltPool {
public:
    BoltPool(int map_px_w, int map_px_h, const bn::camera_ptr& cam);
    void update(bool fire_pressed, logic::Vec2 muzzle, int facing, const logic::Tilemap& map);
    // If an active bolt overlaps `target`, despawn that bolt and return true (one hit).
    bool consume_hit(const logic::Body& target);
private:
    struct Bolt {
        bool active = false;
        logic::Body body;
        logic::Vec2 vel{};
        bn::optional<bn::sprite_ptr> sprite;
    };
    static constexpr int N = 4;
    Bolt _bolts[N];
    logic::BoltCaster _caster;
    int _half_w_px;
    int _half_h_px;
    bn::camera_ptr _camera;
};
}
