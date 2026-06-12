#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

TEST(latches_default_zero){ World w; CHECK_EQ((int)w.latches, 0); CHECK(!w.latched(0)); }
TEST(set_and_test_latch){ World w; w.set_latch(5); CHECK(w.latched(5)); CHECK(!w.latched(6)); }

TEST(save_v3_roundtrip_latches){
    World w; w.grant(Ability::Fire); w.set_latch(0); w.set_latch(31); w.current_dungeon = 3;
    SaveData s = make_save(w);
    CHECK_EQ((int)s.version, 3);
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
