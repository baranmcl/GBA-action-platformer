#pragma once
#include <cstdint>
#include <type_traits>
namespace logic {
enum class Ability : uint8_t { Featherleap=0, Fire, Ice, Glide, Dash, Grapple, Stone, Light };

struct World {
    uint16_t spronks_freed = 0;  // bit (d-1) = dungeon d's spronk (d in 1..8)
    uint16_t abilities = 0;      // bit (int)Ability
    uint8_t  current_dungeon = 0; // 0 = hub
    // PRECONDITION: d in 1..8 (bit d-1). Never call with current_dungeon==0 (the hub) —
    // (1u << -1) is undefined behavior.
    bool spronk_freed(int d) const { return (spronks_freed >> (d-1)) & 1u; }
    void free_spronk(int d){ spronks_freed |= (uint16_t)(1u << (d-1)); }
    bool has(Ability a) const { return (abilities >> (int)a) & 1u; }
    void grant(Ability a){ abilities |= (uint16_t)(1u << (int)a); }
};

struct SaveData {
    uint32_t magic;          // 'SPRK'
    uint16_t version;        // 2
    uint16_t spronks;        // World.spronks_freed
    uint16_t abilities;      // World.abilities
    uint8_t  current_dungeon;
    uint8_t  checksum;       // sum of bytes [0..10] & 0xFF (magic..current_dungeon)
    uint8_t  _pad[4];
};
static_assert(std::is_trivially_copyable<SaveData>::value, "SaveData must be trivially copyable (SRAM)");
static_assert(sizeof(SaveData) == 16, "SaveData v2 layout changed — bump version + migrate");

static constexpr uint32_t SAVE_MAGIC = 0x5350524B; // "SPRK"
static constexpr uint16_t SAVE_VERSION = 2;

inline uint8_t checksum_bytes(const uint8_t* p, int n){ uint32_t s=0; for(int i=0;i<n;++i) s+=p[i]; return (uint8_t)(s&0xFF); }

inline SaveData make_save(const World& w){
    SaveData s{};
    s.magic=SAVE_MAGIC; s.version=SAVE_VERSION;
    s.spronks=w.spronks_freed; s.abilities=w.abilities; s.current_dungeon=w.current_dungeon;
    s.checksum = checksum_bytes(reinterpret_cast<const uint8_t*>(&s), 11); // magic(4)+ver(2)+spronks(2)+abil(2)+cur(1)
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
        if(checksum_bytes(b, 11) != s.checksum) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities; out.current_dungeon = s.current_dungeon;
        return true;
    }
    return false;
}
}
