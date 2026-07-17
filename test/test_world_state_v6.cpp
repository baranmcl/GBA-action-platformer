#include "test_framework.h"
#include "logic/world_state.h"
#include <cstring>
using namespace logic;

// --- Layout constants ---

TEST(v6_save_version_is_6){ CHECK_EQ((int)SAVE_VERSION_V6, 6); }
TEST(v6_save_data_size_is_24){ CHECK_EQ((int)sizeof(SaveDataV6), 24); }

// --- fletcher16 known vector ---

TEST(fletcher16_abcde_is_0xC8F0){
    const uint8_t bytes[5] = {'a','b','c','d','e'};
    CHECK_EQ((int)fletcher16(bytes, 5), 0xC8F0);
}

// --- v6 roundtrip (every World field, incl. boss_defeats) ---

TEST(v6_roundtrip_all_fields){
    World w;
    w.free_spronk(1); w.free_spronk(3);
    w.grant(Ability::Fire); w.grant(Ability::Grapple);
    w.current_dungeon = 4;
    w.lives = 5;
    w.latches = 0xDEADBEEFu;
    w.beaten = true;
    w.set_boss_defeated(2);
    w.set_boss_defeated(7);

    SaveDataV6 s = make_save_v6(w, 42);
    CHECK_EQ((int)s.version, 6);
    CHECK_EQ((int)s.seq, 42);

    World w2; uint16_t seq_out = 0;
    CHECK(load_save_v6(s, w2, seq_out) == true);
    CHECK_EQ((int)seq_out, 42);
    CHECK(w2.spronk_freed(1)); CHECK(w2.spronk_freed(3)); CHECK(!w2.spronk_freed(2));
    CHECK(w2.has(Ability::Fire)); CHECK(w2.has(Ability::Grapple)); CHECK(!w2.has(Ability::Ice));
    CHECK_EQ((int)w2.current_dungeon, 4);
    CHECK_EQ((int)w2.lives, 5);
    CHECK_EQ((long long)w2.latches, (long long)0xDEADBEEFu);
    CHECK(w2.beaten == true);
    CHECK(w2.boss_defeated(2)); CHECK(w2.boss_defeated(7));
    CHECK(!w2.boss_defeated(1)); CHECK(!w2.boss_defeated(3));
}

TEST(v6_roundtrip_all_zero_world_boss_defeats_zero){
    World w; // defaults
    SaveDataV6 s = make_save_v6(w, 0);
    World w2; uint16_t seq_out = 0;
    CHECK(load_save_v6(s, w2, seq_out) == true);
    CHECK_EQ((int)w2.boss_defeats, 0);
}

// --- Corruption rejection: single-byte flip in each region ---

TEST(v6_corrupt_header_byte_rejected){
    World w; w.current_dungeon = 2;
    SaveDataV6 s = make_save_v6(w, 7);
    uint8_t buf[sizeof(SaveDataV6)]; std::memcpy(buf, &s, sizeof(s));
    buf[1] ^= 0xFFu; // header region (magic)
    SaveDataV6 s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2; uint16_t seq_out = 0;
    CHECK(load_save_v6(s2, w2, seq_out) == false);
}

TEST(v6_corrupt_latches_byte_rejected){
    World w; w.latches = 0x12345678u;
    SaveDataV6 s = make_save_v6(w, 3);
    uint8_t buf[sizeof(SaveDataV6)]; std::memcpy(buf, &s, sizeof(s));
    buf[13] ^= 0xFFu; // latches region [12..15]
    SaveDataV6 s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2; uint16_t seq_out = 0;
    CHECK(load_save_v6(s2, w2, seq_out) == false);
}

TEST(v6_corrupt_boss_defeats_byte_rejected){
    World w; w.set_boss_defeated(5);
    SaveDataV6 s = make_save_v6(w, 1);
    uint8_t buf[sizeof(SaveDataV6)]; std::memcpy(buf, &s, sizeof(s));
    buf[18] ^= 0xFFu; // boss_defeats byte [18]
    SaveDataV6 s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2; uint16_t seq_out = 0;
    CHECK(load_save_v6(s2, w2, seq_out) == false);
}

// --- Byte transposition: the class the old 8-bit additive sum missed ---

TEST(v6_byte_transposition_rejected){
    World w;
    w.spronks_freed = 0x0102u; // distinct bytes 0x02 @[8], 0x01 @[9] so swap is detectable
    SaveDataV6 s = make_save_v6(w, 9);
    uint8_t buf[sizeof(SaveDataV6)]; std::memcpy(buf, &s, sizeof(s));
    std::swap(buf[8], buf[9]); // transpose spronks bytes 8 and 9 (additive sum would miss this)
    SaveDataV6 s2; std::memcpy(&s2, buf, sizeof(s2));
    World w2; uint16_t seq_out = 0;
    CHECK(load_save_v6(s2, w2, seq_out) == false);
}

// --- decide_load: dual-slot arbitration ---

TEST(decide_load_both_valid_picks_higher_seq){
    World wa; wa.current_dungeon = 1;
    World wb; wb.current_dungeon = 2;
    SaveDataV6 slot_a = make_save_v6(wa, 5);
    SaveDataV6 slot_b = make_save_v6(wb, 9);
    SaveData legacy{}; // invalid legacy (all zero, unused since both v6 slots valid)
    SaveDecision d = decide_load(slot_a, slot_b, legacy);
    CHECK(d.valid == true);
    CHECK_EQ((int)d.seq, 9);
    CHECK_EQ((int)d.world.current_dungeon, 2);
    CHECK_EQ(d.next_slot, 0); // slot B holds the winner -> next write goes to slot 0
}

TEST(decide_load_wraparound_0x0000_beats_0xFFFF){
    World wa; wa.current_dungeon = 1;
    World wb; wb.current_dungeon = 2;
    SaveDataV6 slot_a = make_save_v6(wa, 0xFFFF);
    SaveDataV6 slot_b = make_save_v6(wb, 0x0000);
    SaveData legacy{};
    SaveDecision d = decide_load(slot_a, slot_b, legacy);
    CHECK(d.valid == true);
    CHECK_EQ((int)d.seq, 0x0000);
    CHECK_EQ((int)d.world.current_dungeon, 2);
    CHECK_EQ(d.next_slot, 0); // slot B (seq 0x0000) won -> next write to slot 0
}

TEST(decide_load_torn_write_slotA_valid_slotB_corrupt_A_wins){
    World wa; wa.current_dungeon = 3;
    SaveDataV6 slot_a = make_save_v6(wa, 5);
    SaveDataV6 slot_b{}; // all-zero -> invalid (bad magic/version/checksum)
    SaveData legacy{};
    SaveDecision d = decide_load(slot_a, slot_b, legacy);
    CHECK(d.valid == true);
    CHECK_EQ((int)d.seq, 5);
    CHECK_EQ((int)d.world.current_dungeon, 3);
    CHECK_EQ(d.next_slot, 1); // slot A holds the winner -> next write goes to slot 1
}

// --- decide_load: legacy v5 migration when both v6 slots invalid ---

TEST(decide_load_migrates_legacy_v5_when_both_v6_slots_invalid){
    World legacy_world;
    legacy_world.free_spronk(4);
    legacy_world.grant(Ability::Ice);
    legacy_world.current_dungeon = 6;
    legacy_world.lives = 4;
    legacy_world.beaten = true;
    SaveData legacy = make_save(legacy_world); // existing v5 machinery

    SaveDataV6 slot_a{}; // invalid
    SaveDataV6 slot_b{}; // invalid
    SaveDecision d = decide_load(slot_a, slot_b, legacy);
    CHECK(d.valid == true);
    CHECK_EQ((int)d.seq, 0);
    CHECK_EQ(d.next_slot, 0);
    CHECK(d.world.spronk_freed(4));
    CHECK(d.world.has(Ability::Ice));
    CHECK_EQ((int)d.world.current_dungeon, 6);
    CHECK(d.world.beaten == true);
    CHECK_EQ((int)d.world.boss_defeats, 0);
}

// --- decide_load: first boot, everything all-zero (TEST-4) ---

TEST(decide_load_all_zero_slots_and_legacy_invalid){
    SaveDataV6 slot_a{};
    SaveDataV6 slot_b{};
    SaveData legacy{};
    SaveDecision d = decide_load(slot_a, slot_b, legacy);
    CHECK(d.valid == false);
    CHECK_EQ((int)d.seq, 0);
    CHECK_EQ(d.next_slot, 0);
}

// --- Bit independence: boss_defeated doesn't disturb spronks/latches ---

TEST(boss_defeated_bit_independence){
    World w;
    w.free_spronk(1); w.free_spronk(5);
    w.set_latch(3); w.set_latch(20);
    uint16_t spronks_before = w.spronks_freed;
    uint32_t latches_before = w.latches;

    w.set_boss_defeated(3);

    CHECK_EQ((int)w.spronks_freed, (int)spronks_before);
    CHECK_EQ((long long)w.latches, (long long)latches_before);
    CHECK(w.boss_defeated(3));
    CHECK(!w.boss_defeated(1)); CHECK(!w.boss_defeated(2));
    CHECK(!w.boss_defeated(4)); CHECK(!w.boss_defeated(5));
    CHECK(!w.boss_defeated(6)); CHECK(!w.boss_defeated(7)); CHECK(!w.boss_defeated(8));

    // Setting a second, independent bit doesn't disturb the first.
    w.set_boss_defeated(7);
    CHECK(w.boss_defeated(3)); CHECK(w.boss_defeated(7));
    CHECK_EQ((int)w.boss_defeats, (1 << 2) | (1 << 6));
}
