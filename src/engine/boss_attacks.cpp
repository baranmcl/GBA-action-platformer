#include "engine/boss_attacks.h"

#include "logic/collision.h"   // aabb_overlap
#include "logic/fixed.h"

namespace engine {
namespace {
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
}

// 8 rotating directions for the spiral (integer approx of a circle, ~3 px/frame).
const int BOSS_SPIRAL_DIRS[BOSS_SPIRAL_DIR_COUNT][2] = {
    {0,-3},{2,-2},{3,0},{2,2},{0,3},{-2,2},{-3,0},{-2,-2}
};

// ---------------------------------------------------------------------------
// AttackPool
// ---------------------------------------------------------------------------
AttackPool::AttackPool(const bn::sprite_item& bolt_item, const bn::camera_ptr& cam,
                       int map_px_w, int map_px_h)
    : _half_w_px(map_px_w / 2), _half_h_px(map_px_h / 2)
{
    for(int i = 0; i < CAP; ++i){
        _pool[i].sprite = bolt_item.create_sprite(0, 0);
        _pool[i].sprite->set_camera(cam);
        _pool[i].sprite->set_visible(false);
        _pool[i].sprite->set_scale(2.0);
    }
}

void AttackPool::launch(int idx, int cx_px, int cy_px, int vx, int vy){
    _pool[idx].active = true;
    _pool[idx].body.half_w = fx(6);
    _pool[idx].body.half_h = fx(6);
    _pool[idx].body.pos = { fx(cx_px - 6), fx(cy_px - 6) };
    _pool[idx].vel = { fx(vx), fx(vy) };
}

void AttackPool::clear(){
    for(AttackInst& a : _pool){
        a.active = false;
        if(a.sprite) a.sprite->set_visible(false);
    }
}

bool AttackPool::advance(const logic::Body& player_body, int arena_w, int arena_h,
                         bool player_iframed, const logic::Tilemap& map){
    bool hit = false;
    for(AttackInst& a : _pool){
        if(!a.active) continue;
        a.body.pos.x = a.body.pos.x + a.vel.x;
        a.body.pos.y = a.body.pos.y + a.vel.y;
        int ax = a.body.pos.x.to_int() + a.body.half_w.to_int();
        int ay = a.body.pos.y.to_int() + a.body.half_h.to_int();
        // Despawn on a SOLID wall/floor (centre tile); pass through one-way platforms + empty.
        // Arena-bounds check stays as a backstop below.
        if(ax < 0 || ax > arena_w || ay < 0 || ay > arena_h
           || map.is_solid(ax / 8, ay / 8)){   // expire off-arena OR into solid geometry
            a.active = false;
            if(a.sprite) a.sprite->set_visible(false);
        } else {
            if(a.sprite){ a.sprite->set_position(ax - _half_w_px, ay - _half_h_px); a.sprite->set_visible(true); }
            if(!player_iframed && logic::aabb_overlap(player_body, a.body)){
                hit = true;
            }
        }
    }
    return hit;
}

// ---------------------------------------------------------------------------
// spawn_attack — aimed / fan (the spiral is streamed via SpiralEmitter)
// ---------------------------------------------------------------------------
void spawn_attack(AttackPool& pool, int variant,
                  int boss_cx, int boss_cy, int player_cx, int player_cy,
                  int speed, int phase, bool aim_full){
    (void)phase;   // speed already encodes the per-phase scaling (caller passes it in)
    int dir = (player_cx >= boss_cx) ? 1 : -1;
    if(variant == logic::BOSS_ATK_AIMED){
        if(aim_full){
            // Aim a velocity VECTOR (magnitude ~speed) straight at the player's position, so the bolt
            // heads to wherever they stand — they must MOVE to dodge, not just stand in a dead spot.
            int dx = player_cx - boss_cx, dy = player_cy - boss_cy;
            int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
            int vx, vy;
            if(adx >= ady){ vx = (dx >= 0 ? 1 : -1) * speed; vy = adx ? dy * speed / adx : 0; }
            else          { vy = (dy >= 0 ? 1 : -1) * speed; vx = ady ? dx * speed / ady : 0; }
            pool.launch(0, boss_cx, boss_cy, vx, vy);
        } else {
            pool.launch(0, boss_cx, boss_cy, speed * dir, 0);             // horizontal (King — unchanged)
        }
    } else if(variant == logic::BOSS_ATK_FAN){
        if(aim_full){
            // D1: a 3-bolt HORIZONTAL spread at three heights — all travel straight to the side wall
            // (no downward drift into the floor, which was despawning mid-arena via the solid-tile
            // check). The player jumps/moves to dodge the band; every bolt reaches a wall.
            pool.launch(0, boss_cx, boss_cy - 10, speed * dir, 0);
            pool.launch(1, boss_cx, boss_cy,      speed * dir, 0);
            pool.launch(2, boss_cx, boss_cy + 10, speed * dir, 0);
        } else {
            pool.launch(0, boss_cx, boss_cy, speed * dir, 0);             // King: angular fan (unchanged)
            pool.launch(1, boss_cx, boss_cy, speed * dir, -2);
            pool.launch(2, boss_cx, boss_cy, speed * dir, 2);
        }
    }
}

// ---------------------------------------------------------------------------
// SpiralEmitter
// ---------------------------------------------------------------------------
void SpiralEmitter::tick(AttackPool& pool, int boss_cx, int boss_cy){
    if(idx >= STEPS) return;
    if(cd <= 0){
        int di = ((rot * idx) % BOSS_SPIRAL_DIR_COUNT + BOSS_SPIRAL_DIR_COUNT) % BOSS_SPIRAL_DIR_COUNT;
        pool.launch(idx, boss_cx, boss_cy, BOSS_SPIRAL_DIRS[di][0], BOSS_SPIRAL_DIRS[di][1]);
        ++idx;
        cd = 5;
    } else {
        --cd;
    }
}

// ---------------------------------------------------------------------------
// RockfallEmitter
// ---------------------------------------------------------------------------
RockfallEmitter::RockfallEmitter(const bn::sprite_item& marker_item, const bn::camera_ptr& cam,
                                 int map_px_w, int map_px_h)
    : _half_w_px(map_px_w / 2), _half_h_px(map_px_h / 2)
{
    for(int i = 0; i < MAXCOLS; ++i){
        _markers.push_back(marker_item.create_sprite(0, 0));
        _markers[i].set_camera(cam);
        _markers[i].set_visible(false);
    }
}

void RockfallEmitter::begin(int player_tx, int arena_w_tiles, int arena_h_tiles, int count, int seed){
    _dropped = false;
    _warn = WARN_FRAMES;
    if(count > MAXCOLS) count = MAXCOLS;
    _col_count = logic::rockfall_columns(player_tx, arena_w_tiles, count, seed, _cols, MAXCOLS);
    // show a ground marker at each target column, resting on the floor surface (row h-2).
    const int floor_y = (arena_h_tiles - 2) * 8;
    for(int i = 0; i < MAXCOLS; ++i){
        bool on = i < _col_count;
        if(on) _markers[i].set_position(_cols[i] * 8 + 4 - _half_w_px, floor_y - _half_h_px);
        _markers[i].set_visible(on);
    }
}

void RockfallEmitter::tick(AttackPool& rocks){
    if(_dropped) return;
    if(--_warn > 0) return;
    // warn elapsed: drop one rock per column from the ceiling (row 1), hide the markers.
    for(int i = 0; i < _col_count; ++i)
        rocks.launch(i, _cols[i] * 8 + 4, 8 + 6, 0, ROCK_VY);   // ceiling, straight down
    for(int i = 0; i < MAXCOLS; ++i) _markers[i].set_visible(false);
    _dropped = true;
}

void RockfallEmitter::clear(){
    for(int i = 0; i < MAXCOLS; ++i) _markers[i].set_visible(false);
    _col_count = 0; _warn = 0; _dropped = false;
}

int RockfallEmitter::leap_offset() const {
    if(_dropped || _warn <= 0) return 0;
    int t = WARN_FRAMES - _warn;                 // frames since the leap began (0..WARN_FRAMES)
    int peak = WARN_FRAMES / 2;
    int d = (t < peak) ? t : (WARN_FRAMES - t);  // triangle: rise to the peak, fall back down
    return d;                                     // ~0..13 px (cosmetic)
}

// ---------------------------------------------------------------------------
// TelegraphCue
// ---------------------------------------------------------------------------
TelegraphCue::TelegraphCue(const bn::sprite_item& fallback_item, const bn::camera_ptr& cam,
                           int map_px_w, int map_px_h)
    : _orb(fallback_item.create_sprite(0, 0)),
      _half_w_px(map_px_w / 2), _half_h_px(map_px_h / 2)
{
    _orb.set_camera(cam);
    _orb.set_visible(false);
}

void TelegraphCue::show(int variant, int boss_cx, int boss_cy, int attack_timer,
                        const bn::sprite_item& aimed_item, const bn::sprite_item& spiral_item,
                        const bn::sprite_item& fan_item){
    if(variant == logic::BOSS_ATK_AIMED)        _orb.set_item(aimed_item);
    else if(variant == logic::BOSS_ATK_SPIRAL)  _orb.set_item(spiral_item);
    else                                        _orb.set_item(fan_item);
    _orb.set_position(boss_cx - _half_w_px, (boss_cy - 14) - _half_h_px);   // charge above the boss
    _orb.set_visible((attack_timer / 6) % 2 == 0);                          // pulse the cue
}

void TelegraphCue::hide(){ _orb.set_visible(false); }

// ---------------------------------------------------------------------------
// BossHpBar
// ---------------------------------------------------------------------------
BossHpBar::BossHpBar(const logic::BossDef& def, const bn::sprite_item& pip_item)
    : _wound_dmg(def.wound_dmg)
{
    int pips = def.max_hp / def.wound_dmg;
    if(pips > MAX_PIPS) pips = MAX_PIPS;
    for(int i = 0; i < pips; ++i)
        _pips.push_back(pip_item.create_sprite(-32 + i * 8, 68));
}

void BossHpBar::refresh(int hp){
    int alive = (hp + _wound_dmg - 1) / _wound_dmg;   // ceil(hp / wound_dmg)
    for(int i = 0; i < _pips.size(); ++i)
        _pips[i].set_visible(i < alive);
}

}  // namespace engine
