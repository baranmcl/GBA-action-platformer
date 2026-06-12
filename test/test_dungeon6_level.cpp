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

// ----------------------------------------------------------------------------
// M7 re-author (Thornwild Marsh playtest fixes). The five issues fixed below:
//   1. Brazier wouldn't light  -> braziers now on the floor content row (solid below).
//   2. Pull-block purpose unclear -> one block + one plate (target = the gate) + one gate.
//   3. Freeze unnecessary       -> Room 2 water gap >=6 wide, unbridged (Ice required).
//   4. Spronk sank in platform   -> cage + exit on the floor content row (grounded).
//   5. One-way doors             -> every door is paired with a co-located return entrance.
// ----------------------------------------------------------------------------

static bool d6_solid(const LevelData& L, int tx, int ty){
    if(tx < 0 || ty < 0 || tx >= L.w || ty >= L.h) return true; // OOB is solid
    return (int)L.tiles[ty*L.w + tx] == (int)TileKind::Solid;
}

TEST(d6_brazier_on_floor_row){
    // Issue #1: every brazier sits where its hardcoded fire-hit zone (rows 14..19) is valid AND
    // has a solid tile directly below it (so it grounds and lights). A brazier on a ledge (the old
    // bug) fails this. The single D6 brazier is in Room 1 on row 18, solid (row 19) directly below.
    int braziers = 0;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.brazier_count; ++i){
            ++braziers;
            int bx = L.braziers[i].tx, by = L.braziers[i].ty;
            CHECK_EQ(by, 18);                         // floor content row (was a ledge before -> the bug)
            CHECK(d6_solid(L, bx, 20));               // solid floor below -> grounds; hit-zone rows 14..19 valid
        }
    }
    CHECK(braziers >= 1);
}

TEST(d6_cage_and_exit_on_floor_row){
    // Issue #4: the spronk cage 'C' and the exit 'E' are SPRITES; on a ledge they sink. They must
    // be on the floor content row (18) with a solid floor below so they ground. (Floor-scanned in
    // the engine, but we assert solid-below to guarantee a real floor exists.)
    int cages = 0, exits = 0;
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        if(L.has_cage){ ++cages; CHECK_EQ(L.cage_ty, 18); CHECK(d6_solid(L, L.cage_tx, 20)); }
        if(L.has_exit){ ++exits; CHECK_EQ(L.exit_ty, 18); CHECK(d6_solid(L, L.exit_tx, 20)); }
    }
    CHECK_EQ(cages, 1);
    CHECK_EQ(exits, 1);
}

// Read the generated per-room arrays directly so the door/entrance wiring is machine-checked.
#include "game/levels/dungeon6_room0.h"
#include "game/levels/dungeon6_room1.h"
#include "game/levels/dungeon6_room2.h"

static bool d6_has_entrance(const LevelData& L, int id){
    for(int i = 0; i < L.entrance_count; ++i) if(L.entrances[i].id == id) return true;
    return false;
}
static bool d6_has_room_door(const LevelData& L, int target_room, int target_entrance){
    for(int i = 0; i < L.room_door_count; ++i)
        if(L.room_doors[i].target_room == target_room &&
           L.room_doors[i].target_entrance == target_entrance) return true;
    return false;
}

TEST(d6_doors_are_two_way){
    // Issue #5: every connection is a PAIR (door + co-located return entrance, cross-referenced),
    // so the player can always walk back to where they came from.
    // Room 0 hubs out to Room 1 and Room 2:
    CHECK(d6_has_room_door(DUNGEON6_ROOM0_DATA, 1, 0));
    CHECK(d6_has_room_door(DUNGEON6_ROOM0_DATA, 2, 0));
    // Room 1 returns to Room 0 entrance 1; its own arrival entrance (id0) exists:
    CHECK(d6_has_room_door(DUNGEON6_ROOM1_DATA, 0, 1));
    CHECK(d6_has_entrance(DUNGEON6_ROOM1_DATA, 0));
    // Room 2 returns to Room 0 entrance 2; its own arrival entrance (id0) exists:
    CHECK(d6_has_room_door(DUNGEON6_ROOM2_DATA, 0, 2));
    CHECK(d6_has_entrance(DUNGEON6_ROOM2_DATA, 0));
    // Room 0 offers the two distinct return entrances the children target:
    CHECK(d6_has_entrance(DUNGEON6_ROOM0_DATA, 1));
    CHECK(d6_has_entrance(DUNGEON6_ROOM0_DATA, 2));
    // Each room-door is co-located with a return entrance (adjacent, within 2 tiles, same row) so
    // arriving places the player ON/BESIDE the door back. Check every door has a nearby entrance.
    for(int r = 0; r < D6_N; ++r){
        const LevelData& L = *D6_ROOMS[r];
        for(int i = 0; i < L.room_door_count; ++i){
            const RoomDoorSpawn& d = L.room_doors[i];
            bool near = false;
            for(int e = 0; e < L.entrance_count; ++e){
                const EntranceSpawn& n = L.entrances[e];
                int dx = n.tx - d.tx; if(dx < 0) dx = -dx;
                if(n.ty == d.ty && dx >= 1 && dx <= 2) near = true; // adjacent, not on the same tile
            }
            CHECK(near);
        }
    }
}

TEST(d6_water_gap_requires_ice){
    // Issue #3: Room 2 has a contiguous water run on the floor row (20) of width >=6 with NO solid
    // tile bridging it, so it cannot be walked or (being that wide) jumped/dashed -- the player MUST
    // freeze it with Ice to cross to the spronk. Assert the longest unbridged water run >= 6.
    const LevelData& L = DUNGEON6_ROOM2_DATA;
    constexpr int ROW = 20;
    int best = 0, run = 0;
    for(int x = 0; x < L.w; ++x){
        if((int)L.tiles[ROW*L.w + x] == (int)TileKind::Water){ ++run; if(run > best) best = run; }
        else run = 0;
    }
    CHECK(best >= 6);
    // And no solid floor inside that run (the run itself is the gap -- already implied, but assert
    // the floor row has the water contiguous, not solid-water-solid stepping stones).
    int water = 0; for(int x = 0; x < L.w; ++x) if((int)L.tiles[ROW*L.w + x] == (int)TileKind::Water) ++water;
    CHECK_EQ(water, best);   // all water on the row belongs to the single contiguous run
}

TEST(d6_pull_block_onto_plate_solvable){
    // Issue #2: Room 1 has EXACTLY one pullable block, one plate, and one gate; the plate's target
    // is the gate column; and the geometry makes a PULL the solution and a push fail.
    const LevelData& L = DUNGEON6_ROOM1_DATA;
    int pull = 0; for(int i = 0; i < L.block_count; ++i) if(L.blocks[i].pullable) ++pull;
    CHECK_EQ(pull, 1);
    CHECK_EQ(L.plate_count, 1);
    CHECK_EQ(L.gate_count, 1);
    const BlockSpawn& blk = L.blocks[0];
    const PlateSpawn& pl  = L.plates[0];
    const GateSpawn&  gt  = L.gates[0];
    // Plate targets the gate's column (lighting/holding the plate opens the gate).
    CHECK_EQ(pl.target_tx, gt.tx);
    // Block rests on the SAME row as the plate, one tile away, and PULLABLE toward the plate.
    CHECK_EQ(blk.ty, pl.ty);
    CHECK_EQ(blk.tx, pl.tx + 1);              // block is right of the plate -> pulled LEFT onto it
    // The plate tile is non-solid (so the block can sit on it) and has solid directly below (so the
    // block does not fall off the plate row).
    CHECK(!d6_solid(L, pl.tx, pl.ty));
    CHECK(d6_solid(L, pl.tx, pl.ty + 1));
    // PUSH must FAIL: the tile to the block's right (away from the player) is the closed gate
    // column -> solid -> the block cannot be pushed forward; only a pull resolves the puzzle.
    CHECK_EQ(gt.tx, blk.tx + 1);
}

TEST(d6_room1_has_heart_container){
    // Part B reward: Room 1 has EXACTLY one heart container, on the floor content row (ty==18) with
    // a solid floor below, sealed in the pocket BEHIND the brazier shortcut wall (col >= the brazier
    // group's target column, i.e. inside the area the brazier opens). It must NOT be reachable before
    // lighting the brazier: the 2-wide shortcut wall (the brazier target column + the next) seals it.
    const LevelData& L = DUNGEON6_ROOM1_DATA;
    CHECK_EQ(L.heart_container_count, 1);
    const HeartContainerSpawn& hc = L.heart_containers[0];
    CHECK_EQ(hc.ty, 18);                          // floor content row
    CHECK(d6_solid(L, hc.tx, hc.ty + 2));         // solid floor two below (grounds the 16px sprite)
    CHECK(!d6_solid(L, hc.tx, hc.ty));            // the container's own tile is open (stands on it)
    // The container is in the pocket to the RIGHT of the brazier shortcut wall the brazier opens.
    CHECK_EQ(L.brazier_group_count, 1);
    int wall_tx = L.brazier_groups[0].target_tx;  // open_column opens columns wall_tx and wall_tx+1
    CHECK(hc.tx > wall_tx + 1);                    // container sits beyond the 2-wide opened wall
    // Pocket SEALED until lit: the full-height shortcut wall (both opened columns) is solid on the
    // content row, so the player cannot cross into the pocket before the brazier opens it.
    CHECK(d6_solid(L, wall_tx,     18));
    CHECK(d6_solid(L, wall_tx + 1, 18));
}

TEST(d6_room1_block_cannot_fall_off){
    // Soft-lock fix (III): the pull-block's reachable horizontal range sits entirely on a CONTINUOUS
    // solid sub-floor (row 19), so the block can never drop a vertical level (which would strand it
    // off the plate, unrecoverable). The block starts at blk.tx and is pulled LEFT toward the player;
    // a solid stopper wall one tile left of the plate caps the range so it can't be pulled off the
    // sub-floor's left edge. Reachable columns = [plate.tx, block.tx]; assert solid directly below
    // every one of them, AND a solid stopper at plate.tx-1 (block.row) so it can't go further left.
    const LevelData& L = DUNGEON6_ROOM1_DATA;
    const BlockSpawn& blk = L.blocks[0];
    const PlateSpawn& pl  = L.plates[0];
    int lo = pl.tx, hi = blk.tx;                  // pull range: plate (left) .. block start (right)
    CHECK(lo <= hi);
    for(int x = lo; x <= hi; ++x)
        CHECK(d6_solid(L, x, blk.ty + 1));        // continuous solid sub-floor under the whole range
    // Stopper wall: the tile one column LEFT of the plate, on the block's row, is solid -> the block
    // cannot be over-pulled off the sub-floor's left edge.
    CHECK(d6_solid(L, pl.tx - 1, blk.ty));
}

// ----------------------------------------------------------------------------
// R8: the D6 player carries the FULL kit (Featherleap double-jump, Glide, Dash, Fire, Ice).
// Puzzles must resist ALL of them, not just Dash. These two invariants encode the geometry that
// makes the Ice freeze and the pull-block UNSKIPPABLE by jumping/double-jumping/gliding/dashing.
// ----------------------------------------------------------------------------

TEST(d6_water_corridor_has_ceiling){
    // Issue A: an OPEN water gap is glide/featherleap-crossable (~12-14 tiles of air reach), so
    // widening it never helps. The fix is a TUNNEL: a solid ceiling caps the water span to a
    // ~4-tall corridor (rows 16..19 open, floor=water on row 20), giving the player ~0 tiles of
    // headroom -> they cannot get airborne to jump/featherleap/glide, so the ONLY way across is to
    // freeze the water into an IcePlatform floor and walk. Assert, for EVERY water column on row 20:
    //   * the player corridor (rows 16,17,18,19 = the 4 tiles a standing player occupies) is passable
    //   * the tile directly above the player's head row (row 15, capping row 16) is SOLID
    // i.e. <=1 tile of jump clearance across the whole span.
    const LevelData& L = DUNGEON6_ROOM2_DATA;
    constexpr int FLOOR = 20, HEAD = 16, CEIL = 15;
    int capped = 0, water_cols = 0;
    for(int x = 0; x < L.w; ++x){
        if((int)L.tiles[FLOOR*L.w + x] != (int)TileKind::Water) continue;
        ++water_cols;
        // 4-tall corridor open (so the player fits AFTER freezing) ...
        for(int y = HEAD; y < FLOOR; ++y) CHECK(!d6_solid(L, x, y));
        // ... but capped immediately above the head row -> no room to jump/glide.
        CHECK(d6_solid(L, x, CEIL));
        if(d6_solid(L, x, CEIL)) ++capped;
    }
    CHECK(water_cols >= 6);
    CHECK_EQ(capped, water_cols);   // the ENTIRE water span is ceilinged (no open arc-over column)
}

TEST(d6_room1_gate_is_full_barrier){
    // Issue B: the Gap gate must be a FULL-HEIGHT barrier so the pull-block-onto-plate cannot be
    // skipped by jumping/double-jumping/gliding/dashing over it. The engine's fill_column makes a
    // closed gate solid rows 1..h-3 at runtime, but we ALSO author a static ceiling above the gate
    // column so the barrier is verifiable in the level data: from row 1 down to the row above the
    // 4-tall passage (row 15), the gate column is solid -> nothing to jump over.
    const LevelData& L = DUNGEON6_ROOM1_DATA;
    CHECK_EQ(L.gate_count, 1);
    const GateSpawn& gt = L.gates[0];
    // The gate is a 2-wide column (tx, tx+1). Both columns must be statically capped solid from the
    // top border down to row 15 (the cap above the player's head when standing on the floor),
    // leaving only the <=4-tall runtime-gated passage (rows 16..18) -> no jump/glide bypass.
    for(int dx = 0; dx <= 1; ++dx)
        for(int y = 1; y <= 15; ++y)
            CHECK(d6_solid(L, gt.tx + dx, y));
    // And the gate sits on the floor content row (its passage grounds on the row-19 platform / row-20
    // floor), so opening it yields a real walkable opening rather than a floating gap.
    CHECK_EQ(gt.ty, 18);
    CHECK(d6_solid(L, gt.tx, 19));
}
