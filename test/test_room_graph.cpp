#include "test_framework.h"
#include "logic/room_graph.h"
#include "logic/level_data.h"
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/fixed.h"
using namespace logic;

static constexpr unsigned char T[] = { 1 };
static constexpr EntranceSpawn ENTR[] = { {0, 3, 5, 1}, {2, 20, 7, -1} };
static constexpr RoomDoorSpawn DOORS[] = { {12, 5, 1, 0}, {40, 7, 2, 2} };
static constexpr LevelData ROOM{ T, 50, 12, 9, 9, false,0,0, false,0,0,
    nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,nullptr,0,
    ENTR, 2, DOORS, 2 };

TEST(find_entrance_by_id){
    EntranceSpawn e = find_entrance(ROOM, 2);
    CHECK_EQ(e.id, 2); CHECK_EQ(e.tx, 20); CHECK_EQ(e.ty, 7); CHECK_EQ(e.facing, -1);
}
TEST(find_entrance_fallback_to_spawn){
    // id not present -> fall back to the room's default spawn tile, facing +1.
    EntranceSpawn e = find_entrance(ROOM, 99);
    CHECK_EQ(e.tx, 9); CHECK_EQ(e.ty, 9); CHECK_EQ(e.facing, 1);
}
TEST(room_door_at_overlap){
    // Player AABB centred on door 0's tile (12,5) -> px (96..). Body pos is top-left in px.
    Body p{}; p.half_w = Fixed::from_int(8); p.half_h = Fixed::from_int(16);
    p.pos = { Fixed::from_int(12*8), Fixed::from_int(5*8) };
    const RoomDoorSpawn* d = room_door_at(ROOM, p);
    CHECK(d != nullptr);
    CHECK_EQ(d->target_room, 1);
}
TEST(room_door_at_none){
    Body p{}; p.half_w = Fixed::from_int(8); p.half_h = Fixed::from_int(16);
    p.pos = { Fixed::from_int(2*8), Fixed::from_int(2*8) };   // far from any door
    CHECK(room_door_at(ROOM, p) == nullptr);
}
