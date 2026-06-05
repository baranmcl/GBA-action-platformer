#!/usr/bin/env python3
"""Compile an ASCII level (.txt) + JSON sidecar into a C++ header for Spronk Quest.

Usage: python tools/build_level.py <in.txt> <out.h>
  Reads <in.txt> and its sibling <in>.json. Emits <out.h> defining inline constexpr
  arrays + an `inline constexpr logic::LevelData <NAME>_DATA` (NAME = uppercased out
  basename), consumed by the engine level loader.

Grid symbols:
  #=solid(1)  .=empty(0)  ^=one-way(2)   (these set the collision tile)
  @=spawn(one)  C=cage  E=exit  o=enemy  G=gate  1-8=dungeon door
  (all content symbols set collision tile 0; their positions are recorded as entities)

JSON sidecar: { "tileset":"tiles",
                "enemies":[{"patrol":[l,r]}...],   # Nth entry -> Nth 'o' in row-major order
                "gates":[{"type":"gap"}...] }       # Nth entry -> Nth 'G'
"""
import json
import os
import sys

TILE = {'#': 1, '.': 0, '^': 2}
GATE_ENUM = {
    'gap': 'Gap', 'grapple_point': 'GrapplePoint', 'vine': 'Vine', 'ice': 'Ice',
    'water': 'Water', 'cracked_wall': 'CrackedWall', 'cracked_floor': 'CrackedFloor',
    'dark_veil': 'DarkVeil',
}
CONTENT = set('@CEoG12345678')


class LevelError(Exception):
    pass


def compile_level(txt_path, json_path):
    with open(txt_path, 'r', encoding='utf-8') as f:
        rows = [line.rstrip('\n').rstrip('\r') for line in f]
    rows = [r for r in rows if r != '']  # ignore blank lines
    if not rows:
        raise LevelError("empty level")
    w = len(rows[0])
    h = len(rows)
    for y, r in enumerate(rows):
        if len(r) != w:
            raise LevelError(f"row {y} width {len(r)} != {w} (level must be rectangular)")

    meta = {}
    if os.path.exists(json_path):
        with open(json_path, 'r', encoding='utf-8') as f:
            meta = json.load(f)
    json_enemies = meta.get('enemies', [])
    json_gates = meta.get('gates', [])

    tiles = []
    spawns = []
    cages = []
    exits = []
    enemies = []   # (tx,ty,param0,param1)
    gates = []     # (tx,ty,GateEnum)
    doors = []     # (tx,ty,dungeon)
    e_idx = 0
    g_idx = 0
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c in TILE:
                tiles.append(TILE[c])
                continue
            if c not in CONTENT:
                raise LevelError(f"unknown symbol '{c}' at ({x},{y})")
            tiles.append(0)  # content symbols sit on an empty collision tile
            if c == '@':
                spawns.append((x, y))
            elif c == 'C':
                cages.append((x, y))
            elif c == 'E':
                exits.append((x, y))
            elif c == 'o':
                patrol = json_enemies[e_idx].get('patrol', [x - 2, x + 2]) if e_idx < len(json_enemies) else [x - 2, x + 2]
                enemies.append((x, y, patrol[0], patrol[1]))
                e_idx += 1
            elif c == 'G':
                if not json_gates:
                    raise LevelError(f"gate 'G' at ({x},{y}) but JSON has no 'gates' entry")
                # Nth 'G' -> Nth JSON entry; extra 'G's (e.g. a wall of one gate type) clamp to last.
                entry = json_gates[g_idx] if g_idx < len(json_gates) else json_gates[-1]
                gtype = entry['type']
                if gtype not in GATE_ENUM:
                    raise LevelError(f"unknown gate type '{gtype}'")
                gates.append((x, y, GATE_ENUM[gtype]))
                g_idx += 1
            elif c in '12345678':
                doors.append((x, y, int(c)))

    if len(spawns) != 1:
        raise LevelError(f"need exactly one '@' spawn, found {len(spawns)}")

    # Solid-border invariant (matches the collision engine's OOB-as-solid expectation).
    def tile_at(x, y):
        return tiles[y * w + x]
    for x in range(w):
        if tile_at(x, 0) != 1 or tile_at(x, h - 1) != 1:
            raise LevelError(f"top/bottom border not solid at column {x}")
    for y in range(h):
        if tile_at(0, y) != 1 or tile_at(w - 1, y) != 1:
            raise LevelError(f"left/right border not solid at row {y}")

    return {
        'w': w, 'h': h, 'tiles': tiles,
        'spawn': spawns[0],
        'cage': cages[0] if cages else None,
        'exit': exits[0] if exits else None,
        'enemies': enemies, 'gates': gates, 'doors': doors,
    }


def emit_header(level, name):
    def arr(vals):
        return ', '.join(str(v) for v in vals)
    L = []
    L.append('#pragma once')
    L.append('#include "logic/level_data.h"')
    L.append('')
    L.append(f'inline constexpr unsigned char {name}_TILES[] = {{ {arr(level["tiles"])} }};')
    # enemies
    if level['enemies']:
        items = ', '.join(f'{{{tx},{ty},{p0},{p1}}}' for (tx, ty, p0, p1) in level['enemies'])
        L.append(f'inline constexpr logic::EntitySpawn {name}_ENEMIES[] = {{ {items} }};')
        ecount = len(level['enemies'])
    else:
        L.append(f'inline constexpr logic::EntitySpawn {name}_ENEMIES[] = {{ {{0,0,0,0}} }};')
        ecount = 0
    # gates
    if level['gates']:
        items = ', '.join(f'{{{tx},{ty},logic::GateType::{gt}}}' for (tx, ty, gt) in level['gates'])
        L.append(f'inline constexpr logic::GateSpawn {name}_GATES[] = {{ {items} }};')
        gcount = len(level['gates'])
    else:
        L.append(f'inline constexpr logic::GateSpawn {name}_GATES[] = {{ {{0,0,logic::GateType::Gap}} }};')
        gcount = 0
    # doors
    if level['doors']:
        items = ', '.join(f'{{{tx},{ty},{d}}}' for (tx, ty, d) in level['doors'])
        L.append(f'inline constexpr logic::DoorSpawn {name}_DOORS[] = {{ {items} }};')
        dcount = len(level['doors'])
    else:
        L.append(f'inline constexpr logic::DoorSpawn {name}_DOORS[] = {{ {{0,0,0}} }};')
        dcount = 0
    # LevelData (field order MUST match logic::LevelData)
    sx, sy = level['spawn']
    has_cage = 1 if level['cage'] else 0
    cx, cy = level['cage'] if level['cage'] else (0, 0)
    has_exit = 1 if level['exit'] else 0
    ex, ey = level['exit'] if level['exit'] else (0, 0)
    L.append(
        f'inline constexpr logic::LevelData {name}_DATA = {{ '
        f'{name}_TILES, {level["w"]}, {level["h"]}, '
        f'{sx}, {sy}, '
        f'{ "true" if has_cage else "false" }, {cx}, {cy}, '
        f'{ "true" if has_exit else "false" }, {ex}, {ey}, '
        f'{name}_ENEMIES, {ecount}, '
        f'{name}_GATES, {gcount}, '
        f'{name}_DOORS, {dcount} }};'
    )
    L.append('')
    return '\n'.join(L)


def main():
    if len(sys.argv) != 3:
        print("usage: build_level.py <in.txt> <out.h>", file=sys.stderr)
        sys.exit(2)
    in_txt, out_h = sys.argv[1], sys.argv[2]
    json_path = os.path.splitext(in_txt)[0] + '.json'
    name = os.path.splitext(os.path.basename(out_h))[0].upper()
    level = compile_level(in_txt, json_path)
    os.makedirs(os.path.dirname(out_h) or '.', exist_ok=True)
    with open(out_h, 'w', encoding='utf-8') as f:
        f.write(emit_header(level, name) + '\n')
    print(f"compiled {in_txt} -> {out_h} ({level['w']}x{level['h']}, "
          f"{len(level['enemies'])} enemies, {len(level['gates'])} gates, {len(level['doors'])} doors)")


if __name__ == '__main__':
    main()
