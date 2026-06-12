#include "test_framework.h"
#include "game/levels/dungeons.h"
using namespace logic;

// Thornwild Marsh (Dungeon 6) — the first multi-room grapple dungeon (M7).
// These tests assert the authored data is structurally valid and contains the
// required M7 beats. Geometry/feel reachability is verified separately on mGBA.

static const LevelData* const D6_ROOMS[] = {
    &DUNGEON6_ROOM0_DATA, &DUNGEON6_ROOM1_DATA, &DUNGEON6_ROOM2_DATA };
static constexpr int D6_N = 3;

TEST(d6_dungeon_table){
    CHECK_EQ(DUNGEON6_DUNGEON.room_count, 3);
    CHECK_EQ(DUNGEON6_DUNGEON.start_room, 0);
    CHECK(DUNGEON6_DUNGEON.rooms[0] == &DUNGEON6_ROOM0_DATA);
    CHECK(DUNGEON6_DUNGEON.rooms[2] == &DUNGEON6_ROOM2_DATA);
}

TEST(d6_rooms_min_size){
    // Camera clamp requires every room >= the 240x160 viewport (>=30 wide, >=20 tall).
    for(int r = 0; r < D6_N; ++r){
        CHECK(D6_ROOMS[r]->w >= 30);
        CHECK(D6_ROOMS[r]->h >= 20);
    }
}

TEST(d6_rooms_solid_border){
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int x = 0; x < L.w; ++x){
            CHECK_EQ((int)L.tiles[x], 1);                 // top row
            CHECK_EQ((int)L.tiles[(L.h-1)*L.w + x], 1);   // bottom row
        }
        for(int y = 0; y < L.h; ++y){
            CHECK_EQ((int)L.tiles[y*L.w + 0], 1);         // left col
            CHECK_EQ((int)L.tiles[y*L.w + (L.w-1)], 1);   // right col
        }
    }
}

TEST(d6_one_grapple_shrine){
    // Exactly one ability shrine across the set, and it grants Grapple.
    int shrines = 0; bool grapple = false;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.pickup_count; ++i){
            ++shrines;
            if(L.pickups[i].ability == Ability::Grapple) grapple = true;
        }
    }
    CHECK_EQ(shrines, 1);
    CHECK(grapple);
}

TEST(d6_one_spronk_one_exit){
    int cages = 0, exits = 0;
    for(int r = 0; r < D6_N; ++r){
        if(D6_ROOMS[r]->has_cage) ++cages;
        if(D6_ROOMS[r]->has_exit) ++exits;
    }
    CHECK_EQ(cages, 1);
    CHECK_EQ(exits, 1);
}

TEST(d6_has_grapple_anchor){
    // >=1 GrapplePoint anchor tile somewhere in the set (the headline grapple beat).
    int anchors = 0;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.w*L.h; ++i)
            if(L.tiles[i] == (uint8_t)TileKind::GrapplePoint) ++anchors;
    }
    CHECK(anchors >= 1);
}

TEST(d6_every_anchor_has_solid_below){
    // R1 grapple-landing rule: on arrival the engine scans DOWN the anchor column for the
    // first solid tile and snaps the player to STAND on top of it. So every authored anchor
    // MUST have a Solid tile directly below it (the ledge the player lands on) — otherwise the
    // player snaps to the distant floor (or out of bounds), which is the bug this dungeon fixes.
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int y = 0; y < L.h; ++y)
        for(int x = 0; x < L.w; ++x){
            if(L.tiles[y*L.w + x] != (uint8_t)TileKind::GrapplePoint) continue;
            CHECK(y + 1 < L.h);                                   // anchor not on the bottom row
            CHECK_EQ((int)L.tiles[(y+1)*L.w + x], (int)TileKind::Solid);  // solid ledge directly below
        }
    }
}

TEST(d6_floor_content_on_row_18){
    // Grounding guard (the R4 regression): floor-bound content — spawn, enemy, shrine, pullable
    // block — must sit on the CONTENT row (18) so a 16px sprite's bottom rests on the row-20 floor.
    // Row 19 sinks it; row 20 collides with the floor. (Ledge content like braziers/doors/cage/exit
    // legitimately lives on the higher ledge content row 12, grounded on the row-13 ledge surface;
    // those are covered by d6_no_content_on_gap_or_floor_row + d6_content_is_grounded below.)
    constexpr int ROW = 18;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        CHECK_EQ(L.spawn_ty, ROW);
        for(int i = 0; i < L.enemy_count; ++i)   CHECK_EQ((int)L.enemies[i].ty, ROW);
        for(int i = 0; i < L.pickup_count; ++i)  CHECK_EQ((int)L.pickups[i].ty, ROW);
        for(int i = 0; i < L.block_count; ++i)   CHECK_EQ((int)L.blocks[i].ty, ROW);
    }
}

TEST(d6_floor_content_is_grounded){
    // The shrine (regression #1) is positioned at a FIXED tile offset and its 16px body's feet land
    // at row ty+2 (no floor auto-scan) — so a shrine on row 19 has its feet at row 21 (the border)
    // and renders BELOW the floor. Assert each floor-bound content entity (spawn, enemy, shrine,
    // block) has a SOLID floor exactly two tiles below (row 18 -> solid row 20). This is the precise
    // grounding invariant R4 broke. (Braziers/doors/cage/exit auto-snap to floor_row_below() or are
    // static overlap targets, so they ground from the ledge surface regardless of content row.)
    auto grounded = [](const LevelData& L, int tx, int ty){
        return ty + 2 < L.h && (int)L.tiles[(ty + 2) * L.w + tx] == (int)TileKind::Solid;
    };
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        CHECK(grounded(L, L.spawn_tx, L.spawn_ty));
        for(int i = 0; i < L.enemy_count; ++i)  CHECK(grounded(L, L.enemies[i].tx, L.enemies[i].ty));
        for(int i = 0; i < L.pickup_count; ++i) CHECK(grounded(L, L.pickups[i].tx, L.pickups[i].ty));
        for(int i = 0; i < L.block_count; ++i)  CHECK(grounded(L, L.blocks[i].tx,  L.blocks[i].ty));
    }
}

TEST(d6_no_content_on_gap_or_floor_row){
    // Stronger anti-regression: NO content entity may be authored on the gap row (19) or the floor
    // row (20) in any room — those rows sink/clip the sprite. This catches the lava-on-content and
    // shrine-on-row-19 regressions for ALL entity kinds including ledge content.
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        auto bad = [](int ty){ return ty == 19 || ty == 20; };
        CHECK(!bad(L.spawn_ty));
        if(L.has_cage) CHECK(!bad(L.cage_ty));
        if(L.has_exit) CHECK(!bad(L.exit_ty));
        for(int i = 0; i < L.enemy_count; ++i)     CHECK(!bad((int)L.enemies[i].ty));
        for(int i = 0; i < L.pickup_count; ++i)    CHECK(!bad((int)L.pickups[i].ty));
        for(int i = 0; i < L.block_count; ++i)     CHECK(!bad((int)L.blocks[i].ty));
        for(int i = 0; i < L.brazier_count; ++i)   CHECK(!bad((int)L.braziers[i].ty));
        for(int i = 0; i < L.door_count; ++i)      CHECK(!bad((int)L.doors[i].ty));
        for(int i = 0; i < L.entrance_count; ++i)  CHECK(!bad((int)L.entrances[i].ty));
        for(int i = 0; i < L.room_door_count; ++i) CHECK(!bad((int)L.room_doors[i].ty));
    }
}

TEST(d6_has_pullable_block){
    // >=1 pullable block (the grapple pull-block puzzle).
    int pull = 0;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.block_count; ++i)
            if(L.blocks[i].pullable) ++pull;
    }
    CHECK(pull >= 1);
}

TEST(d6_has_latched_shortcut){
    // >=1 brazier-group with a latch_id >= 0 (the SRAM-persisted shortcut).
    int latched = 0;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.brazier_group_count; ++i)
            if(L.brazier_groups[i].latch_id >= 0) ++latched;
    }
    CHECK(latched >= 1);
}

TEST(d6_has_carried_kit_obstacle){
    // The combo theme: >=1 carried-power obstacle besides the grapple (e.g. a water
    // span to freeze with Ice, or a Fire/Ice gate).
    bool obstacle = false;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.w*L.h; ++i){
            uint8_t k = L.tiles[i];
            if(k == (uint8_t)TileKind::Water || k == (uint8_t)TileKind::IcePlatform ||
               k == (uint8_t)TileKind::Updraft || k == (uint8_t)TileKind::WindLeft ||
               k == (uint8_t)TileKind::WindRight) obstacle = true;
        }
        for(int i = 0; i < L.gate_count; ++i){
            GateType t = L.gates[i].type;
            if(t == GateType::Vine || t == GateType::Ice ||
               t == GateType::Water || t == GateType::FireWall) obstacle = true;
        }
    }
    CHECK(obstacle);
}

TEST(d6_room_doors_resolve_in_range){
    // Every room-door target_room is a valid room index, and the entrance the door
    // points at exists in the target room (cross-room links resolve).
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            CHECK(d.target_room >= 0 && d.target_room < D6_N);
            const LevelData& T = *D6_ROOMS[d.target_room];
            bool found = false;
            for(int e = 0; e < T.entrance_count; ++e)
                if(T.entrances[e].id == d.target_entrance) found = true;
            CHECK(found);
        }
    }
}

TEST(d6_bolt_enemy_per_room){
    // A magic source (bolt-enemy) in every room so carried spells stay affordable.
    for(int r = 0; r < D6_N; ++r)
        CHECK(D6_ROOMS[r]->enemy_count >= 1);
}
