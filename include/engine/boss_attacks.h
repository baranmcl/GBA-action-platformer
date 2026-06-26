#pragma once
#include "bn_optional.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_item.h"
#include "bn_camera_ptr.h"
#include "bn_vector.h"
#include "logic/physics.h"   // logic::Body
#include "logic/vec2.h"      // logic::Vec2
#include "logic/tilemap.h"   // logic::Tilemap (boss bolts despawn on a solid wall/floor)
#include "logic/boss.h"      // logic::BossState / BossDef / BOSS_ATK_* (SINGLE SOURCE of attack bits)
#include "logic/rockfall.h"  // logic::rockfall_columns (M13 fair-column picker)

namespace engine {

// =============================================================================
// boss_attacks — reusable scene-side combat primitives for any data-described
// boss (logic::BossDef / logic::BossState). Faithful extraction of the M11
// Nightmare King's inline combat code, parameterized by boss/player positions +
// the BossState. Engine layer (bn:: allowed); #includes logic/boss.h and uses
// the SAME logic::BOSS_ATK_AIMED/SPIRAL/FAN bit constants to interpret a phase's
// `attacks` mask — NO duplicated bit values. MUST NOT include anything from game/.
//
// Screen-space mapping mirrors scene_boss.cpp's wx/wy lambdas: a world pixel maps
// to screen via (px - map_px_w/2, px - map_px_h/2). Each helper that touches
// sprites takes map_px_w/map_px_h so it can do this conversion itself.
// =============================================================================

// One attack projectile: a moving logic::Body + a reused bolt-sprite VFX.
// Spawned at AttackStep::Active; cleared at Recovery / while exposed / on teleport.
struct AttackInst {
    bool active = false;
    logic::Body body;
    logic::Vec2 vel{};
    bn::optional<bn::sprite_ptr> sprite;
};

// 8 rotating directions for the spiral (integer approx of a circle, ~3 px/frame).
// Exposed so a consumer can reference the same step count if needed.
inline constexpr int BOSS_SPIRAL_DIR_COUNT = 8;
extern const int BOSS_SPIRAL_DIRS[BOSS_SPIRAL_DIR_COUNT][2];

// -----------------------------------------------------------------------------
// AttackPool — fixed pool of projectile bodies + bolt-sprite VFX (no heap), plus
// the per-frame advance/render/expire + contact loop and the block defense.
// Mirrors scene_boss.cpp's `attacks` vector + `launch` + the advance/contact and
// block loops. Capacity 8 matches the King (fan uses slots 0-2, spiral 0-4).
// -----------------------------------------------------------------------------
class AttackPool {
public:
    static constexpr int CAP = 8;

    // Allocate the pool's bolt-sprite VFX (scaled 2x, hidden) on `cam`. Mirrors the
    // King's pool-init loop.
    AttackPool(const bn::sprite_item& bolt_item, const bn::camera_ptr& cam,
               int map_px_w, int map_px_h);

    // Launch projectile in slot `idx`, a 12x12 body centred on (cx_px, cy_px) with
    // integer velocity (vx, vy) px/frame. (Slot addressing is safe because only ONE
    // attack variant is live per Active window; a later window's launch into the same
    // slot simply overwrites any bolt still lingering there, and clear() runs on
    // expose/teleport. NOTE: clear() is NOT called at Recovery — both consumers let
    // in-flight bolts travel to the wall, so a fresh Active window may reuse a slot
    // whose previous bolt is still mid-flight.)
    void launch(int idx, int cx_px, int cy_px, int vx, int vy);

    // Deactivate + hide every projectile (expose / recovery / teleport).
    void clear();

    // Advance every active projectile, render it, expire off-arena, and apply
    // contact damage to the player. arena_w/arena_h are level pixel dimensions
    // (level.w*8, level.h*8). `map` is the room tilemap: a projectile despawns when
    // its centre enters a SOLID tile (wall/floor) — it PASSES THROUGH one-way
    // platforms + empty space (so bolts stop at geometry, not platforms). The
    // arena-bounds despawn stays as a backstop. player_iframed = the scene's
    // (invuln || dash-invincible) gate. On a fresh contact (not i-framed) returns
    // true so the scene applies the hit; the scene owns the actual
    // health.damage()/invuln assignment. Mirrors the King's "attack advance +
    // render + contact" loop.
    bool advance(const logic::Body& player_body, int arena_w, int arena_h,
                 bool player_iframed, const logic::Tilemap& map);

    // DEFENSE/BLOCK: a player bolt or Fire/Ice spell DESTROYS an incoming boss
    // projectile on contact (the shot is consumed). Light is NOT a blocker. Call
    // each frame after the player projectile pools update, BEFORE the boss-damage
    // check. Generic over the project's pools via templates so the library needs no
    // game/ include: BoltLike has `consume_hit(body)`; SpellLike has
    // `consume_hit(body, SpellId)`. Mirrors the King's "DEFENSE" loop.
    template<typename BoltLike, typename SpellLike>
    void block_player_shots(BoltLike& bolts, SpellLike& spells){
        for(AttackInst& a : _pool){
            if(!a.active) continue;
            if(bolts.consume_hit(a.body)
               || spells.consume_hit(a.body, logic::SpellId::Fire)
               || spells.consume_hit(a.body, logic::SpellId::Ice)){
                a.active = false;
                if(a.sprite) a.sprite->set_visible(false);
            }
        }
    }

private:
    AttackInst _pool[CAP];
    int _half_w_px;
    int _half_h_px;
};

// -----------------------------------------------------------------------------
// spawn_attack — fire one attack variant FROM the boss toward the player.
//   variant: use logic::BOSS_ATK_AIMED / BOSS_ATK_FAN (the spiral is streamed
//            per-frame via SpiralEmitter, not spawned here).
//   aimed (BOSS_ATK_AIMED): one bolt at the player. If `aim_full`, it's aimed at the player's actual
//            (player_cx, player_cy) — a velocity VECTOR toward the player, so a standing player can't
//            sit in a dead spot (they must MOVE to dodge). If !aim_full, it's a horizontal bolt at the
//            boss's height toward the player's side (the King's original behaviour — unchanged).
//   fan   (BOSS_ATK_FAN):   3-bolt vertical spread.
//   speed: px/frame (the King uses 2 in phase 0, 3 otherwise).
// -----------------------------------------------------------------------------
void spawn_attack(AttackPool& pool, int variant,
                  int boss_cx, int boss_cy, int player_cx, int player_cy,
                  int speed, int phase, bool aim_full);

// -----------------------------------------------------------------------------
// SpiralEmitter — the rotating spiral: ONE arm sweeping over BOSS_SPIRAL_STEPS
// directions on the player's side. Reset at the start of an Active window, then
// ticked each frame while the spiral attack is live. Mirrors scene_boss.cpp's
// spiral_idx/spiral_cd/spiral_rot state machine.
// -----------------------------------------------------------------------------
struct SpiralEmitter {
    static constexpr int STEPS = 5;   // arms launched per spiral window (King: 5)
    int idx = 0;
    int cd  = 0;
    int rot = 1;

    // Begin a spiral window: rot sweeps toward the player's side.
    void begin(int boss_cx, int player_cx){ idx = 0; cd = 0; rot = (player_cx >= boss_cx) ? 1 : -1; }

    // Tick: emit the next arm (if due) into `pool` from (boss_cx, boss_cy).
    void tick(AttackPool& pool, int boss_cx, int boss_cy);
};

// -----------------------------------------------------------------------------
// RockfallEmitter — Slagshell's jump-rockfall. begin() picks fair columns (logic::rockfall_columns)
// and shows a ground crack-marker at each for a warn delay; tick() drops rocks from the ceiling into
// the supplied rock AttackPool when the warn elapses (one drop per window). Markers are screen-fixed
// to the camera like the bolt VFX. clear() hides markers + resets (call on fight restart).
// -----------------------------------------------------------------------------
class RockfallEmitter {
public:
    static constexpr int MAXCOLS = 6;     // P2 uses up to 5; one slot of margin
    static constexpr int WARN_FRAMES = 26;// telegraph before rocks drop. INVARIANT: must be < the
                                          // rockfall phase's attack_active_frames (D2 = 30) so the
                                          // drop lands inside the Active window (tick runs only in Active).
    static constexpr int ROCK_VY = 3;     // downward px/frame

    RockfallEmitter(const bn::sprite_item& marker_item, const bn::camera_ptr& cam,
                    int map_px_w, int map_px_h);

    // Pick columns + show markers + start the warn timer. count = rocks this window (3 P1 / 5 P2);
    // seed = a rolling counter for variety. arena_*_tiles = level.w / level.h.
    void begin(int player_tx, int arena_w_tiles, int arena_h_tiles, int count, int seed);
    // Advance the warn timer; when it elapses, launch rocks from the ceiling into `rocks` (once) and
    // hide the markers. Call each frame while the rockfall attack is the live Active attack.
    void tick(AttackPool& rocks);
    void clear();
    // Cosmetic: the boss's "leap" height this frame (a triangle arc peaking mid-warn), 0 when
    // grounded/idle. run_room_boss subtracts it from the boss sprite's y so the rockfall reads as a
    // JUMP (the user-requested move). 0 outside a rockfall warn -> applying it always is a no-op then.
    int leap_offset() const;

private:
    bn::vector<bn::sprite_ptr, MAXCOLS> _markers;
    int _cols[MAXCOLS];
    int _col_count = 0;
    int _warn = 0;
    bool _dropped = false;
    int _arena_h_tiles = 0;
    int _half_w_px, _half_h_px;
};

// -----------------------------------------------------------------------------
// TelegraphCue — the coloured charge orb shown AT the boss during
// AttackStep::Telegraph so the incoming attack reads. Colour by variant:
// aimed -> aimed_item, spiral -> spiral_item, fan -> fan_item (the King wired
// red=fire/gold=light/cyan=ice). Mirrors scene_boss.cpp's telegraph_orb.
// -----------------------------------------------------------------------------
class TelegraphCue {
public:
    TelegraphCue(const bn::sprite_item& fallback_item, const bn::camera_ptr& cam,
                 int map_px_w, int map_px_h);

    // Show the cue for `variant` (logic::BOSS_ATK_*) above the boss, pulsing on
    // `attack_timer`. Call during Telegraph.
    void show(int variant, int boss_cx, int boss_cy, int attack_timer,
              const bn::sprite_item& aimed_item, const bn::sprite_item& spiral_item,
              const bn::sprite_item& fan_item);
    void hide();

private:
    bn::sprite_ptr _orb;
    int _half_w_px;
    int _half_h_px;
};

// -----------------------------------------------------------------------------
// BossHpBar — the boss HP-pip bar along the bottom-centre (screen-space, no
// camera), one pip per wound. Pip count = def.max_hp / def.wound_dmg, bounded to
// MAX_PIPS so it fits the King's 9 and D1's 6. Mirrors scene_boss.cpp's
// king_hp_pips + refresh_king_hp.
// -----------------------------------------------------------------------------
class BossHpBar {
public:
    static constexpr int MAX_PIPS = 16;

    // Build the pip row for `def` using `pip_item` (the King uses king_hp).
    BossHpBar(const logic::BossDef& def, const bn::sprite_item& pip_item);

    // Show pips proportional to `hp` (ceil(hp / wound_dmg) remain visible).
    void refresh(int hp);

private:
    bn::vector<bn::sprite_ptr, MAX_PIPS> _pips;
    int _wound_dmg;
};

// -----------------------------------------------------------------------------
// resolve_damage — the boss damage resolution (expose + wound + magic refill),
// driven entirely by logic::BossState. Generic over the project's spell/bolt
// pools via templates (no game/ include). Mirrors scene_boss.cpp's
// "damage resolution" block:
//   * Light (def's expose spell) ALWAYS exposes (every frame) — on_expose_hit.
//   * While exposed, a bolt OR Fire/Ice wound lands; an elemental wound refills
//     magic (heal 25). For AlwaysVulnerable bosses (no expose gate) the wound
//     check runs unconditionally — vulnerable() short-circuits expose.
// Returns true if a wound landed this frame (the scene reads this to react —
// e.g. the King teleports away). magic_heal is the elemental-wound magic refill
// (King: 25); the magic Meter is mutated via the supplied SpellLike/Meter only.
// -----------------------------------------------------------------------------
template<typename BoltLike, typename SpellLike, typename MeterLike>
bool resolve_damage(logic::BossState& b, const logic::Body& boss_body,
                    BoltLike& bolts, SpellLike& spells, MeterLike& magic, int magic_heal){
    // Light always exposes/refreshes (no-op once defeated/i-framed or AlwaysVulnerable).
    if(b.def->vuln == logic::VulnMode::SpellExpose &&
       spells.consume_hit(boss_body, b.def->expose_spell)){
        b.on_expose_hit(b.def->expose_spell);
    }
    int hp_before = b.hp;
    // Wounding lands only while vulnerable (exposed, or AlwaysVulnerable). Gate here
    // so a shot never silently vanishes off a shielded boss mid-frame.
    if(b.vulnerable()){
        if(bolts.consume_hit(boss_body)){
            b.on_wound(b.def->wound_dmg);
        } else if(spells.consume_hit(boss_body, logic::SpellId::Fire)
               || spells.consume_hit(boss_body, logic::SpellId::Ice)){
            b.on_wound(b.def->wound_dmg);
            magic.heal(magic_heal);
        }
    }
    return b.hp < hp_before;
}

}  // namespace engine
