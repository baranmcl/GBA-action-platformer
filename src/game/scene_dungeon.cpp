#include "game/scene_dungeon.h"

#include "bn_core.h"
#include "bn_assert.h"
#include "bn_bg_palettes.h"
#include "bn_color.h"
#include "bn_camera_ptr.h"
#include "bn_sprite_ptr.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_guardian.h"   // M12: per-dungeon boss sprite (D1 Whispering Woods Guardian, 2 frames)
#include "bn_sprite_items_slagshell.h" // M13: D2 Ember Caverns boss sprite (Slagshell, 2 frames)
#include "bn_sprite_items_coldforge.h" // M14: D3 Frost Hollow boss sprite (Coldforge Twins, 4 frames)

#include "logic/tilemap.h"
#include "logic/world_state.h"   // max_health_for, collect/has heart container
#include "logic/player.h"
#include "logic/meters.h"
#include "logic/combat_rules.h"  // shared damage/i-frame/respawn constants + frame-step (M-remediation)
#include "logic/spell.h"
#include "logic/hazard.h"
#include "logic/tile_ids.h"
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
#include "game/scene_game_over.h" // run_game_over (death -> 0 lives flow)
#include "game/player_session.h" // play_room: PlayerSession, CrystalStation, set_clamped_cam
#include "game/boss_fight.h"     // run_boss_fight (Task 5.4: room bosses share the King's fight loop)
#include "game/room/room_ctx.h"          // Ctx: shared refs threaded into room systems (Task 6.1)
#include "game/room/pickups_system.h"    // shrines/hearts/crystals/cage-spronk/exit (Task 6.1)
#include "game/room/doors_system.h"      // room-door + exit archway render/Up-press (Task 6.1)
#include "game/room/gates_system.h"      // gates + cracked floors (Task 6.2)
#include "game/room/triggers_system.h"   // braziers + plate/button/brazier-group triggers (Task 6.2)
#include "game/room/enemies_system.h"    // patrolling enemies (Task 6.3)
#include "game/room/terrain_system.h"    // blocks/boulders/loose+hidden platforms + pound resolution (Task 6.3)

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
                 return bn::sprite_items::guardian;
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
    // Camera centers on the player but is clamped to the level bounds (IMPL-9).

    logic::EntranceSpawn ent = logic::find_entrance(level, entrance_id);
    const logic::Vec2 spawn_pos { fx(ent.tx * 8), fx(ent.ty * 8) };

    logic::Player player;
    player.body.half_w = fx(8); player.body.half_h = fx(16);
    player.body.pos = spawn_pos;
    player.facing = ent.facing;   // face inward at the entrance

    // A defeated room boss stays defeated (persisted, save v6) — re-entering the arena
    // while backtracking must not re-trigger a mandatory fight (D1 decision).
    if(level.boss != nullptr && d >= 1 && d <= 8 && !world.boss_defeated(d)){
        // Entry vitals: run_boss_fight refills health.cur to health.max (health.max is already
        // current -- run_dungeon syncs the cap once before the room loop). Magic carries in from
        // the previous room by design (room-boss semantics).
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
    // Respawn grace i-frames on death (logic::respawn_vitals) -- see IMPL-8.

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
    game::Ctx ctx{ world, player, ps, lvl, cam, hw, hh, invuln };
    game::PickupsSystem pickups;
    game::DoorsSystem doors;
    game::GatesSystem gates;       // spawned below, at its original per-frame-order spot (Task 6.2)
    game::TriggersSystem triggers; // spawned below, at its original per-frame-order spot (Task 6.2)
    game::EnemiesSystem enemies_sys;   // spawned below, at its original per-frame-order spot (Task 6.3)
    game::TerrainSystem terrain;   // blocks/boulders/loose+hidden platforms (Task 6.3), spawned below
    pickups.spawn(level, ctx);
    doors.spawn(level, ctx);

    // ---- enemies (Task 6.3: game::EnemiesSystem; fire_immune decoded from param2 bit0 inside spawn()) ----
    enemies_sys.spawn(level, ctx);

    // ---- gates + cracked floors (Task 6.2: game::GatesSystem) ----
    gates.spawn(level, ctx);

    // ---- pushable blocks, boulders, loose + hidden platforms (Task 6.3: game::TerrainSystem) ----
    terrain.spawn(level, ctx);

    // ---- braziers + plate/button/brazier-group triggers (Task 6.2: game::TriggersSystem) ----
    triggers.spawn(level, ctx);

    // Centre camera on the player before fading in (avoids a snap on frame 0).
    int cx0 = player.body.pos.x.to_int() + player.body.half_w.to_int();
    int cy0 = player.body.pos.y.to_int() + player.body.half_h.to_int();
    game::set_clamped_cam(cam, lvl.view.map_px_w, lvl.view.map_px_h, level.w, level.h, cx0, cy0);
    engine::set_fade(16);
    int fade_in_t = 16;
    // push_cd (block-push cooldown) moved into game::TerrainSystem (Task 6.3) -- it's fully
    // internal to update_blocks(). grapple_pull_cd stays HERE: it's the shared cooldown for BOTH
    // the block-pull and enemy-pull grapple targeting below, which both stay in play_room.
    int grapple_pull_cd = 0;
    int lr_restart_hold = 0; // frames L+R held — anti-soft-lock manual restart (moved off START)

    // ---- Per-frame update order (I28: single canonical list -- replaces the ordering comments
    //      that used to be scattered across this loop; do NOT reorder without checking every
    //      numbered constraint below) ----
    //   1. Global controls: pause, quit (SELECT), anti-soft-lock restart hold (L+R), door Up-press.
    //   2. Ability sync + intent read, then grapple targeting (block-pull -> enemy-pull -> anchor
    //      fallback) -- reads intent before player.update() consumes it.
    //   3. player.update() + avatar sync + anchor-miss detection.
    //   4. Pound impact resolution (terrain.resolve_pound): cracked-floor smash (may re-arm the
    //      pound to chain through stacked floors, see IMPL-7) -> heavy-plate trip -> boulder
    //      break -> loose-platform shockwave arm.
    //   5. Loose-platform drop-to-rest stepping.
    //   6. Bolts update, then spell cast.
    //   7. Hidden-platform (Light reveal) toggle.
    //   8. Spell resolution: gates -> braziers -> enemies.
    //   9. Freeze/melt tile transforms (Ice water->ice, Fire ice->water) -- MUST precede
    //      despawn_on_solid (IMPL-6).
    //  10. despawn_on_solid.
    //  11. Hazard damage (lava/water/spikes).
    //  12. Pushable-block push/gravity/sprite update.
    //  13. Triggers (plate/button/brazier-group) open/close, fed this frame's block tiles.
    //  14. i-frame tick + death/respawn (lose a life, or respawn with grace i-frames + reset
    //      transient player state + reset blocks + reset magic crystals).
    //  15. Ability shrines, then heart containers + magic crystals.
    //  16. Spronk-rescue + exit check (may return RoomOutcome::ExitDungeon).
    //  17. Vine VFX, pound VFX/camera-shake tick, HUD, camera clamp, fade-in tick.
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
            for(BlockInst& bi : terrain.blocks()){
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
                for(EnemyInst& ei : enemies_sys.enemies()){
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

        // ---- pound impact resolution (order: step 4 above); play_room owns only the VFX ----
        game::TerrainSystem::PoundImpact impact = terrain.resolve_pound(ctx, gates, triggers);
        if(impact.landed){
            // Pound VFX (placeholder): puff of dust at the impacted floor + a brief camera shake.
            pound_dust.set_position(wx(impact.cx * 8 + 4), wy(impact.floor * 8 + 4));
            pound_dust.set_visible(true);
            pound_vfx_t = 8;
            pound_shake_t = 6;
        }

        // ---- loose platforms: drop-to-rest one tile/frame while falling (Task 6.3: game::TerrainSystem) ----
        terrain.update_loose_platforms(ctx);

        // Shot aim (Zelda II style, shared with the boss/hub): UP = high, DOWN = low, else medium.
        logic::Vec2 muzzle = session.muzzle();
        bolts.update(intent.in.fire_pressed, muzzle, player.facing, lvl.map);

        logic::SpellId fired = spells.update_and_cast(intent.cast_spell, spell, magic, muzzle, player.facing, lvl.map);

        // Light reveal: fired-spell (not a magic delta -- the crystal refill would corrupt a
        // before/after inference) tells TerrainSystem a Light cast actually fired.
        terrain.update_hidden_platforms(ctx, fired == logic::SpellId::Light);

        // ---- spell resolution (order: step 8 above) ----
        gates.update(ctx, spells);
        triggers.update_braziers(ctx, spells);
        enemies_sys.update(ctx, bolts, spells);

        // ---- freeze/melt: Ice turns water into an ice bridge, Fire melts it back (IMPL-6) ----
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

        // ---- pushable blocks: push detection, gravity, sprite (Task 6.3: game::TerrainSystem) ----
        if(grapple_pull_cd > 0) --grapple_pull_cd;   // ticks here; checked in the input phase above (stays in play_room -- shared with enemy-pull)
        terrain.update_blocks(ctx, intent.in.left, intent.in.right);

        // ---- triggers: update inputs, open/close targets ----
        {
            // Cross-system query (Task 6.2 extraction rule; Task 6.3 fulfills it): TriggersSystem
            // doesn't own blocks (game::TerrainSystem does), so play_room hands it this frame's
            // block tiles for the plate's "something is resting on me" check -- mirrors the
            // original inline `for(BlockInst& bi : blocks)` scan exactly, now scanning
            // `terrain.blocks()` instead of a local vector.
            bn::vector<game::BlockTileXY, 8> block_tiles;
            for(BlockInst& bi : terrain.blocks()) block_tiles.push_back({ bi.blk.tx, bi.blk.ty });
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
            // button/brazier gates stay solved.) Task 6.3: game::TerrainSystem::reset_blocks.
            terrain.reset_blocks(ctx);
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
