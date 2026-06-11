#pragma once
#include "logic/level_data.h"
#include "logic/collision.h"   // Body, aabb_overlap
#include "logic/fixed.h"
namespace logic {

// Resolve an entrance id to its spawn point. If the room defines an entrance with that id
// (rooms MAY define id 0), return it. Otherwise fall back to the room's default spawn tile
// (spawn_tx/ty) facing right (+1) — the dungeon-start behavior, used when entering at id 0 a
// room that has only an '@' spawn and no matching 'N'.
inline EntranceSpawn find_entrance(const LevelData& room, int id){
    for(int i = 0; i < room.entrance_count; ++i)
        if(room.entrances[i].id == id) return room.entrances[i];
    return EntranceSpawn{ 0, room.spawn_tx, room.spawn_ty, 1 };
}

// Return the first room-door whose 1-tile (16x16) trigger body overlaps the player, else null.
inline const RoomDoorSpawn* room_door_at(const LevelData& room, const Body& player){
    for(int i = 0; i < room.room_door_count; ++i){
        const RoomDoorSpawn& d = room.room_doors[i];
        Body db{}; db.half_w = Fixed::from_int(8); db.half_h = Fixed::from_int(8);
        db.pos = { Fixed::from_int(d.tx * 8), Fixed::from_int(d.ty * 8) };
        if(aabb_overlap(player, db)) return &room.room_doors[i];
    }
    return nullptr;
}
}
