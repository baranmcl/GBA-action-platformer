#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

// NOTE: as of save v5, make_save() produces v5 and SAVE_VERSION==5.
// Current-version round-trip + corruption coverage now lives in test_world_state_v5.cpp.
// This file retains only the hand-built v1/v2/v3 ON-DISK migration tests — they read
// fixed legacy buffers and assert the resulting (now v5) World, so they remain valid.

// --- v3 -> v5 migration ---

TEST(v3_migrates_to_v5_sets_lives_to_starting){
    // Hand-build a v3-layout 16-byte buffer (version=3, valid v3 checksum).
    // The 20-byte SaveData struct is fine: we only write 16 meaningful bytes,
    // and the v3 branch only reads the first 16 (version at [4..5] fires v3 path).
    uint8_t buf[sizeof(SaveData)];
    std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 3; std::memcpy(buf + 4, &ver, 2);
    uint16_t spr = 0x0005; std::memcpy(buf + 6, &spr, 2);   // spronks 1 + 3
    uint16_t abil = 0x0001; std::memcpy(buf + 8, &abil, 2); // Featherleap
    buf[10] = 1;                                              // current_dungeon = 1
    uint32_t lat = 0xDEAD0001; std::memcpy(buf + 12, &lat, 4);
    // checksum_v3: [0..10] + [12..15]
    uint32_t sum = 0;
    for(int i = 0; i < 11; ++i) sum += buf[i];
    for(int i = 12; i < 16; ++i) sum += buf[i];
    buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(s));
    World w;
    CHECK(load_save(s, w) == true);
    // Progress preserved
    CHECK(w.spronk_freed(1)); CHECK(w.spronk_freed(3)); CHECK(!w.spronk_freed(2));
    CHECK(w.has(Ability::Featherleap));
    CHECK_EQ((int)w.current_dungeon, 1);
    CHECK_EQ((int)w.latches, (int)0xDEAD0001);
    // Migration: lives defaults to STARTING_LIVES
    CHECK_EQ((int)w.lives, (int)World::STARTING_LIVES);
    CHECK_EQ((int)w.lives, 3);
    CHECK(w.beaten == false); // beaten defaults false on migration
}

// --- v2 -> v5 migration ---

TEST(v2_migrates_to_v5_sets_lives_to_starting){
    uint8_t buf[sizeof(SaveData)];
    std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 2; std::memcpy(buf + 4, &ver, 2);
    uint16_t spr = 0x0001; std::memcpy(buf + 6, &spr, 2);
    uint16_t abil = 0x0002; std::memcpy(buf + 8, &abil, 2); // Fire
    buf[10] = 0;
    uint32_t sum = 0; for(int i = 0; i < 11; ++i) sum += buf[i]; buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(s));
    World w;
    CHECK(load_save(s, w) == true);
    CHECK(w.spronk_freed(1));
    CHECK(w.has(Ability::Fire));
    CHECK_EQ((int)w.latches, 0);
    CHECK_EQ((int)w.lives, (int)World::STARTING_LIVES);
    CHECK(w.beaten == false); // beaten defaults false on migration
}

// --- v1 -> v5 migration ---

TEST(v1_migrates_to_v5_sets_lives_to_starting){
    uint8_t buf[sizeof(SaveData)];
    std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 1; std::memcpy(buf + 4, &ver, 2);
    uint16_t flags = 0x0003; std::memcpy(buf + 6, &flags, 2); // spronk1 | featherleap
    uint32_t sum = 0; for(int i = 0; i < 8; ++i) sum += buf[i]; buf[8] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(s));
    World w;
    CHECK(load_save(s, w) == true);
    CHECK(w.spronk_freed(1));
    CHECK(w.has(Ability::Featherleap));
    CHECK_EQ((int)w.lives, (int)World::STARTING_LIVES);
    CHECK(w.beaten == false); // beaten defaults false on migration
}
