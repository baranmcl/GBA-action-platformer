#include "test_framework.h"
#include "game/levels/hub.h"
using namespace logic;
static int tile(const LevelData& L, int x, int y){ return (int)L.tiles[y*L.w + x]; }
TEST(hub_dims){ CHECK_EQ(HUB_DATA.w, 52); CHECK_EQ(HUB_DATA.h, 20); }  // M5: 18->20 floor fills screen; M8: 48->52 for Door 7
TEST(hub_border_solid){
    const LevelData& L = HUB_DATA;
    for(int x=0;x<L.w;++x){ CHECK_EQ(tile(L,x,0),1); CHECK_EQ(tile(L,x,L.h-1),1); }
    for(int y=0;y<L.h;++y){ CHECK_EQ(tile(L,0,y),1); CHECK_EQ(tile(L,L.w-1,y),1); }
}
TEST(hub_has_doors){
    CHECK_EQ(HUB_DATA.door_count, 7);             // M8: door 7 added (D1-D7 built)
    // door 1 must exist (always enterable); door 6 (D6) and door 7 (D7) must exist after M8
    bool has_d1 = false, has_d6 = false, has_d7 = false;
    for(int i=0;i<HUB_DATA.door_count;++i){
        if(HUB_DATA.doors[i].dungeon == 1) has_d1 = true;
        if(HUB_DATA.doors[i].dungeon == 6) has_d6 = true;
        if(HUB_DATA.doors[i].dungeon == 7) has_d7 = true;
    }
    CHECK(has_d1); CHECK(has_d6); CHECK(has_d7);
}
TEST(hub_no_gates){
    // The vestigial full-height Gap-gate wall (required Featherleap, redundant with door gating) was
    // removed when the plaza was re-authored for even door spacing. The hub is now gate-free.
    CHECK_EQ(HUB_DATA.gate_count, 0);
}
TEST(hub_doors_evenly_spaced){
    // The 7 dungeon doors must be on one floor row, in dungeon order (1..7) left-to-right, with an
    // equal column pitch between consecutive doors (each a clean 2-wide archway, even gaps between).
    const LevelData& L = HUB_DATA;
    CHECK_EQ(L.door_count, 7);
    // doors are authored left-to-right as dungeons 1..7 on the same row
    for(int i=0;i<L.door_count;++i){
        CHECK_EQ(L.doors[i].dungeon, i+1);
        CHECK_EQ(L.doors[i].ty, L.doors[0].ty);
    }
    int pitch = L.doors[1].tx - L.doors[0].tx;
    CHECK(pitch >= 4);                              // >=2-wide door + >=2-tile gap
    for(int i=1;i<L.door_count;++i)
        CHECK_EQ(L.doors[i].tx - L.doors[i-1].tx, pitch);  // perfectly even spacing
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
    CHECK_EQ(L.door_count, 7);
    CHECK_EQ(tile(L, L.spawn_tx, L.spawn_ty), 0);   // exactly-one '@' is enforced by the level compiler
    // No door tile got clobbered (doors sit on row 15).
    for(int i = 0; i < L.door_count; ++i)
        CHECK_EQ(tile(L, L.doors[i].tx, L.doors[i].ty), 0);
}
