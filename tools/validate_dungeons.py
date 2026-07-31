#!/usr/bin/env python3
"""Whole-game dungeon validator for Spronk Quest.

Task 1.1's `validate_level` (in build_level.py) enforces PER-ROOM caps mirroring the
runtime bn::vector spawn buffers. This module is the complement: it compiles every room
of every dungeon named in `tools/levels/manifest.json` and checks rules that only make
sense CROSS-room or CROSS-dungeon — a typo'd room-graph edge is an out-of-bounds pointer
read on hardware; a colliding latch/heart id is save-file corruption.

Usage: `python tools/validate_dungeons.py` (run from the repo root or from tools/ —
paths are resolved relative to this file, not the current working directory).

Rules (see docs plan Task 1.2 brief for the full rationale):
  1. Room-graph integrity (I3b): every room_doors target_room is -1 (hub exit) or a
     valid index into that dungeon's room list; every target_entrance matches an
     entrance id present in the target room. A door with no return door back to its
     source room is a WARNING (never an error) — D7 has a deliberate one-way case.
  2. dungeons.h drift check: the manifest's room lists must match the `&<NAME>_DATA`
     symbols actually wired into each `DUNGEON<KEY>_ROOMS[]` array in
     include/game/levels/dungeons.h, in order.
  3. Global latch registry (I3e): every latch_id >= 0 (from gates — including
     CrackedFloor gates — and brazier_groups) must be in [0, 23]. Duplicates are
     allowed WITHIN one room file (a room can deliberately reuse an id for one
     shortcut) but a cross-room duplicate is a save-corruption bug.
  4. Global heart-container registry: every heart id must be in [0, 7] and globally
     unique (no same-room exception — a room repeating an id is still a collision).
  5. Sprite-budget estimate (I34): per room,
     enemies + blocks + boulders + crystals + hearts + shrines + health_pickups
     + Σ loose_platform len + Σ hidden_platform len + 45 (HUD/player/vine/VFX reserve)
     must be <= 110 (Butano's OAM budget is 128; the margin is deliberate).
  6. Arena constraints (I36): any room whose JSON has a "boss" key must have a solid
     tile at (w/2, h-2) and (w/2 +/- 1, h-2) (flat floor under the boss). Magic
     crystals are OPTIONAL in a boss arena (D1's TiredWindow boss needs no magic;
     D3's Coldforge Twins recharge via block-to-charge) — but AT MOST ONE, because
     the boss fight loop only ever reads magic_crystals[0]; a second crystal placed
     in a boss room is silently dead content, which is the actual bug worth catching
     (border columns 0/w-1 are already enforced by build_level's per-room border
     check).

Entity tuples are accessed by PREFIX POSITION ONLY (e.g. a plate's p[0], p[1] for tx,
ty) — never by full-tuple unpacking beyond the fields this module actually reads. A
later task appends a field to the plate tuple; this module must not break when it does.
"""
import json
import os
import re
import sys

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
if _THIS_DIR not in sys.path:
    sys.path.insert(0, _THIS_DIR)
import build_level  # noqa: E402  (path shim above must run first)

_REPO_ROOT = os.path.dirname(_THIS_DIR)
DEFAULT_MANIFEST = os.path.join(_THIS_DIR, 'levels', 'manifest.json')
DEFAULT_LEVELS_DIR = os.path.join(_THIS_DIR, 'levels')
DEFAULT_DUNGEONS_H = os.path.join(_REPO_ROOT, 'include', 'game', 'levels', 'dungeons.h')

MAX_LATCH_ID = 23
MAX_HEART_ID = 7
SPRITE_RESERVE = 45
SPRITE_BUDGET_CAP = 110

# manifest key -> the DUNGEON<...>_ROOMS[] array name in dungeons.h.
ARRAY_NAME = {str(i): f'DUNGEON{i}_ROOMS' for i in range(1, 9)}
ARRAY_NAME['9-approach'] = 'DUNGEON9_APPROACH_ROOMS'
ARRAY_NAME['9-arena'] = 'DUNGEON9_ARENA_ROOMS'


def load_manifest(path):
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def compile_room(name, levels_dir):
    txt_path = os.path.join(levels_dir, name + '.txt')
    json_path = os.path.join(levels_dir, name + '.json')
    return build_level.compile_level(txt_path, json_path)


def check_room_graph(dungeon_key, room_names, rooms):
    """Returns (errors, warnings) for one dungeon's room_doors/entrances."""
    errors = []
    warnings = []
    n = len(rooms)
    for i, lvl in enumerate(rooms):
        room_name = room_names[i]
        for d in lvl['room_doors']:
            tx, ty, target_room, target_entrance = d[0], d[1], d[2], d[3]
            if target_room == -1:
                continue  # hub-exit door: no in-dungeon target to validate
            if not (0 <= target_room < n):
                errors.append(
                    f"[{dungeon_key}] {room_name}: room_door at ({tx},{ty}) target_room "
                    f"{target_room} out of range [0,{n})")
                continue
            target_name = room_names[target_room]
            target_lvl = rooms[target_room]
            entrance_ids = {e[0] for e in target_lvl['entrances']}
            if target_entrance not in entrance_ids:
                errors.append(
                    f"[{dungeon_key}] {room_name}: room_door at ({tx},{ty}) target_entrance "
                    f"{target_entrance} not found in {target_name} (entrances: {sorted(entrance_ids)})")
                continue
            has_return = any(rd[2] == i for rd in target_lvl['room_doors'])
            if not has_return:
                warnings.append(
                    f"[{dungeon_key}] {room_name} -> {target_name} (room {target_room}) has no "
                    f"room_door back to room {i} ({room_name}) — one-way edge")
    return errors, warnings


def check_dungeons_h_drift(manifest, dungeons_h_text):
    errors = []
    for key, room_names in manifest.get('dungeons', {}).items():
        array_name = ARRAY_NAME.get(key)
        if array_name is None:
            errors.append(f"manifest key '{key}' has no known dungeons.h array mapping")
            continue
        m = re.search(re.escape(array_name) + r'\[\]\s*=\s*\{([^}]*)\}', dungeons_h_text)
        if not m:
            errors.append(f"dungeons.h: array '{array_name}' not found (manifest key '{key}')")
            continue
        actual_syms = re.findall(r'&(\w+)_DATA', m.group(1))
        expected_syms = [name.upper() for name in room_names]
        if actual_syms != expected_syms:
            errors.append(
                f"dungeons.h drift for '{array_name}' (manifest key '{key}'): "
                f"manifest expects {expected_syms}, dungeons.h has {actual_syms}")
    return errors


def check_latch_registry(all_rooms):
    """all_rooms: iterable of (room_name, compiled_level). Cross-room latch_id
    collisions are errors; same-room reuse is allowed (D7's shortcut pattern)."""
    errors = []
    locations = {}  # latch_id -> set of room_names using it
    for room_name, lvl in all_rooms:
        room_ids = set()
        for g in lvl['gates']:
            lid = g[3]
            if lid is not None and lid >= 0:
                room_ids.add(lid)
        for bg in lvl['brazier_groups']:
            lid = bg[3]
            if lid >= 0:
                room_ids.add(lid)
        for lid in room_ids:
            if not (0 <= lid <= MAX_LATCH_ID):
                errors.append(f"{room_name}: latch_id {lid} out of range [0,{MAX_LATCH_ID}]")
            locations.setdefault(lid, set()).add(room_name)
    for lid, rooms_using in locations.items():
        if len(rooms_using) > 1:
            errors.append(
                f"latch_id {lid} used in multiple rooms (cross-room collision): {sorted(rooms_using)}")
    return errors


def check_heart_registry(all_rooms):
    """Heart-container ids must be globally unique (no same-room exception)."""
    errors = []
    locations = {}  # heart_id -> list of (room_name, tx, ty)
    for room_name, lvl in all_rooms:
        for hc in lvl['heart_containers']:
            tx, ty, hid = hc[0], hc[1], hc[2]
            if not (0 <= hid <= MAX_HEART_ID):
                errors.append(f"{room_name}: heart id {hid} out of range [0,{MAX_HEART_ID}]")
            locations.setdefault(hid, []).append((room_name, tx, ty))
    for hid, locs in locations.items():
        if len(locs) > 1:
            errors.append(f"heart id {hid} duplicated across: {locs}")
    return errors


def check_sprite_budget(room_name, lvl):
    total = (
        len(lvl['enemies']) + len(lvl['blocks']) + len(lvl['boulders'])
        + len(lvl['magic_crystals']) + len(lvl['heart_containers']) + len(lvl['pickups'])
        + len(lvl['health_pickups'])
        + sum(p[2] for p in lvl['loose_platforms'])
        + sum(p[2] for p in lvl['hidden_platforms'])
        + SPRITE_RESERVE
    )
    if total > SPRITE_BUDGET_CAP:
        return [f"{room_name}: sprite budget estimate {total} > {SPRITE_BUDGET_CAP}"]
    return []


def check_arena_constraints(room_name, lvl):
    if not lvl.get('boss'):
        return []
    errors = []
    w, h = lvl['w'], lvl['h']
    floor_y = h - 2

    def tile_at(x, y):
        return lvl['tiles'][y * w + x]

    for dx in (-1, 0, 1):
        x = w // 2 + dx
        if not (0 <= x < w) or tile_at(x, floor_y) != 1:
            errors.append(f"{room_name}: boss arena floor not solid at ({x},{floor_y})")
    # Zero crystals is legal (some bosses need no magic, or recharge via block-to-charge);
    # more than one is a bug — the boss fight loop only ever reads magic_crystals[0].
    if len(lvl['magic_crystals']) > 1:
        errors.append(
            f"{room_name}: boss arena has {len(lvl['magic_crystals'])} magic crystals "
            f"(only magic_crystals[0] is ever read by the boss fight loop; extras are dead content)")
    return errors


def validate_all(manifest_path, levels_dir, dungeons_h_path=None):
    """Compile every room named in the manifest and run all cross-room/cross-dungeon
    checks. Returns (errors, warnings): both lists of human-readable strings.
    `dungeons_h_path=None` skips rule 2 (used by tests that don't fabricate a header).
    """
    errors = []
    warnings = []
    manifest = load_manifest(manifest_path)

    room_cache = {}

    def get_room(name):
        if name not in room_cache:
            room_cache[name] = compile_room(name, levels_dir)
        return room_cache[name]

    all_rooms = []  # (room_name, compiled_level) across the whole game

    for dungeon_key, room_names in manifest.get('dungeons', {}).items():
        rooms = [get_room(n) for n in room_names]
        all_rooms.extend(zip(room_names, rooms))
        g_errors, g_warnings = check_room_graph(dungeon_key, room_names, rooms)
        errors.extend(g_errors)
        warnings.extend(g_warnings)
        for room_name, lvl in zip(room_names, rooms):
            errors.extend(check_sprite_budget(room_name, lvl))
            errors.extend(check_arena_constraints(room_name, lvl))

    for room_name in manifest.get('standalone', []):
        lvl = get_room(room_name)
        all_rooms.append((room_name, lvl))
        errors.extend(check_sprite_budget(room_name, lvl))
        errors.extend(check_arena_constraints(room_name, lvl))

    errors.extend(check_latch_registry(all_rooms))
    errors.extend(check_heart_registry(all_rooms))

    if dungeons_h_path is not None:
        if os.path.exists(dungeons_h_path):
            with open(dungeons_h_path, 'r', encoding='utf-8') as f:
                dungeons_h_text = f.read()
            errors.extend(check_dungeons_h_drift(manifest, dungeons_h_text))
        else:
            errors.append(f"dungeons.h not found at {dungeons_h_path}")

    return errors, warnings


def main():
    errors, warnings = validate_all(DEFAULT_MANIFEST, DEFAULT_LEVELS_DIR, DEFAULT_DUNGEONS_H)
    for w in warnings:
        print(f"WARNING: {w}", file=sys.stderr)
    if errors:
        for e in errors:
            print(f"ERROR: {e}", file=sys.stderr)
        print(f"validate_dungeons: {len(errors)} error(s), {len(warnings)} warning(s)", file=sys.stderr)
        sys.exit(1)
    print(f"validate_dungeons: OK ({len(warnings)} warning(s))")


if __name__ == '__main__':
    main()
