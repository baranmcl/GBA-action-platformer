#include "game/scene_boss.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_text_generator.h"
#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_items_king.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
#include "bn_sprite_items_light_proj.h"
#include "bn_sprite_items_grapple_icon.h"
#include "bn_sprite_items_magic_crystal.h"
#include "bn_sprite_items_king_hp.h"

#include "logic/boss.h"
#include "logic/tilemap.h"
#include "logic/world_state.h"   // max_health_for, lose_life, refill_lives
#include "logic/player.h"
#include "logic/collision.h"     // aabb_overlap
#include "logic/meters.h"
#include "logic/spell.h"
#include "logic/room_graph.h"    // find_entrance
#include "engine/input.h"
#include "engine/spell_input.h"
#include "engine/level_loader.h" // load_level, set_collision_tile
#include "engine/level_view.h"   // set_level_tile
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/spell_pool.h"
#include "engine/boss_attacks.h"   // engine::AttackPool / SpiralEmitter / TelegraphCue / BossHpBar / spawn_attack / resolve_damage
#include "engine/hud.h"
#include "engine/fade.h"
#include "engine/save.h"         // write_world (persist refilled lives)
#include "engine/pause.h"        // check_pause (START -> GAME PAUSED)
#include "game/scene_game_over.h"

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }

    // King perches (center tile coords). The King TELEPORTS between these so it isn't a sitting duck.
    // All sit at a FLOOR-HITTABLE height (row 27 — spell bolts travel HORIZONTALLY, so the King must
    // share the player's standing height to be shootable from the ground; the read that tested well).
    // X spreads across the arena so it floats "around the platforms"; the platforms are for DODGING.
    // QA-tunable (positions + TELEPORT_PERIOD).
    struct Perch { int cx, cy; };
    // A mix of FLOOR perches (row 27 — shoot from the ground) and PLATFORM perches (row 17 — the King
    // stands atop the central/right platforms; hit it from the OTHER platform at the same height). The
    // teleport cycles high+low so the player must reposition (climb / cross) to keep hitting it.
    constexpr Perch KING_PERCHES[] = {
        {19, 27},   // floor centre
        {17, 17},   // atop the central platform (climb a platform to hit)
        {30, 27},   // floor right
        {28, 17},   // atop the right platform
        {9, 27},    // floor left
        {25, 27},   // floor centre-right
    };
    constexpr int KING_PERCH_COUNT = 6;
    constexpr int TELEPORT_PERIOD = 200;   // frames between teleports (paused while EXPOSED)

    logic::Body tile_body(int tx, int ty, int hw, int hh){
        logic::Body b{}; b.half_w = fx(hw); b.half_h = fx(hh);
        b.pos = { fx(tx * 8), fx(ty * 8) }; return b;
    }
}

BossResult run_boss(const logic::DungeonData& arena, logic::World& world, logic::PlayerState& ps)
{
    logic::SpellState& spell = ps.spell;
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    const logic::LevelData& level = *arena.rooms[arena.start_room];
    engine::LoadedLevel lvl = engine::load_level(level);
    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);

    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };

    // ---- player spawn (entrance id 0) ----
    logic::EntranceSpawn ent = logic::find_entrance(level, 0);
    const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };
    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;
    player.facing = ent.facing;

    // Vitals: carried-in references, started at FULL for the fight (sync cap first, as run_dungeon does).
    logic::Meter& health = ps.health;
    logic::Meter& magic  = ps.magic;
    ps.health.max = logic::max_health_for(world);
    health.cur = health.max;
    magic.cur  = magic.max;
    spell.ensure_valid(world);
    int invuln = 0;
    constexpr int RESPAWN_IFRAMES = 60;

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // ---- King ----
    logic::BossState b; b.reset(logic::KING_DEF);
    // King hit-body: ~28x32, roughly matching the 32x32 sprite (a forgiving hitbox so the player can
    // reliably land Light/bolt/Fire/Ice from the floor — QA: it was too hard to hit). pos = top-left.
    logic::Body king_body;
    king_body.half_w = fx(14); king_body.half_h = fx(16);
    int perch_idx = 0;
    auto set_king_perch = [&](int idx){
        perch_idx = idx;
        king_body.pos = { fx(KING_PERCHES[idx].cx * 8 - king_body.half_w.to_int()),
                          fx(KING_PERCHES[idx].cy * 8 - king_body.half_h.to_int()) };
    };
    set_king_perch(0);
    int teleport_timer = TELEPORT_PERIOD;
    int teleport_flash = 0;   // brief blink after a teleport so it reads as a warp, not a glitch
    bn::sprite_ptr king = bn::sprite_items::king.create_sprite(0, 0);
    king.set_camera(cam);
    {
        int kcx = king_body.pos.x.to_int() + king_body.half_w.to_int();
        int kcy = king_body.pos.y.to_int() + king_body.half_h.to_int();
        king.set_position(wx(kcx), wy(kcy));
    }

    // ---- King HP bar: violet King-HP pips along the BOTTOM-centre (screen-space, no camera), one
    //      per wound. Bottom + dedicated art so it never overlaps the player's top-left HUD. Derived
    //      from KING_DEF (max_hp / wound_dmg = 9) via the shared boss_attacks library. ----
    engine::BossHpBar king_hp_bar(logic::KING_DEF, bn::sprite_items::king_hp);
    king_hp_bar.refresh(b.hp);

    // ---- attack projectiles (shared library pool; EVERY attack is emitted FROM the King) ----
    engine::AttackPool attacks(bn::sprite_items::bolt, cam, lvl.view.map_px_w, lvl.view.map_px_h);
    engine::SpiralEmitter spiral;           // one arm sweeping toward the player during a spiral window
    bool atk_spawned_this_active = false;   // edge-detect the Active step
    // Attack SELECTION (King-only rotation; reproduces today's 3-attack cycle aimed->spiral->fan over
    // KING_PHASES[b.phase].attacks, which is the full KING_ATTACKS mask in every phase).
    static constexpr int KING_ATTACK_CYCLE[3] = {
        logic::BOSS_ATK_AIMED, logic::BOSS_ATK_SPIRAL, logic::BOSS_ATK_FAN };
    int attack_slot = 0;                     // NEXT slot in the rotation (0 aimed, 1 spiral, 2 fan)
    int current_attack = logic::BOSS_ATK_AIMED;  // the attack firing during the CURRENT Active window

    // Telegraph cue: a coloured charge orb shown AT the King during the wind-up so the incoming attack
    // is READABLE — red (fire) = aimed, gold (light) = spiral, cyan (ice) = spread (shared library cue).
    engine::TelegraphCue telegraph(bn::sprite_items::fire_proj, cam,
                                   lvl.view.map_px_w, lvl.view.map_px_h);

    auto king_cx = [&]{ return king_body.pos.x.to_int() + king_body.half_w.to_int(); };
    auto king_cy = [&]{ return king_body.pos.y.to_int() + king_body.half_h.to_int(); };

    // ---- magic crystal (M10 pattern: full refill on overlap; reset un-collected each attempt) ----
    bool crystal_collected = false;
    bn::optional<bn::sprite_ptr> crystal_sprite;
    int crystal_tx = 0, crystal_ty = 0;
    if(level.magic_crystal_count > 0){
        const logic::MagicCrystalSpawn& mc = level.magic_crystals[0];
        crystal_tx = mc.tx; crystal_ty = mc.ty;
        crystal_sprite = bn::sprite_items::magic_crystal.create_sprite(0, 0);
        crystal_sprite->set_camera(cam);
        crystal_sprite->set_position(wx(mc.tx * 8 + 8), wy(mc.ty * 8 + 8));
    }
    logic::Body crystal_body = tile_body(crystal_tx, crystal_ty, 6, 8);

    // ---- spell HUD icon (top-right; swap art on selection change only) ----
    bn::sprite_ptr spell_icon = bn::sprite_items::fire_proj.create_sprite(104, -68);
    logic::SpellId last_icon = logic::SpellId::None;
    auto refresh_spell_icon = [&]{
        if(spell.selected != last_icon){
            if(spell.selected == logic::SpellId::Ice)          spell_icon.set_item(bn::sprite_items::ice_proj);
            else if(spell.selected == logic::SpellId::Fire)    spell_icon.set_item(bn::sprite_items::fire_proj);
            else if(spell.selected == logic::SpellId::Grapple) spell_icon.set_item(bn::sprite_items::grapple_icon);
            else if(spell.selected == logic::SpellId::Light)   spell_icon.set_item(bn::sprite_items::light_proj);
            last_icon = spell.selected;
        }
        spell_icon.set_visible(spell.selected != logic::SpellId::None);
    };
    refresh_spell_icon();

    // Clamped follow-camera (the arena is larger than the 240x160 screen, so a fixed camera would
    // leave a corner spawn off-screen — the QA bug). Mirrors scene_dungeon's set_clamped_cam: centre
    // on the player, clamped so the view never scrolls past the authored level.
    auto set_clamped_cam = [&](int cx, int cy){
        const int ll = -hw, lt = -hh, lr = ll + level.w * 8, lb = lt + level.h * 8;
        const int minx = ll + 120, maxx = lr - 120, miny = lt + 80, maxy = lb - 80;
        int camx = cx - hw, camy = cy - hh;
        camx = (minx <= maxx) ? (camx < minx ? minx : camx > maxx ? maxx : camx) : (ll + lr) / 2;
        camy = (miny <= maxy) ? (camy < miny ? miny : camy > maxy ? maxy : camy) : (lt + lb) / 2;
        cam.set_position(camx, camy);
    };

    // Phase-transition announcement so the player feels progression (the fight has 3 HP-gated phases,
    // shown by the King HP bar — every 3 pips). combat_phase() = the underlying phase even mid-expose.
    auto combat_phase = [&]{ return b.phase; };   // phase-as-index: b.phase holds the combat phase even mid-expose
    int last_phase = 0;

    // restart_fight: every restart path (death-with-lives, Game-Over Continue) runs this.
    auto restart_fight = [&]{
        b.on_player_death();                       // BossState -> P1, full HP, timers cleared
        health.cur = health.max; magic.cur = magic.max;
        player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
        player.body.on_ground = false;
        player.dash = logic::DashState{};
        player.grapple = logic::GrappleState{};
        player.stone = logic::StoneState{};
        player.facing = ent.facing;
        avatar.sync(player);
        invuln = RESPAWN_IFRAMES;
        attacks.clear(); atk_spawned_this_active = false;
        attack_slot = 0; current_attack = logic::BOSS_ATK_AIMED; spiral = engine::SpiralEmitter{};
        telegraph.hide();
        set_king_perch(0); teleport_timer = TELEPORT_PERIOD; teleport_flash = 0;
        last_phase = 0;
        crystal_collected = false;
        if(crystal_sprite) crystal_sprite->set_visible(true);
    };

    // ---- boss dialogue (King speaks before + after the fight) ----
    bn::sprite_text_generator text_gen(common::variable_8x16_sprite_font);
    text_gen.set_center_alignment();
    auto boss_say = [&](const char* line){
        bn::vector<bn::sprite_ptr, 32> say;
        text_gen.generate(0, 54, line, say);   // lower-centre, below the King, clear of the HUD
        int t = 0;
        while(true){
            // ignore input briefly so a button held from the approach doesn't skip instantly
            if(t > 20 && (bn::keypad::a_pressed() || bn::keypad::start_pressed())) break;
            ++t;
            bn::core::update();
        }
    };  // 'say' sprites destroyed here -> dialogue clears

    // Settle the player onto the floor (gravity) BEFORE the intro renders, so they aren't shown
    // embedded in the floor at the raw spawn row (QA: player appeared under the floor during dialogue).
    { logic::InputFrame nin{}; for(int i = 0; i < 24; ++i) player.update(nin, lvl.map); }

    // Intro: fade in on the player + King (so the player is VISIBLE at the start — QA fix), then greet.
    avatar.sync(player);
    set_clamped_cam(player.body.pos.x.to_int() + player.body.half_w.to_int(),
                    player.body.pos.y.to_int() + player.body.half_h.to_int());
    engine::fade_in(16);
    boss_say("YOU FINALLY MADE IT");
    int fade_in_t = 0;   // already faded in by the intro; the Game-Over path re-arms this

    while(true)
    {
        engine::check_pause();   // START -> freeze + "GAME PAUSED" until START again

        // ---- input + abilities (no quit/restart keys: run_boss has no mid-fight exit) ----
        logic::InputFrame in = engine::read_input();
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.abilities.glide       = world.has(logic::Ability::Glide);
        player.abilities.dash        = world.has(logic::Ability::Dash);
        player.abilities.grapple     = world.has(logic::Ability::Grapple);
        player.abilities.stone       = world.has(logic::Ability::Stone);

        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);
        bool want_grapple = si.cast && spell.selected == logic::SpellId::Grapple;
        in.grapple_fire = want_grapple;   // arena has anchors but no pull-blocks/enemies to grab
        bool cast_spell = si.cast && (spell.selected == logic::SpellId::Fire ||
                                      spell.selected == logic::SpellId::Ice  ||
                                      spell.selected == logic::SpellId::Light);

        player.update(in, lvl.map);
        avatar.sync(player);

        // ---- boss logic tick ----
        b.tick();

        // ---- King teleport: cycle perches so it isn't a sitting duck (it was too easy to spam down).
        //      Paused while EXPOSED so the player keeps a stationary window to wound it. ----
        if(!b.exposed() && !b.defeated()){
            if(--teleport_timer <= 0){
                set_king_perch((perch_idx + 1) % KING_PERCH_COUNT);
                teleport_timer = TELEPORT_PERIOD;
                teleport_flash = 12;
                attacks.clear();                 // don't leave a bolt flying from the old position
                atk_spawned_this_active = false;
            }
        }
        if(teleport_flash > 0) --teleport_flash;

        // ---- King render: teleport flash, then EXPOSED blink, then telegraph pulse ----
        {
            int kcx = king_body.pos.x.to_int() + king_body.half_w.to_int();
            int kcy = king_body.pos.y.to_int() + king_body.half_h.to_int();
            king.set_position(wx(kcx), wy(kcy));
            if(teleport_flash > 0){
                king.set_visible((teleport_flash / 2) % 2 == 0);     // fast warp blink
            } else if(b.exposed()){
                king.set_visible((b.expose_timer / 4) % 2 == 0);     // vulnerable / stunned blink
            } else if(b.current_step() == logic::AttackStep::Telegraph){
                king.set_visible((b.attack_timer / 8) % 2 == 0);     // wind-up pulse (warning)
            } else {
                king.set_visible(true);
            }
        }

        // ---- attack telegraph + spawn — ONLY while NOT exposed (expose = clean window) ----
        //   Telegraph: a coloured charge orb at the King reads the incoming attack. Active: aimed/spread
        //   fire once from the King; the spiral streams orbs from the King in rotating directions.
        if(b.exposed()){
            attacks.clear(); atk_spawned_this_active = false; telegraph.hide();
        } else {
            logic::AttackStep step = b.current_step();
            if(step == logic::AttackStep::Telegraph){
                atk_spawned_this_active = false;
                // colour the cue by the NEXT attack in the rotation (fire=aimed, light=spiral, ice=fan).
                telegraph.show(KING_ATTACK_CYCLE[attack_slot], king_cx(), king_cy(), b.attack_timer,
                               bn::sprite_items::fire_proj, bn::sprite_items::light_proj,
                               bn::sprite_items::ice_proj);
            } else if(step == logic::AttackStep::Active){
                telegraph.hide();
                if(!atk_spawned_this_active){
                    current_attack = KING_ATTACK_CYCLE[attack_slot];   // lock the attack for this window
                    attack_slot = (attack_slot + 1) % 3;               // advance for next time
                    int pcx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
                    spiral.begin(king_cx(), pcx0);                      // sweep toward the player's side
                    if(current_attack != logic::BOSS_ATK_SPIRAL){      // aimed/fan fire now (FROM the King)
                        int spd = (b.phase == 0) ? 2 : 3;
                        engine::spawn_attack(attacks, current_attack, king_cx(), king_cy(),
                                             pcx0, king_cy(), spd, b.phase, /*aim_full=*/false);
                    }
                    atk_spawned_this_active = true;
                }
                if(current_attack == logic::BOSS_ATK_SPIRAL)           // spiral streams during Active
                    spiral.tick(attacks, king_cx(), king_cy());
            } else { // Recovery — let in-flight bolts KEEP travelling to the wall (AttackPool::advance
                     // despawns them on solid geometry / off-arena); only reset the per-window spawn
                     // latch + hide the cue. Clearing here was the "King bolts vanish before the wall"
                     // bug — the SAME fix already applied to the D1 room boss. (A teleport / expose
                     // still clears the pool, and a new Active window's launch overwrites its slots.)
                atk_spawned_this_active = false; telegraph.hide();
            }
        }

        // ---- attack advance + render + contact (all active projectiles, via the library pool) ----
        bool proj_hit = attacks.advance(player.body, level.w * 8, level.h * 8,
                                        invuln != 0 || player.dash.invincible(), lvl.map);
        if(proj_hit){ health.damage(20); invuln = 45; }   // 20-dmg / 45-iframe contact tuning is King-scene

        // ---- King contact damage: touching the King's body hurts (fight from range) ----
        if(invuln == 0 && !player.dash.invincible() && logic::aabb_overlap(player.body, king_body)){
            health.damage(20); invuln = 45;
        }

        // ---- projectile pools ----
        // Shot aim (Zelda II style, shared by all scenes via engine::read_aim_dy): UP = high, DOWN =
        // low, else medium. Bolt + spell share the muzzle, so both aim — makes BLOCKING practical.
        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h + fx(engine::read_aim_dy()) };
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);
        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);

        // ---- DEFENSE: the player's bolt / Fire / Ice DESTROYS an incoming boss projectile on contact
        //      (block). The shot is consumed by the block, so it can't also reach the King. (Light is
        //      NOT a blocker — it must pass through to expose.) Runs BEFORE the King-damage check so a
        //      blocked shot never doubles as a wound. (Shared library block defense.) ----
        attacks.block_player_shots(bolts, spells);

        // ---- damage resolution (shared library; Light exposes, bolt/Fire/Ice wounds while exposed,
        //      elemental wound refills magic 25). Returns whether a wound landed THIS frame -> the King
        //      reacts by teleporting away (King-only side effect, applied below). ----
        bool wounded = engine::resolve_damage(b, king_body, bolts, spells, magic, /*magic_heal=*/25);
        // ---- phase-change dialogue (the King taunts as he escalates — in-character, not a banner) ----
        {
            int cp = combat_phase();
            if(cp != last_phase && !b.defeated()){
                last_phase = cp;
                if(cp == 1)      boss_say("NOW YOU'RE GETTING ME ANGRY");
                else if(cp == 2) boss_say("I'M DONE TOYING WITH YOU");
            }
        }

        if(b.defeated()){ boss_say("NOOOOO!"); return BossResult::Victory; }

        // ---- King teleports AWAY after taking a wound (fairer than attacking immediately): warp to a
        //      new perch, clear in-flight attacks, and restart the attack cycle (fresh telegraph) so it
        //      winds up at the new spot rather than striking instantly. (BossState i-frames already
        //      prevent an immediate re-expose.) ----
        if(wounded){
            set_king_perch((perch_idx + 1) % KING_PERCH_COUNT);
            teleport_timer = TELEPORT_PERIOD;
            teleport_flash = 12;
            attacks.clear();
            atk_spawned_this_active = false;
            b.attack_timer = 0;
        }

        spells.despawn_on_solid(lvl.map);

        // ---- magic crystal: full refill on overlap; REAPPEARS once magic is spent below one cast,
        //      so the player can recharge again mid-fight (a repeatable station, not one-shot). ----
        if(!crystal_collected && logic::aabb_overlap(player.body, crystal_body)){
            magic.cur = magic.max;
            crystal_collected = true;
            if(crystal_sprite) crystal_sprite->set_visible(false);
        }
        if(crystal_collected && magic.cur < 10){   // 10 = one spell cast (SpellCast.cost) -> depleted
            crystal_collected = false;
            if(crystal_sprite) crystal_sprite->set_visible(true);
        }

        // ---- i-frames blink ----
        if(invuln > 0){ --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);

        // ---- death / lives / Game-Over (M9 flow; the WHOLE fight restarts, not the approach) ----
        if(health.is_empty()){
            logic::lose_life(world);
            engine::write_world(world);   // persist the decremented count immediately
            if(world.lives > 0){
                restart_fight();          // full-fight restart: BossState->P1, vitals, position, crystal
            } else {
                game::GameOverChoice c = game::run_game_over(world);
                logic::refill_lives(world);   // both choices refill to max
                engine::write_world(world);   // persist the refill — the save never holds lives==0
                if(c == game::GameOverChoice::QuitToTitle) return BossResult::QuitToTitle;
                restart_fight();              // Continue: restart the fight (vitals refilled inside)
            }
            // run_game_over took over the screen; re-arm the fade-in so the fight fades back in.
            engine::set_fade(16); fade_in_t = 16;
        }

        refresh_spell_icon();
        king_hp_bar.refresh(b.hp);
        hud.update(health, magic, world.lives);
        set_clamped_cam(player.body.pos.x.to_int() + player.body.half_w.to_int(),
                        player.body.pos.y.to_int() + player.body.half_h.to_int());
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
