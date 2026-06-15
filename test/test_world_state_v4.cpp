#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

// --- Layout constants ---

TEST(v4_save_version_is_4){
    CHECK_EQ((int)SAVE_VERSION, 4);
}

TEST(v4_save_data_size_is_20){
    CHECK_EQ((int)sizeof(SaveData), 20);
}

// --- v4 round-trip ---

TEST(v4_roundtrip_lives_survives){
    World w;
    w.free_spronk(1); w.free_spronk(3);
    w.grant(Ability::Fire); w.grant(Ability::Ice);
    w.set_latch(7); w.set_latch(15);
    w.current_dungeon = 2;
    w.lives = 2; // below default
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 4);
    World w2;
    CHECK(load_save(s, w2) == true);
    CHECK_EQ((int)w2.lives, 2);
    CHECK(w2.spronk_freed(1)); CHECK(w2.spronk_freed(3)); CHECK(!w2.spronk_freed(2));
    CHECK(w2.has(Ability::Fire)); CHECK(w2.has(Ability::Ice));
    CHECK(w2.latched(7)); CHECK(w2.latched(15)); CHECK(!w2.latched(0));
    CHECK_EQ((int)w2.current_dungeon, 2);
}

TEST(v4_roundtrip_full_lives){
    World w;
    w.free_spronk(1);  // max_lives = 4
    w.lives = 4;
    SaveData s = make_save(w);
    World w2;
    CHECK(load_save(s, w2) == true);
    CHECK_EQ((int)w2.lives, 4);
}

TEST(v4_roundtrip_zero_lives_clamped_on_load){
    // A v4 save with lives=0 must be clamped to max on load (boot safety).
    World w;
    w.free_spronk(2); // max = 4
    w.lives = 0;
    SaveData s = make_save(w);
    World w2;
    CHECK(load_save(s, w2) == true);
    // clamp_lives_on_load fires in the v4 branch — lives must be max (4), not 0
    CHECK_EQ((int)w2.lives, 4);
}

TEST(v4_roundtrip_lives_above_max_clamped_on_load){
    // lives=255 in the buffer (corruption); must clamp to max on load
    World w;
    // no spronks -> max = 3
    SaveData s = make_save(w);
    // Manually poke lives to 255 in the buffer and recompute checksum_v4
    uint8_t buf[sizeof(SaveData)];
    std::memcpy(buf, &s, sizeof(s));
    buf[16] = 255; // lives byte at offset 16
    // recompute checksum_v4: [0..10] + [12..15] + [16]
    uint32_t sum = 0;
    for(int i = 0; i < 11; ++i) sum += buf[i];
    for(int i = 12; i < 16; ++i) sum += buf[i];
    sum += buf[16];
    buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2;
    CHECK(load_save(s2, w2) == true);
    CHECK_EQ((int)w2.lives, 3); // clamped to max
}

// --- v3 -> v4 migration ---

TEST(v3_migrates_to_v4_sets_lives_to_starting){
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
}

// --- v2 -> v4 migration ---

TEST(v2_migrates_sets_lives_to_starting){
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
}

// --- v1 -> v4 migration ---

TEST(v1_migrates_sets_lives_to_starting){
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
}

// --- Corruption / empty SRAM (TEST-4) ---

TEST(v4_bad_magic_rejected){
    World w;
    SaveData s = make_save(w);
    // corrupt magic
    uint8_t buf[sizeof(SaveData)]; std::memcpy(buf, &s, sizeof(s));
    buf[0] ^= 0xFF;
    SaveData s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2;
    CHECK(load_save(s2, w2) == false);
}

TEST(v4_bad_checksum_rejected){
    World w;
    SaveData s = make_save(w);
    s.checksum ^= 0xFFu;
    World w2;
    CHECK(load_save(s, w2) == false);
}

TEST(v4_empty_sram_rejected){
    SaveData s{};
    World w2;
    CHECK(load_save(s, w2) == false);
}

TEST(v4_checksum_covers_lives_byte){
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

// --- checksum_v4 is distinct from checksum_v3 ---

TEST(v4_checksum_function_covers_correct_range){
    // A v4 save where lives != 0 should fail if only a v3 checksum were stored
    // (v3 doesn't cover byte [16]). Verify the checksum byte [11] changes when
    // we change lives (byte [16]) while keeping all other bytes fixed.
    World w1; w1.lives = 1;
    World w2; w2.lives = 2;
    SaveData s1 = make_save(w1);
    SaveData s2 = make_save(w2);
    const uint8_t* b1 = reinterpret_cast<const uint8_t*>(&s1);
    const uint8_t* b2 = reinterpret_cast<const uint8_t*>(&s2);
    // checksums must differ because lives differs
    CHECK(b1[11] != b2[11]);
}
