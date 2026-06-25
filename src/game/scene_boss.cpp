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

    // One placeholder attack hitbox: a moving logic::Body + a reused bolt-sprite VFX.
    // Spawned at the start of AttackStep::Active; cleared at Recovery / while exposed.
    struct AttackInst {
        bool active = false;
        logic::Body body;
        logic::Vec2 vel{};
        bn::optional<bn::sprite_ptr> sprite;
    };
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
    logic::BossState b; b.reset();
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
    //      per WOUND_DMG. Bottom + dedicated art so it never overlaps the player's top-left HUD. ----
    constexpr int KING_HP_PIPS = logic::KING_MAX_HP / logic::WOUND_DMG;  // 9
    bn::vector<bn::sprite_ptr, 9> king_hp_pips;
    for(int i = 0; i < KING_HP_PIPS; ++i)
        king_hp_pips.push_back(bn::sprite_items::king_hp.create_sprite(-32 + i * 8, 68));
    auto refresh_king_hp = [&]{
        int alive = (b.hp + logic::WOUND_DMG - 1) / logic::WOUND_DMG;  // ceil(hp / WOUND_DMG)
        for(int i = 0; i < king_hp_pips.size(); ++i)
            king_hp_pips[i].set_visible(i < alive);
    };
    refresh_king_hp();

    // ---- attack projectiles (pool; EVERY attack is emitted FROM the King) ----
    bn::vector<AttackInst, 8> attacks;
    for(int i = 0; i < 8; ++i){
        attacks.push_back(AttackInst{});
        attacks[i].sprite = bn::sprite_items::bolt.create_sprite(0, 0);
        attacks[i].sprite->set_camera(cam);
        attacks[i].sprite->set_visible(false);
        attacks[i].sprite->set_scale(2.0);
    }
    bool atk_spawned_this_active = false;   // edge-detect the Active step
    int attack_variant = 0;                 // NEXT attack to fire: 0 aimed, 1 spiral, 2 spread/fan
    int current_attack = 0;                 // the attack firing during the CURRENT Active window
    int spiral_idx = 0, spiral_cd = 0, spiral_rot = 1;   // spiral emitter state (one arm toward player)
    // 8 rotating directions for the spiral (integer approx of a circle, ~3 px/frame).
    static const int SPIRAL_DIRS[8][2] = { {0,-3},{2,-2},{3,0},{2,2},{0,3},{-2,2},{-3,0},{-2,-2} };

    // Telegraph cue: a coloured charge orb shown AT the King during the wind-up so the incoming attack
    // is READABLE — red (fire) = aimed, gold (light) = spiral, cyan (ice) = spread.
    bn::sprite_ptr telegraph_orb = bn::sprite_items::fire_proj.create_sprite(0, 0);
    telegraph_orb.set_camera(cam);
    telegraph_orb.set_visible(false);

    auto king_cx = [&]{ return king_body.pos.x.to_int() + king_body.half_w.to_int(); };
    auto king_cy = [&]{ return king_body.pos.y.to_int() + king_body.half_h.to_int(); };
    auto clear_attacks = [&]{
        for(AttackInst& a : attacks){ a.active = false; if(a.sprite) a.sprite->set_visible(false); }
    };
    auto launch = [&](int idx, int cx_px, int cy_px, int vx, int vy){
        attacks[idx].active = true;
        attacks[idx].body.half_w = fx(6); attacks[idx].body.half_h = fx(6);
        attacks[idx].body.pos = { fx(cx_px - 6), fx(cy_px - 6) };
        attacks[idx].vel = { fx(vx), fx(vy) };
    };
    // Aimed (0) + spread (2) fire once at the Active edge, FROM the King toward the player. The spiral
    // (1) is emitted continuously during Active (in the loop). Speed scales with the phase.
    auto spawn_attack = [&](int variant){
        int pcx_ = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int dir  = (pcx_ >= king_cx()) ? 1 : -1;
        int spd  = (b.phase == logic::BossPhase::P1) ? 2 : 3;
        if(variant == 0){
            launch(0, king_cx(), king_cy(), spd * dir, 0);                 // aimed bolt at the player
        } else if(variant == 2){
            launch(0, king_cx(), king_cy(), spd * dir, 0);                 // 3-bolt fan from the King
            launch(1, king_cx(), king_cy(), spd * dir, -2);
            launch(2, king_cx(), king_cy(), spd * dir, 2);
        }
    };

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
    auto combat_phase = [&]{ return b.exposed() ? b.exposed_return : b.phase; };
    logic::BossPhase last_phase = logic::BossPhase::P1;

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
        clear_attacks(); atk_spawned_this_active = false;
        attack_variant = 0; current_attack = 0; spiral_idx = 0; spiral_cd = 0;
        telegraph_orb.set_visible(false);
        set_king_perch(0); teleport_timer = TELEPORT_PERIOD; teleport_flash = 0;
        last_phase = logic::BossPhase::P1;
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
                clear_attacks();                 // don't leave a bolt flying from the old position
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
            clear_attacks(); atk_spawned_this_active = false; telegraph_orb.set_visible(false);
        } else {
            logic::AttackStep step = b.current_step();
            if(step == logic::AttackStep::Telegraph){
                atk_spawned_this_active = false;
                if(attack_variant == 0)      telegraph_orb.set_item(bn::sprite_items::fire_proj);   // aimed
                else if(attack_variant == 1) telegraph_orb.set_item(bn::sprite_items::light_proj);  // spiral
                else                         telegraph_orb.set_item(bn::sprite_items::ice_proj);    // spread
                telegraph_orb.set_position(wx(king_cx()), wy(king_cy() - 14));   // charge above the King
                telegraph_orb.set_visible((b.attack_timer / 6) % 2 == 0);       // pulse the cue
            } else if(step == logic::AttackStep::Active){
                telegraph_orb.set_visible(false);
                if(!atk_spawned_this_active){
                    current_attack = attack_variant;            // lock the attack for this window
                    attack_variant = (attack_variant + 1) % 3;  // advance for next time
                    spiral_idx = 0; spiral_cd = 0;
                    int pcx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
                    spiral_rot = (pcx0 >= king_cx()) ? 1 : -1;  // sweep toward the player's side
                    if(current_attack != 1) spawn_attack(current_attack);  // aimed/spread fire now
                    atk_spawned_this_active = true;
                }
                if(current_attack == 1 && spiral_idx < 5){   // spiral: ONE arm sweeping up->down on the
                    if(spiral_cd <= 0){                       // player's side (rot picks left vs right)
                        int di = ((spiral_rot * spiral_idx) % 8 + 8) % 8;
                        launch(spiral_idx, king_cx(), king_cy(), SPIRAL_DIRS[di][0], SPIRAL_DIRS[di][1]);
                        ++spiral_idx; spiral_cd = 5;
                    } else --spiral_cd;
                }
            } else { // Recovery
                clear_attacks(); atk_spawned_this_active = false; telegraph_orb.set_visible(false);
            }
        }

        // ---- attack advance + render + contact (all active projectiles) ----
        for(AttackInst& a : attacks){
            if(!a.active) continue;
            a.body.pos.x = a.body.pos.x + a.vel.x;
            a.body.pos.y = a.body.pos.y + a.vel.y;
            int ax = a.body.pos.x.to_int() + a.body.half_w.to_int();
            int ay = a.body.pos.y.to_int() + a.body.half_h.to_int();
            if(ax < 0 || ax > level.w * 8 || ay < 0 || ay > level.h * 8){   // expire off-arena
                a.active = false;
                if(a.sprite) a.sprite->set_visible(false);
            } else {
                if(a.sprite){ a.sprite->set_position(wx(ax), wy(ay)); a.sprite->set_visible(true); }
                if(invuln == 0 && !player.dash.invincible() && logic::aabb_overlap(player.body, a.body)){
                    health.damage(20); invuln = 45;
                }
            }
        }

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
        //      blocked shot never doubles as a wound. ----
        for(AttackInst& a : attacks){
            if(!a.active) continue;
            if(bolts.consume_hit(a.body)
               || spells.consume_hit(a.body, logic::SpellId::Fire)
               || spells.consume_hit(a.body, logic::SpellId::Ice)){
                a.active = false; if(a.sprite) a.sprite->set_visible(false);
            }
        }

        int hp_before = b.hp;   // detect a wound this frame -> the King teleports away (see below)

        // ---- damage resolution (mirrors the dungeon's consume_hit hooks; King is a logic::Body) ----
        // Light ALWAYS exposes/refreshes — runs every frame regardless of phase (M10 Light-clears
        // hook, repointed to expose). on_light_hit() is a no-op once defeated/i-framed.
        if(spells.consume_hit(king_body, logic::SpellId::Light)) b.on_light_hit();
        // Wounding lands ONLY while EXPOSED (the King is immune while shielded). on_wound() itself
        // also guards on exposed(), but we gate here so a shot never silently vanishes off the King
        // mid-shield. An ELEMENTAL wound also refills magic (sustains casts; dungeon kill-refill feel).
        if(b.exposed()){
            if(bolts.consume_hit(king_body)){
                b.on_wound(logic::WOUND_DMG);
            } else if(spells.consume_hit(king_body, logic::SpellId::Fire)
                   || spells.consume_hit(king_body, logic::SpellId::Ice)){
                b.on_wound(logic::WOUND_DMG);
                magic.heal(25);
            }
        }
        // ---- phase-change dialogue (the King taunts as he escalates — in-character, not a banner) ----
        {
            logic::BossPhase cp = combat_phase();
            if(cp != last_phase && cp != logic::BossPhase::Defeated){
                last_phase = cp;
                if(cp == logic::BossPhase::P2)      boss_say("NOW YOU'RE GETTING ME ANGRY");
                else if(cp == logic::BossPhase::P3) boss_say("I'M DONE TOYING WITH YOU");
            }
        }

        if(b.defeated()){ boss_say("NOOOOO!"); return BossResult::Victory; }

        // ---- King teleports AWAY after taking a wound (fairer than attacking immediately): warp to a
        //      new perch, clear in-flight attacks, and restart the attack cycle (fresh telegraph) so it
        //      winds up at the new spot rather than striking instantly. (BossState i-frames already
        //      prevent an immediate re-expose.) ----
        if(b.hp < hp_before){
            set_king_perch((perch_idx + 1) % KING_PERCH_COUNT);
            teleport_timer = TELEPORT_PERIOD;
            teleport_flash = 12;
            clear_attacks();
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
        refresh_king_hp();
        hud.update(health, magic, world.lives);
        set_clamped_cam(player.body.pos.x.to_int() + player.body.half_w.to_int(),
                        player.body.pos.y.to_int() + player.body.half_h.to_int());
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}
}
