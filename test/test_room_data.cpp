#include "test_framework.h"
#include "logic/level_data.h"
using namespace logic;

// A minimal room: 1 entrance, 1 room-door, no other content.
static constexpr unsigned char R_TILES[] = { 1 };
static constexpr EntranceSpawn  R_ENTR[]  = { {0, 3, 5, 1}, {1, 10, 5, -1} };
static constexpr RoomDoorSpawn  R_DOORS[] = { {12, 5, 1, 0} };  // at (12,5) -> room 1, entrance 0

TEST(leveldata_defaults_room_fields_absent){
    // A LevelData built the OLD way (no entrance/room-door fields) defaults them empty.
    LevelData lv{ R_TILES, 1, 1, 0, 0, false, 0, 0, false, 0, 0,
                  nullptr,0, nullptr,0, nullptr,0, nullptr,0, nullptr,0,
                  nullptr,0, nullptr,0, nullptr,0, nullptr,0 };
    CHECK(lv.entrances == nullptr);
    CHECK_EQ(lv.entrance_count, 0);
    CHECK(lv.room_doors == nullptr);
    CHECK_EQ(lv.room_door_count, 0);
}

TEST(entrance_and_roomdoor_fields){
    EntranceSpawn e = R_ENTR[1];
    CHECK_EQ(e.id, 1); CHECK_EQ(e.tx, 10); CHECK_EQ(e.ty, 5); CHECK_EQ(e.facing, -1);
    RoomDoorSpawn d = R_DOORS[0];
    CHECK_EQ(d.tx, 12); CHECK_EQ(d.ty, 5); CHECK_EQ(d.target_room, 1); CHECK_EQ(d.target_entrance, 0);
}

TEST(dungeondata_indexes_rooms){
    static constexpr LevelData ROOMA{ R_TILES,1,1,0,0,false,0,0,false,0,0,
        nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0 };
    static constexpr LevelData ROOMB = ROOMA;
    static constexpr const LevelData* ROOMS[] = { &ROOMA, &ROOMB };
    DungeonData dg{ ROOMS, 2, 0 };
    CHECK_EQ(dg.room_count, 2);
    CHECK_EQ(dg.start_room, 0);
    CHECK(dg.rooms[1] == &ROOMB);
}
