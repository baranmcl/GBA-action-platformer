#include "test_framework.h"
#include "logic/player.h"
using namespace logic;
static uint8_t floor_cells[] = {0,0,0,0, 0,0,0,0, 0,0,0,0, 1,1,1,1};
static Tilemap floormap(){ return Tilemap{4,4,floor_cells}; }
static Player grounded_player(){
    Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
    p.body.pos={Fixed::from_int(8),Fixed::from_int(18)}; return p;
}
TEST(walk_right_accelerates){
    Player p=grounded_player(); Tilemap m=floormap();
    InputFrame in{}; in.right=true; p.update(in,m); CHECK(p.body.vel.x.raw>0);
}
TEST(jump_only_when_grounded){
    Player p=grounded_player(); Tilemap m=floormap();
    InputFrame settle{}; p.update(settle,m); CHECK(p.body.on_ground==true);
    InputFrame jin{}; jin.jump_pressed=true; p.update(jin,m); CHECK(p.body.vel.y.raw<0);
}
TEST(no_double_jump_without_ability){
    Player p=grounded_player(); p.abilities.featherleap=false; Tilemap m=floormap();
    InputFrame s{}; p.update(s,m);
    InputFrame j{}; j.jump_pressed=true; p.update(j,m); // first jump
    p.update(s,m);                                      // airborne
    Fixed vy_before=p.body.vel.y; p.update(j,m);        // attempt double jump
    CHECK(p.body.vel.y.raw >= vy_before.raw);           // no upward boost
}
TEST(double_jump_with_ability){
    Player p=grounded_player(); p.abilities.featherleap=true; Tilemap m=floormap();
    InputFrame s{}; p.update(s,m);
    InputFrame j{}; j.jump_pressed=true; p.update(j,m);
    p.update(s,m);
    p.update(j,m);                                      // double jump
    CHECK(p.body.vel.y.raw < 0); CHECK(p.air_jumps_left==0);
}
