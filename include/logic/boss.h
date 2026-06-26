#pragma once
#include <cstdint>
#include "logic/spell.h"   // SpellId (pure logic header)
namespace logic {
// --- Shared-button switch budget (see spec §2). Frames at 60fps. ---
// Worst-case time to cycle (L) to the needed spell + react + cast: reaction(~15)
// + worst chain 2 presses x ~10 + cast/travel(~15) ~= 50, rounded up with margin.
inline constexpr int SWITCH_BUDGET = 60;

// --- Per-phase attack pattern (data-described; escalating). telegraph MUST be
//     >= SWITCH_BUDGET for every phase. Escalation = shorter telegraph each phase
//     but never below the budget floor. ---
struct PhasePattern { int telegraph_frames; int attack_active_frames; int recovery_frames; };

enum class AttackStep : uint8_t { Telegraph=0, Active, Recovery };

// --- Vulnerability model. SpellExpose: the boss is only woundable during an
//     expose window opened by the expose spell (the King). AlwaysVulnerable: the
//     boss is woundable any time (i-frames still pace wounds) — no expose gate.
//     TiredWindow: the boss is INVULNERABLE while attacking and periodically gets
//     TIRED (a vulnerable window, `expose_frames` long) after `tired_after` completed
//     attack cycles — the player must wait out the pattern and strike when it tires. ---
enum class VulnMode : uint8_t { SpellExpose, AlwaysVulnerable, TiredWindow };

// --- Locomotion model. Stationary: the boss holds its placed position (D1; and the King, which does
//     its own teleport in run_boss). Pacing: a room boss walks the arena floor, reversing at the
//     walls (D2 Slagshell) — run_room_boss reads this. Movement pauses while EXPOSED (clean window). ---
enum class Locomotion : uint8_t { Stationary, Pacing };

// --- Attack-bit scheme (SINGLE SOURCE — the engine attack library #includes this
//     header and uses these SAME constants to interpret a phase's `attacks` mask.
//     Logic must NOT include engine; engine may include logic.) ---
inline constexpr uint8_t BOSS_ATK_AIMED   = 1 << 0;
inline constexpr uint8_t BOSS_ATK_SPIRAL  = 1 << 1;
inline constexpr uint8_t BOSS_ATK_FAN     = 1 << 2;
inline constexpr uint8_t BOSS_ATK_ROCKFALL = 1 << 3;  // M13: jump -> rocks fall from the ceiling

// Per-phase data: the HP threshold ending this phase (phase i active while
// hp > end_hp, except the last), the attack pattern, and the attack mask.
struct BossPhaseDef { int end_hp; PhasePattern pattern; uint8_t attacks; };

// A boss is fully described by data. BossState points at one of these.
struct BossDef {
    int max_hp;
    int wound_dmg;
    int hit_iframes;
    int expose_frames;            // SpellExpose: expose window length. TiredWindow: tired window length.
                                  // unused (0) for AlwaysVulnerable.
    VulnMode vuln;
    SpellId expose_spell;         // unused (None) for AlwaysVulnerable / TiredWindow
    const BossPhaseDef* phases;
    int phase_count;
    int tired_after = 0;          // TiredWindow only: completed attack cycles before the boss tires
    const char* intro_line = nullptr;   // optional pre-fight dialogue (null = none)
    const char* death_line = nullptr;   // optional on-defeat dialogue (null = none)
    Locomotion  locomotion = Locomotion::Stationary;   // M13: Pacing = walks the floor (run_room_boss)
};

// --- King attack masks + phase table (reproduces the SHIPPED Nightmare King). ---
inline constexpr uint8_t KING_ATTACKS = BOSS_ATK_AIMED | BOSS_ATK_SPIRAL | BOSS_ATK_FAN;
inline constexpr BossPhaseDef KING_PHASES[3] = {
    { 60, { 72, 30, 40 }, KING_ATTACKS },   // P1 (aerial)  — telegraph 72 >= 60
    { 30, { 66, 30, 30 }, KING_ATTACKS },   // P2 (ground)  — telegraph 66 >= 60
    {  0, { 60, 30, 20 }, KING_ATTACKS },   // P3 (all-in)  — telegraph 60 == SWITCH_BUDGET (tightest)
};
inline constexpr BossDef KING_DEF{
    90, 10, 45, 75, VulnMode::SpellExpose, SpellId::Light, KING_PHASES, 3
};

// --- D1 Whispering Woods Guardian (TiredWindow, 2 phases). Invulnerable while attacking;
//     tires (a 90-frame vulnerable window) after 3 completed attack cycles. ---
inline constexpr uint8_t D1_ATTACKS_P1 = BOSS_ATK_AIMED;
inline constexpr uint8_t D1_ATTACKS_P2 = BOSS_ATK_AIMED | BOSS_ATK_FAN;
inline constexpr BossPhaseDef D1_PHASES[2] = {
    { 30, { 80, 30, 40 }, D1_ATTACKS_P1 },
    {  0, { 70, 30, 30 }, D1_ATTACKS_P2 },
};
inline constexpr BossDef D1_DEF{
    60, 10, 30, 90, VulnMode::TiredWindow, SpellId::None, D1_PHASES, 2,
    /*tired_after=*/3,
    /*intro_line=*/"So, you seek the spronks?",
    /*death_line=*/"Fool...you can't stop him"
};

// --- D2 Ember Caverns boss: the Magma Golem "Slagshell" (SpellExpose+Fire, Pacing, 2 phases).
//     Fire melts the crust -> exposed window -> bolt/Fire wounds. A wound triggers a LONG re-armor
//     (hit_iframes) during which Fire cannot re-expose and the boss keeps attacking -> the player must
//     DODGE to earn the next opening (anti-spam). Both phases run AIMED + ROCKFALL; the phase boundary
//     escalates rockfall intensity (run_room_boss reads b.phase for the rock count). ---
inline constexpr uint8_t D2_ATTACKS_P1 = BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL;
inline constexpr uint8_t D2_ATTACKS_P2 = BOSS_ATK_AIMED | BOSS_ATK_ROCKFALL;
inline constexpr BossPhaseDef D2_PHASES[2] = {
    { 35, { 80, 30, 40 }, D2_ATTACKS_P1 },   // P1 70->35  (telegraph 80, active 30, recovery 40)
    {  0, { 70, 30, 30 }, D2_ATTACKS_P2 },   // P2 35->0   (escalated rockfall)
    // NOTE: attack_active_frames (30) MUST exceed RockfallEmitter::WARN_FRAMES (26) so the rock drop
    // lands inside the Active window (the emitter is ticked only during AttackStep::Active). Keep >= 28.
};
inline constexpr BossDef D2_DEF{
    70, 10, 90, 90, VulnMode::SpellExpose, SpellId::Fire, D2_PHASES, 2,
    /*tired_after=*/0,
    /*intro_line=*/"You'll cook in my caverns.",
    /*death_line=*/"The embers...fade...",
    /*locomotion=*/Locomotion::Pacing
};

// Def-driven boss state. `phase` is an integer INDEX (0..phase_count-1) so the
// phase count is variable. "Exposed" is just expose_timer>0; "Defeated" is hp<=0.
struct BossState {
    const BossDef* def = nullptr;
    int hp = 0;
    int phase = 0;                // combat-phase index (held across expose)
    int phase_start_hp = 0;       // HP at the active phase's entry
    int expose_timer = 0;         // >0 while EXPOSED (SpellExpose) / TIRED (TiredWindow)
    int attack_timer = 0;         // drives the per-phase attack pattern
    int hit_iframes = 0;          // >0 = just wounded: immune, not re-exposable, attacking
    int attack_cycles = 0;        // TiredWindow: completed attack cycles since the last tired window

    void reset(const BossDef& d){
        def = &d; hp = d.max_hp; phase = 0; phase_start_hp = d.max_hp;
        expose_timer = 0; attack_timer = 0; hit_iframes = 0; attack_cycles = 0;
    }
    bool exposed() const { return expose_timer > 0; }
    bool defeated() const { return hp <= 0; }
    bool vulnerable() const { return def->vuln == VulnMode::AlwaysVulnerable || exposed(); }
    int active_phase() const { return phase; }
    const PhasePattern& pat() const { return def->phases[phase].pattern; }
    int phase_period() const {
        const PhasePattern& p = pat();
        return p.telegraph_frames + p.attack_active_frames + p.recovery_frames;
    }

    // Expose hit (renamed from on_light_hit). For SpellExpose only, and only when the
    // spell matches the def's expose spell. AlwaysVulnerable -> no-op.
    void on_expose_hit(SpellId s){
        if(defeated() || hit_iframes > 0) return;          // can't re-expose during post-wound i-frames
        if(def->vuln != VulnMode::SpellExpose) return;     // AlwaysVulnerable: no expose mechanic
        if(s != def->expose_spell) return;                 // wrong spell
        expose_timer = def->expose_frames;                 // phase stays the index; "Exposed" = expose_timer>0
    }

    void advance_phase_for_hp(){
        // Walk the phase table to the index whose band contains hp (phase i active
        // while hp > phases[i].end_hp, except the last). On change, record entry HP.
        int n = def->phase_count;
        int idx = n - 1;
        for(int i = 0; i < n; ++i){
            if(hp > def->phases[i].end_hp){ idx = i; break; }
        }
        if(idx != phase){ phase = idx; phase_start_hp = hp; }
    }

    void on_wound(int dmg){
        if(defeated() || hit_iframes > 0 || !vulnerable()) return;   // dead / i-frames / shielded -> immune
        hp -= dmg; if(hp < 0) hp = 0;
        advance_phase_for_hp();
        hit_iframes = def->hit_iframes;                  // ONE wound per expose/tired window, then recover
        if(def->vuln != VulnMode::AlwaysVulnerable) expose_timer = 0;  // a wound ends the expose/tired window
    }

    void tick(){
        if(defeated()) return;
        if(hit_iframes > 0) --hit_iframes;   // post-wound recovery (boss is attacking, not exposable)
        if(expose_timer > 0){
            // EXPOSED = a clean damage window: attack pattern FROZEN (do not advance attack_timer)
            // so the player never has to switch spells AND dodge on the same clock.
            --expose_timer;
            return;
        }
        if(++attack_timer >= phase_period()){
            attack_timer = 0;
            // TiredWindow: a completed attack cycle. After `tired_after` cycles the boss tires —
            // open a vulnerable window (expose_timer) which the existing branch above then freezes.
            // Skip if already tired/in i-frames so a cycle boundary mid-recovery doesn't double-count.
            if(def->vuln == VulnMode::TiredWindow && expose_timer == 0 && hit_iframes == 0){
                if(++attack_cycles >= def->tired_after){
                    expose_timer = def->expose_frames;
                    attack_cycles = 0;
                }
            }
        }
    }

    // Player died: full-fight restart (spec design decision). Own named method so the
    // intent is explicit and a per-boss override is possible.
    void on_player_death(){ reset(*def); }

    AttackStep current_step() const {
        const PhasePattern& p = pat();
        if(attack_timer < p.telegraph_frames) return AttackStep::Telegraph;
        if(attack_timer < p.telegraph_frames + p.attack_active_frames) return AttackStep::Active;
        return AttackStep::Recovery;
    }
};
}
