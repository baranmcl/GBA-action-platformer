#pragma once
#include "bn_sprite_ptr.h"
#include "bn_camera_ptr.h"
#include "logic/player.h"
namespace engine {
// Owns Laurel's hardware sprite and keeps it in sync with the pure logic::Player:
// converts level-space (top-left, pixels) to Butano world-space (centred), flips by
// facing, and picks an animation frame. Attached to the shared camera.
class Avatar {
public:
    Avatar(const logic::Player& p, int map_px_w, int map_px_h, const bn::camera_ptr& cam);
    void sync(const logic::Player& p);
private:
    bn::sprite_ptr _sprite;
    int _half_w_px;   // map_px_w / 2  (level -> world x offset)
    int _half_h_px;   // map_px_h / 2
    int _frame = -1;  // cached current frame; only re-tile on change
};
}
