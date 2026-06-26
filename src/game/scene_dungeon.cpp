#include "game/scene_dungeon.h"

#include "bn_core.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_items_spronk.h"
#include "bn_sprite_items_enemy.h"
#include "bn_sprite_items_fire_enemy.h"
#include "bn_sprite_items_block.h"
#include "bn_sprite_items_shrine.h"
#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
#include "bn_sprite_items_light_proj.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_boss_bolt.h"   // M12: distinct red boss-attack projectile (D1)
#include "bn_sprite_items_grapple_icon.h"
#include "bn_sprite_items_heart_container.h"
#include "bn_sprite_items_magic_crystal.h"
#include "bn_sprite_items_guardian.h"   // M12: per-dungeon boss sprite (D1 Whispering Woods Guardian, 2 frames)
#include "bn_sprite_items_slagshell.h" // M13: D2 Ember Caverns boss sprite (Slagshell, 2 frames)
#include "bn_sprite_items_rock.h"      // M13: falling rock for Slagshell's rockfall attack
#include "bn_sprite_items_rock_marker.h" // M13: ground crack-telegraph for the rockfall
#include "bn_sprite_tiles_item.h"       // M12 QA r1: swap guardian frame 1 (tired pose) during the tired window
#include "bn_sprite_tiles_ptr.h"        // complete type for set_tiles(create_tiles(...))
#include "bn_sprite_text_generator.h"   // M12 QA r1: data-driven boss intro/death dialogue (run_room_boss)
#include "common_variable_8x16_sprite_font.h"
#include "bn_sprite_items_king_hp.h"    // M12: reuse the boss HP-pip art for the room boss bar

#include "logic/reveal.h"

#include "logic/tilemap.h"
#include "logic/world_state.h"   // max_health_for, collect/has heart container
#include "logic/player.h"
#include "logic/dungeon1.h"      // try_free_spronk
#include "logic/enemy.h"
#include "logic/meters.h"
#include "logic/spell.h"
#include "logic/hazard.h"
#include "logic/frost.h"
#include "logic/fire_effect.h"
#include "logic/pushable_block.h"
#include "logic/puzzle.h"
#include "logic/gates.h"
#include "logic/stone_impact.h" // loose_platform_in_shockwave (pound shockwave radius)
#include "logic/room_graph.h"   // find_entrance, room_door_at
#include "engine/input.h"
#include "engine/spell_input.h"
#include "engine/level_loader.h"  // load_level, set_collision_tile
#include "engine/level_view.h"    // set_level_tile
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/pause.h"        // check_pause (START -> GAME PAUSED; global pause)
#include "engine/spell_pool.h"
#include "engine/hud.h"
#include "engine/fade.h"
#include "engine/save.h"        // write_world (persist latches)
#include "engine/boss_attacks.h" // M12: AttackPool/SpiralEmitter/TelegraphCue/BossHpBar/spawn_attack/resolve_damage
#include "logic/boss.h"          // M12: BossState (a boss room runs run_room_boss before the normal loop)
#include "logic/collision.h"     // aabb_overlap (boss contact)
#include "game/scene_game_over.h" // run_game_over (death -> 0 lives flow)

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
    int px2t(logic::Fixed p){ return logic::Tilemap::px_to_tile(p); }

    struct RoomOutcome {
        enum Kind { ExitDungeon, Quit, Restart, GoToRoom, GameOver } kind;
        int target_room = 0;
        int target_entrance = 0;
    };

    struct EnemyInst { logic::Enemy e; bn::optional<bn::sprite_ptr> sprite; };
    struct GateInst  { logic::GateSpawn spawn; logic::Body body; bool open = false; };
    struct BlockInst { logic::PushableBlock blk; bn::optional<bn::sprite_ptr> sprite; bool pullable = false; };
    struct BrazierInst { int tx, ty, group; logic::Body body; bool lit = false; int draw_ty = 0; };

    // First STANDABLE collision row at/below start_ty in this column (the floor the content rests on).
    // Standable = solid OR one-way platform (D4's exit rests on a one-way platform, not a solid floor).
    // Falls back to start_ty+1 if none found within the map.
    int floor_row_below(const logic::Tilemap& map, int tx, int start_ty){
        for(int y = start_ty + 1; y < map.h; ++y)
            if(map.is_solid(tx, y) || map.is_oneway(tx, y)) return y;   // standable: solid or one-way platform
        return start_ty + 1;
    }
    // M8 Stone: a cracked FLOOR (NOT a vertical wall gate). Solid until a pound smashes it.
    struct CrackedFloorInst { int tx, ty, latch_id; bool broken = false; };
    // M8 Stone: a breakable solid boulder (NOT pushable). Solid tile + sprite; removed on a pound.
    struct BoulderInst { int tx, ty; bn::optional<bn::sprite_ptr> sprite; bool broken = false; };
    // M8 Stone: a horizontal run of `len` tiles, suspended, that DROPS straight down (drop-to-rest,
    // no momentum) when a pound's shockwave lands within Chebyshev distance <=6. Collision tiles + sprites.
    struct LoosePlatformInst {
        int tx, ty, len, cur_ty;
        bool falling = false, fallen = false;
        bn::vector<bn::sprite_ptr, 8> sprites;   // one per tile in the run
    };
    // M10 Light: a horizontal run of `len` tiles that is NON-solid + invisible until a Light cast
    // reveals it (RevealState window) — then solid + visible; reverts when the timer expires.
    struct HiddenPlatformInst {
        int tx, ty, len;
        bool shown = false;
        bn::vector<bn::sprite_ptr, 8> sprites;   // one per tile in the run
    };
    // M10 Light: a respawning full-magic-refill pickup (reset each attempt — never latched).
    struct MagicCrystalInst { int tx, ty; logic::Body body; bn::optional<bn::sprite_ptr> sprite; bool collected = false; };
    struct ShrineInst { logic::AbilityPickup pk; logic::Body body; bn::optional<bn::sprite_ptr> sprite; };
    struct HeartInst  { logic::HeartContainerSpawn hc; logic::Body body; bn::optional<bn::sprite_ptr> sprite; bool collected = false; };
    // src_tx/ty: plate or button tile to test; group: brazier group (Braziers kind)
    struct TriggerInst { logic::Trigger trig; int src_tx, src_ty, group; bool applied = false; };

    // Persist a progress latch to SRAM on first trigger; no-op if already set or unlatched (-1).
    void persist_latch(logic::World& world, int latch_id){
        if(latch_id >= 0 && !world.latched(latch_id)){
            world.set_latch(latch_id);
            engine::write_world(world);
        }
    }

    logic::Body tile_body(int tx, int ty, int hw, int hh){
        logic::Body b{}; b.half_w = fx(hw); b.half_h = fx(hh);
        b.pos = { fx(tx * 8), fx(ty * 8) }; return b;
    }
    // A gate/wall is a FULL-height 2-wide column (rows 1..floor-1) at column tx, so it
    // can't be double-jumped over — you must clear it. Clearing opens the whole column.
    void fill_column(engine::LevelView& view, int tx, int level_h, int bg){
        for(int ty = 1; ty < level_h - 2; ++ty) for(int dx = 0; dx < 2; ++dx){
            engine::set_collision_tile(tx + dx, ty, 1);
            engine::set_level_tile(view, tx + dx, ty, bg);
        }
    }
    void open_column(engine::LevelView& view, int tx, int level_h){
        for(int ty = 1; ty < level_h - 2; ++ty) for(int dx = 0; dx < 2; ++dx){
            engine::set_collision_tile(tx + dx, ty, 0);
            engine::set_level_tile(view, tx + dx, ty, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Map a boss def to its 2-frame sprite (frame 0 = normal/armored, frame 1 = exposed/vulnerable).
// Pointer-compare against the canonical def symbols (BossDef is pure logic -> can't name bn:: items).
// Extend per new per-dungeon boss.
static const bn::sprite_item& boss_sprite_for(const logic::BossDef* def){
    if(def == &logic::D2_DEF) return bn::sprite_items::slagshell;
    return bn::sprite_items::guardian;   // D1 (default)
}

// ---------------------------------------------------------------------------
// run_room_boss — a self-contained boss fight that uses the dungeon ROOM as the arena, for any room
// whose LevelData::boss is non-null (M12). It is a NEW thin consumer of the boss framework, PARALLEL
// to the King's scene_boss.cpp run_boss (NOT a shared monolith): its own fight loop driving
// logic::BossState + the engine::boss_attacks helpers + player movement + shot-aim + pause, with a
// full-fight restart on death-while-lives-remain. Deliberately minimal vs the King: NO teleport, NO
// dialogue (King-only). The boss stands centred on the arena floor. Returns Victory (boss defeated)
// or GameOver (player hit 0 lives — the dungeon's existing flow handles Continue/QuitToTitle).
enum class BossRoomOutcome { Victory, GameOver };

static BossRoomOutcome run_room_boss(const logic::LevelData& level, logic::World& world,
                                     logic::PlayerState& ps, engine::LoadedLevel& lvl,
                                     bn::camera_ptr& cam, logic::Player& player,
                                     const logic::Vec2& spawn_pos, const logic::EntranceSpawn& ent)
{
    logic::SpellState& spell = ps.spell;
    logic::Meter& health = ps.health;
    logic::Meter& magic  = ps.magic;

    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };

    // Start the fight at full vitals (the boss room is its own self-contained challenge).
    health.cur = health.max;
    magic.cur  = magic.max;

    int invuln = 0;
    constexpr int RESPAWN_IFRAMES = 60;

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // ---- boss state + body (stationary, centred on the arena floor) ----
    logic::BossState b; b.reset(*level.boss);
    logic::Body boss_body;
    boss_body.half_w = fx(14); boss_body.half_h = fx(16);
    // Centre column of the arena; rest its feet on the main floor (row h-2 surface).
    const int boss_cx_tile = level.w / 2;
    const int boss_floor_y = (level.h - 2) * 8;       // floor surface (px)
    auto place_boss = [&]{
        boss_body.pos = { fx(boss_cx_tile * 8 - boss_body.half_w.to_int()),
                          fx(boss_floor_y - boss_body.half_h.to_int() * 2) };
    };
    place_boss();
    auto boss_cx = [&]{ return boss_body.pos.x.to_int() + boss_body.half_w.to_int(); };
    auto boss_cy = [&]{ return boss_body.pos.y.to_int() + boss_body.half_h.to_int(); };

    // M13 pacing: a Pacing boss walks the floor between the interior walls; Stationary bosses never move.
    int pace_dir = 1;                                            // +1 right, -1 left
    const int PACE_SPEED = 1;                                    // px/frame (slow; tunable in QA)
    const int pace_min_cx = 8 + boss_body.half_w.to_int();              // just inside the left wall (col 1)
    const int pace_max_cx = (level.w - 1) * 8 - boss_body.half_w.to_int(); // just inside the right wall

    const bn::sprite_item& boss_item = boss_sprite_for(level.boss);
    bn::sprite_ptr boss_spr = boss_item.create_sprite(0, 0);
    boss_spr.set_camera(cam);
    boss_spr.set_position(wx(boss_cx()), wy(boss_cy()));
    int boss_frame = 0;   // 0 = normal, 1 = tired/slumped (shown while exposed); track to swap tiles only on change

    engine::BossHpBar hp_bar(*level.boss, bn::sprite_items::king_hp);
    hp_bar.refresh(b.hp);

    // ---- attacks (shared library pool) ----
    engine::AttackPool attacks(bn::sprite_items::boss_bolt, cam, lvl.view.map_px_w, lvl.view.map_px_h);
    engine::AttackPool rocks(bn::sprite_items::rock, cam, lvl.view.map_px_w, lvl.view.map_px_h);
    engine::RockfallEmitter rockfall(bn::sprite_items::rock_marker, cam,
                                     lvl.view.map_px_w, lvl.view.map_px_h);
    int rockfall_seed = 0;   // rolling counter -> varied rock spreads
    engine::SpiralEmitter spiral;
    bool atk_spawned_this_active = false;
    int current_attack = logic::BOSS_ATK_AIMED;   // the attack firing during the CURRENT Active window
    int attack_slot = 0;                           // rotates over the bits set in this phase's mask

    engine::TelegraphCue telegraph(bn::sprite_items::fire_proj, cam,
                                   lvl.view.map_px_w, lvl.view.map_px_h);

    // Respawning magic crystal (M10/King pattern): full refill on touch; reappears once magic is spent
    // below one cast, so a SpellExpose room boss (Fire to expose) can never magic-soft-lock.
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

    // Pick the next attack variant from the bits set in the active phase's mask (cycles AIMED->SPIRAL->FAN->ROCKFALL,
    // skipping bits not present). For D1: P1 = {AIMED}, P2 = {AIMED, FAN}. D2: {AIMED, ROCKFALL}.
    auto next_attack_for_phase = [&](int& slot)->int{
        const uint8_t mask = level.boss->phases[b.phase].attacks;
        static constexpr int ORDER[4] = {
            logic::BOSS_ATK_AIMED, logic::BOSS_ATK_SPIRAL, logic::BOSS_ATK_FAN, logic::BOSS_ATK_ROCKFALL };
        for(int step = 0; step < 4; ++step){
            int idx = (slot + step) % 4;
            if(mask & ORDER[idx]){ slot = (idx + 1) % 4; return ORDER[idx]; }
        }
        return logic::BOSS_ATK_AIMED;   // mask should never be empty
    };

    // ---- spell HUD icon (top-right) ----
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

    auto set_clamped_cam = [&](int cx, int cy){
        const int ll = -hw, lt = -hh, lr = ll + level.w * 8, lb = lt + level.h * 8;
        const int minx = ll + 120, maxx = lr - 120, miny = lt + 80, maxy = lb - 80;
        int camx = cx - hw, camy = cy - hh;
        camx = (minx <= maxx) ? (camx < minx ? minx : camx > maxx ? maxx : camx) : (ll + lr) / 2;
        camy = (miny <= maxy) ? (camy < miny ? miny : camy > maxy ? maxy : camy) : (lt + lb) / 2;
        cam.set_position(camx, camy);
    };

    // ---- boss dialogue (data-driven: def->intro_line / def->death_line; null = silent).
    //      Mirrors the King's boss_say (scene_boss.cpp): centred text, wait for A/START. ----
    bn::sprite_text_generator text_gen(common::variable_8x16_sprite_font);
    text_gen.set_center_alignment();
    auto boss_say = [&](const char* line){
        if(line == nullptr) return;
        bn::vector<bn::sprite_ptr, 32> say;
        text_gen.generate(0, 54, line, say);   // lower-centre, below the boss, clear of the HUD
        int t = 0;
        while(true){
            if(t > 20 && (bn::keypad::a_pressed() || bn::keypad::start_pressed())) break;
            ++t;
            bn::core::update();
        }
    };  // 'say' sprites destroyed here -> dialogue clears

    // full-fight restart (death with lives left, or Game-Over Continue from the caller's flow)
    auto restart_fight = [&]{
        b.on_player_death();                       // BossState -> phase 0, full HP, timers cleared
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
        rocks.clear(); rockfall.clear();
        crystal_collected = false; if(crystal_sprite) crystal_sprite->set_visible(true);
    };

    // Settle the player onto the floor before fading in (parallel to the King intro).
    { logic::InputFrame nin{}; for(int i = 0; i < 24; ++i) player.update(nin, lvl.map); }
    avatar.sync(player);
    set_clamped_cam(player.body.pos.x.to_int() + player.body.half_w.to_int(),
                    player.body.pos.y.to_int() + player.body.half_h.to_int());
    // Fade in on the player + boss, then the boss's data-driven intro line (if any), BEFORE the fight.
    engine::fade_in(16);
    boss_say(level.boss->intro_line);
    int fade_in_t = 0;   // already faded in; the Game-Over restart path re-arms this

    while(true)
    {
        engine::check_pause();   // START -> freeze + "GAME PAUSED" until START again

        logic::InputFrame in = engine::read_input();
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.abilities.glide       = world.has(logic::Ability::Glide);
        player.abilities.dash        = world.has(logic::Ability::Dash);
        player.abilities.grapple     = world.has(logic::Ability::Grapple);
        player.abilities.stone       = world.has(logic::Ability::Stone);

        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);
        bool want_grapple = si.cast && spell.selected == logic::SpellId::Grapple;
        in.grapple_fire = want_grapple;   // arena has no anchors/blocks to grab; harmless
        bool cast_spell = si.cast && (spell.selected == logic::SpellId::Fire ||
                                      spell.selected == logic::SpellId::Ice  ||
                                      spell.selected == logic::SpellId::Light);

        player.update(in, lvl.map);
        avatar.sync(player);

        // ---- boss logic tick ----
        b.tick();

        // Pacing (data-gated): move horizontally, reverse at the walls. Paused while EXPOSED so the clean
        // wound window doesn't also require tracking a moving target (mirrors the frozen-attack invariant).
        if(level.boss->locomotion == logic::Locomotion::Pacing && !b.exposed()){
            int cx = boss_cx() + pace_dir * PACE_SPEED;
            if(cx <= pace_min_cx){ cx = pace_min_cx; pace_dir = 1; }
            else if(cx >= pace_max_cx){ cx = pace_max_cx; pace_dir = -1; }
            boss_body.pos.x = fx(cx - boss_body.half_w.to_int());
        }

        // ---- boss render: swap to the tired/slumped frame while EXPOSED (the tired window), then
        //      blink/telegraph-pulse for emphasis. Frame swap only on change (no per-frame set_tiles). ----
        boss_spr.set_position(wx(boss_cx()), wy(boss_cy()) - rockfall.leap_offset());
        int want_frame = b.exposed() ? 1 : 0;   // TiredWindow: exposed() == the tired window
        if(want_frame != boss_frame){
            boss_spr.set_tiles(boss_item.tiles_item().create_tiles(want_frame));
            boss_frame = want_frame;
        }
        if(b.exposed())                                       boss_spr.set_visible((b.expose_timer / 4) % 2 == 0);
        else if(b.current_step() == logic::AttackStep::Telegraph) boss_spr.set_visible((b.attack_timer / 8) % 2 == 0);
        else                                                  boss_spr.set_visible(true);

        // ---- attack telegraph + spawn (skipped while EXPOSED = a clean damage window) ----
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
                        int rock_count = (b.phase == 0) ? 3 : 5;            // escalate in P2
                        rockfall.begin(player_tx, level.w, level.h, rock_count, rockfall_seed++);
                    } else if(current_attack != logic::BOSS_ATK_SPIRAL){
                        int spd = (b.phase == 0) ? 3 : 4;   // a touch faster than the King — harder to outrun
                        engine::spawn_attack(attacks, current_attack, boss_cx(), boss_cy(),
                                             pcx0, pcy0, spd, b.phase, /*aim_full=*/true);  // aim AT the player
                    }
                    atk_spawned_this_active = true;
                }
                if(current_attack == logic::BOSS_ATK_SPIRAL)
                    spiral.tick(attacks, boss_cx(), boss_cy());
                if(current_attack == logic::BOSS_ATK_ROCKFALL) rockfall.tick(rocks);
            } else { // Recovery — do NOT clear in-flight bolts: they must keep flying until they hit a
                     // wall (else a stationary boss's shots vanish mid-arena when the cycle ends — the QA
                     // bug where bolts "didn't reach the wall"). They despawn naturally in advance().
                atk_spawned_this_active = false; telegraph.hide(); rockfall.clear();
            }
        }

        // ---- attack advance + render + contact ----
        bool proj_hit = attacks.advance(player.body, level.w * 8, level.h * 8,
                                        invuln != 0 || player.dash.invincible(), lvl.map);
        if(proj_hit){ health.damage(20); invuln = 45; }

        bool rock_hit = rocks.advance(player.body, level.w * 8, level.h * 8,
                                      invuln != 0 || player.dash.invincible(), lvl.map);
        if(rock_hit){ health.damage(20); invuln = 45; }

        // ---- boss contact damage ----
        if(invuln == 0 && !player.dash.invincible() && logic::aabb_overlap(player.body, boss_body)){
            health.damage(20); invuln = 45;
        }

        // ---- projectile pools (shot aim shared via engine::read_aim_dy) ----
        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h + fx(engine::read_aim_dy()) };
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);
        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);

        // magic crystal: full refill on overlap; reappears once magic drops below one cast (repeatable).
        if(level.magic_crystal_count > 0){
            if(!crystal_collected && logic::aabb_overlap(player.body, crystal_body)){
                magic.cur = magic.max; crystal_collected = true;
                if(crystal_sprite) crystal_sprite->set_visible(false);
            }
            if(crystal_collected && magic.cur < 10){
                crystal_collected = false; if(crystal_sprite) crystal_sprite->set_visible(true);
            }
        }

        // NOTE: the D1 boss does NOT use the block defense (that's a King mechanic). Letting the player's
        // free bolt destroy incoming boss bolts meant spamming B auto-blocked everything (the boss's
        // bolts "didn't travel wall-to-wall") AND won the fight — QA r2. D1 must be DODGED: the boss's
        // bolts now pass the player's shots and travel until a wall/floor, so the player must move.

        // ---- damage resolution (bolt/Fire/Ice wounds while vulnerable; elemental refills magic) ----
        engine::resolve_damage(b, boss_body, bolts, spells, magic, /*magic_heal=*/25);

        if(b.defeated()){
            boss_say(level.boss->death_line);
            health.cur = health.max; magic.cur = magic.max;   // exit the fight at FULL vitals (reward +
                                                              // avoids a low-HP death on the way to the spronk)
            return BossRoomOutcome::Victory;
        }

        spells.despawn_on_solid(lvl.map);

        // ---- i-frames blink ----
        if(invuln > 0){ --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);

        // ---- death / lives / Game-Over: full-fight restart on lives-left; else hand back GameOver ----
        if(health.is_empty()){
            logic::lose_life(world);
            engine::write_world(world);
            if(world.lives > 0){
                restart_fight();
                engine::set_fade(16); fade_in_t = 16;
            } else {
                return BossRoomOutcome::GameOver;   // caller's run_dungeon flow runs run_game_over
            }
        }

        refresh_spell_icon();
        hp_bar.refresh(b.hp);
        hud.update(health, magic, world.lives);
        set_clamped_cam(player.body.pos.x.to_int() + player.body.half_w.to_int(),
                        player.body.pos.y.to_int() + player.body.half_h.to_int());
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}

static RoomOutcome play_room(const logic::LevelData& level, int entrance_id, logic::World& world, logic::PlayerState& ps)
{
    logic::SpellState& spell = ps.spell;   // selected tool lives in PlayerState -> persists across rooms/hub/scenes
    const int d = world.current_dungeon;
    bn::bg_palettes::set_transparent_color(bn::color(8, 8, 24));

    engine::LoadedLevel lvl = engine::load_level(level);
    bn::camera_ptr cam = bn::camera_ptr::create(0, 0);
    lvl.view.bg.set_camera(cam);

    const int hw = lvl.view.map_px_w / 2;
    const int hh = lvl.view.map_px_h / 2;
    auto wx = [&](int px){ return px - hw; };
    auto wy = [&](int px){ return px - hh; };
    // Centre the camera on the player (cx,cy in level pixels), but CLAMP so the 240x160 view never
    // scrolls past the authored level into the blank/wrapping region of the fixed 64x32 background
    // (the BG repeats; a tall level would otherwise show its top wrapped onto the screen bottom).
    auto set_clamped_cam = [&](int cx, int cy){
        const int ll = -hw, lt = -hh, lr = ll + level.w * 8, lb = lt + level.h * 8;
        const int minx = ll + 120, maxx = lr - 120, miny = lt + 80, maxy = lb - 80;
        int camx = cx - hw, camy = cy - hh;
        camx = (minx <= maxx) ? (camx < minx ? minx : camx > maxx ? maxx : camx) : (ll + lr) / 2;
        camy = (miny <= maxy) ? (camy < miny ? miny : camy > maxy ? maxy : camy) : (lt + lb) / 2;
        cam.set_position(camx, camy);
    };

    logic::EntranceSpawn ent = logic::find_entrance(level, entrance_id);
    const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };

    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;
    player.facing = ent.facing;   // face inward at the entrance

    // ---- M12 boss room: a room with a non-null boss runs a self-contained fight on entry, BEFORE the
    //      normal room loop. The fight BLOCKS until the boss is defeated (Victory) — then we fall
    //      through to the normal loop so the player walks to the onward room-door. On Game-Over (0
    //      lives) we return RoomOutcome::GameOver and REUSE the dungeon's existing flow (run_dungeon
    //      handles Continue/QuitToTitle). boss_defeated is a per-play_room local, so re-entering the
    //      room re-fights the boss (the boss is NOT persisted; no save change). A boss room has no
    //      enemy/gate/cage/exit content during the fight — run_room_boss owns the screen. ----
    bool boss_defeated = false;
    if(level.boss != nullptr && !boss_defeated){
        BossRoomOutcome bo = run_room_boss(level, world, ps, lvl, cam, player, spawn_pos, ent);
        if(bo == BossRoomOutcome::GameOver) return RoomOutcome{ RoomOutcome::GameOver };
        boss_defeated = true;   // Victory: fall through to the normal loop (walk to the onward door)
        engine::fade_out(16);   // clear the boss screen; the normal room loop fades back in
    }

    logic::Meter& health = ps.health;   // persist across hub <-> dungeon (no reset on entry)
    logic::Meter& magic  = ps.magic;
    int invuln = 0;
    // Post-respawn grace: i-frames granted on death so a player who died in a sub-floor
    // hazard pit cannot be re-damaged before regaining control. MUST exceed the hazard
    // re-arm (invuln=45 per hit) so even an authored-unsafe spawn yields real control
    // frames instead of an unbreakable every-frame death loop (the "stuck at the bottom"
    // report). The entrance is authored safe, but this guarantees robustness regardless.
    constexpr int RESPAWN_IFRAMES = 60;

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // Vine VFX: 4 dot sprites (bolt reused as placeholder) drawn along player->anchor line.
    // Positioned in level-px space (same world->screen transform as other sprites).
    // Visible only while player.grapple.active().
    constexpr int VINE_SEGS = 4;
    bn::vector<bn::sprite_ptr, VINE_SEGS> vine_segs;
    for(int i = 0; i < VINE_SEGS; ++i){
        vine_segs.push_back(bn::sprite_items::bolt.create_sprite(0, 0));
        vine_segs.back().set_camera(cam);
        vine_segs.back().set_visible(false);
        vine_segs.back().set_scale(0.5);
    }

    // Spell HUD icon — screen-fixed top-RIGHT, clear of the top-left bars. Shows the SELECTED
    // spell's art (fire/ice); swap the image only when the selection changes (set_item, no recreate).
    bn::sprite_ptr spell_icon = bn::sprite_items::fire_proj.create_sprite(104, -68);
    logic::SpellId last_icon = logic::SpellId::None;
    auto refresh_spell_icon = [&]{
        if(spell.selected != last_icon){
            if(spell.selected == logic::SpellId::Ice)         spell_icon.set_item(bn::sprite_items::ice_proj);
            else if(spell.selected == logic::SpellId::Fire)   spell_icon.set_item(bn::sprite_items::fire_proj);
            else if(spell.selected == logic::SpellId::Grapple) spell_icon.set_item(bn::sprite_items::grapple_icon); // green hook icon (distinct from cyan Ice)
            else if(spell.selected == logic::SpellId::Light)   spell_icon.set_item(bn::sprite_items::light_proj);   // white/gold Light bolt (distinct from red Fire / cyan Ice)
            last_icon = spell.selected;
        }
        spell_icon.set_visible(spell.selected != logic::SpellId::None);
    };
    refresh_spell_icon();

    // ---- pound VFX (placeholder): a brief dust/impact dot (bolt sprite reused, like the vine VFX)
    //      shown for a few frames at the impact point on each pound landing, + a tiny camera nudge. ----
    bn::sprite_ptr pound_dust = bn::sprite_items::bolt.create_sprite(0, 0);
    pound_dust.set_camera(cam);
    pound_dust.set_visible(false);
    pound_dust.set_scale(1.5);   // squashed puff (placeholder)
    int pound_vfx_t = 0;         // frames remaining the dust is shown
    int pound_shake_t = 0;       // frames remaining of camera nudge

    // ---- cage / spronk ----
    logic::Body cage;
    bn::optional<bn::sprite_ptr> spronk;
    if(level.has_cage){
        cage = tile_body(level.cage_tx, level.cage_ty, 8, 12);
        spronk = bn::sprite_items::spronk.create_sprite(0, 0);
        spronk->set_camera(cam);
        // Ground the 16x16 spronk sprite on the FIRST SOLID/one-way tile below the authored cage
        // row, so its BOTTOM rests on the floor surface (not embedded in it).  Sprite half-height
        // is 8 px, so centre = floor_surface - 8.  For the D1-D7 convention (cage_ty=18, floor at
        // row 20): floor_row_below(…,18)==20 → wy(20*8 - 8) == wy(cage.pos.y.to_int()+8) — no
        // visible regression.  For ledge cages (D8+ Room 2) the sprite now rests on the ledge
        // instead of being embedded in it.
        int cage_fr = floor_row_below(lvl.map, level.cage_tx, level.cage_ty);
        spronk->set_position(wx(level.cage_tx * 8 + 8), wy(cage_fr * 8 - 8));
        spronk->set_visible(!world.spronk_freed(d));
    }
    logic::Body exit;
    if(level.has_exit) exit = tile_body(level.exit_tx, level.exit_ty, 12, 12);

    // ---- enemies (fire_immune from param2 bit0) ----
    bn::vector<EnemyInst, 8> enemies;
    for(int i = 0; i < level.enemy_count && i < 8; ++i){
        const logic::EntitySpawn& s = level.enemies[i];
        enemies.push_back(EnemyInst{});
        EnemyInst& inst = enemies.back();
        inst.e.body.half_w = fx(8); inst.e.body.half_h = fx(8);
        inst.e.body.pos = { fx(s.tx * 8), fx(s.ty * 8) };
        inst.e.left_bound = fx(s.param0 * 8); inst.e.right_bound = fx(s.param1 * 8);
        inst.e.fire_immune = (s.param2 & 1) != 0;
        inst.sprite = (inst.e.fire_immune ? bn::sprite_items::fire_enemy.create_sprite(0, 0)
                                          : bn::sprite_items::enemy.create_sprite(0, 0));
        inst.sprite->set_camera(cam);
    }

    // ---- gates (vine/ice obstacle; gap geometry) ----
    // M8: CrackedFloor is a horizontal FLOOR, NOT a full-column vertical wall — it is SKIPPED here
    // and collected into cracked_floors below (made solid as a single floor tile, not a column).
    bn::vector<GateInst, 24> gates;
    bn::vector<CrackedFloorInst, 16> cracked_floors;
    for(int i = 0; i < level.gate_count && i < 24; ++i){
        const logic::GateSpawn& g = level.gates[i];
        if(g.type == logic::GateType::CrackedFloor){
            // A cracked floor is a single SOLID floor tile the player walks on; only a pound breaks it.
            // The compiler emits content symbols on collision tile 0, so make it solid here + render bg 11.
            // If already latched-open (smashed on a prior visit and persisted), leave it broken/empty.
            bool latched_open = (g.latch_id >= 0) && world.latched(g.latch_id);
            cracked_floors.push_back(CrackedFloorInst{ g.tx, g.ty, g.latch_id, latched_open });
            if(!latched_open){
                engine::set_collision_tile(g.tx, g.ty, 1);
                engine::set_level_tile(lvl.view, g.tx, g.ty, logic::gate_info(logic::GateType::CrackedFloor).bg_tile); // 11
            }
            continue;
        }
        logic::Body gb; gb.half_w = fx(8); gb.half_h = fx((level.h - 3) * 4); // full-column body
        gb.pos = { fx(g.tx * 8), fx(8) };
        gates.push_back(GateInst{ g, gb, false });
        GateInst& gi = gates.back();
        const logic::GateInfo& info = logic::gate_info(g.type);
        bool passable = info.is_geometry && logic::can_pass(g.type, world.abilities);
        // latched_open: a shortcut opened on a prior visit, persisted in SRAM — re-open it on room load.
        bool latched_open = (g.latch_id >= 0) && world.latched(g.latch_id);
        if(passable || latched_open){ gi.open = true; }              // geometry gate owned OR latch set -> open
        else { fill_column(lvl.view, g.tx, level.h, info.bg_tile); } // closed -> full-height vine/ice wall
    }

    // ---- ability shrines ----
    bn::vector<ShrineInst, 4> shrines;
    for(int i = 0; i < level.pickup_count && i < 4; ++i){
        const logic::AbilityPickup& p = level.pickups[i];
        shrines.push_back(ShrineInst{ p, tile_body(p.tx, p.ty, 6, 8), {} });
        ShrineInst& si = shrines.back();
        si.sprite = bn::sprite_items::shrine.create_sprite(0, 0);
        si.sprite->set_camera(cam);
        si.sprite->set_position(wx(p.tx * 8 + 8), wy(p.ty * 8 + 8));
        si.sprite->set_visible(!world.has(p.ability));   // already taken on a continued game
    }

    // ---- heart containers (permanent max-HP upgrade pickup) ----
    // Spawn only if NOT already collected (persisted in latches bits [24..31]); a collected
    // one stays hidden forever. Body matches the tile-sized shrine pickup; sprite grounded on
    // the content row exactly like the shrine (centre at tile-centre + 8).
    bn::vector<HeartInst, 4> hearts;
    for(int i = 0; i < level.heart_container_count && i < 4; ++i){
        const logic::HeartContainerSpawn& hc = level.heart_containers[i];
        if(world.heart_container_collected(hc.id)) continue;  // already taken -> never show it
        hearts.push_back(HeartInst{ hc, tile_body(hc.tx, hc.ty, 6, 8), {}, false });
        HeartInst& hi = hearts.back();
        hi.sprite = bn::sprite_items::heart_container.create_sprite(0, 0);
        hi.sprite->set_camera(cam);
        // Ground the 16x16 sprite on the FIRST SOLID tile below the authored row (like the
        // cage/exit/room-doors), so its BOTTOM rests on the floor surface. M7 hard-coded a
        // tile-centre position assuming a row-18 floor-2-below layout; in D7's tight alcove the
        // floor is only 1 row below, so that sank the sprite INTO the platform. Floor surface is
        // at fr*8; sprite half-height is 8, so centre = fr*8 - 8.
        int hc_fr = floor_row_below(lvl.map, hc.tx, hc.ty);
        hi.sprite->set_position(wx(hc.tx * 8 + 8), wy(hc_fr * 8 - 8));
    }

    // ---- pushable blocks (solid collision cell + 8x8 sprite) ----
    bn::vector<BlockInst, 8> blocks;
    for(int i = 0; i < level.block_count && i < 8; ++i){
        const logic::BlockSpawn& b = level.blocks[i];
        blocks.push_back(BlockInst{ logic::PushableBlock{ b.tx, b.ty }, {}, b.pullable });
        BlockInst& bi = blocks.back();
        engine::set_collision_tile(b.tx, b.ty, 1);       // block is solid; bg stays blank, sprite shows it
        bi.sprite = bn::sprite_items::block.create_sprite(0, 0);
        bi.sprite->set_camera(cam);
    }

    // ---- boulders (M8 Stone: breakable solid; like a block but NOT pushable; pound removes it) ----
    bn::vector<BoulderInst, 8> boulders;
    for(int i = 0; i < level.boulder_count && i < 8; ++i){
        const logic::BoulderSpawn& b = level.boulders[i];
        boulders.push_back(BoulderInst{ b.tx, b.ty, {}, false });
        BoulderInst& bo = boulders.back();
        engine::set_collision_tile(b.tx, b.ty, 1);                // solid; bg stays blank, sprite shows it
        bo.sprite = bn::sprite_items::block.create_sprite(0, 0);  // placeholder art (reuse block)
        bo.sprite->set_camera(cam);
        bo.sprite->set_position(wx(b.tx * 8 + 4), wy(b.ty * 8 + 4));
    }

    // ---- loose platforms (M8 Stone: drop straight down on a nearby pound shockwave) ----
    bn::vector<LoosePlatformInst, 8> loose_platforms;
    for(int i = 0; i < level.loose_platform_count && i < 8; ++i){
        const logic::LoosePlatformSpawn& lp = level.loose_platforms[i];
        loose_platforms.push_back(LoosePlatformInst{ lp.tx, lp.ty, lp.len, lp.ty, false, false, {} });
        LoosePlatformInst& li = loose_platforms.back();
        for(int dx = 0; dx < lp.len && dx < 8; ++dx){
            engine::set_collision_tile(lp.tx + dx, lp.ty, 1);     // solid run; bg blank, sprites show it
            li.sprites.push_back(bn::sprite_items::block.create_sprite(0, 0));  // placeholder art
            li.sprites.back().set_camera(cam);
            li.sprites.back().set_position(wx((lp.tx + dx) * 8 + 4), wy(lp.ty * 8 + 4));
        }
    }

    // ---- hidden platforms (M10 Light: NON-solid + invisible at spawn; a Light cast reveals them
    //      solid+visible for the RevealState window, then they revert). Mirrors loose platforms but
    //      we do NOT make them solid here and the sprites start hidden. ----
    bn::vector<HiddenPlatformInst, 8> hidden_platforms;
    for(int i = 0; i < level.hidden_platform_count && i < 8; ++i){
        const logic::HiddenPlatformSpawn& hp = level.hidden_platforms[i];
        hidden_platforms.push_back(HiddenPlatformInst{ hp.tx, hp.ty, hp.len, false, {} });
        HiddenPlatformInst& hi2 = hidden_platforms.back();
        for(int dx = 0; dx < hp.len && dx < 8; ++dx){
            // NON-solid + invisible until revealed (do NOT set_collision_tile here).
            hi2.sprites.push_back(bn::sprite_items::block.create_sprite(0, 0));  // placeholder art (reuse block)
            hi2.sprites.back().set_camera(cam);
            hi2.sprites.back().set_position(wx((hp.tx + dx) * 8 + 4), wy(hp.ty * 8 + 4));
            hi2.sprites.back().set_visible(false);
        }
    }
    logic::RevealState reveal;   // room-wide Light reveal timer (a Light cast (re)starts it)

    // ---- magic crystals (M10 Light: full-magic-refill pickup; respawns each attempt, NOT latched) ----
    bn::vector<MagicCrystalInst, 8> magic_crystals;
    for(int i = 0; i < level.magic_crystal_count && i < 8; ++i){
        const logic::MagicCrystalSpawn& mc = level.magic_crystals[i];
        magic_crystals.push_back(MagicCrystalInst{ mc.tx, mc.ty, tile_body(mc.tx, mc.ty, 6, 8), {}, false });
        MagicCrystalInst& ci = magic_crystals.back();
        ci.sprite = bn::sprite_items::magic_crystal.create_sprite(0, 0);
        ci.sprite->set_camera(cam);
        ci.sprite->set_position(wx(mc.tx * 8 + 8), wy(mc.ty * 8 + 8));
    }

    // ---- room-doors (bg tile 5 open-door; 2-wide x 4-tall archway grounded on the floor,
    //      matching the hub's archway). Floor-scanned so a row-18-authored door reaches the
    //      floor (row 20) instead of floating. Collision unchanged (door stays walkable). ----
    for(int i = 0; i < level.room_door_count && i < 8; ++i){
        const logic::RoomDoorSpawn& rd = level.room_doors[i];
        int fr = floor_row_below(lvl.map, rd.tx, rd.ty);
        // target_room == -1 is the exit-to-hub door: render with a DISTINCT bg tile (26, hub portal)
        // so it reads differently from a normal room-door (tile 5) and the dungeon goal/exit (tile 6).
        // (13 is lava, so 26 is the next free strip slot — see make_placeholder_art.py gen_tiles.)
        int door_bg = (rd.target_room == -1) ? 26 : 5;
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx)
            engine::set_level_tile(lvl.view, rd.tx + dx, fr - 1 - dy, door_bg);
    }
    // ---- exit marker (bg tile 6 door-locked = distinct closed door = dungeon goal;
    //      same 2-wide x 4-tall grounded archway. room-doors use tile 5 so they're distinct.
    //      Exit collision body untouched. ----
    if(level.has_exit){
        int fr = floor_row_below(lvl.map, level.exit_tx, level.exit_ty);
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx)
            engine::set_level_tile(lvl.view, level.exit_tx + dx, fr - 1 - dy, 6);
    }

    // ---- braziers (bg tile 14 unlit; Body for fire-hit) ----
    bn::vector<BrazierInst, 16> braziers;
    for(int i = 0; i < level.brazier_count && i < 16; ++i){
        const logic::BrazierSpawn& b = level.braziers[i];
        // Tall hit-body (rows 14..19) so a horizontal Fire shot at the player's chest height
        // still hits a brazier sitting on the floor. Visual grounded on the floor (fr-1).
        int draw_ty = floor_row_below(lvl.map, b.tx, b.ty) - 1;
        braziers.push_back(BrazierInst{ b.tx, b.ty, b.group, tile_body(b.tx, 14, 6, 24), false, draw_ty });
        engine::set_level_tile(lvl.view, b.tx, draw_ty, 14);
    }

    // ---- plates (tile 17) / buttons (tile 18) + triggers ----
    bn::vector<TriggerInst, 16> triggers;
    for(int i = 0; i < level.plate_count && i < 16; ++i){
        const logic::PlateSpawn& p = level.plates[i];
        engine::set_level_tile(lvl.view, p.tx, p.ty, 17);
        // M8: a HEAVY plate trips ONLY on a Stone pound (resolved in the just_landed() block, NOT here).
        // Skip it from the normal step/block trigger loop so it never trips on a footstep or pushed block.
        if(p.heavy) continue;
        logic::Trigger t = logic::Trigger::plate(); t.target_tx = p.target_tx; t.target_ty = p.target_ty;
        triggers.push_back(TriggerInst{ t, p.tx, p.ty, -1, false });
    }
    for(int i = 0; i < level.button_count && i < 16; ++i){
        const logic::ButtonSpawn& b = level.buttons[i];
        engine::set_level_tile(lvl.view, b.tx, b.ty, 18);
        logic::Trigger t = logic::Trigger::button(); t.target_tx = b.target_tx; t.target_ty = b.target_ty;
        triggers.push_back(TriggerInst{ t, b.tx, b.ty, -1, false });
    }
    for(int g = 0; g < level.brazier_group_count && g < 16; ++g){
        const logic::BrazierGroupSpawn& bg = level.brazier_groups[g];
        logic::Trigger t = logic::Trigger::braziers(bg.total); t.target_tx = bg.target_tx; t.target_ty = bg.target_ty;
        bool latched_open = (bg.latch_id >= 0) && world.latched(bg.latch_id);
        triggers.push_back(TriggerInst{ t, 0, 0, g, latched_open });
        if(latched_open) open_column(lvl.view, bg.target_tx, level.h);
    }

    // Centre camera on the player before fading in (avoids a snap on frame 0).
    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    set_clamped_cam(cx0, cy0);
    engine::set_fade(16);
    int fade_in_t = 16;
    int push_cd = 0;
    int grapple_pull_cd = 0;
    int miss_vine_t   = 0;  // counts down from 10; >0 => miss-vine animation active
    int miss_vine_dir = 1;  // facing direction when the miss was fired
    int lr_restart_hold = 0; // frames L+R held — anti-soft-lock manual restart (moved off START)

    while(true)
    {
        engine::check_pause();   // START -> freeze + "GAME PAUSED" until START again (global pause)
        if(bn::keypad::select_pressed()) { return RoomOutcome{ RoomOutcome::Quit }; }
        // Anti-soft-lock manual restart: HOLD L+R for ~30 frames. Moved off START (now the pause key);
        // a deliberate hold so the constant L=cycle / R=fire taps can't trigger it accidentally.
        if(bn::keypad::l_held() && bn::keypad::r_held()){
            if(++lr_restart_hold >= 30) return RoomOutcome{ RoomOutcome::Restart };
        } else lr_restart_hold = 0;
        if(bn::keypad::up_pressed()){
            if(const logic::RoomDoorSpawn* dr = logic::room_door_at(level, player.body)){
                // target_room == -1 is the sentinel "exit-to-hub" door: a diegetic Up-press
                // equivalent of SELECT=quit. Return Quit so run_dungeon returns DungeonResult::Quit
                // (NOT Cleared) -> the hub loop resumes WITHOUT marking the dungeon cleared, and the
                // dungeon stays re-enterable.
                if(dr->target_room == -1) return RoomOutcome{ RoomOutcome::Quit };
                return RoomOutcome{ RoomOutcome::GoToRoom, dr->target_room, dr->target_entrance };
            }
        }

        logic::InputFrame in = engine::read_input();
        player.abilities.featherleap = world.has(logic::Ability::Featherleap);
        player.abilities.glide       = world.has(logic::Ability::Glide);
        player.abilities.dash        = world.has(logic::Ability::Dash);
        player.abilities.grapple     = world.has(logic::Ability::Grapple);
        player.abilities.stone       = world.has(logic::Ability::Stone);
        // Read spell intent + cycle FIRST so the selection is current for the grapple/cast branch:
        engine::SpellIntent si = engine::read_spell_intent();
        if(si.cycle) spell.cycle(world);
        // R fires the SELECTED tool: Grapple -> pull a nearby pullable block one tile toward the
        // player (if one is in range/arc), else latch the player to an anchor; Fire/Ice -> cast.
        bool want_grapple = si.cast && spell.selected == logic::SpellId::Grapple;
        in.grapple_fire = false;
        if(want_grapple){
            // Find a pullable block within grapple range in the facing/up arc.
            BlockInst* target = nullptr;
            int ptx = px2t(player.body.pos.x + player.body.half_w);   // player centre tile x
            int pty = px2t(player.body.pos.y + player.body.half_h);   // player centre tile y
            for(BlockInst& bi : blocks){
                if(!bi.pullable) continue;
                int dxt = bi.blk.tx - ptx, dyt = bi.blk.ty - pty;
                int adx = dxt < 0 ? -dxt : dxt, ady = dyt < 0 ? -dyt : dyt;
                if(adx > logic::GrappleState::RANGE || ady > logic::GrappleState::RANGE) continue;
                int sx = (dxt > 0) - (dxt < 0);           // horizontal sign relative to player
                if(sx == -player.facing && dxt != 0) continue;  // exclude strictly-behind blocks (arc rule matches nearest_grapple_anchor)
                // first in-range pullable block by spawn order (rooms have at most one pull-block puzzle, so nearest-tiebreak is unnecessary)
                target = &bi; break;
            }
            if(target && grapple_pull_cd == 0){
                // Pull direction in tile space (same idiom as the scan): block right of player -> pull it left, toward the player.
                int target_dxt = target->blk.tx - ptx;    // block tile minus player centre tile
                int pull_dir = (target_dxt > 0) ? -1 : 1;
                int oldx = target->blk.tx;
                if(target->blk.pull(pull_dir, lvl.map)){
                    engine::set_collision_tile(oldx, target->blk.ty, 0);
                    engine::set_collision_tile(target->blk.tx, target->blk.ty, 1);
                    grapple_pull_cd = 8;
                }
            } else if(!target){
                // No pullable block — try pulling a nearby non-immune enemy one tile toward the player.
                EnemyInst* etarget = nullptr;
                int ebest_dist = 999;
                for(EnemyInst& ei : enemies){
                    if(!ei.e.alive || ei.e.fire_immune) continue; // immune enemies resist the vine
                    int etx = px2t(ei.e.body.pos.x + ei.e.body.half_w);
                    int ety = px2t(ei.e.body.pos.y + ei.e.body.half_h);
                    int dxt = etx - ptx, dyt = ety - pty;
                    int adx = dxt < 0 ? -dxt : dxt, ady = dyt < 0 ? -dyt : dyt;
                    if(adx > logic::GrappleState::RANGE || ady > logic::GrappleState::RANGE) continue;
                    int sx = (dxt > 0) - (dxt < 0);
                    if(sx == -player.facing && dxt != 0) continue; // arc rule: exclude strictly-behind
                    int dist = adx + ady; // Manhattan distance (nearest)
                    if(dist < ebest_dist){ ebest_dist = dist; etarget = &ei; }
                }
                if(etarget && grapple_pull_cd == 0){
                    int etx = px2t(etarget->e.body.pos.x + etarget->e.body.half_w);
                    int ety = px2t(etarget->e.body.pos.y + etarget->e.body.half_h);
                    int edxt = etx - ptx; // enemy col minus player col
                    int edir = (edxt > 0) ? -1 : 1; // nudge enemy toward player (one tile = 8 px)
                    int dest_tx = etx + edir;
                    // Guard: only nudge if the destination tile is non-solid and within map bounds.
                    if(dest_tx >= 0 && dest_tx < lvl.map.w && !lvl.map.is_solid(dest_tx, ety)){
                        etarget->e.body.pos.x = etarget->e.body.pos.x + logic::Fixed::from_int(8 * edir);
                    }
                    grapple_pull_cd = 8; // consumed by enemy pull — don't fire anchor grapple
                } else if(!etarget){
                    in.grapple_fire = true; // no block, no enemy -> player anchor-grapple
                }
            }
        }
        bool cast_spell = si.cast && (spell.selected == logic::SpellId::Fire ||
                                      spell.selected == logic::SpellId::Ice  ||
                                      spell.selected == logic::SpellId::Light);
        // Capture the "tried to anchor-grapple" flag BEFORE player.update consumes it.
        bool tried_anchor = want_grapple && in.grapple_fire;
        player.update(in, lvl.map);
        avatar.sync(player);
        // Miss detection: player tried an anchor-grapple but nothing latched (no grapple point in range).
        if(tried_anchor && !player.grapple.active()){
            miss_vine_t   = 10;
            miss_vine_dir = player.facing;
        }

        // ---- M8 Stone pound impact resolution (on the one frame the pound lands) ----
        // Order: cracked-floor smash (may re-arm to chain through stacked floors) -> heavy switch ->
        // boulder break -> loose-platform shockwave. Crush is in the enemy loop below. The pound is
        // armed via player.stone (logic); the scene resolves WHAT it hits, mirroring dash->CrackedWall.
        if(player.stone.just_landed()){
            int impact_cx = px2t(player.body.pos.x + player.body.half_w);                                  // centre column
            // Two distinct rows (the collision resolver leaves the body resting just ABOVE the floor):
            //  - impact_fy: the body's lowest OCCUPIED tile (matches the plate-trip convention, ~500-505);
            //    a plate/heavy-plate marker is a non-solid tile the body stands ON, so we match this row.
            //  - impact_floor: the SOLID tile directly under the feet (matches the on_ground probe,
            //    collision.cpp:75); cracked floors + boulders are SOLID tiles the player lands ON TOP of,
            //    so they live at this row, not impact_fy.
            int impact_fy    = px2t(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1));
            int impact_floor = px2t(player.body.pos.y + player.body.half_h + player.body.half_h);
            // Pound VFX (placeholder): puff of dust at the impacted floor + a brief camera shake.
            pound_dust.set_position(wx(impact_cx * 8 + 4), wy(impact_floor * 8 + 4));
            pound_dust.set_visible(true);
            pound_vfx_t = 8;
            pound_shake_t = 6;
            // 1. CrackedFloor smash + continue the plunge. The landed tile is solid; if it is an unbroken
            //    cracked floor, break the WHOLE contiguous cracked-floor run at that row and RE-ARM the
            //    pound so the next frame plunges into the area below. Re-arm ONLY on a cracked tile, so one
            //    pound chains through STACKED cracked floors and naturally ends on the first non-cracked solid.
            bool smashed = false;
            for(CrackedFloorInst& cf : cracked_floors){
                if(cf.broken || cf.tx != impact_cx || cf.ty != impact_floor) continue;
                // Break the contiguous run of cracked-floor tiles at this row (left + right of impact).
                for(CrackedFloorInst& run : cracked_floors){
                    if(run.broken || run.ty != cf.ty) continue;
                    // a tile is in the run if it connects to impact_cx via adjacent cracked tiles at this row
                    // (simple: break any same-row cracked tile within the maximal contiguous span)
                    bool contiguous = true;
                    int lo = run.tx < impact_cx ? run.tx : impact_cx;
                    int hi = run.tx < impact_cx ? impact_cx : run.tx;
                    for(int x = lo; x <= hi && contiguous; ++x){
                        bool found = false;
                        for(CrackedFloorInst& q : cracked_floors)
                            if(!q.broken && q.ty == cf.ty && q.tx == x){ found = true; break; }
                        if(!found) contiguous = false;
                    }
                    if(!contiguous) continue;
                    run.broken = true;
                    engine::set_collision_tile(run.tx, run.ty, 0);
                    engine::set_level_tile(lvl.view, run.tx, run.ty, 0);
                    persist_latch(world, run.latch_id);
                }
                smashed = true;
                break;
            }
            if(smashed) player.stone.start();   // re-arm: plunge through to the next floor below

            // 2. Heavy switch: a heavy plate trips ONLY on a pound. Fire its gate target (open_column)
            //    when the player's feet/centre land on the plate tile. (Normal plates are handled in the
            //    trigger loop below, which now SKIPS heavy plates so they never trip on a step/block.)
            for(int i = 0; i < level.plate_count && i < 16; ++i){
                const logic::PlateSpawn& p = level.plates[i];
                if(!p.heavy) continue;
                if(impact_cx == p.tx && impact_fy == p.ty){
                    open_column(lvl.view, p.target_tx, level.h);
                    // Heavy plates may carry a latch via a co-located latched gate target; persist not
                    // wired through PlateSpawn (no latch_id field), so the open_column holds for the visit.
                }
            }

            // 3. Boulder break: if a boulder is the tile directly below the player's feet (or the landed
            //    tile itself), remove it so the path clears. (Boulders rebuild on room re-entry — fine.)
            for(BoulderInst& bo : boulders){
                if(bo.broken) continue;
                // The boulder the player landed ON TOP of is the solid tile under the feet (impact_floor).
                bool below = (bo.tx == impact_cx && bo.ty == impact_floor);
                if(below){
                    bo.broken = true;
                    engine::set_collision_tile(bo.tx, bo.ty, 0);
                    if(bo.sprite) bo.sprite->set_visible(false);
                }
            }

            // 4. Loose-platform shockwave: any not-yet-falling loose platform whose run is within
            //    Chebyshev distance <=6 of the impact begins falling (drop-to-rest; see step loop below).
            for(LoosePlatformInst& li : loose_platforms){
                if(li.falling || li.fallen) continue;
                if(logic::loose_platform_in_shockwave(li.tx, li.cur_ty, li.len, impact_cx, impact_floor))
                    li.falling = true;
            }
        }

        // ---- loose platforms: drop-to-rest one tile/frame while falling (solid-grid test only) ----
        // The fall test considers ONLY the collision grid; the player is not a collision tile, so a
        // platform never rests on the player (content guarantees the player isn't under a dropping run).
        for(LoosePlatformInst& li : loose_platforms){
            if(!li.falling) continue;
            bool clear_below = true;
            for(int dx = 0; dx < li.len; ++dx)
                if(lvl.map.is_solid(li.tx + dx, li.cur_ty + 1)){ clear_below = false; break; }
            if(clear_below){
                for(int dx = 0; dx < li.len; ++dx){
                    engine::set_collision_tile(li.tx + dx, li.cur_ty, 0);       // clear old row
                    engine::set_collision_tile(li.tx + dx, li.cur_ty + 1, 1);   // set new row solid
                }
                ++li.cur_ty;
                for(int dx = 0; dx < (int)li.sprites.size(); ++dx)
                    li.sprites[dx].set_position(wx((li.tx + dx) * 8 + 4), wy(li.cur_ty * 8 + 4));
            } else {
                li.falling = false; li.fallen = true;   // rest; drop-to-rest, no bounce
            }
        }

        // Shot aim (Zelda II style, shared with the boss/hub): UP = high, DOWN = low, else medium.
        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h + fx(engine::read_aim_dy()) };
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);

        logic::SpellId fired = spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);

        // ---- M10 Light reveal: a Light cast that ACTUALLY fired (re)starts the room-wide window.
        //      Detect via the returned fired-spell (NOT a magic delta — the crystal refill mutates
        //      magic.cur the same frame and would corrupt a before/after inference). Then tick; toggle
        //      hidden-platform collision+visibility on the timer EDGE (don't rewrite every frame).
        if(fired == logic::SpellId::Light) reveal.on_cast();
        reveal.tick();
        {
            bool want_shown = reveal.revealed();
            for(HiddenPlatformInst& hp : hidden_platforms){
                if(want_shown == hp.shown) continue;            // edge only
                for(int dx = 0; dx < hp.len && dx < 8; ++dx)
                    engine::set_collision_tile(hp.tx + dx, hp.ty, want_shown ? 1 : 0);
                for(int dx = 0; dx < (int)hp.sprites.size(); ++dx)
                    hp.sprites[dx].set_visible(want_shown);
                hp.shown = want_shown;
            }
        }

        // ---- spell resolution (ORDER: gates -> braziers -> enemies -> freeze/melt -> despawn-on-solid) ----
        for(GateInst& gi : gates){
            logic::SpellId clears = logic::gate_cleared_by(gi.spawn.type);
            if(!gi.open && clears != logic::SpellId::None && spells.consume_hit(gi.body, clears)){
                gi.open = true;
                open_column(lvl.view, gi.spawn.tx, level.h);
                persist_latch(world, gi.spawn.latch_id);
            }
            // M6: cracked walls aren't spell-cleared (gate_cleared_by==None); a dashing body smashes them on contact.
            if(!gi.open && gi.spawn.type == logic::GateType::CrackedWall
               && player.dash.active() && logic::aabb_overlap(player.body, gi.body)){
                gi.open = true;
                open_column(lvl.view, gi.spawn.tx, level.h);
                persist_latch(world, gi.spawn.latch_id);
            }
        }
        for(BrazierInst& bi : braziers){
            if(!bi.lit && spells.consume_hit(bi.body, logic::SpellId::Fire)){  // only Fire lights braziers
                bi.lit = true;
                engine::set_level_tile(lvl.view, bi.tx, bi.draw_ty, 15);
            }
        }

        // ---- enemies: patrol, render, bolt-kill(+magic), fire-kill(no magic unless immune), contact ----
        for(EnemyInst& inst : enemies){
            inst.e.update(lvl.map);
            if(!inst.e.alive) continue;
            int ex = inst.e.body.pos.x.to_int() + inst.e.body.half_w.to_int();
            int ey = inst.e.body.pos.y.to_int() + inst.e.body.half_h.to_int();
            inst.sprite->set_position(ex - hw, ey - hh);
            if(player.stone.active() && logic::aabb_overlap(player.body, inst.e.body)){
                // M8: a pound CRUSHES any enemy on contact (including fire_immune), refilling magic
                // like a bolt-kill. Guarded by stone.active() (pound i-frames), parallel to dash i-frames.
                inst.e.kill(); magic.heal(25); inst.sprite->set_visible(false);
            } else if(bolts.consume_hit(inst.e.body)){
                inst.e.kill(); magic.heal(25); inst.sprite->set_visible(false);
            } else if(spells.consume_hit(inst.e.body, logic::SpellId::Fire)){
                if(!inst.e.fire_immune){ inst.e.kill(); inst.sprite->set_visible(false); } // no magic refill from fire
            } else if(invuln == 0 && !player.dash.invincible() && logic::aabb_overlap(player.body, inst.e.body)){
                health.damage(20); invuln = 45;   // dash i-frames blink through contact
            }
        }

        // ---- reversible terrain: Ice freezes water it flies over; Fire melts ice platforms.
        // Spells fly at chest height but water/ice sit at floor level, so consume_tile_hit scans
        // DOWN the shot's column (the M3 brazier-height lesson). MUST precede despawn_on_solid
        // because IcePlatform is solid — otherwise a melt-shot is killed before it can melt.
        constexpr int WATER_BG = 16, ICE_PLATFORM_BG = 19;       // pinned bg indices (gates.h tile map)
        int ftx, fty;
        while(spells.consume_tile_hit(lvl.map, logic::TileKind::Water, logic::SpellId::Ice, ftx, fty)){
            // Freeze the WHOLE contiguous horizontal run of water into one ice bridge (one cast),
            // not just the box the shot touched.
            int x0 = ftx; while(lvl.map.is_water(x0 - 1, fty)) --x0;
            int x1 = ftx; while(lvl.map.is_water(x1 + 1, fty)) ++x1;
            for(int x = x0; x <= x1; ++x){
                engine::set_collision_tile(x, fty, (int)logic::TileKind::IcePlatform); // collision VALUE 5
                engine::set_level_tile(lvl.view, x, fty, ICE_PLATFORM_BG);             // bg INDEX 19
            }
        }
        while(spells.consume_tile_hit(lvl.map, logic::TileKind::IcePlatform, logic::SpellId::Fire, ftx, fty)){
            // Melt the WHOLE contiguous ice run back to water (one cast), symmetric to the freeze.
            int x0 = ftx; while(lvl.map.at(x0 - 1, fty) == logic::TileKind::IcePlatform) --x0;
            int x1 = ftx; while(lvl.map.at(x1 + 1, fty) == logic::TileKind::IcePlatform) ++x1;
            for(int x = x0; x <= x1; ++x){
                engine::set_collision_tile(x, fty, (int)logic::TileKind::Water);     // collision VALUE 4
                engine::set_level_tile(lvl.view, x, fty, WATER_BG);                  // bg INDEX 16
            }
        }
        spells.despawn_on_solid(lvl.map);

        // ---- hazards (lava, water, or spikes): same damage; dash i-frames blink through ----
        if(invuln == 0 && !player.dash.invincible() && logic::hazard_overlap(player.body, lvl.map)){ health.damage(20); invuln = 45; }

        // ---- pushable blocks: push detection, gravity, sprite ----
        if(push_cd > 0) --push_cd;
        if(grapple_pull_cd > 0) --grapple_pull_cd;   // ticks here; checked in the input phase above
        for(BlockInst& bi : blocks){
            // push when grounded, holding a dir, and the tile in front of the player == this block
            if(push_cd == 0 && player.body.on_ground && (in.left || in.right)){
                int dir = in.right ? 1 : -1;
                int lead_px = in.right ? player.body.pos.x.to_int() + 16 : player.body.pos.x.to_int() - 1;
                int feet_ty = px2t(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1));
                if(px2t(fx(lead_px)) == bi.blk.tx && feet_ty == bi.blk.ty){
                    int oldx = bi.blk.tx;
                    if(bi.blk.push(dir, lvl.map)){
                        engine::set_collision_tile(oldx, bi.blk.ty, 0);
                        engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);
                        push_cd = 8;
                    }
                }
            }
            int oldy = bi.blk.ty;
            if(bi.blk.apply_gravity_step(lvl.map)){
                engine::set_collision_tile(bi.blk.tx, oldy, 0);
                engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);
            }
            int bx = bi.blk.tx * 8 + 4, by = bi.blk.ty * 8 + 4;
            bi.sprite->set_position(bx - hw, by - hh);
        }

        // ---- triggers: update inputs, open/close targets ----
        for(TriggerInst& ti : triggers){
            switch(ti.trig.kind){
                case logic::TriggerKind::Plate: {
                    // Pressed by the player OR a block, but only when SQUARELY on the plate —
                    // the player's horizontal centre must be over the plate column AND the player
                    // must be grounded on the plate's row (not merely overlapping the edge while
                    // standing next to/above it; that loose AABB caused the gate to flicker). The
                    // gate is held open only WHILE pressed, so the player can step on it (it opens)
                    // but can't pass alone — only the block, left resting on it, holds it open.
                    int pcx = px2t(player.body.pos.x + player.body.half_w);                       // centre col
                    int pfy = px2t(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1)); // feet row
                    bool on = player.body.on_ground && pcx == ti.src_tx && pfy == ti.src_ty;
                    for(BlockInst& bi : blocks) if(bi.blk.tx == ti.src_tx && bi.blk.ty == ti.src_ty) on = true;
                    ti.trig.pressed = on;
                    if(on && !ti.applied){ ti.applied = true;  open_column(lvl.view, ti.trig.target_tx, level.h); }
                    else if(!on && ti.applied){ ti.applied = false; fill_column(lvl.view, ti.trig.target_tx, level.h, 1); }
                    break; }
                case logic::TriggerKind::Button: {
                    if(logic::aabb_overlap(player.body, tile_body(ti.src_tx, ti.src_ty, 4, 4))) ti.trig.pressed = true; // latch
                    if(!ti.applied && ti.trig.active()){ ti.applied = true; open_column(lvl.view, ti.trig.target_tx, level.h); }
                    break; }
                case logic::TriggerKind::Braziers: {
                    int n = 0; for(BrazierInst& bi : braziers) if(bi.group == ti.group && bi.lit) ++n;
                    ti.trig.lit = n;
                    if(!ti.applied && ti.trig.active()){
                        ti.applied = true; open_column(lvl.view, ti.trig.target_tx, level.h); // latch
                        persist_latch(world, level.brazier_groups[ti.group].latch_id);
                    }
                    break; }
            }
        }

        // ---- i-frames / respawn ----
        if(invuln > 0) { --invuln; avatar.set_visible((invuln / 4) % 2 == 0); }
        else avatar.set_visible(true);
        if(health.is_empty()){
            logic::lose_life(world);
            engine::write_world(world);   // persist the decremented count immediately
            // NOTE: this is the FIRST mid-dungeon save — it persists current_dungeon = n
            // (the old code only wrote post-clear with current_dungeon already 0). This is
            // benign: door_enterable uses spronks_freed, and main re-sets current_dungeon
            // before each run_dungeon, so the stored n is overwritten on the next entry.
            if(world.lives == 0){
                return RoomOutcome{ RoomOutcome::GameOver };   // run_dungeon shows the Game Over scene
            }
            player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
            health.cur = health.max;
            invuln = RESPAWN_IFRAMES;   // grace window (NOT 0): never re-die before regaining control
            // Clear transient movement states so a death mid-dash / mid-pound / mid-grapple doesn't
            // carry velocity or i-frame state into the respawn (which could re-plunge into the pit).
            player.dash = logic::DashState{};
            player.grapple = logic::GrappleState{};
            player.stone = logic::StoneState{};
            player.body.on_ground = false;
            // Re-sync the avatar to the respawn position THIS frame so the sprite doesn't linger
            // one frame at the death spot (cosmetic ghost in the pit).
            avatar.sync(player);
            // Reset pushable blocks to their authored start so a block shoved into a dead corner
            // (a soft-lock) is recoverable by dying. (Plates re-evaluate next frame; latched
            // button/brazier gates stay solved.)
            for(int i = 0; i < (int)blocks.size(); ++i){
                BlockInst& bi = blocks[i];
                engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 0);          // clear where it ended up
                bi.blk.tx = level.blocks[i].tx; bi.blk.ty = level.blocks[i].ty;
                engine::set_collision_tile(bi.blk.tx, bi.blk.ty, 1);          // solid at the start cell
                if(bi.sprite) bi.sprite->set_position(wx(bi.blk.tx * 8 + 4), wy(bi.blk.ty * 8 + 4));
            }
            // M10: reset magic crystals each attempt (NOT latched) so a fresh full-refill is always
            // available after a death-respawn — guarantees no magic soft-lock on the Light ascent.
            for(MagicCrystalInst& ci : magic_crystals){
                ci.collected = false;
                if(ci.sprite) ci.sprite->set_visible(true);
            }
        }

        // ---- ability shrines ----
        for(ShrineInst& si2 : shrines){
            if(!world.has(si2.pk.ability) && logic::aabb_overlap(player.body, si2.body)){
                world.grant(si2.pk.ability);
                spell.ensure_valid(world);   // auto-select the new ability ONLY if nothing valid was selected; never clobber a cycled choice
                if(si2.sprite) si2.sprite->set_visible(false);
            }
        }
        // ---- heart containers: collect on overlap -> grow max HP + refill to full, persist. ----
        for(HeartInst& hi : hearts){
            if(hi.collected) continue;
            if(logic::aabb_overlap(player.body, hi.body)){
                world.collect_heart_container(hi.hc.id);
                health.max = logic::max_health_for(world);   // grow the cap (+25 per container)
                health.cur = health.max;                     // and refill to full — the payoff moment
                engine::write_world(world);                  // persist immediately (same path as latches)
                hi.collected = true;
                if(hi.sprite) hi.sprite->set_visible(false);
            }
        }

        // ---- magic crystals: collect on overlap -> full magic refill. NOT latched (resets each
        //      attempt below) so a Light beat never soft-locks on empty magic. ----
        for(MagicCrystalInst& ci : magic_crystals){
            if(ci.collected) continue;
            if(logic::aabb_overlap(player.body, ci.body)){
                magic.cur = magic.max;   // full refill — the guaranteed combat-free magic source
                ci.collected = true;
                if(ci.sprite) ci.sprite->set_visible(false);
            }
        }

        refresh_spell_icon();   // reflect cycle (L) and shrine pickups in the HUD icon

        // ---- spronk rescue (marks the dungeon cleared; abilities now come from F pickups) ----
        if(level.has_cage){
            bool was = world.spronk_freed(d);
            logic::try_free_spronk(player.body, cage, world, d);
            if(world.spronk_freed(d) && !was){
                if(spronk) spronk->set_visible(false);
                logic::refill_lives(world);   // freeing the spronk grants +1 max (via spronks_freed) AND refills NOW (on pickup, not on exit)
                engine::write_world(world);   // persist the new max + refilled lives immediately
            }
        }

        bool spronk_ok = !level.has_cage || world.spronk_freed(d);
        // Must LAND on the exit (grounded), not bump it from underneath — clearing requires
        // standing on the platform, which matters for the gated vertical climb (no head-bump cheese).
        if(level.has_exit && spronk_ok && player.body.on_ground && logic::aabb_overlap(player.body, exit)){
            return RoomOutcome{ RoomOutcome::ExitDungeon };
        }

        // ---- vine VFX: draw dot segments along player->anchor while grapple active ----
        if(miss_vine_t > 0) --miss_vine_t;
        if(player.grapple.active()){
            // Latched: draw dots from player to anchor (unchanged).
            int px_ = player.body.pos.x.to_int() + player.body.half_w.to_int();
            int py_ = player.body.pos.y.to_int() + player.body.half_h.to_int();
            int ax_ = player.grapple.anchor_tx * 8 + 4;
            int ay_ = player.grapple.anchor_ty * 8 + 4;
            for(int i = 0; i < VINE_SEGS; ++i){
                int t = i + 1;  // t in 1..VINE_SEGS (skip the player pos itself)
                int sx_ = px_ + (ax_ - px_) * t / (VINE_SEGS + 1);
                int sy_ = py_ + (ay_ - py_) * t / (VINE_SEGS + 1);
                vine_segs[i].set_position(sx_ - hw, sy_ - hh);
                vine_segs[i].set_visible(true);
            }
        } else if(miss_vine_t > 0){
            // Miss: shoot vine out and retract. Duration 10 frames; peaks at frame 5.
            // reach in tiles: extend 0->RANGE over first half, retract RANGE->0 over second half.
            // miss_vine_t counts down from 10 to 1; elapsed = 10 - miss_vine_t (0=just fired).
            int elapsed = 10 - miss_vine_t;           // 0..9 as the timer runs from 10 down to 1
            int half = 5;
            // phase: 0..half-1 = extending, half..9 = retracting
            int reach_tiles;
            if(elapsed < half){
                reach_tiles = (logic::GrappleState::RANGE * (elapsed + 1)) / half; // 0->RANGE
            } else {
                reach_tiles = (logic::GrappleState::RANGE * (10 - elapsed)) / half; // RANGE->0
            }
            if(reach_tiles < 1) reach_tiles = 1; // always at least one segment visible while active
            int px_ = player.body.pos.x.to_int() + player.body.half_w.to_int();
            int py_ = player.body.pos.y.to_int() + player.body.half_h.to_int();
            int reach_px = reach_tiles * 8;
            for(int i = 0; i < VINE_SEGS; ++i){
                // Space VINE_SEGS dots evenly from player to reach point.
                int t = i + 1;
                int sx_ = px_ + (miss_vine_dir * reach_px * t) / (VINE_SEGS + 1);
                int sy_ = py_;  // horizontal shot (stays at player centre height)
                vine_segs[i].set_position(sx_ - hw, sy_ - hh);
                vine_segs[i].set_visible(true);
            }
        } else {
            for(int i = 0; i < VINE_SEGS; ++i) vine_segs[i].set_visible(false);
        }

        // ---- pound VFX tick (placeholder): fade out the dust; apply a tiny vertical camera shake ----
        if(pound_vfx_t > 0){ if(--pound_vfx_t == 0) pound_dust.set_visible(false); }
        hud.update(health, magic, world.lives);
        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        if(pound_shake_t > 0){ cy += (pound_shake_t % 2 == 0) ? 2 : -2; --pound_shake_t; }  // 2px jitter
        set_clamped_cam(cx, cy);
        if(fade_in_t > 0) engine::set_fade(--fade_in_t);
        bn::core::update();
    }
}

DungeonResult run_dungeon(const logic::DungeonData& dungeon, logic::World& world, logic::PlayerState& ps)
{
    int cur_room = dungeon.start_room;
    int cur_entrance = 0;
    // Remember which dungeon this is so the hub can spawn the player at the matching door on return.
    // main() sets world.current_dungeon = n BEFORE calling run_dungeon; capturing it here persists the
    // number in PlayerState even after the scene resets world.current_dungeon to 0 on exit.
    ps.last_dungeon = world.current_dungeon;
    // Sync the max-HP cap to the collected heart containers (PlayerState defaults to 100/100, but a
    // continued game may have upgrades). Only raise the CAP here; the pickup itself refills to full.
    ps.health.max = logic::max_health_for(world);
    if(ps.health.cur > ps.health.max) ps.health.cur = ps.health.max;
    ps.spell.ensure_valid(world);  // selected tool lives in PlayerState; init a default without clobbering a carried-in choice (persists across rooms, hub, hub<->dungeon)
    while(true){
        RoomOutcome out = play_room(*dungeon.rooms[cur_room], cur_entrance, world, ps);
        engine::fade_out(16);   // one fade-out per room exit; next play_room fades in
        switch(out.kind){
            case RoomOutcome::ExitDungeon:
                return DungeonResult::Cleared;   // lives already refilled on spronk-free (in play_room); main persists on Cleared
            case RoomOutcome::Quit:        return DungeonResult::Quit;
            case RoomOutcome::Restart:
                ps.health.cur = ps.health.max;   // anti-soft-lock: refill vitals, replay same room
                ps.magic.cur  = ps.magic.max;
                break;   // cur_room/cur_entrance unchanged -> replay same room
            case RoomOutcome::GoToRoom:
                cur_room = out.target_room;
                cur_entrance = out.target_entrance;
                break;
            case RoomOutcome::GameOver: {
                game::GameOverChoice c = game::run_game_over(world);
                logic::refill_lives(world);   // both choices refill to max
                engine::write_world(world);   // persist the refill — the save never holds lives==0
                ps.health.cur = ps.health.max;   // refill vitals for BOTH choices — never return to the
                ps.magic.cur  = ps.magic.max;    // hub/title at 0 HP (the empty-health-bar bug)
                if(c == game::GameOverChoice::QuitToTitle) return DungeonResult::QuitToTitle;
                // Continue: restart THIS dungeon from the start room (vitals already refilled above).
                cur_room = dungeon.start_room; cur_entrance = 0;
                break;   // loop re-enters play_room at the start room
            }
        }
    }
}
}
