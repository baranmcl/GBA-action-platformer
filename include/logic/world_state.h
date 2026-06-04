#pragma once
#include <cstdint>
#include <type_traits>
namespace logic {
struct World { bool spronk1_freed=false, featherleap=false, gate1_open=false; };
struct SaveData {
    uint32_t magic;     // 'SPRK'
    uint16_t version;   // 1
    uint16_t flags;     // bit0 spronk1, bit1 featherleap, bit2 gate1
    uint8_t  checksum;  // sum of first 8 bytes (magic,version,flags) & 0xFF; excludes itself + pad
    uint8_t  _pad[3];
};
// Butano's SRAM API (bn_sram.h) requires a trivially-copyable, fixed-size record. Lock both:
static_assert(std::is_trivially_copyable<SaveData>::value, "SaveData must be trivially copyable for SRAM");
static_assert(sizeof(SaveData) == 12, "SaveData layout changed — bump SAVE_VERSION and migrate");

static constexpr uint32_t SAVE_MAGIC = 0x5350524B; // "SPRK"
static constexpr uint16_t SAVE_VERSION = 1;

inline uint8_t calc_checksum(const SaveData& s){
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&s);
    uint32_t sum = 0;
    for(int i = 0; i < 8; ++i) sum += p[i]; // magic(4)+version(2)+flags(2)
    return (uint8_t)(sum & 0xFF);
}
inline SaveData make_save(const World& w){
    SaveData s{};
    s.magic = SAVE_MAGIC;
    s.version = SAVE_VERSION;
    s.flags = (uint16_t)((w.spronk1_freed ? 1 : 0) | (w.featherleap ? 2 : 0) | (w.gate1_open ? 4 : 0));
    s.checksum = calc_checksum(s);
    return s;
}
inline bool load_save(const SaveData& s, World& out){
    if(s.magic != SAVE_MAGIC || s.version != SAVE_VERSION) return false;
    if(s.checksum != calc_checksum(s)) return false;
    out.spronk1_freed = (s.flags & 1) != 0;
    out.featherleap   = (s.flags & 2) != 0;
    out.gate1_open    = (s.flags & 4) != 0;
    return true;
}
}
