#include "game/boss_fight.h"

#include "bn_core.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_text_generator.h"
#include "bn_sprite_tiles_item.h"        // graphics_count() / create_tiles() for the tired/element frame swap
#include "bn_sprite_tiles_ptr.h"         // complete type for set_tiles(create_tiles(...))
#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_items_bolt.h"        // King attack projectile (white bolt)
#include "bn_sprite_items_boss_bolt.h"   // room-boss attack projectile (distinct red bolt)
#include "bn_sprite_items_fire_proj.h"   // telegraph cue: red = aimed
#include "bn_sprite_items_light_proj.h"  // telegraph cue: gold = spiral
#include "bn_sprite_items_ice_proj.h"    // telegraph cue: cyan = fan
#include "bn_sprite_items_rock.h"        // rockfall rocks (D2)
#include "bn_sprite_items_rock_marker.h" // rockfall ground telegraph (D2)
#include "bn_sprite_items_king_hp.h"     // shared boss HP-pip art

#include "logic/boss.h"
#include "logic/tilemap.h"
#include "logic/world_state.h"   // lose_life, refill_lives, has(Ability)
#include "logic/player.h"        // Player, InputFrame
#include "logic/collision.h"     // aabb_overlap
#include "logic/meters.h"
#include "logic/spell.h"

#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/spell_pool.h"
#include "engine/boss_attacks.h" // AttackPool / SpiralEmitter / RockfallEmitter / TelegraphCue / BossHpBar / spawn_attack / resolve_damage
#include "engine/hud.h"
#include "engine/fade.h"
#include "engine/save.h"         // write_world (persist lives)
#include "engine/pause.h"        // check_pause

#include "game/player_session.h" // PlayerSession, CrystalStation, set_clamped_cam
#include "game/scene_game_over.h" // run_game_over (King inline Game-Over)

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
}

// -----------------------------------------------------------------------------
// run_boss_fight — see boss_fight.h. Faithful merge of run_boss (King) and
// run_room_boss (D1-D3); every per-boss fork below is annotated with the def
// field (or `inline_game_over`) that drives it.
// -----------------------------------------------------------------------------
FightOutcome run_boss_fight(const logic::LevelData& level, const logic::BossDef& def,
                            const bn::sprite_item& boss_sprite, logic::World& world,
                            logic::PlayerState& ps, engine::LoadedLevel& lvl,
                            bn::camera_ptr& cam, logic::Player& player,
                            const logic::Vec2& spawn_pos, const logic::EntranceSpawn& ent,
                            bool inline_game_over)
{
    logic::SpellState& spell = ps.spell;
    logic::Meter& health = ps.health;
    logic::Meter& magic  = ps.magic;

    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };

    // M14: the player now enters at their CARRIED-IN HP (no auto-heal on boss entry) —
    // the top-up is the pre-boss health pickup ('+') in the approach room (PickupsSystem),
    // which the player may skip. The King's extra entry setup (ps.health.max =
    // max_health_for, magic refill, spell.ensure_valid) is still the CALLER's job (see
    // header) — a room boss carries its magic in from the prior room.

    int invuln = 0;
    constexpr int RESPAWN_IFRAMES = 60;

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // Player session: spell HUD icon + ability-sync + input intent + aim muzzle
    // (replaces the 4x-duplicated local copies). refresh once now (Phase-4 review).
    PlayerSession session(cam, world, ps, player);
    session.refresh_spell_icon();

    // ---- boss state + body (14x16 hit-body, matching both originals) ----
    logic::BossState b; b.reset(def);
    logic::Body boss_body;
    boss_body.half_w = fx(14); boss_body.half_h = fx(16);

    // Non-perch placement (D1/D2/D3): centred on the arena floor, feet on the row-(h-2) surface.
    const int boss_cx_tile = level.w / 2;
    const int boss_floor_y = (level.h - 2) * 8;
    auto place_boss = [&]{
        boss_body.pos = { fx(boss_cx_tile * 8 - boss_body.half_w.to_int()),
                          fx(boss_floor_y - boss_body.half_h.to_int() * 2) };
    };
    auto boss_cx = [&]{ return boss_body.pos.x.to_int() + boss_body.half_w.to_int(); };
    auto boss_cy = [&]{ return boss_body.pos.y.to_int() + boss_body.half_h.to_int(); };

    // Pacing (Locomotion::Pacing = D2): walk the floor, reverse at the interior walls. Inert for
    // Stationary bosses (never advanced). Sub-pixel velocity so the pace can be < 1 px/frame.
    int pace_dir = 1;
    const logic::Fixed PACE_VEL = logic::Fixed::from_raw(128);   // 0.5 px/frame
    const int pace_min_cx = 8 + boss_body.half_w.to_int();
    const int pace_max_cx = (level.w - 1) * 8 - boss_body.half_w.to_int();

    // Teleport (perch_count > 0 = the King): cycle placed perches so it isn't a sitting duck.
    int perch_idx = 0;
    auto set_perch = [&](int idx){
        perch_idx = idx;
        boss_body.pos = { fx(def.perches[idx].cx * 8 - boss_body.half_w.to_int()),
                          fx(def.perches[idx].cy * 8 - boss_body.half_h.to_int()) };
    };
    int teleport_timer = 0;
    int teleport_flash = 0;   // brief blink after a teleport so it reads as a warp, not a glitch
    if(def.perch_count > 0){ set_perch(0); teleport_timer = def.teleport_period; }
    else                   { place_boss(); }

    // ---- boss sprite. A multi-frame sprite (guardian/slagshell/coldforge) swaps to a
    //      tired/element frame while exposed; a single-frame sprite (the King, 1 graphic)
    //      never swaps (create_tiles(1) would be out of range) — faithfully reproduces the
    //      King's blink-only render. ----
    const bool boss_has_frames = boss_sprite.tiles_item().graphics_count() > 1;
    bn::sprite_ptr boss_spr = boss_sprite.create_sprite(0, 0);
    boss_spr.set_camera(cam);
    boss_spr.set_position(wx(boss_cx()), wy(boss_cy()));
    int boss_frame = 0;

    engine::BossHpBar hp_bar(def, bn::sprite_items::king_hp);
    hp_bar.refresh(b.hp);

    // ---- attacks. The King's projectiles are white `bolt`; grounded room bosses use the
    //      distinct red `boss_bolt`. Keyed off perch_count (the King is the perched boss). Rocks +
    //      rockfall are always built (idle for non-ROCKFALL bosses — the King never begins one). ----
    const bn::sprite_item& atk_bolt_item = (def.perch_count > 0) ? bn::sprite_items::bolt
                                                                 : bn::sprite_items::boss_bolt;
    engine::AttackPool attacks(atk_bolt_item, cam, lvl.view.map_px_w, lvl.view.map_px_h);
    engine::AttackPool rocks(bn::sprite_items::rock, cam, lvl.view.map_px_w, lvl.view.map_px_h);
    engine::RockfallEmitter rockfall(bn::sprite_items::rock_marker, cam,
                                     lvl.view.map_px_w, lvl.view.map_px_h);
    int rockfall_seed = 0;
    engine::SpiralEmitter spiral;
    bool atk_spawned_this_active = false;
    int current_attack = logic::BOSS_ATK_AIMED;   // the attack firing during the CURRENT Active window
    int attack_slot = 0;                           // rotates over the bits set in this phase's mask

    // I20: ALL bosses select attacks via the SAME mask-rotation over phases[b.phase].attacks
    // (AIMED->SPIRAL->FAN->ROCKFALL, skipping absent bits). For the King's full AIMED|SPIRAL|FAN mask
    // this yields AIMED->SPIRAL->FAN, identical to its old hardcoded KING_ATTACK_CYCLE.
    auto next_attack_for_phase = [&](int& slot)->int{
        const uint8_t mask = def.phases[b.phase].attacks;
        static constexpr int ORDER[4] = {
            logic::BOSS_ATK_AIMED, logic::BOSS_ATK_SPIRAL, logic::BOSS_ATK_FAN, logic::BOSS_ATK_ROCKFALL };
        for(int step = 0; step < 4; ++step){
            int idx = (slot + step) % 4;
            if(mask & ORDER[idx]){ slot = (idx + 1) % 4; return ORDER[idx]; }
        }
        return logic::BOSS_ATK_AIMED;   // mask should never be empty
    };

    engine::TelegraphCue telegraph(bn::sprite_items::fire_proj, cam,
                                   lvl.view.map_px_w, lvl.view.map_px_h);

    // Respawning full-refill magic crystal (boss-arena semantics): FloorBelow grounding +
    // respawn_when_depleted so a SpellExpose boss can never magic-soft-lock. Handles the arena's
    // single crystal (the validator guarantees <= 1). Collision uses the authored tile position, so
    // this matches both originals' behaviour; only the King's crystal SPRITE now grounds on the floor
    // surface (the M13 fix) instead of the tile centre — the same collision, a corrected visual.
    CrystalStation station;
    station.spawn(level, cam, lvl.map, hw, hh,
                  CrystalStation::Grounding::FloorBelow, /*respawn_when_depleted=*/true);

    // Aimed bolts: a grounded boss (perch_count==0) aims a velocity VECTOR at the player (aim_full);
    // the perched King fires HORIZONTAL bolts at its perch height (the read that tested well). This
    // also selects the FAN shape (horizontal 3-height spread vs the King's angular fan).
    const bool aim_full = (def.perch_count == 0);

    int last_phase = 0;   // drives the phase-entry taunt (def.phase_lines)

    // ---- boss dialogue (centred text, wait for A/START). Lines come from the def; null = silent. ----
    bn::sprite_text_generator text_gen(common::variable_8x16_sprite_font);
    text_gen.set_center_alignment();
    auto boss_say = [&](const char* line){
        if(line == nullptr) return;
        bn::vector<bn::sprite_ptr, 32> say;
        text_gen.generate(0, 54, line, say);   // lower-centre, below the boss, clear of the HUD
        int t = 0;
        while(true){
            // ignore input briefly so a button held from the approach doesn't skip instantly
            if(t > 20 && (bn::keypad::a_pressed() || bn::keypad::start_pressed())) break;
            ++t;
            bn::core::update();
        }
    };  // 'say' sprites destroyed here -> dialogue clears

    // full-fight restart (death-with-lives, or the King's inline Game-Over Continue)
    auto restart_fight = [&]{
        b.on_player_death();                       // BossState -> phase 0, full HP, timers cleared
        if(def.perch_count > 0){ set_perch(0); teleport_timer = def.teleport_period; teleport_flash = 0; }
        else                   { place_boss(); pace_dir = 1; }   // re-centre a Pacing boss off the entrance
        health.cur = health.max; magic.cur = magic.max;
        player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
        player.body.on_ground = false;
        player.dash = logic::DashState{};
        player.grapple = logic::GrappleState{};
        player.stone = logic::StoneState{};
        player.facing = ent.facing;
        avatar.sync(player);
        invuln = RESPAWN_IFRAMES;
        attacks.clear(); rocks.clear(); rockfall.clear();
        atk_spawned_this_active = false;
        attack_slot = 0; current_attack = logic::BOSS_ATK_AIMED; spiral = engine::SpiralEmitter{};
        telegraph.hide();
        last_phase = 0;
        station.reset();
    };

    // Settle the player onto the floor before fading in (so they aren't shown embedded in the floor
    // during the intro), then fade in on the player + boss and speak the intro line (if any).
    { logic::InputFrame nin{}; for(int i = 0; i < 24; ++i) player.update(nin, lvl.map); }
    avatar.sync(player);
    set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, level.w, level.h,
                    player.body.pos.x.to_int() + player.body.half_w.to_int(),
                    player.body.pos.y.to_int() + player.body.half_h.to_int());
    engine::fade_in(16);
    boss_say(def.intro_line);
    int fade_in_t = 0;   // already faded in; the Game-Over restart path re-arms this

    while(true)
    {
        engine::check_pause();   // START -> freeze + "GAME PAUSED" until START again

        // ---- input + abilities. The arena has grapple anchors but no pull targets, so re-assert
        //      grapple_fire = want_grapple after read_intent (which clears it) — both originals did. ----
        SessionIntent intent = session.read_intent();
        session.sync_abilities();
        logic::InputFrame in = intent.in;
        in.grapple_fire = intent.want_grapple;
        bool cast_spell = intent.cast_spell;

        player.update(in, lvl.map);
        avatar.sync(player);

        // ---- boss logic tick ----
        b.tick();

        // ---- teleport (perch_count > 0): cycle perches, paused while EXPOSED so the player keeps a
        //      stationary wound window. ----
        if(def.perch_count > 0 && !b.exposed() && !b.defeated()){
            if(--teleport_timer <= 0){
                set_perch((perch_idx + 1) % def.perch_count);
                teleport_timer = def.teleport_period;
                teleport_flash = 12;
                attacks.clear();                 // don't leave a bolt flying from the old position
                atk_spawned_this_active = false;
            }
        }
        if(teleport_flash > 0) --teleport_flash;

        // ---- pacing (Locomotion::Pacing): move horizontally, reverse at the walls. Paused while
        //      EXPOSED so the clean wound window doesn't also require tracking a moving target. ----
        if(def.locomotion == logic::Locomotion::Pacing && !b.exposed()){
            boss_body.pos.x = boss_body.pos.x + (pace_dir > 0 ? PACE_VEL : -PACE_VEL);
            int cx = boss_cx();
            if(cx <= pace_min_cx){ boss_body.pos.x = fx(pace_min_cx - boss_body.half_w.to_int()); pace_dir = 1; }
            else if(cx >= pace_max_cx){ boss_body.pos.x = fx(pace_max_cx - boss_body.half_w.to_int()); pace_dir = -1; }
        }

        // ---- boss render: leap arc (rockfall; 0 otherwise), tired/element frame swap (multi-frame
        //      sprites only), then teleport-flash / EXPOSED / telegraph pulse blink. ----
        boss_spr.set_position(wx(boss_cx()), wy(boss_cy()) - rockfall.leap_offset());
        if(boss_has_frames){
            // 4-frame shift boss (D3): frames 0-1 = element A (expose_spell), 2-3 = element B
            // (expose_spell_alt). Non-shift bosses keep elem_base 0 -> unchanged 2-frame behaviour.
            // +1 within a pair = the exposed frame.
            int elem_base = (def.expose_spell_alt != logic::SpellId::None
                             && b.cur_expose == def.expose_spell_alt) ? 2 : 0;
            int want_frame = elem_base + (b.exposed() ? 1 : 0);
            if(want_frame != boss_frame){
                boss_spr.set_tiles(boss_sprite.tiles_item().create_tiles(want_frame));
                boss_frame = want_frame;
            }
        }
        if(teleport_flash > 0)                                        boss_spr.set_visible((teleport_flash / 2) % 2 == 0);
        else if(b.exposed())                                         boss_spr.set_visible((b.expose_timer / 4) % 2 == 0);
        else if(b.current_step() == logic::AttackStep::Telegraph)    boss_spr.set_visible((b.attack_timer / 8) % 2 == 0);
        else                                                         boss_spr.set_visible(true);

        // ---- attack telegraph + spawn — skipped while EXPOSED (= a clean damage window) ----
        if(b.exposed()){
            attacks.clear(); rockfall.clear(); atk_spawned_this_active = false; telegraph.hide();
        } else {
            logic::AttackStep step = b.current_step();
            if(step == logic::AttackStep::Telegraph){
                atk_spawned_this_active = false;
                int peek = attack_slot;
                int next_variant = next_attack_for_phase(peek);   // peek without advancing
                telegraph.show(next_variant, boss_cx(), boss_cy(), b.attack_timer,
                               bn::sprite_items::fire_proj, bn::sprite_items::light_proj,
                               bn::sprite_items::ice_proj);
            } else if(step == logic::AttackStep::Active){
                telegraph.hide();
                if(!atk_spawned_this_active){
                    current_attack = next_attack_for_phase(attack_slot);   // lock + advance
                    int pcx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
                    int pcy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
                    spiral.begin(boss_cx(), pcx0);
                    if(current_attack == logic::BOSS_ATK_ROCKFALL){
                        int player_tx = (player.body.pos.x.to_int() + player.body.half_w.to_int()) / 8;
                        int rock_count = def.phases[b.phase].rock_count;    // per-phase (def; 3 P1 / 5 P2)
                        rockfall.begin(player_tx, level.w, level.h, rock_count, rockfall_seed++);
                    } else if(current_attack != logic::BOSS_ATK_SPIRAL){
                        int spd = def.phases[b.phase].proj_speed;           // per-phase (def; 2 / 3)
                        engine::spawn_attack(attacks, current_attack, boss_cx(), boss_cy(),
                                             pcx0, pcy0, spd, b.phase, aim_full);
                    }
                    atk_spawned_this_active = true;
                }
                if(current_attack == logic::BOSS_ATK_SPIRAL)   spiral.tick(attacks, boss_cx(), boss_cy());
                if(current_attack == logic::BOSS_ATK_ROCKFALL) rockfall.tick(rocks);
            } else { // Recovery — let in-flight bolts KEEP travelling to the wall (they despawn on solid
                     // geometry / off-arena in advance()); only reset the per-window latch + hide the cue.
                atk_spawned_this_active = false; telegraph.hide(); rockfall.clear();
            }
        }

        // ---- attack advance + render + contact (bolts and rocks) ----
        bool proj_hit = attacks.advance(player.body, level.w * 8, level.h * 8,
                                        invuln != 0 || player.dash.invincible(), lvl.map);
        if(proj_hit){ health.damage(20); invuln = 45; }

        bool rock_hit = rocks.advance(player.body, level.w * 8, level.h * 8,
                                      invuln != 0 || player.dash.invincible(), lvl.map);
        if(rock_hit){ health.damage(20); invuln = 45; }

        // ---- boss contact damage (touching the boss body hurts — fight from range) ----
        if(invuln == 0 && !player.dash.invincible() && logic::aabb_overlap(player.body, boss_body)){
            health.damage(20); invuln = 45;
        }

        // ---- projectile pools (shot aim shared via the session muzzle / engine::read_aim_dy) ----
        logic::Vec2 muzzle = session.muzzle();
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);
        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);

        // ---- magic crystal: full refill on overlap; reappears once magic drops below one cast ----
        station.update(player.body, magic);

        // ---- DEFENSE / block (BEFORE the wound check, so a blocked shot can't also wound) ----
        switch(def.block_mode){
            case logic::BlockMode::BoltAndSpellBlock:
                // King: the free BOLT and Fire/Ice both auto-block incoming bolts (Light passes
                // through to expose). No magic reward — the King's magic comes from wounds + crystal.
                attacks.block_player_shots(bolts, spells);
                break;
            case logic::BlockMode::SpellBlock:
            case logic::BlockMode::SpellAndBoltBlock: {
                // D2/D3: a cast of block_spell (and, dual-element, block_spell2) destroys a boss bolt;
                // each block RECHARGES magic (the block IS the magic economy). Rocks are NOT blockable.
                // SpellAndBoltBlock (D3): the free bolt ALSO blocks + charges, alongside Fire/Ice.
                constexpr int BLOCK_MAGIC_CHARGE = 25;
                int blocked = 0;
                if(def.block_spell  != logic::SpellId::None) blocked += attacks.block_with_spell(spells, def.block_spell);
                if(def.block_spell2 != logic::SpellId::None) blocked += attacks.block_with_spell(spells, def.block_spell2);
                if(def.block_mode == logic::BlockMode::SpellAndBoltBlock) blocked += attacks.block_with_bolt(bolts);
                if(blocked) magic.heal(BLOCK_MAGIC_CHARGE * blocked);
                break;
            }
            case logic::BlockMode::None:
            default:
                break;   // D1 dodge-only
        }

        // ---- damage resolution (cur_expose exposes; bolt/Fire/Ice wounds while vulnerable; an
        //      elemental wound refills magic 25). Returns whether a wound landed this frame. ----
        bool wounded = engine::resolve_damage(b, boss_body, bolts, spells, magic, /*magic_heal=*/25);

        // ---- phase-entry taunt (def.phase_lines; null = silent -> no-op for room bosses) ----
        {
            int cp = b.phase;
            if(cp != last_phase && !b.defeated()){
                last_phase = cp;
                boss_say(def.phase_lines[cp]);
            }
        }

        if(b.defeated()){
            boss_say(def.death_line);
            // Room-boss victory: vitals are NOT restored here — recovery is deferred to the spronk touch (PickupsSystem). Health stays as-is; magic carries.
            return FightOutcome::Victory;
        }

        // ---- teleport-on-wound (perch_count>0 && teleport_on_wound = the King): warp to a new perch,
        //      clear in-flight attacks, restart the attack cycle so it winds up at the new spot. ----
        if(wounded && def.perch_count > 0 && def.teleport_on_wound){
            set_perch((perch_idx + 1) % def.perch_count);
            teleport_timer = def.teleport_period;
            teleport_flash = 12;
            attacks.clear();
            atk_spawned_this_active = false;
            b.attack_timer = 0;
        }

        spells.despawn_on_solid(lvl.map);

        // ---- i-frames blink ----
        if(invuln > 0){ --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);

        // ---- death / lives / Game-Over ----
        if(health.is_empty()){
            logic::lose_life(world);
            engine::write_world(world);   // persist the decremented count immediately
            if(inline_game_over){
                // King semantics: run the Game-Over scene INLINE; never return GameOver.
                if(world.lives > 0){
                    restart_fight();          // full-fight restart (soft fade-back below)
                } else {
                    game::GameOverChoice c = game::run_game_over(world);
                    logic::refill_lives(world);   // both choices refill to max
                    engine::write_world(world);   // persist the refill — the save never holds lives==0
                    if(c == game::GameOverChoice::QuitToTitle) return FightOutcome::QuitToTitle;
                    restart_fight();              // Continue: restart the fight
                }
                // run_game_over / the restart took over the screen; re-arm the soft fade-in.
                engine::set_fade(16); fade_in_t = 16;
            } else {
                // Room semantics: restart on lives-left (blocking fade + REPLAY the intro line); else
                // hand GameOver back to the dungeon flow (which owns the Game-Over scene).
                if(world.lives > 0){
                    engine::fade_out(16);
                    restart_fight();
                    set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, level.w, level.h,
                                    player.body.pos.x.to_int() + player.body.half_w.to_int(),
                                    player.body.pos.y.to_int() + player.body.half_h.to_int());
                    engine::fade_in(16);
                    boss_say(def.intro_line);
                    fade_in_t = 0;   // already faded in above
                } else {
                    return FightOutcome::GameOver;
                }
            }
        }

        session.refresh_spell_icon();
        hp_bar.refresh(b.hp);
        hud.update(health, magic, world.lives);
        set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, level.w, level.h,
                        player.body.pos.x.to_int() + player.body.half_w.to_int(),
                        player.body.pos.y.to_int() + player.body.half_h.to_int());
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
