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
#include "bn_sprite_items_bolt.h"
#include "bn_sprite_items_grapple_icon.h"
#include "bn_sprite_items_heart_container.h"

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
#include "engine/spell_pool.h"
#include "engine/hud.h"
#include "engine/fade.h"
#include "engine/save.h"        // write_world (persist latches)

namespace game
{
namespace
{
    logic::Fixed fx(int v){ return logic::Fixed::from_int(v); }
    int px2t(logic::Fixed p){ return logic::Tilemap::px_to_tile(p); }

    struct RoomOutcome {
        enum Kind { ExitDungeon, Quit, Restart, GoToRoom } kind;
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

static RoomOutcome play_room(const logic::LevelData& level, int entrance_id, logic::World& world, logic::PlayerState& ps, logic::SpellState& spell)
{
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

    logic::Meter& health = ps.health;   // persist across hub <-> dungeon (no reset on entry)
    logic::Meter& magic  = ps.magic;
    int invuln = 0;

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
        spronk->set_position(wx(cage.pos.x.to_int() + 8), wy(cage.pos.y.to_int() + 8));
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
        hi.sprite->set_position(wx(hc.tx * 8 + 8), wy(hc.ty * 8 + 8));
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

    // ---- room-doors (bg tile 5 open-door; 2-wide x 4-tall archway grounded on the floor,
    //      matching the hub's archway). Floor-scanned so a row-18-authored door reaches the
    //      floor (row 20) instead of floating. Collision unchanged (door stays walkable). ----
    for(int i = 0; i < level.room_door_count && i < 8; ++i){
        const logic::RoomDoorSpawn& rd = level.room_doors[i];
        int fr = floor_row_below(lvl.map, rd.tx, rd.ty);
        for(int dy = 0; dy < 4; ++dy) for(int dx = 0; dx < 2; ++dx)
            engine::set_level_tile(lvl.view, rd.tx + dx, fr - 1 - dy, 5);
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

    while(true)
    {
        if(bn::keypad::select_pressed()) { return RoomOutcome{ RoomOutcome::Quit }; }
        if(bn::keypad::start_pressed())  { return RoomOutcome{ RoomOutcome::Restart }; }  // anti-soft-lock level reset
        if(bn::keypad::up_pressed()){
            if(const logic::RoomDoorSpawn* dr = logic::room_door_at(level, player.body)){
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
                                      spell.selected == logic::SpellId::Ice);
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
            int impact_fy = px2t(player.body.pos.y + player.body.half_h + player.body.half_h - fx(1));     // feet row (tile landed on)
            // Pound VFX (placeholder): puff of dust at the feet + a brief camera shake.
            pound_dust.set_position(wx(impact_cx * 8 + 4), wy(impact_fy * 8 + 4));
            pound_dust.set_visible(true);
            pound_vfx_t = 8;
            pound_shake_t = 6;
            // 1. CrackedFloor smash + continue the plunge. The landed tile is solid; if it is an unbroken
            //    cracked floor, break the WHOLE contiguous cracked-floor run at that row and RE-ARM the
            //    pound so the next frame plunges into the area below. Re-arm ONLY on a cracked tile, so one
            //    pound chains through STACKED cracked floors and naturally ends on the first non-cracked solid.
            bool smashed = false;
            for(CrackedFloorInst& cf : cracked_floors){
                if(cf.broken || cf.tx != impact_cx || cf.ty != impact_fy) continue;
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
                bool below = (bo.tx == impact_cx && (bo.ty == impact_fy || bo.ty == impact_fy + 1));
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
                if(logic::loose_platform_in_shockwave(li.tx, li.cur_ty, li.len, impact_cx, impact_fy))
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

        logic::Vec2 muzzle = { player.body.pos.x + player.body.half_w,
                               player.body.pos.y + player.body.half_h };
        bolts.update(in.fire_pressed, muzzle, player.facing, lvl.map);

        spells.update_and_cast(cast_spell, spell, magic, muzzle, player.facing, lvl.map);

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
            player.body.pos = spawn_pos; player.body.vel = { fx(0), fx(0) };
            health.cur = health.max; invuln = 0;
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
        }

        // ---- ability shrines ----
        for(ShrineInst& si2 : shrines){
            if(!world.has(si2.pk.ability) && logic::aabb_overlap(player.body, si2.body)){
                world.grant(si2.pk.ability);
                spell.refresh(world);
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

        refresh_spell_icon();   // reflect cycle (L) and shrine pickups in the HUD icon

        // ---- spronk rescue (marks the dungeon cleared; abilities now come from F pickups) ----
        if(level.has_cage){
            bool was = world.spronk_freed(d);
            logic::try_free_spronk(player.body, cage, world, d);
            if(world.spronk_freed(d) && !was){
                if(spronk) spronk->set_visible(false);
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
        hud.update(health, magic);
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
    // Sync the max-HP cap to the collected heart containers (PlayerState defaults to 100/100, but a
    // continued game may have upgrades). Only raise the CAP here; the pickup itself refills to full.
    ps.health.max = logic::max_health_for(world);
    if(ps.health.cur > ps.health.max) ps.health.cur = ps.health.max;
    logic::SpellState spell; spell.refresh(world);  // selected spell persists across room transitions
    while(true){
        RoomOutcome out = play_room(*dungeon.rooms[cur_room], cur_entrance, world, ps, spell);
        engine::fade_out(16);   // one fade-out per room exit; next play_room fades in
        switch(out.kind){
            case RoomOutcome::ExitDungeon: return DungeonResult::Cleared;
            case RoomOutcome::Quit:        return DungeonResult::Quit;
            case RoomOutcome::Restart:
                ps.health.cur = ps.health.max;   // anti-soft-lock: refill vitals, replay same room
                ps.magic.cur  = ps.magic.max;
                break;   // cur_room/cur_entrance unchanged -> replay same room
            case RoomOutcome::GoToRoom:
                cur_room = out.target_room;
                cur_entrance = out.target_entrance;
                break;
        }
    }
}
}
