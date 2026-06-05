#include "test_framework.h"
#include "game/levels/hub.h"
using namespace logic;
static int tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(hub_dims){ CHECK_EQ(HUB_DATA.w, 48); CHECK_EQ(HUB_DATA.h, 18); }
TEST(hub_border_solid){
    const LevelData& L = HUB_DATA;
    for(int x=0;x<L.w;++x){ CHECK_EQ(tile(L,x,0),1); CHECK_EQ(tile(L,x,L.h-1),1); }
    for(int y=0;y<L.h;++y){ CHECK_EQ(tile(L,0,y),1); CHECK_EQ(tile(L,L.w-1,y),1); }
}
TEST(hub_has_doors){
    CHECK_EQ(HUB_DATA.door_count, 4);
    // door 1 must exist (the only enterable one in M2)
    bool has_d1 = false;
    for(int i=0;i<HUB_DATA.door_count;++i) if(HUB_DATA.doors[i].dungeon == 1) has_d1 = true;
    CHECK(has_d1);
}
TEST(hub_gap_gate_wall){
    CHECK_EQ(HUB_DATA.gate_count, 15);            // full-height gap-gate wall
    CHECK(HUB_DATA.gates[0].type == GateType::Gap);
}
TEST(hub_spawn_on_empty){
    const LevelData& L = HUB_DATA;
    CHECK_EQ(tile(L, L.spawn_tx, L.spawn_ty), 0);
}
