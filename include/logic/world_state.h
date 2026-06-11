#pragma once
#include <cstdint>
#include <type_traits>
namespace logic {
enum class Ability : uint8_t { Featherleap=0, Fire, Ice, Glide, Dash, Grapple, Stone, Light };

struct World {
    uint16_t spronks_freed = 0;  // bit (d-1) = dungeon d's spronk (d in 1..8)
    uint16_t abilities = 0;      // bit (int)Ability
    uint8_t  current_dungeon = 0; // 0 = hub
    uint32_t latches = 0;        // bit i = global latchable event i triggered (0..31)
    // PRECONDITION: d in 1..8 (bit d-1). Never call with current_dungeon==0 (the hub) —
    // (1u << -1) is undefined behavior.
    bool spronk_freed(int d) const { return (spronks_freed >> (d-1)) & 1u; }
    void free_spronk(int d){ spronks_freed |= (uint16_t)(1u << (d-1)); }
    bool has(Ability a) const { return (abilities >> (int)a) & 1u; }
    void grant(Ability a){ abilities |= (uint16_t)(1u << (int)a); }
    bool latched(int i) const { return (latches >> i) & 1u; }
    void set_latch(int i){ latches |= (uint32_t)(1u << i); }
};

// v3 SaveData layout (16 bytes, naturally aligned, no padding):
//   [0..3]   magic           uint32_t  'SPRK'
//   [4..5]   version         uint16_t  3  (same offset as v1/v2 for backward-compat detection)
//   [6..7]   spronks         uint16_t  World.spronks_freed
//   [8..9]   abilities       uint16_t  World.abilities
//   [10]     current_dungeon uint8_t
//   [11]     checksum        uint8_t   sum of bytes [0..10]+[12..15] & 0xFF
//   [12..15] latches         uint32_t  World.latches  (4-byte aligned at offset 12)
struct SaveData {
    uint32_t magic;          // [0..3]   'SPRK'
    uint16_t version;        // [4..5]   3  (same offset as v1/v2 — backward-compat)
    uint16_t spronks;        // [6..7]   World.spronks_freed
    uint16_t abilities;      // [8..9]   World.abilities
    uint8_t  current_dungeon;// [10]
    uint8_t  checksum;       // [11]     sum of bytes [0..10]+[12..15] & 0xFF
    uint32_t latches;        // [12..15] World.latches (4-byte aligned -> no padding)
};
static_assert(std::is_trivially_copyable<SaveData>::value, "SaveData must be trivially copyable (SRAM)");
static_assert(sizeof(SaveData) == 16, "SaveData v3 layout changed — bump version + migrate");

static constexpr uint32_t SAVE_MAGIC = 0x5350524B; // "SPRK"
static constexpr uint16_t SAVE_VERSION = 3;

inline uint8_t checksum_bytes(const uint8_t* p, int n){ uint32_t s=0; for(int i=0;i<n;++i) s+=p[i]; return (uint8_t)(s&0xFF); }

// v3 checksum covers bytes [0..10] (header) + [12..15] (latches), skipping checksum byte [11] itself
inline uint8_t checksum_v3(const uint8_t* b){
    uint32_t s = 0;
    for(int i = 0; i < 11; ++i) s += b[i];  // [0..10]
    for(int i = 12; i < 16; ++i) s += b[i]; // [12..15]
    return (uint8_t)(s & 0xFF);
}

inline SaveData make_save(const World& w){
    SaveData s{};
    s.magic=SAVE_MAGIC; s.version=SAVE_VERSION;
    s.spronks=w.spronks_freed; s.abilities=w.abilities; s.current_dungeon=w.current_dungeon;
    s.latches=w.latches;
    s.checksum = checksum_v3(reinterpret_cast<const uint8_t*>(&s));
    return s;
}
inline bool load_save(const SaveData& s, World& out){
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&s);
    uint32_t magic; uint16_t version;
    __builtin_memcpy(&magic, b, 4); __builtin_memcpy(&version, b+4, 2);
    if(magic != SAVE_MAGIC) return false;
    if(version == 1){ // migrate: v1 layout flags@6(2), checksum@8(1), checksum=sum(b[0..7])
        if(checksum_bytes(b, 8) != b[8]) return false;
        uint16_t flags; __builtin_memcpy(&flags, b+6, 2);
        out = World{};
        if(flags & 1) out.free_spronk(1);
        if(flags & 2) out.grant(Ability::Featherleap);
        return true;
    }
    if(version == 2){
        // v2 on-disk layout: magic(4)@0, version(2)@4, spronks(2)@6, abilities(2)@8,
        //                    current_dungeon(1)@10, checksum(1)@11, _pad[4]@12
        // Version and spronks/abilities offsets match v3 struct, so s.* members are safe to use.
        if(checksum_bytes(b, 11) != b[11]) return false;   // v2 checksum at offset 11
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon;
        // latches stays 0 (new in v3)
        return true;
    }
    if(version == 3){
        if(checksum_v3(b) != b[11]) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon; out.latches = s.latches;
        return true;
    }
    return false;
}
}
