#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

// --- Layout constants ---

TEST(v5_save_version_is_5){ CHECK_EQ((int)SAVE_VERSION, 5); }
TEST(v5_save_data_size_is_20){ CHECK_EQ((int)sizeof(SaveData), 20); }

// --- v5 round-trip ---

TEST(v5_roundtrip_beaten_true){
    World w; w.free_spronk(1); w.beaten = true; w.lives = 4;
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 5);
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.beaten == true);
    CHECK_EQ((int)w2.lives, 4);
}
TEST(v5_roundtrip_beaten_false_default){
    World w; SaveData s = make_save(w);
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.beaten == false);
}
TEST(v5_checksum_covers_beaten_byte){
    World w; w.beaten = true; SaveData s = make_save(w);
    uint8_t buf[sizeof(SaveData)]; std::memcpy(buf, &s, sizeof(s));
    buf[17] ^= 0xFFu; // corrupt beaten without recomputing checksum
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2; CHECK(load_save(s2, w2) == false);
}

// --- lives clamp / coverage (ported from the v4 file, adapted to v5) ---

TEST(v5_roundtrip_zero_lives_clamped_on_load){
    // A v5 save with lives=0 must be clamped to max on load (boot safety).
    World w;
    w.free_spronk(2); // max = 4
    w.lives = 0;
    SaveData s = make_save(w);
    World w2;
    CHECK(load_save(s, w2) == true);
    CHECK_EQ((int)w2.lives, 4);
}

TEST(v5_roundtrip_lives_above_max_clamped_on_load){
    // lives=255 in the buffer (corruption); must clamp to max on load
    World w;
    // no spronks -> max = 3
    SaveData s = make_save(w);
    uint8_t buf[sizeof(SaveData)];
    std::memcpy(buf, &s, sizeof(s));
    buf[16] = 255; // lives byte at offset 16
    // recompute checksum_v5: [0..10] + [12..17]
    uint32_t sum = 0;
    for(int i = 0; i < 11; ++i) sum += buf[i];
    for(int i = 12; i < 18; ++i) sum += buf[i];
    buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2;
    CHECK(load_save(s2, w2) == true);
    CHECK_EQ((int)w2.lives, 3); // clamped to max
}

TEST(v5_checksum_covers_lives_byte){
    // Flip the lives byte — checksum must reject it without recomputing
    World w;
    w.lives = 2;
    SaveData s = make_save(w);
    uint8_t buf[sizeof(SaveData)]; std::memcpy(buf, &s, sizeof(s));
    buf[16] ^= 0xFFu; // corrupt lives without touching checksum
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2;
    CHECK(load_save(s2, w2) == false);
}

// --- v4 on-disk -> v5 migration ---

TEST(v4_on_disk_migrates_to_v5_beaten_false){
    // Hand-build a valid v4 buffer (version=4, checksum_v4). Must load, beaten=false.
    uint8_t buf[sizeof(SaveData)]; std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 4; std::memcpy(buf + 4, &ver, 2);
    uint16_t spr = 0x00FF; std::memcpy(buf + 6, &spr, 2); // all 8 spronks
    uint16_t abil = 0x00FF; std::memcpy(buf + 8, &abil, 2);
    buf[10] = 0;
    uint32_t lat = 0; std::memcpy(buf + 12, &lat, 4);
    buf[16] = 5; // lives (will clamp to max=11; fine)
    // checksum_v4: [0..10] + [12..16]
    uint32_t sum = 0; for(int i=0;i<11;++i) sum+=buf[i]; for(int i=12;i<17;++i) sum+=buf[i];
    buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(s));
    World w; CHECK(load_save(s, w) == true);
    CHECK(w.spronk_freed(8)); CHECK(w.beaten == false);
}

// --- v3 -> v5 migration ---

TEST(v3_on_disk_migrates_to_v5_sets_lives_and_beaten){
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
    CHECK(w.spronk_freed(1)); CHECK(w.spronk_freed(3)); CHECK(!w.spronk_freed(2));
    CHECK(w.has(Ability::Featherleap));
    CHECK_EQ((int)w.current_dungeon, 1);
    CHECK_EQ((int)w.latches, (int)0xDEAD0001);
    CHECK_EQ((int)w.lives, (int)World::STARTING_LIVES);
    CHECK_EQ((int)w.lives, 3);
    CHECK(w.beaten == false);
}

// --- v2 -> v5 migration ---

TEST(v2_on_disk_migrates_to_v5_sets_lives_and_beaten){
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
    CHECK(w.beaten == false);
}

// --- v1 -> v5 migration ---

TEST(v1_on_disk_migrates_to_v5_sets_lives_and_beaten){
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
    CHECK(w.beaten == false);
}

// --- Corruption / empty SRAM (TEST-4) ---

TEST(v5_bad_magic_rejected){
    World w;
    SaveData s = make_save(w);
    uint8_t buf[sizeof(SaveData)]; std::memcpy(buf, &s, sizeof(s));
    buf[0] ^= 0xFF;
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2;
    CHECK(load_save(s2, w2) == false);
}

TEST(v5_bad_checksum_rejected){ World w; SaveData s = make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s, w2) == false); }

TEST(v5_empty_sram_rejected){ SaveData s{}; World w2; CHECK(load_save(s, w2) == false); }

// --- checksum_v5 distinct: changing beaten changes the stored checksum ---

TEST(v5_checksum_function_covers_beaten_range){
    World w1; w1.beaten = false;
    World w2; w2.beaten = true;
    SaveData s1 = make_save(w1);
    SaveData s2 = make_save(w2);
    const uint8_t* b1 = reinterpret_cast<const uint8_t*>(&s1);
    const uint8_t* b2 = reinterpret_cast<const uint8_t*>(&s2);
    CHECK(b1[11] != b2[11]);
}
