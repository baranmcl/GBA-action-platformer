#include "game/scene_dungeon.h"

#include "bn_core.h"
#include "bn_assert.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_optional.h"
#include "bn_sprite_items_enemy.h"
#include "bn_sprite_items_fire_enemy.h"
#include "bn_sprite_items_block.h"
#include "bn_sprite_items_fire_proj.h"
#include "bn_sprite_items_ice_proj.h"
#include "bn_sprite_items_light_proj.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_grapple_icon.h"
#include "bn_sprite_items_guardian.h"   // M12: per-dungeon boss sprite (D1 Whispering Woods Guardian, 2 frames)
#include "bn_sprite_items_slagshell.h" // M13: D2 Ember Caverns boss sprite (Slagshell, 2 frames)
#include "bn_sprite_items_coldforge.h" // M14: D3 Frost Hollow boss sprite (Coldforge Twins, 4 frames)

#include "logic/reveal.h"

#include "logic/tilemap.h"
#include "logic/world_state.h"   // max_health_for, collect/has heart container
#include "logic/player.h"
#include "logic/enemy.h"
#include "logic/meters.h"
#include "logic/combat_rules.h"  // shared damage/i-frame/respawn constants + frame-step (M-remediation)
#include "logic/spell.h"
#include "logic/hazard.h"
#include "logic/pushable_block.h"
#include "logic/tile_ids.h"
#include "logic/stone_impact.h" // loose_platform_in_shockwave (pound shockwave radius)
#include "logic/room_graph.h"   // find_entrance (room_door_at now lives in game::DoorsSystem)
#include "engine/level_loader.h"  // load_level, set_collision_tile
#include "engine/level_view.h"    // set_level_tile
#include "engine/avatar.h"
#include "engine/bolts.h"
#include "engine/pause.h"        // check_pause (START -> GAME PAUSED; global pause)
#include "engine/spell_pool.h"
#include "engine/hud.h"
#include "engine/fade.h"
#include "engine/save.h"        // write_world (persist latches)
#include "logic/boss.h"          // BossDef/BossId (boss_sprite_for) — the fight itself runs in game::run_boss_fight
#include "logic/collision.h"     // aabb_overlap (enemy/gate/pickup contact)
#include "game/scene_game_over.h" // run_game_over (death -> 0 lives flow)
#include "game/player_session.h" // play_room: PlayerSession, CrystalStation, set_clamped_cam
#include "game/boss_fight.h"     // run_boss_fight (Task 5.4: room bosses share the King's fight loop)
#include "game/room/room_ctx.h"          // Ctx: shared refs threaded into room systems (Task 6.1)
#include "game/room/pickups_system.h"    // shrines/hearts/crystals/cage-spronk/exit (Task 6.1)
#include "game/room/doors_system.h"      // room-door + exit archway render/Up-press (Task 6.1)
#include "game/room/gates_system.h"      // gates + cracked floors (Task 6.2)
#include "game/room/triggers_system.h"   // braziers + plate/button/brazier-group triggers (Task 6.2)

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
    struct BlockInst { logic::PushableBlock blk; bn::optional<bn::sprite_ptr> sprite; bool pullable = false; };
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
    // NOTE (Task 6.2): GateInst/CrackedFloorInst/BrazierInst/TriggerInst + floor_row_below/
    // persist_latch/tile_body/fill_column/open_column moved out with the gates/braziers/triggers
    // families -- see game::GatesSystem (src/game/room/gates_system.cpp) and game::TriggersSystem
    // (src/game/room/triggers_system.cpp). Nothing left in this function needs them.
}

// ---------------------------------------------------------------------------
// Map a boss def to its sprite (frame 0 = normal/armored, frame 1 = exposed/vulnerable; D3 Coldforge
// has 4 frames — see run_boss_fight's frame-swap). Keyed by BossDef::id (a stable, explicit tag)
// rather than a pointer-compare against the canonical def symbols — an unmapped future boss id now
// fails LOUDLY at fight start (BN_ERROR) instead of silently wearing D1's guardian art. The King
// (BossId::King) is never routed through this function (scene_boss resolves its own sprite), so it
// intentionally has no case here; it falls into default, which is fine since default is BN_ERROR.
static const bn::sprite_item& boss_sprite_for(const logic::BossDef* def){
    switch(def->id){
        case logic::BossId::D2Slagshell: return bn::sprite_items::slagshell;
        case logic::BossId::D3Coldforge: return bn::sprite_items::coldforge;
        case logic::BossId::D1Guardian:  return bn::sprite_items::guardian;
        default: BN_ERROR("no sprite mapped for boss id ", (int)def->id);
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

    logic::EntranceSpawn ent = logic::find_entrance(level, entrance_id);
    const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };

    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;
    player.facing = ent.facing;   // face inward at the entrance

    // A defeated room boss stays defeated (persisted, save v6) — re-entering the arena
    // while backtracking must not re-trigger a mandatory fight (D1 decision).
    if(level.boss != nullptr && d >= 1 && d <= 8 && !world.boss_defeated(d)){
        // Room-boss entry vitals (Task 5.4 verification): run_boss_fight's inline_game_over=false path
        // only does health.cur = health.max (matching the deleted run_room_boss's own entry — it never
        // touched health.max or refilled magic). ps.health.max is already correct here without any extra
        // line: run_dungeon calls logic::sync_health_cap(ps, world) ONCE before the room loop starts, and
        // the only other place health.max changes is the heart-container pickup below (which also
        // refills health.cur to match) — so the cap carried into this room is already up to date. Magic
        // is deliberately left carried-in from the previous room (room-boss semantics), same as before.
        game::FightOutcome fo = run_boss_fight(level, *level.boss, boss_sprite_for(level.boss),
                                               world, ps, lvl, cam, player, spawn_pos, ent,
                                               /*inline_game_over=*/false);
        if(fo == game::FightOutcome::GameOver) return RoomOutcome{ RoomOutcome::GameOver };
        world.set_boss_defeated(d);
        engine::write_world(world);
        engine::fade_out(16);   // clear the boss screen; the normal room loop fades back in
    }

    logic::Meter& health = ps.health;   // persist across hub <-> dungeon (no reset on entry)
    logic::Meter& magic  = ps.magic;
    int invuln = 0;
    // Post-respawn grace: i-frames granted on death (logic::respawn_vitals, logic::RESPAWN_IFRAMES)
    // so a player who died in a sub-floor hazard pit cannot be re-damaged before regaining control.
    // The header's static_assert enforces RESPAWN_IFRAMES > HIT_IFRAMES so even an authored-unsafe
    // spawn yields real control frames instead of an unbreakable every-frame death loop (the
    // "stuck at the bottom" report). The entrance is authored safe, but this guarantees robustness
    // regardless.

    engine::Avatar avatar(player, lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::BoltPool bolts(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::SpellPool spells(lvl.view.map_px_w, lvl.view.map_px_h, cam);
    engine::Hud hud;

    // Player-loop unit: ability sync, input decode, spell HUD icon, vine VFX, muzzle calc.
    game::PlayerSession session(cam, world, ps, player);
    session.refresh_spell_icon();   // one-time: show the carried-in selection immediately

    // ---- pound VFX (placeholder): a brief dust/impact dot (bolt sprite reused, like the vine VFX)
    //      shown for a few frames at the impact point on each pound landing, + a tiny camera nudge. ----
    bn::sprite_ptr pound_dust = bn::sprite_items::bolt.create_sprite(0, 0);
    pound_dust.set_camera(cam);
    pound_dust.set_visible(false);
    pound_dust.set_scale(1.5);   // squashed puff (placeholder)
    int pound_vfx_t = 0;         // frames remaining the dust is shown
    int pound_shake_t = 0;       // frames remaining of camera nudge

    // ---- room systems (Task 6.1): shared Ctx threaded into the pickups/doors families that
    //      were extracted out of this function's body. spawn() below fills in for the removed
    //      cage/spronk, exit, ability-shrine, heart-container, magic-crystal, room-door-archway,
    //      and exit-archway inline blocks (verbatim moves — see src/game/room/*.cpp). ----
    game::Ctx ctx{ world, player, ps, lvl, cam, hw, hh };
    game::PickupsSystem pickups;
    game::DoorsSystem doors;
    game::GatesSystem gates;       // spawned below, at its original per-frame-order spot (Task 6.2)
    game::TriggersSystem triggers; // spawned below, at its original per-frame-order spot (Task 6.2)
    pickups.spawn(level, ctx);
    doors.spawn(level, ctx);

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

    // ---- gates + cracked floors (Task 6.2: game::GatesSystem) ----
    gates.spawn(level, ctx);

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

    // ---- braziers + plate/button/brazier-group triggers (Task 6.2: game::TriggersSystem) ----
    triggers.spawn(level, ctx);

    // Centre camera on the player before fading in (avoids a snap on frame 0).
    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    game::set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, level.w, level.h, cx0, cy0);
    engine::set_fade(16);
    int fade_in_t = 16;
    int push_cd = 0;
    int grapple_pull_cd = 0;
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
            game::DoorsSystem::UpPressResult dr = doors.on_up_pressed(level, player);
            // target_room == -1 is the sentinel "exit-to-hub" door: a diegetic Up-press
            // equivalent of SELECT=quit. Return Quit so run_dungeon returns DungeonResult::Quit
            // (NOT Cleared) -> the hub loop resumes WITHOUT marking the dungeon cleared, and the
            // dungeon stays re-enterable.
            if(dr.kind == game::DoorsSystem::UpPressResult::ExitToHub) return RoomOutcome{ RoomOutcome::Quit };
            if(dr.kind == game::DoorsSystem::UpPressResult::GoToRoom)
                return RoomOutcome{ RoomOutcome::GoToRoom, dr.target_room, dr.target_entrance };
        }

        session.sync_abilities();
        // Read spell intent + cycle FIRST so the selection is current for the grapple/cast branch:
        SessionIntent intent = session.read_intent();
        // R fires the SELECTED tool: Grapple -> pull a nearby pullable block one tile toward the
        // player (if one is in range/arc), else latch the player to an anchor; Fire/Ice -> cast.
        if(intent.want_grapple){
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
                    intent.in.grapple_fire = true; // no block, no enemy -> player anchor-grapple
                }
            }
        }
        // Capture the "tried to anchor-grapple" flag BEFORE player.update consumes it.
        bool tried_anchor = intent.want_grapple && intent.in.grapple_fire;
        player.update(intent.in, lvl.map);
        avatar.sync(player);
        // Miss detection: player tried an anchor-grapple but nothing latched (no grapple point in range).
        if(tried_anchor && !player.grapple.active())
            session.note_anchor_miss(player.facing);

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
            //    cracked floor, break the WHOLE contiguous cracked-floor run at that row (I27: a 2-direction
            //    walk from the impact tile — game::GatesSystem::break_cracked_run_at) and RE-ARM the pound
            //    so the next frame plunges into the area below. Re-arm ONLY on a cracked tile, so one pound
            //    chains through STACKED cracked floors and naturally ends on the first non-cracked solid.
            bool smashed = gates.break_cracked_run_at(impact_cx, impact_floor, ctx);
            if(smashed) player.stone.start();   // re-arm: plunge through to the next floor below

            // 2. Heavy switch: a heavy plate trips ONLY on a pound (game::TriggersSystem::trip_heavy_plate_at).
            //    Fires its gate target when the player's feet/centre land on the plate tile. (Normal plates
            //    are handled in the trigger loop below, which SKIPS heavy plates so they never trip on a
            //    step/block.)
            triggers.trip_heavy_plate_at(impact_cx, impact_fy, ctx);

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
        logic::Vec2 muzzle = session.muzzle();
        bolts.update(intent.in.fire_pressed, muzzle, player.facing, lvl.map);

        logic::SpellId fired = spells.update_and_cast(intent.cast_spell, spell, magic, muzzle, player.facing, lvl.map);

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
        gates.update(ctx, spells);
        triggers.update_braziers(ctx, spells);

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
                inst.e.kill(); magic.heal(logic::KILL_MAGIC_REFILL); inst.sprite->set_visible(false);
            } else if(bolts.consume_hit(inst.e.body)){
                inst.e.kill(); magic.heal(logic::KILL_MAGIC_REFILL); inst.sprite->set_visible(false);
            } else if(spells.consume_hit(inst.e.body, logic::SpellId::Fire)){
                if(!inst.e.fire_immune){ inst.e.kill(); inst.sprite->set_visible(false); } // no magic refill from fire
            } else {
                logic::try_hit(health, invuln, player.dash.invincible(), logic::aabb_overlap(player.body, inst.e.body));  // dash i-frames blink through contact
            }
        }

        // ---- reversible terrain: Ice freezes water it flies over; Fire melts ice platforms.
        // Spells fly at chest height but water/ice sit at floor level, so consume_tile_hit scans
        // DOWN the shot's column (the M3 brazier-height lesson). MUST precede despawn_on_solid
        // because IcePlatform is solid — otherwise a melt-shot is killed before it can melt.
        int ftx, fty;
        while(spells.consume_tile_hit(lvl.map, logic::TileKind::Water, logic::SpellId::Ice, ftx, fty)){
            // Freeze the WHOLE contiguous horizontal run of water into one ice bridge (one cast),
            // not just the box the shot touched.
            int x0 = ftx; while(lvl.map.is_water(x0 - 1, fty)) --x0;
            int x1 = ftx; while(lvl.map.is_water(x1 + 1, fty)) ++x1;
            for(int x = x0; x <= x1; ++x){
                engine::set_collision_tile(x, fty, (int)logic::TileKind::IcePlatform); // collision VALUE 5
                engine::set_level_tile(lvl.view, x, fty, logic::tiles::ICE_PLATFORM);  // bg INDEX 19
            }
        }
        while(spells.consume_tile_hit(lvl.map, logic::TileKind::IcePlatform, logic::SpellId::Fire, ftx, fty)){
            // Melt the WHOLE contiguous ice run back to water (one cast), symmetric to the freeze.
            int x0 = ftx; while(lvl.map.at(x0 - 1, fty) == logic::TileKind::IcePlatform) --x0;
            int x1 = ftx; while(lvl.map.at(x1 + 1, fty) == logic::TileKind::IcePlatform) ++x1;
            for(int x = x0; x <= x1; ++x){
                engine::set_collision_tile(x, fty, (int)logic::TileKind::Water);     // collision VALUE 4
                engine::set_level_tile(lvl.view, x, fty, logic::tiles::WATER);       // bg INDEX 16
            }
        }
        spells.despawn_on_solid(lvl.map);

        // ---- hazards (lava, water, or spikes): same damage; dash i-frames blink through ----
        logic::try_hit(health, invuln, player.dash.invincible(), logic::hazard_overlap(player.body, lvl.map));

        // ---- pushable blocks: push detection, gravity, sprite ----
        if(push_cd > 0) --push_cd;
        if(grapple_pull_cd > 0) --grapple_pull_cd;   // ticks here; checked in the input phase above
        for(BlockInst& bi : blocks){
            // push when grounded, holding a dir, and the tile in front of the player == this block
            if(push_cd == 0 && player.body.on_ground && (intent.in.left || intent.in.right)){
                int dir = intent.in.right ? 1 : -1;
                int lead_px = intent.in.right ? player.body.pos.x.to_int() + 16 : player.body.pos.x.to_int() - 1;
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
        {
            // Cross-system query (Task 6.2 extraction rule): TriggersSystem doesn't own blocks
            // (Task 6.3's terrain_system will), so play_room hands it this frame's block tiles for
            // the plate's "something is resting on me" check -- mirrors the original inline
            // `for(BlockInst& bi : blocks)` scan exactly.
            bn::vector<game::BlockTileXY, 8> block_tiles;
            for(BlockInst& bi : blocks) block_tiles.push_back({ bi.blk.tx, bi.blk.ty });
            triggers.update_triggers(ctx, block_tiles);
        }

        // ---- i-frames / respawn ----
        bool was_invuln = invuln > 0;
        logic::tick_iframes(invuln);
        if(was_invuln) avatar.set_visible((invuln / 4) % 2 == 0);
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
            logic::respawn_vitals(health, invuln);   // grace window (NOT 0): never re-die before regaining control
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
            pickups.crystals.reset();
        }

        // ---- ability shrines ----
        pickups.update_shrines(ctx);
        // ---- heart containers (collect -> grow max HP + refill) + magic crystals (collect ->
        //      full magic refill; NOT latched, resets each attempt above) ----
        pickups.update_hearts_and_crystals(ctx);

        session.refresh_spell_icon();   // reflect cycle (L) and shrine pickups in the HUD icon

        // ---- spronk rescue (marks the dungeon cleared) + exit: must LAND on the exit (grounded),
        //      not bump it from underneath — clearing requires standing on the platform, which
        //      matters for the gated vertical climb (no head-bump cheese). ----
        if(pickups.check_spronk_and_exit(ctx)){
            return RoomOutcome{ RoomOutcome::ExitDungeon };
        }

        session.update_vine_vfx(hw, hh);

        // ---- pound VFX tick (placeholder): fade out the dust; apply a tiny vertical camera shake ----
        if(pound_vfx_t > 0){ if(--pound_vfx_t == 0) pound_dust.set_visible(false); }
        hud.update(health, magic, world.lives);
        int cx = player.body.pos.x.to_int() + player.body.half_w.to_int();
        int cy = player.body.pos.y.to_int() + player.body.half_h.to_int();
        if(pound_shake_t > 0){ cy += (pound_shake_t % 2 == 0) ? 2 : -2; --pound_shake_t; }  // 2px jitter
        game::set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, level.w, level.h, cx, cy);
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
    logic::sync_health_cap(ps, world);
    ps.spell.ensure_valid(world);  // selected tool lives in PlayerState; init a default without clobbering a carried-in choice (persists across rooms, hub, hub<->dungeon)
    while(true){
        BN_ASSERT(cur_room >= 0 && cur_room < dungeon.room_count,
                  "room index out of range: ", cur_room, " of ", dungeon.room_count);
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
