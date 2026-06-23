#include "test_framework.h"
#include "game/levels/hub.h"
using namespace logic;
static int tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(hub_dims){ CHECK_EQ(HUB_DATA.w, 58); CHECK_EQ(HUB_DATA.h, 20); }  // M5: 18->20 floor; M8: ->52 (Door 7); M10: ->58 (Door 8); M11: 9 doors re-spaced at pitch 6 within width 58
TEST(hub_border_solid){
    const LevelData& L = HUB_DATA;
    for(int x=0;x<L.w;++x){ CHECK_EQ(tile(L,x,0),1); CHECK_EQ(tile(L,x,L.h-1),1); }
    for(int y=0;y<L.h;++y){ CHECK_EQ(tile(L,0,y),1); CHECK_EQ(tile(L,L.w-1,y),1); }
}
TEST(hub_has_doors){
    CHECK_EQ(HUB_DATA.door_count, 9);             // M11: door 9 added (finale; opens at 8 spronks)
    // door 1 must exist (always enterable); door 8 (D8) and door 9 (finale) must exist after M11
    bool has_d1 = false, has_d8 = false, has_d9 = false;
    for(int i=0;i<HUB_DATA.door_count;++i){
        if(HUB_DATA.doors[i].dungeon == 1) has_d1 = true;
        if(HUB_DATA.doors[i].dungeon == 8) has_d8 = true;
        if(HUB_DATA.doors[i].dungeon == 9) has_d9 = true;
    }
    CHECK(has_d1); CHECK(has_d8); CHECK(has_d9);
}
TEST(hub_no_gates){
    // The vestigial full-height Gap-gate wall (required Featherleap, redundant with door gating) was
    // removed when the plaza was re-authored for even door spacing. The hub is now gate-free.
    CHECK_EQ(HUB_DATA.gate_count, 0);
}
TEST(hub_doors_evenly_spaced){
    // The 9 dungeon doors must be on one floor row, in dungeon order (1..9) left-to-right, with an
    // equal column pitch between consecutive doors (each a clean 2-wide archway, even gaps between).
    const LevelData& L = HUB_DATA;
    CHECK_EQ(L.door_count, 9);
    // doors are authored left-to-right as dungeons 1..9 on the same row
    for(int i=0;i<L.door_count;++i){
        CHECK_EQ(L.doors[i].dungeon, i+1);
        CHECK_EQ(L.doors[i].ty, L.doors[0].ty);
    }
    int pitch = L.doors[1].tx - L.doors[0].tx;
    CHECK_EQ(pitch, 6);                             // M11: pitch 6 (2-wide door + 4-tile gap)
    CHECK(pitch >= 4);                              // >=2-wide door + >=2-tile gap
    for(int i=1;i<L.door_count;++i)
        CHECK_EQ(L.doors[i].tx - L.doors[i-1].tx, pitch);  // perfectly even spacing
    // every door (incl. its 2-wide archway) fits on-map: tx in [1, w-2]
    for(int i=0;i<L.door_count;++i){
        CHECK(L.doors[i].tx >= 1);
        CHECK(L.doors[i].tx + 1 <= L.w - 2);
    }
}
TEST(hub_spawn_on_empty){
    const LevelData& L = HUB_DATA;
    CHECK_EQ(tile(L, L.spawn_tx, L.spawn_ty), 0);
}
TEST(hub_no_grapple_anchors){
    // The M7 hub grapple anchors + raised platforms were removed (unnecessary). The plaza is a
    // clean floor again: no GrapplePoint tiles remain.
    const LevelData& L = HUB_DATA;
    int anchors = 0;
    for(int i = 0; i < L.w*L.h; ++i)
        if(L.tiles[i] == (uint8_t)TileKind::GrapplePoint) ++anchors;
    CHECK_EQ(anchors, 0);
}
TEST(hub_doors_and_spawn_intact){
    // Removing the anchors/platforms must NOT disturb the door layout or the single spawn.
    const LevelData& L = HUB_DATA;
    CHECK_EQ(L.door_count, 9);
    CHECK_EQ(tile(L, L.spawn_tx, L.spawn_ty), 0);   // exactly-one '@' is enforced by the level compiler
    // No door tile got clobbered (doors sit on row 15).
    for(int i = 0; i < L.door_count; ++i)
        CHECK_EQ(tile(L, L.doors[i].tx, L.doors[i].ty), 0);
}
