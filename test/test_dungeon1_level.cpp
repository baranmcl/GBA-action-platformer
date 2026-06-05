#include "test_framework.h"
#include "game/levels/dungeon1.h"
using namespace logic;
static int tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(d1_dims){ CHECK_EQ(DUNGEON1_DATA.w, 48); CHECK_EQ(DUNGEON1_DATA.h, 20); }
TEST(d1_spawn_in_bounds_on_empty){
    const LevelData& L = DUNGEON1_DATA;
    CHECK(L.spawn_tx > 0 && L.spawn_tx < L.w-1);
    CHECK(L.spawn_ty > 0 && L.spawn_ty < L.h-1);
    CHECK_EQ(tile(L, L.spawn_tx, L.spawn_ty), 0);
}
TEST(d1_has_cage_and_exit){ CHECK(DUNGEON1_DATA.has_cage); CHECK(DUNGEON1_DATA.has_exit); }
TEST(d1_one_enemy){ CHECK_EQ(DUNGEON1_DATA.enemy_count, 1); CHECK_EQ(DUNGEON1_DATA.enemies[0].param0, 14); CHECK_EQ(DUNGEON1_DATA.enemies[0].param1, 22); }
TEST(d1_border_solid){
    const LevelData& L = DUNGEON1_DATA;
    for(int x=0;x<L.w;++x){ CHECK_EQ(tile(L,x,0),1); CHECK_EQ(tile(L,x,L.h-1),1); }
    for(int y=0;y<L.h;++y){ CHECK_EQ(tile(L,0,y),1); CHECK_EQ(tile(L,L.w-1,y),1); }
}
