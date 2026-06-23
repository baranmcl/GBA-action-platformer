#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

TEST(latches_default_zero){ World w; CHECK_EQ((int)w.latches, 0); CHECK(!w.latched(0)); }
TEST(set_and_test_latch){ World w; w.set_latch(5); CHECK(w.latched(5)); CHECK(!w.latched(6)); }

TEST(save_v3_roundtrip_latches){
    World w; w.grant(Ability::Fire); w.set_latch(0); w.set_latch(31); w.current_dungeon = 3;
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 5); // bumped to v5 (was v3->v4; v5 added beaten)
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.has(Ability::Fire));
    CHECK(w2.latched(0)); CHECK(w2.latched(31)); CHECK(!w2.latched(1));
    CHECK_EQ((int)w2.current_dungeon, 3);
}
TEST(save_v3_bad_checksum_rejected){ World w; SaveData s = make_save(w); s.checksum ^= 0xFFu; World w2; CHECK(load_save(s, w2) == false); }
TEST(save_v3_empty_rejected){ SaveData s{}; World w2; CHECK(load_save(s, w2) == false); }

TEST(v2_save_migrates_latches_zero){
    // Build a v2-layout 16-byte buffer by hand: magic(4) ver=2(2) spronks(2) abilities(2)
    // current_dungeon(1) checksum@11(1) pad[4]. Latches must come back 0.
    uint8_t buf[sizeof(SaveData)]; std::memset(buf, 0, sizeof(buf));
    uint32_t magic = 0x5350524B; std::memcpy(buf, &magic, 4);
    uint16_t ver = 2; std::memcpy(buf + 4, &ver, 2);
    uint16_t spr = 0x0001; std::memcpy(buf + 6, &spr, 2);
    uint16_t abil = 0x0002; std::memcpy(buf + 8, &abil, 2);  // Fire = bit 1 = 0x0002
    buf[10] = 0;                                              // current_dungeon
    uint32_t sum = 0; for(int i = 0; i < 11; ++i) sum += buf[i]; buf[11] = (uint8_t)(sum & 0xFF);
    SaveData s; std::memcpy(&s, buf, sizeof(SaveData));
    World w2; CHECK(load_save(s, w2) == true);
    CHECK(w2.spronk_freed(1)); CHECK(w2.has(Ability::Fire));
    CHECK_EQ((int)w2.latches, 0);
}

// --- Heart container tests ---

TEST(heart_container_collected_initially_false){
    World w;
    for(int id = 0; id < 8; ++id)
        CHECK(!w.heart_container_collected(id));
}

TEST(heart_container_collect_sets_flag){
    World w;
    w.collect_heart_container(2);
    CHECK(w.heart_container_collected(2));
    CHECK(!w.heart_container_collected(0));
    CHECK(!w.heart_container_collected(1));
    CHECK(!w.heart_container_collected(3));
}

TEST(heart_container_count_increments){
    World w;
    CHECK_EQ(w.heart_container_count(), 0);
    w.collect_heart_container(0);
    CHECK_EQ(w.heart_container_count(), 1);
    w.collect_heart_container(7);
    CHECK_EQ(w.heart_container_count(), 2);
}

TEST(max_health_for_grows_by_25_per_container){
    World w;
    CHECK_EQ(max_health_for(w), 100);
    w.collect_heart_container(0);
    CHECK_EQ(max_health_for(w), 125);
    w.collect_heart_container(1);
    CHECK_EQ(max_health_for(w), 150);
    w.collect_heart_container(2);
    CHECK_EQ(max_health_for(w), 175);
}

TEST(heart_container_persists_save_load){
    World w;
    w.collect_heart_container(0);
    w.collect_heart_container(5);
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 5); // bumped to v5
    CHECK_EQ((int)sizeof(SaveData), 20); // v5: still 20 bytes (beaten in former padding)
    World w2;
    CHECK(load_save(s, w2) == true);
    CHECK(w2.heart_container_collected(0));
    CHECK(w2.heart_container_collected(5));
    CHECK(!w2.heart_container_collected(1));
    CHECK_EQ(w2.heart_container_count(), 2);
    CHECK_EQ(max_health_for(w2), 150);
}

TEST(heart_container_no_collision_with_dungeon_latches){
    World w;
    // Setting a low dungeon latch (id 0..23) must not affect heart containers
    for(int i = 0; i < 24; ++i) w.set_latch(i);
    CHECK_EQ(w.heart_container_count(), 0);
    for(int id = 0; id < 8; ++id)
        CHECK(!w.heart_container_collected(id));

    // Collecting heart container must not affect low latches
    World w2;
    w2.collect_heart_container(0);
    for(int i = 0; i < 24; ++i)
        CHECK(!w2.latched(i));
}

TEST(save_version_is_5_and_size_is_20){
    // Updated for v5: beaten byte added at offset 17 (former padding); struct stays 20 bytes.
    CHECK_EQ((int)SAVE_VERSION, 5);
    CHECK_EQ((int)sizeof(SaveData), 20);
}
