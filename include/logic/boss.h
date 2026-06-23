#pragma once
#include <cstdint>
namespace logic {
// --- Shared-button switch budget (see spec §2). Frames at 60fps. ---
// Worst-case time to cycle (L) to the needed spell + react + cast: reaction(~15)
// + worst chain 2 presses x ~10 + cast/travel(~15) ~= 50, rounded up with margin.
inline constexpr int SWITCH_BUDGET = 60;

// --- King HP + phase thresholds. Defeat at hp <= 0. Wound subtracts WOUND_DMG. ---
inline constexpr int KING_MAX_HP = 90;
inline constexpr int P1_END_HP   = 60;   // crossing 60 -> advance to P2
inline constexpr int P2_END_HP   = 30;   // crossing 30 -> advance to P3
inline constexpr int WOUND_DMG   = 10;   // damage per bolt/element hit while EXPOSED (3 hits/phase)

// --- Expose window (vulnerable). MUST be >= SWITCH_BUDGET (so the player can
//     switch to a wound spell, or just bolt). ---
inline constexpr int EXPOSE_FRAMES = 75;

// --- Per-phase attack pattern (data-described; escalating). telegraph MUST be
//     >= SWITCH_BUDGET for every phase. Escalation = shorter telegraph each phase
//     but never below the budget floor (P3 sits AT the floor). ---
struct PhasePattern { int telegraph_frames; int attack_active_frames; int recovery_frames; };
inline constexpr PhasePattern PHASE_PATTERNS[3] = {
    { 72, 30, 40 },   // P1 (aerial)  — telegraph 72 >= 60
    { 66, 30, 30 },   // P2 (ground)  — telegraph 66 >= 60
    { 60, 30, 20 },   // P3 (all-in)  — telegraph 60 == SWITCH_BUDGET (tightest allowed)
};

// The 2-room approach runs OUTSIDE BossState; BossState begins at P1 when run_boss()
// starts. EXPOSED is a phase overlay: on_light_hit() saves the current phase into
// exposed_return and sets phase=Exposed; when expose_timer hits 0, phase returns to
// exposed_return. Generic naming so M12 can lift this into a shared boss framework.
enum class BossPhase : uint8_t { P1=0, P2, P3, Exposed, Defeated };

struct BossState {
    int hp = KING_MAX_HP;
    BossPhase phase = BossPhase::P1;
    BossPhase exposed_return = BossPhase::P1; // phase to resume when expose ends
    int phase_start_hp = KING_MAX_HP;         // HP at the active phase's entry
    int expose_timer = 0;                     // >0 while EXPOSED (vulnerable)
    int attack_timer = 0;                     // drives the per-phase attack pattern

    void reset(){ hp=KING_MAX_HP; phase=BossPhase::P1; exposed_return=BossPhase::P1;
                  phase_start_hp=KING_MAX_HP; expose_timer=0; attack_timer=0; }
    bool exposed() const { return expose_timer > 0; }
    bool defeated() const { return hp <= 0 || phase == BossPhase::Defeated; }
    // (on_light_hit / on_wound / tick / on_player_death / current_step added in later tasks)
};
}
