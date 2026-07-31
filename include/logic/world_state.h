#pragma once
#include <cstddef>
#include <cstdint>
#include <type_traits>
namespace logic {
enum class Ability : uint8_t { Featherleap=0, Fire, Ice, Glide, Dash, Grapple, Stone, Light };

// Runtime-only game state; NOT persisted directly. Serialize via make_save/load_save
// (SaveData is the v1-v5 SRAM layout) or make_save_v6/load_save_v6 (SaveDataV6, current layout).
struct World {
    static constexpr int STARTING_LIVES = 3; // lives a new game starts with; also the base before spronk bonuses
    uint16_t spronks_freed = 0;  // bit (d-1) = dungeon d's spronk (d in 1..8)
    uint16_t abilities = 0;      // bit (int)Ability
    uint8_t  current_dungeon = 0; // 0 = hub
    uint8_t  lives = STARTING_LIVES; // current lives (persistent); max is derived via max_lives()
    uint32_t latches = 0;        // bit i = global latchable event i triggered (0..31)
    bool     beaten = false;     // true once the game has been completed (Nightmare King defeated)
    uint8_t  boss_defeats = 0;   // bit (d-1) = dungeon d's boss defeated (d in 1..8)
    // PRECONDITION: d in 1..8 (bit d-1). Never call with current_dungeon==0 (the hub) —
    // (1u << -1) is undefined behavior.
    bool spronk_freed(int d) const { return (spronks_freed >> (d-1)) & 1u; }
    void free_spronk(int d){ spronks_freed |= (uint16_t)(1u << (d-1)); }
    bool has(Ability a) const { return (abilities >> (int)a) & 1u; }
    void grant(Ability a){ abilities |= (uint16_t)(1u << (int)a); }
    bool latched(int i) const { return (latches >> i) & 1u; }
    void set_latch(int i){ latches |= (uint32_t)(1u << i); }
    // PRECONDITION: d in 1..8 (bit d-1). Mirrors spronk_freed's precondition — never call
    // with a dungeon id of 0.
    bool boss_defeated(int d) const { return (boss_defeats >> (d-1)) & 1u; }
    void set_boss_defeated(int d){ boss_defeats |= (uint8_t)(1u << (d-1)); }

    // Heart containers: bits [24..31] of latches reserved for permanent HP upgrades (id 0..7).
    // Bits [0..23] are for dungeon shortcuts (brazier gates, etc.) and must never overlap.
    static constexpr int HEART_CONTAINER_LATCH_BASE = 24;
    bool heart_container_collected(int id) const { return latched(HEART_CONTAINER_LATCH_BASE + id); }
    void collect_heart_container(int id){ set_latch(HEART_CONTAINER_LATCH_BASE + id); }
    int heart_container_count() const {
        int n = 0;
        for(int id = 0; id < 8; ++id)
            if(heart_container_collected(id)) ++n;
        return n;
    }
};

// Derived max-health formula — single source of truth shared by engine + tests.
static constexpr int BASE_MAX_HEALTH    = 100;
static constexpr int HEART_CONTAINER_BONUS = 25;
inline int max_health_for(const World& w){
    return BASE_MAX_HEALTH + HEART_CONTAINER_BONUS * w.heart_container_count();
}

// Lives helpers — mirror max_health_for pattern.
// max_lives: 3 base + 1 per freed spronk (each rescued spronk grants +1 permanent max).
inline int spronk_count(const World& w){ return __builtin_popcount((unsigned)w.spronks_freed); }

// Door-gating rule (I8): door 1 (n==1) is always enterable; doors 2..8 each need the PRIOR
// dungeon's spronk freed (door n needs spronk n-1); door 9 (the finale) needs all 8 spronks
// freed. Single source of truth for scene_hub's door render + door-entry check, replacing a
// hand-scattered 9-disjunct if-chain duplicated in two call sites.
inline bool door_enterable(int n, const World& w){
    return n == 1
        || (n >= 2 && n <= 8 && w.spronk_freed(n - 1))
        || (n == 9 && spronk_count(w) == 8);
}

inline int max_lives(const World& w){ return World::STARTING_LIVES + spronk_count(w); }
inline void refill_lives(World& w){ w.lives = (uint8_t)max_lives(w); }
// Game-Over revive: come back with the STARTING life count (3), NOT max_lives — a game over resets
// your run's life stock regardless of how many spronks are freed. (max_lives still caps growth via spronk rescues.)
inline void revive_lives(World& w){ w.lives = (uint8_t)World::STARTING_LIVES; }
inline void lose_life(World& w){ if(w.lives > 0) --w.lives; }
// Grants exactly +1 life, capped at max_lives (unlike refill_lives, which jumps straight to max).
// Used for spronk-rescue: each spronk should give +1 life, not a full refill.
inline void gain_life(World& w){ int m = max_lives(w); if(w.lives < m) ++w.lives; }

// I35 dev tooling: builds a synthetic, session-only World for the title-screen debug selector
// (game::run_debug_select). Pure and host-testable, unlike the engine-coupled scene that calls it.
// `ability_mask` is a raw bit-OR of (1u << (int)Ability) bits (the debug UI toggles bits directly
// rather than calling grant() per ability, so this takes the already-built mask); `spronk_count`
// frees spronks 1..spronk_count (clamped to 0..8) so door-gating (door_enterable) and the
// lives bonus (max_lives) behave the same as a real playthrough that rescued that many spronks.
// current_dungeon is left at 0 — the caller (main.cpp) sets it right before launching, mirroring
// the normal hub->dungeon flow's own world.current_dungeon assignment.
inline World make_debug_world(uint16_t ability_mask, int spronk_count){
    World w{};
    w.abilities = ability_mask;
    int n = spronk_count < 0 ? 0 : (spronk_count > 8 ? 8 : spronk_count);
    for(int d = 1; d <= n; ++d) w.free_spronk(d);
    refill_lives(w); // lives/max_lives scale with spronks freed, same as a real save
    return w;
}

// Boot safety: loaded save must never have lives==0 (instant game-over) or lives>max.
inline void clamp_lives_on_load(World& w){
    int m = max_lives(w);
    if(w.lives == 0 || w.lives > (uint8_t)m) w.lives = (uint8_t)m;
}

// v5 SaveData layout (20 bytes; 17 meaningful + 2 compiler padding after beaten):
//   [0..3]   magic           uint32_t  'SPRK'
//   [4..5]   version         uint16_t  5  (same offset as v1/v2/v3/v4 for backward-compat detection)
//   [6..7]   spronks         uint16_t  World.spronks_freed
//   [8..9]   abilities       uint16_t  World.abilities
//   [10]     current_dungeon uint8_t
//   [11]     checksum        uint8_t   checksum_v5 (covers [0..10]+[12..17])
//   [12..15] latches         uint32_t  World.latches  (4-byte aligned at offset 12)
//   [16]     lives           uint8_t   World.lives
//   [17]     beaten          uint8_t   World.beaten (was v4 padding; sizeof stays 20)
//   [18..19] _pad            (2 bytes, compiler padding — zeroed by SaveData s{})
struct SaveData {
    uint32_t magic;          // [0..3]   'SPRK'
    uint16_t version;        // [4..5]   5  (same offset as v1/v2/v3/v4 — backward-compat)
    uint16_t spronks;        // [6..7]   World.spronks_freed
    uint16_t abilities;      // [8..9]   World.abilities
    uint8_t  current_dungeon;// [10]
    uint8_t  checksum;       // [11]     checksum_v5 result
    uint32_t latches;        // [12..15] World.latches (4-byte aligned)
    uint8_t  lives;          // [16]     World.lives
    uint8_t  beaten;         // [17]     World.beaten (former padding byte)
    // [18..19] compiler padding (uint32_t alignment) — zero-inited by SaveData s{}
};
static_assert(std::is_trivially_copyable<SaveData>::value, "SaveData must be trivially copyable (SRAM)");
static_assert(sizeof(SaveData) == 20, "SaveData v5 layout changed — bump SAVE_VERSION and add a migration branch");

static constexpr uint32_t SAVE_MAGIC = 0x5350524B; // "SPRK"
static constexpr uint16_t SAVE_VERSION = 5;

// contiguous-range checksum; used by v1/v2 migration.
inline uint8_t checksum_bytes(const uint8_t* p, int n){ uint32_t s=0; for(int i=0;i<n;++i) s+=p[i]; return (uint8_t)(s&0xFF); }

// v3 checksum covers bytes [0..10] (header) + [12..15] (latches), skipping checksum byte [11] and padding.
inline uint8_t checksum_v3(const uint8_t* b){
    uint32_t s = 0;
    for(int i = 0; i < 11; ++i) s += b[i];  // [0..10]
    for(int i = 12; i < 16; ++i) s += b[i]; // [12..15]
    return (uint8_t)(s & 0xFF);
}

// v4 checksum covers [0..10] (header) + [12..15] (latches) + [16] (lives).
// Skips checksum byte [11] and compiler padding [17..19].
inline uint8_t checksum_v4(const uint8_t* b){
    uint32_t s = 0;
    for(int i = 0; i < 11; ++i) s += b[i];  // [0..10]
    for(int i = 12; i < 17; ++i) s += b[i]; // [12..16]: latches + lives
    return (uint8_t)(s & 0xFF);
}

// v5 checksum covers [0..10] (header) + [12..17] (latches + lives + beaten).
// Skips checksum byte [11] and compiler padding [18..19].
inline uint8_t checksum_v5(const uint8_t* b){
    uint32_t s = 0;
    for(int i = 0; i < 11; ++i) s += b[i];  // [0..10]
    for(int i = 12; i < 18; ++i) s += b[i]; // [12..17]: latches + lives + beaten
    return (uint8_t)(s & 0xFF);
}

inline SaveData make_save(const World& w){
    SaveData s{};  // zero-inits all fields including padding
    s.magic=SAVE_MAGIC; s.version=SAVE_VERSION;
    s.spronks=w.spronks_freed; s.abilities=w.abilities; s.current_dungeon=w.current_dungeon;
    s.latches=w.latches; s.lives=w.lives; s.beaten = w.beaten ? 1 : 0;
    s.checksum = checksum_v5(reinterpret_cast<const uint8_t*>(&s));
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
        out.lives = World::STARTING_LIVES; // v1 predates lives — default to 3
        out.beaten = false;                // v1 predates beaten — default false
        return true;
    }
    if(version == 2){
        // v2 on-disk layout: magic(4)@0, version(2)@4, spronks(2)@6, abilities(2)@8,
        //                    current_dungeon(1)@10, checksum(1)@11, _pad[4]@12
        // Version and spronks/abilities offsets match v4 struct, so s.* members are safe to use.
        if(checksum_bytes(b, 11) != b[11]) return false;   // v2 checksum at offset 11
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon;
        // latches stays 0 (new in v3)
        out.lives = World::STARTING_LIVES; // v2 predates lives — default to 3
        out.beaten = false;                // v2 predates beaten — default false
        return true;
    }
    if(version == 3){
        if(checksum_v3(b) != b[11]) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon; out.latches = s.latches;
        out.lives = World::STARTING_LIVES; // v3 predates lives — default to 3
        out.beaten = false;                // v3 predates beaten — default false
        return true;
    }
    if(version == 4){
        if(checksum_v4(b) != b[11]) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon; out.latches = s.latches;
        out.lives = s.lives;
        out.beaten = false;       // v4 predates beaten — default false
        clamp_lives_on_load(out); // boot safety: never 0, never > max
        return true;
    }
    if(version == 5){
        if(checksum_v5(b) != b[11]) return false;
        out = World{};
        out.spronks_freed = s.spronks; out.abilities = s.abilities;
        out.current_dungeon = s.current_dungeon; out.latches = s.latches;
        out.lives = s.lives;
        out.beaten = (s.beaten != 0);
        clamp_lives_on_load(out); // boot safety: never 0, never > max
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// v6 save layout (24 bytes; dual-slot SRAM with sequence-number arbitration +
// Fletcher-16 checksum, and boss-defeat persistence). The v1-v5 SaveData/
// checksum_v3/v4/v5/make_save/load_save machinery above is kept untouched —
// it is still needed for one-time migration of pre-v6 SRAM into v6.
//
//   [0..3]   magic            uint32_t  'SPRK' (SAVE_MAGIC)
//   [4..5]   version          uint16_t  6 (SAVE_VERSION_V6)
//   [6..7]   seq              uint16_t  monotonically-increasing save sequence (wraps at 0xFFFF)
//   [8..9]   spronks          uint16_t  World.spronks_freed
//   [10..11] abilities        uint16_t  World.abilities
//   [12..15] latches          uint32_t  World.latches (4-byte aligned at offset 12)
//   [16]     lives            uint8_t   World.lives
//   [17]     beaten           uint8_t   World.beaten
//   [18]     boss_defeats     uint8_t   World.boss_defeats
//   [19]     current_dungeon  uint8_t
//   [20..21] checksum16       uint16_t  fletcher16 over bytes [0..19]
//   [22..23] _pad             (2 bytes, compiler padding — zeroed by SaveDataV6 s{})
// ---------------------------------------------------------------------------
static constexpr uint16_t SAVE_VERSION_V6 = 6;

struct SaveDataV6 {
    uint32_t magic;           // [0..3]
    uint16_t version;         // [4..5]
    uint16_t seq;             // [6..7]
    uint16_t spronks;         // [8..9]
    uint16_t abilities;       // [10..11]
    uint32_t latches;         // [12..15]
    uint8_t  lives;           // [16]
    uint8_t  beaten;          // [17]
    uint8_t  boss_defeats;    // [18]
    uint8_t  current_dungeon; // [19]
    uint16_t checksum16;      // [20..21]
    uint8_t  _pad[2];         // [22..23] compiler padding — zero-inited by SaveDataV6 s{}
};
static_assert(std::is_trivially_copyable<SaveDataV6>::value, "SaveDataV6 must be trivially copyable (SRAM)");
static_assert(sizeof(SaveDataV6) == 24, "SaveDataV6 v6 layout changed — this layout is locked");
static_assert(offsetof(SaveDataV6, magic) == 0, "SaveDataV6.magic offset locked at 0");
static_assert(offsetof(SaveDataV6, version) == 4, "SaveDataV6.version offset locked at 4");
static_assert(offsetof(SaveDataV6, seq) == 6, "SaveDataV6.seq offset locked at 6");
static_assert(offsetof(SaveDataV6, spronks) == 8, "SaveDataV6.spronks offset locked at 8");
static_assert(offsetof(SaveDataV6, abilities) == 10, "SaveDataV6.abilities offset locked at 10");
static_assert(offsetof(SaveDataV6, latches) == 12, "SaveDataV6.latches offset locked at 12");
static_assert(offsetof(SaveDataV6, lives) == 16, "SaveDataV6.lives offset locked at 16");
static_assert(offsetof(SaveDataV6, beaten) == 17, "SaveDataV6.beaten offset locked at 17");
static_assert(offsetof(SaveDataV6, boss_defeats) == 18, "SaveDataV6.boss_defeats offset locked at 18");
static_assert(offsetof(SaveDataV6, current_dungeon) == 19, "SaveDataV6.current_dungeon offset locked at 19");
static_assert(offsetof(SaveDataV6, checksum16) == 20, "SaveDataV6.checksum16 offset locked at 20");

// Fletcher-16: standard two running-sums-mod-255 checksum. Unlike the v1-v5 8-bit additive
// sum, this detects byte transpositions and other compensating corruptions.
inline uint16_t fletcher16(const uint8_t* p, int n){
    uint32_t sum1 = 0, sum2 = 0;
    for(int i = 0; i < n; ++i){
        sum1 = (sum1 + p[i]) % 255u;
        sum2 = (sum2 + sum1) % 255u;
    }
    return (uint16_t)((sum2 << 8) | sum1);
}

inline SaveDataV6 make_save_v6(const World& w, uint16_t seq){
    SaveDataV6 s{}; // zero-inits all fields including padding
    s.magic = SAVE_MAGIC; s.version = SAVE_VERSION_V6; s.seq = seq;
    s.spronks = w.spronks_freed; s.abilities = w.abilities;
    s.latches = w.latches;
    s.lives = w.lives; s.beaten = w.beaten ? 1 : 0;
    s.boss_defeats = w.boss_defeats; s.current_dungeon = w.current_dungeon;
    s.checksum16 = fletcher16(reinterpret_cast<const uint8_t*>(&s), 20);
    return s;
}

inline bool load_save_v6(const SaveDataV6& s, World& out, uint16_t& seq_out){
    const uint8_t* b = reinterpret_cast<const uint8_t*>(&s);
    uint32_t magic; uint16_t version;
    __builtin_memcpy(&magic, b, 4); __builtin_memcpy(&version, b + 4, 2);
    if(magic != SAVE_MAGIC) return false;
    if(version != SAVE_VERSION_V6) return false;
    if(fletcher16(b, 20) != s.checksum16) return false;
    out = World{};
    out.spronks_freed = s.spronks; out.abilities = s.abilities;
    out.current_dungeon = s.current_dungeon; out.latches = s.latches;
    out.lives = s.lives;
    out.beaten = (s.beaten != 0);
    out.boss_defeats = s.boss_defeats;
    clamp_lives_on_load(out); // boot safety: never 0, never > max
    seq_out = s.seq;
    return true;
}

// Pure dual-slot arbitration + legacy migration decision. The engine stays dumb: it just
// reads both v6 SRAM slots (+ legacy slot 0 reinterpreted as v1-v5 SaveData) and hands them
// here. next_slot indicates which physical slot (0 or 1) should receive the NEXT write
// (ping-pong); it is 0 when nothing valid was found or when the legacy migration path won,
// since in both of those cases there is no existing v6 slot to preserve as a fallback.
struct SaveDecision { bool valid; World world; uint16_t seq; int next_slot; };

inline SaveDecision decide_load(const SaveDataV6& slot_a, const SaveDataV6& slot_b, const SaveData& legacy_slot0){
    World world_a{}, world_b{};
    uint16_t seq_a = 0, seq_b = 0;
    bool valid_a = load_save_v6(slot_a, world_a, seq_a);
    bool valid_b = load_save_v6(slot_b, world_b, seq_b);
    if(valid_a && valid_b){
        // Wraparound-safe comparison: treat the signed difference as the tiebreaker so that
        // e.g. seq 0xFFFF vs 0x0000 correctly picks 0x0000 as "newer" across the wrap.
        bool a_is_newer = (int16_t)(seq_a - seq_b) > 0;
        if(a_is_newer) return SaveDecision{true, world_a, seq_a, 1};
        return SaveDecision{true, world_b, seq_b, 0};
    }
    if(valid_a) return SaveDecision{true, world_a, seq_a, 1};
    if(valid_b) return SaveDecision{true, world_b, seq_b, 0};
    World legacy_world{};
    if(load_save(legacy_slot0, legacy_world)){
        return SaveDecision{true, legacy_world, 0, 0};
    }
    return SaveDecision{false, World{}, 0, 0};
}
}
