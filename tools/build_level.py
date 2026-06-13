#!/usr/bin/env python3
"""Compile an ASCII level (.txt) + JSON sidecar into a C++ header for Spronk Quest.

Usage: python tools/build_level.py <in.txt> <out.h>
  Reads <in.txt> and its sibling <in>.json. Emits <out.h> defining inline constexpr
  arrays + an `inline constexpr logic::LevelData <NAME>_DATA` (NAME = uppercased out
  basename), consumed by the engine level loader.

Grid symbols (collision tile in parens):
  #=solid(1)  .=empty(0)  ^=one-way(2)  ~=lava(3)  w=water(4)  u=updraft(6)  <=wind-left(7)  >=wind-right(8)  s=spikes(9)  g=grapple-anchor(10)
  @=spawn(one)  C=cage  E=exit  o=enemy  G=gate  1-8=dungeon door
  V=vine gate  I=ice gate  W=water gate  X=fire-wall gate  K=cracked-wall gate(Dash)
  F=ability shrine  B=pushable block  P=pullable block  ==pressure plate
  ?=hidden button  *=brazier
  N=entrance (JSON: {"id":N,"facing":±1}; id defaults to scan-order index, facing to +1)  D=room-door
  (all content symbols except ~ set collision tile 0; positions recorded as entities)

JSON sidecar (Nth metadata entry -> Nth matching symbol in row-major scan order):
  { "tileset":"tiles",
    "enemies":[{"patrol":[l,r], "fire_immune":false}...],   # 'o'
    "gates":[{"type":"gap"}...],                            # 'G' (V/I set their type directly)
    "pickups":[{"ability":"fire"}...],                      # 'F' (default fire)
    "plates":[{"target":[tx,ty]}...],                       # '='
    "buttons":[{"target":[tx,ty]}...],                      # '?'
    "braziers":[{"group":0}...],                            # '*' (default group 0)
    "brazier_groups":[{"total":3,"target":[tx,ty],"latch_id":-1}...],  # indexed by group id
    "entrances":[{"id":0,"facing":1}...],                   # 'N' (defaults: id by order, facing +1)
    "cracked_floors":[{"latch_id":1}...],                   # 'k' (scan-order; latch_id default -1 = not latched)
    "loose_platforms":[{"len":3}...],                       # ':' (scan-order; len default 1)
    "room_doors":[{"target_room":0,"target_entrance":0}...] }  # 'D' (must have target metadata)
"""
import json
import os
import sys

TILE = {'#': 1, '.': 0, '^': 2, '~': 3, 'w': 4,   # 'w' = water (M4 damaging hazard, Ice-freezable)
        'u': 6, '<': 7, '>': 8,                    # M5 wind: updraft, wind-left, wind-right (non-solid)
        's': 9,                                    # M6 spikes (non-solid damaging hazard; dash i-frames skip it)
        'g': 10}                                   # M7 grapple anchor (non-solid; GrapplePoint=10)
GATE_ENUM = {
    'gap': 'Gap', 'grapple_point': 'GrapplePoint', 'vine': 'Vine', 'ice': 'Ice',
    'water': 'Water', 'cracked_wall': 'CrackedWall', 'cracked_floor': 'CrackedFloor',
    'dark_veil': 'DarkVeil',
}
ABILITY_ENUM = {
    'featherleap': 'Featherleap', 'fire': 'Fire', 'ice': 'Ice', 'glide': 'Glide',
    'dash': 'Dash', 'grapple': 'Grapple', 'stone': 'Stone', 'light': 'Light',
}
CONTENT = set('@CEoG12345678VIWXFBPK=?*NDHkO:')  # 'W' Water gate, 'X' Fire-wall gate (M4); 'K' cracked-wall gate (M6, Dash); 'P' pullable block (M7); 'N' entrance, 'D' room-door; 'H' heart container; 'k' cracked-floor gate (M8, Stone); 'O' boulder (M8); ':' loose platform (M8)


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
    j_enemies = meta.get('enemies', [])
    j_gates = meta.get('gates', [])
    j_pickups = meta.get('pickups', [])
    j_plates = meta.get('plates', [])
    j_buttons = meta.get('buttons', [])
    j_braziers = meta.get('braziers', [])
    j_brazier_groups = meta.get('brazier_groups', [])
    j_entrances = meta.get('entrances', [])
    j_room_doors = meta.get('room_doors', [])
    j_heart_containers = meta.get('heart_containers', [])
    j_loose_platforms = meta.get('loose_platforms', [])
    j_cracked_floors = meta.get('cracked_floors', [])

    tiles = []
    spawns, cages, exits = [], [], []
    enemies = []        # (tx,ty,p0,p1,p2)
    gates = []          # (tx,ty,GateEnum,latch_id)  latch_id=-1 = not latched
    doors = []          # (tx,ty,dungeon)
    pickups = []        # (tx,ty,AbilityEnum)
    blocks = []         # (tx,ty,pullable)
    plates = []         # (tx,ty,target_tx,target_ty,heavy)
    buttons = []        # (tx,ty,target_tx,target_ty)
    braziers = []       # (tx,ty,group)
    entrances = []       # (id, tx, ty, facing)
    room_doors = []      # (tx, ty, target_room, target_entrance)
    heart_containers = [] # (tx, ty, id)
    boulders = []         # (tx, ty)
    loose_platforms = []  # (tx, ty, len)
    e_idx = g_idx = f_idx = pl_idx = b_idx = br_idx = n_idx = rd_idx = hc_idx = lp_idx = 0
    cf_idx = 0  # scan-order index for cracked floors ('k'), maps into j_cracked_floors

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
                je = j_enemies[e_idx] if e_idx < len(j_enemies) else {}
                patrol = je.get('patrol', [x - 2, x + 2])
                p2 = 1 if je.get('fire_immune') else 0
                enemies.append((x, y, patrol[0], patrol[1], p2))
                e_idx += 1
            elif c == 'G':
                if not j_gates:
                    raise LevelError(f"gate 'G' at ({x},{y}) but JSON has no 'gates' entry")
                entry = j_gates[g_idx] if g_idx < len(j_gates) else j_gates[-1]
                gtype = entry['type']
                if gtype not in GATE_ENUM:
                    raise LevelError(f"unknown gate type '{gtype}'")
                gates.append((x, y, GATE_ENUM[gtype], -1))
                g_idx += 1
            elif c == 'V':
                gates.append((x, y, 'Vine', -1))
            elif c == 'I':
                gates.append((x, y, 'Ice', -1))
            elif c == 'W':
                gates.append((x, y, 'Water', -1))     # M4 Water gate, cleared by the Ice spell
            elif c == 'X':
                gates.append((x, y, 'FireWall', -1))  # M4 fire-wall gate, Ice extinguishes it
            elif c == 'K':
                gates.append((x, y, 'CrackedWall', -1))  # M6 cracked-wall gate, Dash smashes it
            elif c == 'k':
                # M8 cracked-floor gate, Stone ground-pound smashes it. Optional latch_id (SRAM-persisted
                # shortcut) from JSON 'cracked_floors' (scan-order indexed); default -1 = not latched.
                jcf = j_cracked_floors[cf_idx] if cf_idx < len(j_cracked_floors) else {}
                cf_latch = jcf.get('latch_id', -1)
                gates.append((x, y, 'CrackedFloor', cf_latch))
                cf_idx += 1
            elif c == 'F':
                ab = (j_pickups[f_idx].get('ability', 'fire') if f_idx < len(j_pickups) else 'fire')
                if ab not in ABILITY_ENUM:
                    raise LevelError(f"unknown ability '{ab}' for shrine 'F'")
                pickups.append((x, y, ABILITY_ENUM[ab]))
                f_idx += 1
            elif c == 'B':
                blocks.append((x, y, False))
            elif c == 'P':
                blocks.append((x, y, True))
            elif c == '=':
                if pl_idx >= len(j_plates):
                    raise LevelError(f"plate '=' at ({x},{y}) but JSON has no 'plates' entry #{pl_idx}")
                jp = j_plates[pl_idx]
                t = jp['target']
                heavy = jp.get('heavy', False)
                plates.append((x, y, t[0], t[1], heavy))
                pl_idx += 1
            elif c == '?':
                if b_idx >= len(j_buttons):
                    raise LevelError(f"button '?' at ({x},{y}) but JSON has no 'buttons' entry #{b_idx}")
                t = j_buttons[b_idx]['target']
                buttons.append((x, y, t[0], t[1]))
                b_idx += 1
            elif c == '*':
                grp = (j_braziers[br_idx].get('group', 0) if br_idx < len(j_braziers) else 0)
                braziers.append((x, y, grp))
                br_idx += 1
            elif c == 'N':
                je = j_entrances[n_idx] if n_idx < len(j_entrances) else {}
                eid = je.get('id', n_idx)
                facing = je.get('facing', 1)
                entrances.append((eid, x, y, facing))
                n_idx += 1
            elif c == 'D':
                if rd_idx >= len(j_room_doors):
                    raise LevelError(f"room-door 'D' at ({x},{y}) but JSON has no 'room_doors' entry #{rd_idx}")
                t = j_room_doors[rd_idx]
                room_doors.append((x, y, t['target_room'], t['target_entrance']))
                rd_idx += 1
            elif c == 'H':
                je = j_heart_containers[hc_idx] if hc_idx < len(j_heart_containers) else {}
                hid = je.get('id', hc_idx)  # default: scan-order index
                heart_containers.append((x, y, hid))
                hc_idx += 1
            elif c == 'O':
                boulders.append((x, y))  # M8 rolling boulder obstacle
            elif c == ':':
                jlp = j_loose_platforms[lp_idx] if lp_idx < len(j_loose_platforms) else {}
                llen = jlp.get('len', 1)  # default len=1
                loose_platforms.append((x, y, llen))
                lp_idx += 1
            elif c in '12345678':
                doors.append((x, y, int(c)))

    if len(spawns) != 1:
        raise LevelError(f"need exactly one '@' spawn, found {len(spawns)}")

    brazier_groups = [(g['total'], g['target'][0], g['target'][1], g.get('latch_id', -1))
                      for g in j_brazier_groups]

    # Solid-border invariant (collision engine treats OOB as solid).
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
        'spawn': spawns[0], 'cage': cages[0] if cages else None, 'exit': exits[0] if exits else None,
        'enemies': enemies, 'gates': gates, 'doors': doors, 'pickups': pickups,
        'blocks': blocks, 'plates': plates, 'buttons': buttons,
        'braziers': braziers, 'brazier_groups': brazier_groups,
        'entrances': entrances, 'room_doors': room_doors,
        'heart_containers': heart_containers,
        'boulders': boulders, 'loose_platforms': loose_platforms,
    }


def emit_header(level, name):
    def arr(vals):
        return ', '.join(str(v) for v in vals)

    def emit_array(cpp_type, var, items, dummy):
        """Emit an inline constexpr array (1-element dummy when empty) + return its count."""
        if items:
            body = ', '.join(items)
            return (f'inline constexpr {cpp_type} {name}_{var}[] = {{ {body} }};', len(items))
        return (f'inline constexpr {cpp_type} {name}_{var}[] = {{ {dummy} }};', 0)

    L = ['#pragma once', '#include "logic/level_data.h"', '']
    L.append(f'inline constexpr unsigned char {name}_TILES[] = {{ {arr(level["tiles"])} }};')

    line, ecount = emit_array('logic::EntitySpawn', 'ENEMIES',
                              [f'{{{tx},{ty},{p0},{p1},{p2}}}' for (tx, ty, p0, p1, p2) in level['enemies']],
                              '{0,0,0,0,0}'); L.append(line)
    line, gcount = emit_array('logic::GateSpawn', 'GATES',
                              [f'{{{tx},{ty},logic::GateType::{gt},{lat}}}' for (tx, ty, gt, lat) in level['gates']],
                              '{0,0,logic::GateType::Gap,-1}'); L.append(line)
    line, dcount = emit_array('logic::DoorSpawn', 'DOORS',
                              [f'{{{tx},{ty},{d}}}' for (tx, ty, d) in level['doors']],
                              '{0,0,0}'); L.append(line)
    line, pcount = emit_array('logic::AbilityPickup', 'PICKUPS',
                              [f'{{{tx},{ty},logic::Ability::{ab}}}' for (tx, ty, ab) in level['pickups']],
                              '{0,0,logic::Ability::Fire}'); L.append(line)
    line, bcount = emit_array('logic::BlockSpawn', 'BLOCKS',
                              [f'{{{tx},{ty},{"true" if pull else "false"}}}' for (tx, ty, pull) in level['blocks']],
                              '{0,0,false}'); L.append(line)
    line, plcount = emit_array('logic::PlateSpawn', 'PLATES',
                               [f'{{{tx},{ty},{ttx},{tty},{"true" if heavy else "false"}}}' for (tx, ty, ttx, tty, heavy) in level['plates']],
                               '{0,0,0,0,false}'); L.append(line)
    line, btcount = emit_array('logic::ButtonSpawn', 'BUTTONS',
                               [f'{{{tx},{ty},{ttx},{tty}}}' for (tx, ty, ttx, tty) in level['buttons']],
                               '{0,0,0,0}'); L.append(line)
    line, brcount = emit_array('logic::BrazierSpawn', 'BRAZIERS',
                               [f'{{{tx},{ty},{g}}}' for (tx, ty, g) in level['braziers']],
                               '{0,0,0}'); L.append(line)
    line, bgcount = emit_array('logic::BrazierGroupSpawn', 'BRAZIER_GROUPS',
                               [f'{{{tot},{ttx},{tty},{lat}}}' for (tot, ttx, tty, lat) in level['brazier_groups']],
                               '{0,0,0,-1}'); L.append(line)
    line, encount = emit_array('logic::EntranceSpawn', 'ENTRANCES',
                               [f'{{{eid},{tx},{ty},{fac}}}' for (eid, tx, ty, fac) in level['entrances']],
                               '{0,0,0,1}'); L.append(line)
    line, rdcount = emit_array('logic::RoomDoorSpawn', 'ROOM_DOORS',
                               [f'{{{tx},{ty},{tr},{te}}}' for (tx, ty, tr, te) in level['room_doors']],
                               '{0,0,0,0}'); L.append(line)
    line, hccount = emit_array('logic::HeartContainerSpawn', 'HEART_CONTAINERS',
                               [f'{{{tx},{ty},{hid}}}' for (tx, ty, hid) in level['heart_containers']],
                               '{0,0,0}'); L.append(line)
    line, bocount = emit_array('logic::BoulderSpawn', 'BOULDERS',
                               [f'{{{tx},{ty}}}' for (tx, ty) in level['boulders']],
                               '{0,0}'); L.append(line)
    line, lpcount = emit_array('logic::LoosePlatformSpawn', 'LOOSE_PLATFORMS',
                               [f'{{{tx},{ty},{llen}}}' for (tx, ty, llen) in level['loose_platforms']],
                               '{0,0,1}'); L.append(line)

    sx, sy = level['spawn']
    cx, cy = level['cage'] if level['cage'] else (0, 0)
    ex, ey = level['exit'] if level['exit'] else (0, 0)
    L.append(
        f'inline constexpr logic::LevelData {name}_DATA = {{ '
        f'{name}_TILES, {level["w"]}, {level["h"]}, {sx}, {sy}, '
        f'{ "true" if level["cage"] else "false" }, {cx}, {cy}, '
        f'{ "true" if level["exit"] else "false" }, {ex}, {ey}, '
        f'{name}_ENEMIES, {ecount}, {name}_GATES, {gcount}, {name}_DOORS, {dcount}, '
        f'{name}_PICKUPS, {pcount}, {name}_BLOCKS, {bcount}, {name}_PLATES, {plcount}, '
        f'{name}_BUTTONS, {btcount}, {name}_BRAZIERS, {brcount}, '
        f'{name}_BRAZIER_GROUPS, {bgcount}, '
        f'{name}_ENTRANCES, {encount}, {name}_ROOM_DOORS, {rdcount}, '
        f'{name}_HEART_CONTAINERS, {hccount}, '
        f'{name}_BOULDERS, {bocount}, {name}_LOOSE_PLATFORMS, {lpcount} }};'
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
          f"{len(level['enemies'])} enemies, {len(level['gates'])} gates, {len(level['doors'])} doors, "
          f"{len(level['pickups'])} pickups, {len(level['blocks'])} blocks, {len(level['plates'])} plates, "
          f"{len(level['buttons'])} buttons, {len(level['braziers'])} braziers)")


if __name__ == '__main__':
    main()
