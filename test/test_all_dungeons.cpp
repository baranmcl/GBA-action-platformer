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

// Task 2.6 (registry extension carried over from Task 2.4's review): every shipped room must be at
// least MIN_W x MIN_H. Bound inspected, not guessed: the smallest room across ALL shipped dungeons is
// D4 at 30 wide (single-room vertical dungeon) and D1/D2/D3's non-boss rooms at 19 tall (48x19) -- so
// 30x19 is the honest bound that holds for every room that exists today, not the 30x20/30x22 several
// per-dungeon copies asserted (those would have failed against D1-D3's own 19-tall rooms had they ever
// been run against them). This single loop replaces those now-redundant per-dungeon min-size copies
// (test_dungeon6/7/8_level.cpp's d6/d7/d8_rooms_min_size, deleted alongside this addition).
static constexpr int MIN_ROOM_W = 30, MIN_ROOM_H = 19;
TEST(all_dungeons_min_room_size){
    for(auto& e : ALL) for(int r = 0; r < e.d->room_count; ++r)
        harness::check_min_room_size(*e.d->rooms[r], e.name, MIN_ROOM_W, MIN_ROOM_H);
}
