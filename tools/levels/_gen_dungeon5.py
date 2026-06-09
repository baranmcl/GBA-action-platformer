#!/usr/bin/env python3
"""One-shot generator for dungeon5.txt (Sunken Ruins, M6).

Builds the grid programmatically so the solid border, the Featherleap-climb gap
spacing, the Glide updraft idiom, and the dash-beat hazards are placed at exact
coords. Floor = row h-2 (player walks on it, hazards carved in); row h-1 = border
(catches the player in pits). Content (entities/gates) on row h-4. Run once:
    python tools/levels/_gen_dungeon5.py
"""
import pathlib

H, W = 24, 64
FLOOR = H - 2          # 22 — the walking floor (solid, hazards carved in)
BASE  = H - 4          # 20 — content/entity row (falls/rests on the floor)
g = [['.' for _ in range(W)] for _ in range(H)]

def border():
    for x in range(W):
        g[0][x] = '#'; g[H-1][x] = '#'
    for y in range(H):
        g[y][0] = '#'; g[y][W-1] = '#'

def block(x0, y0, x1, y1, ch='#'):
    for y in range(y0, y1+1):
        for x in range(x0, x1+1):
            g[y][x] = ch

def row(y, x0, x1, ch):
    for x in range(x0, x1+1):
        g[y][x] = ch

def put(x, ch):           # place an entity/gate symbol on the content row
    g[BASE][x] = ch

border()
row(FLOOR, 1, W-2, '#')   # solid walking floor (carve hazards/updraft after)

# --- Station A: spawn + fire-magic enemy ---
put(2, '@')
put(6, 'o')               # magic source for the Fire (vine) beat

# --- Station B: Vine gate (Fire) — full wall at cols 9-10 ---
put(9, 'V')

# --- Station C: Featherleap climb (forced double-jump over a barrier) ---
# barrier blocks the floor at cols 19-20; one-way ledge p1 5 tiles up; climb p1 -> barrier top.
block(19, 13, 20, FLOOR)          # barrier cols19-20 rows13-22 (top surface row13)
row(17, 13, 18, '^')              # p1 one-way ledge (floor->p1 ~5 tiles; p1->top ~4)
# drop column 21 stays open above the floor (player drops back to the floor)

# --- Station D: ice-magic enemy ---
put(24, 'o')                      # magic source for the Ice (water) beat

# --- Station E: Water gate (Ice) — full wall at cols 27-28 ---
put(27, 'W')

# --- Station F: Glide updraft (D4 idiom: ride up THROUGH a cap, fall back onto it) ---
block(31, 13, 32, FLOOR, 'u')     # updraft cols31-32 rows13-22 (carves the floor base)
row(12, 30, 37, '^')              # one-way cap above the updraft (land on top after the pop-up)
block(35, 13, 36, FLOOR)          # barrier cols35-36 blocks the floor (must go up-and-over)
# drop column 37 keeps its solid floor (row22) so the player lands back on the floor

# --- Station G: Dash shrine (MANDATORY, before any dash-only obstacle) ---
put(39, 'F')                      # ability: dash (set in dungeon5.json)

# --- Station H: spike corridor (dash i-frames blink through) ---
row(FLOOR, 41, 43, 's')           # 3-wide spikes (<= one dash)

# --- Station I: cracked wall (dash smashes it) — full wall cols 46-47 ---
put(46, 'K')                      # runway from col44-45; point-blank dash smashes it

# --- Station J: air-dash + glide gap (combo beat) — 6-wide lava, too wide to jump ---
row(FLOOR, 50, 55, '~')           # jump + air-dash (~5) + glide to reach the far ledge (col56)

# --- Station K: spronk + exit ---
put(59, 'C')
put(60, 'E')

# --- validate solid border before writing ---
for x in range(W):
    assert g[0][x] == '#' and g[H-1][x] == '#', f"top/bottom border hole at col {x}"
for y in range(H):
    assert g[y][0] == '#' and g[y][W-1] == '#', f"left/right border hole at row {y}"

out = pathlib.Path(__file__).resolve().parent / 'dungeon5.txt'
out.write_text('\n'.join(''.join(r) for r in g) + '\n', encoding='utf-8')
print(f"wrote {out} ({W}x{H})")
