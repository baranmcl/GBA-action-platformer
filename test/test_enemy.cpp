#include "test_framework.h"
#include "logic/enemy.h"
using namespace logic;
static uint8_t cells[10 * 4];
static Tilemap mkmap(){
    for(int i = 0; i < 40; ++i) cells[i] = 0;
    for(int x = 0; x < 10; ++x) cells[3 * 10 + x] = 1; // floor row 3
    return Tilemap{10, 4, cells};
}
static Enemy ground_enemy(){
    Enemy e;
    e.body.half_w = Fixed::from_int(4); e.body.half_h = Fixed::from_int(4);
    e.body.pos = { Fixed::from_int(16), Fixed::from_int(16) };
    e.left_bound = Fixed::from_int(8); e.right_bound = Fixed::from_int(60);
    return e;
}
TEST(enemy_moves_when_alive){
    Tilemap m = mkmap(); Enemy e = ground_enemy();
    Fixed x0 = e.body.pos.x; e.update(m);
    CHECK(e.body.pos.x.raw != x0.raw);
}
TEST(enemy_reverses_at_right_bound){
    Tilemap m = mkmap(); Enemy e = ground_enemy();
    e.dir = 1; e.body.pos.x = Fixed::from_int(60); e.update(m);
    CHECK_EQ(e.dir, -1);
}
TEST(enemy_reverses_at_left_bound){
    Tilemap m = mkmap(); Enemy e = ground_enemy();
    e.dir = -1; e.body.pos.x = Fixed::from_int(8); e.update(m);
    CHECK_EQ(e.dir, 1);
}
TEST(enemy_dead_does_not_move){
    Tilemap m = mkmap(); Enemy e = ground_enemy(); e.kill();
    Fixed x0 = e.body.pos.x; e.update(m);
    CHECK_EQ(e.body.pos.x.raw, x0.raw);
}
TEST(enemy_kill_flag){ Enemy e; e.kill(); CHECK(!e.alive); }
TEST(enemy_fire_immune_flag){ Enemy e; e.fire_immune=true; CHECK(e.fire_immune); Enemy d; CHECK(!d.fire_immune); }
