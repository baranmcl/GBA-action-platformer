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
// --- M5 wind kit (GLIDE_VY=1 in player.cpp) ---
TEST(glide_caps_fall_speed_when_owned_and_held){
    static uint8_t c[3*40]; for(int i=0;i<3*40;++i) c[i]=0;          // tall empty column, no floor reached
    Tilemap m{3,40,c};
    Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
    p.body.pos={Fixed::from_int(8),Fixed::from_int(0)}; p.abilities.glide=true;
    InputFrame in{}; in.glide_held=true;
    for(int i=0;i<30;++i) p.update(in,m);
    CHECK(p.body.vel.y <= Fixed::from_int(1));           // never exceeds GLIDE_VY
}
TEST(no_glide_without_ability){
    static uint8_t c[3*40]; for(int i=0;i<3*40;++i) c[i]=0;
    Tilemap m{3,40,c};
    Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
    p.body.pos={Fixed::from_int(8),Fixed::from_int(0)};
    InputFrame in{}; in.glide_held=true;                 // held but ability NOT owned
    for(int i=0;i<12;++i) p.update(in,m);                // still airborne
    CHECK(p.body.vel.y > Fixed::from_int(1));            // falls fast, no cap
}
TEST(updraft_rises_only_while_gliding){
    static uint8_t c[3*10]; for(int i=0;i<3*10;++i) c[i]=0;
    for(int y=0;y<10;++y) c[y*3+1]=(uint8_t)TileKind::Updraft;       // updraft column x=1 (px 8..15)
    Tilemap m{3,10,c};
    Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
    p.body.pos={Fixed::from_int(8),Fixed::from_int(40)}; p.abilities.glide=true;
    InputFrame g{}; g.glide_held=true;  for(int i=0;i<8;++i) p.update(g,m);
    CHECK(p.body.vel.y < Fixed::from_int(0));            // GLIDING -> rises
    Player q; q.body.half_w=Fixed::from_int(3); q.body.half_h=Fixed::from_int(3);
    q.body.pos={Fixed::from_int(8),Fixed::from_int(40)}; q.abilities.glide=true;
    InputFrame ng{}; ng.glide_held=false; for(int i=0;i<8;++i) q.update(ng,m);
    CHECK(q.body.vel.y > Fixed::from_int(0));            // NOT gliding -> falls through
}
TEST(wind_pushes_horizontally){
    static uint8_t c[3*4]; for(int i=0;i<3*4;++i) c[i]=(uint8_t)TileKind::WindRight;
    Tilemap m{3,4,c};
    Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
    p.body.pos={Fixed::from_int(0),Fixed::from_int(0)};
    InputFrame in{}; p.update(in,m);
    CHECK(p.body.vel.x > Fixed::from_int(0));            // pushed right
}
TEST(ice_floor_is_slippery){
    auto coast = [](uint8_t floor_kind){
        static uint8_t c[6*3]; for(int i=0;i<6*3;++i) c[i]=0;
        for(int x=0;x<6;++x) c[2*6+x]=floor_kind;       // floor at row 2
        Tilemap m{6,3,c};
        Player p; p.body.half_w=Fixed::from_int(3); p.body.half_h=Fixed::from_int(3);
        p.body.pos={Fixed::from_int(8),Fixed::from_int(8)};
        InputFrame settle{}; for(int i=0;i<4;++i) p.update(settle,m);   // land on the floor
        p.body.vel.x = Fixed::from_int(2);                              // moving right
        InputFrame none{}; for(int i=0;i<4;++i) p.update(none,m);       // coast, no input
        return p.body.vel.x;
    };
    Fixed v_ice = coast((uint8_t)TileKind::IcePlatform);
    Fixed v_gnd = coast((uint8_t)TileKind::Solid);
    CHECK(v_ice > v_gnd);                                // slides further on ice (less friction)
}
