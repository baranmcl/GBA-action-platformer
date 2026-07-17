#include "test_framework.h"
#include "level_harness.h"
#include "game/levels/dungeons.h"

struct Entry { const char* name; const logic::DungeonData* d; };
static const Entry ALL[] = {
    {"D1", &DUNGEON1_DUNGEON}, {"D2", &DUNGEON2_DUNGEON}, {"D3", &DUNGEON3_DUNGEON},
    {"D4", &DUNGEON4_DUNGEON}, {"D5", &DUNGEON5_DUNGEON}, {"D6", &DUNGEON6_DUNGEON},
    {"D7", &DUNGEON7_DUNGEON}, {"D8", &DUNGEON8_DUNGEON},
    {"D9A", &DUNGEON9_APPROACH}, {"D9X", &DUNGEON9_ARENA},
};

TEST(all_dungeons_solid_borders){ for(auto& e : ALL) for(int r = 0; r < e.d->room_count; ++r) harness::check_solid_border(*e.d->rooms[r], e.name); }
TEST(all_dungeons_room_doors_resolve){ for(auto& e : ALL) harness::check_room_doors_resolve(*e.d, e.name); }
TEST(all_dungeons_entrances_settle){ for(auto& e : ALL) for(int r = 0; r < e.d->room_count; ++r) harness::check_entrances_settle_safely(*e.d->rooms[r], e.name); }
