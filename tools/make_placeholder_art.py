#!/usr/bin/env python3
"""Generate GBA-valid placeholder sprite art for Spronk Quest (Milestone 1).

Butano/grit expects indexed ("P" mode) BMPs with <=16 colors (4bpp). For
multi-frame sprites the frames are stacked VERTICALLY: the BMP is
(frame_w) x (n_frames * frame_h), and the .json sets "height" = frame_h.
Palette index 0 is the transparent color.

Outputs into graphics/: laurel (4 frames 16x32), enemy (16x16), spronk (16x16),
bolt (8x8), each with a Butano .json. Real art is a drop-in replacement later.
"""
import json
import pathlib
from PIL import Image

GFX = pathlib.Path(__file__).resolve().parent.parent / "graphics"
GFX.mkdir(exist_ok=True)

# Shared 16-colour palette. Index 0 = transparent (magenta, never drawn).
PAL = [
    (255, 0, 255),    # 0 transparent
    (20, 18, 30),     # 1 outline / near-black
    (232, 93, 138),   # 2 Laurel rose (tunic)
    (250, 205, 170),  # 3 skin
    (106, 170, 60),   # 4 green (sprig)
    (60, 90, 45),     # 5 dark green
    (245, 225, 120),  # 6 spronk gold
    (255, 150, 200),  # 7 rose light
    (130, 225, 255),  # 8 bolt cyan
    (245, 245, 250),  # 9 white (wand)
    (150, 120, 90),   # 10 brown
    (90, 70, 50),     # 11 dark brown
    (110, 90, 140),   # 12 stone
    (200, 60, 90),    # 13 red
    (70, 60, 100),    # 14 shadow
    (255, 255, 255),  # 15 pure white
]

def new_img(w, h):
    im = Image.new("P", (w, h), 0)
    flat = []
    for r, g, b in PAL:          # exactly 16 entries -> a true 4bpp/16-colour palette
        flat += [r, g, b]
    im.putpalette(flat)
    return im

def px(im, x, y, idx):
    if 0 <= x < im.width and 0 <= y < im.height:
        im.putpixel((x, y), idx)

def rect(im, x0, y0, x1, y1, idx):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            px(im, x, y, idx)

def write(im, name, spec):
    im.save(GFX / f"{name}.bmp")
    (GFX / f"{name}.json").write_text(json.dumps(spec, indent=4) + "\n")
    print(f"wrote graphics/{name}.bmp {im.size} + .json")

def draw_laurel_frame(im, oy, frame):
    """One 16x32 Laurel frame at vertical offset oy. frame: 0 idle,1 run,2 jump,3 cast."""
    # head: skin block with green sprig on top
    rect(im, 5, oy + 2, 10, oy + 7, 3)        # face
    rect(im, 5, oy + 0, 10, oy + 1, 4)        # green sprig
    px(im, 7, oy + 1, 4); px(im, 8, oy - 0 + 0, 4)
    px(im, 6, oy + 4, 1); px(im, 9, oy + 4, 1)  # eyes
    # tunic (rose)
    rect(im, 4, oy + 8, 11, oy + 21, 2)
    rect(im, 5, oy + 9, 10, oy + 12, 7)        # lighter chest
    # legs
    if frame == 1:                              # run: split stance
        rect(im, 4, oy + 22, 6, oy + 30, 1)
        rect(im, 9, oy + 22, 11, oy + 28, 1)
    else:
        rect(im, 5, oy + 22, 7, oy + 30, 1)
        rect(im, 8, oy + 22, 10, oy + 30, 1)
    # wand (white) in right hand, with a spark that grows when casting/jumping
    wand_x = 12
    rect(im, wand_x, oy + 9, wand_x, oy + 16, 9)
    spark = {0: 0, 1: 0, 2: 1, 3: 2}[frame]
    if spark:
        rect(im, wand_x - spark, oy + 8 - spark, wand_x + spark, oy + 8 + spark, 8)
    if frame == 2:                              # jump: arms up cue
        px(im, 3, oy + 9, 2); px(im, 12, oy + 8, 2)

def gen_laurel():
    im = new_img(16, 32 * 4)
    for f in range(4):
        draw_laurel_frame(im, f * 32, f)
    write(im, "laurel", {"type": "sprite", "height": 32})

def gen_enemy():
    im = new_img(16, 16)
    rect(im, 3, 5, 12, 14, 4)      # green body
    rect(im, 4, 11, 11, 14, 5)     # darker base
    px(im, 6, 8, 1); px(im, 9, 8, 1)   # eyes
    rect(im, 5, 4, 10, 4, 5)       # brow
    write(im, "enemy", {"type": "sprite"})

def gen_spronk():
    im = new_img(16, 16)
    # little gold star-creature
    rect(im, 6, 2, 9, 13, 6)
    rect(im, 2, 6, 13, 9, 6)
    px(im, 4, 4, 6); px(im, 11, 4, 6); px(im, 4, 11, 6); px(im, 11, 11, 6)
    px(im, 6, 7, 1); px(im, 9, 7, 1)   # eyes
    write(im, "spronk", {"type": "sprite"})

def gen_bolt():
    im = new_img(8, 8)
    rect(im, 2, 2, 5, 5, 8)
    rect(im, 3, 3, 4, 4, 15)
    px(im, 1, 3, 8); px(im, 6, 4, 8); px(im, 3, 1, 8); px(im, 4, 6, 8)
    write(im, "bolt", {"type": "sprite"})

def gen_tiles():
    """Background tileset: 26 tiles of 8x8 in a horizontal strip. Index order:
    0 blank, 1 ground, 2 one-way, 3 gate(closed wall), 4 cage, 5 door-open, 6 door-locked,
    7 vine, 8 ice-gate, 9 water-gate(waterfall, M4), 10 fire-wall-gate(M4), 11 cracked-floor(M8 Stone),
    12 dark-veil barrier(M8 Light gate), 13 lava,
    14 brazier-unlit, 15 brazier-lit, 16 water(M4), 17 plate, 18 button, 19 ice-platform(M4),
    20 updraft(M5), 21 wind-left(M5), 22 wind-right(M5), 23 cracked-wall(M6), 24 spikes(M6),
    25 grapple-anchor(M7), 26 exit-to-hub portal door (target_room=-1; distinct from tiles 5/6).
    (block + shrine are SPRITES, not bg tiles.)"""
    im = new_img(8 * 27, 8)
    # tile 0: blank -> all index 0 (transparent, shows backdrop). nothing to draw.
    # tile 1: ground (brown with grass top, dark bottom)
    ox = 8 * 1
    rect(im, ox, 0, ox + 7, 7, 10)        # brown body
    rect(im, ox, 0, ox + 7, 0, 4)         # grass top
    rect(im, ox, 7, ox + 7, 7, 11)        # dark bottom
    px(im, ox + 2, 3, 11); px(im, ox + 5, 5, 11)  # speckle
    # tile 2: one-way platform (thin top ledge, transparent below)
    ox = 8 * 2
    rect(im, ox, 0, ox + 7, 0, 4)         # grass edge
    rect(im, ox, 1, ox + 7, 2, 10)        # brown lip
    # tile 3: gate (gold vertical bars on transparent)
    ox = 8 * 3
    for cx in (1, 3, 5):
        rect(im, ox + cx, 0, ox + cx, 7, 6)
    rect(im, ox, 0, ox + 7, 0, 6)         # top rail
    # tile 4: cage (stone box outline)
    ox = 8 * 4
    rect(im, ox, 0, ox + 7, 0, 12); rect(im, ox, 7, ox + 7, 7, 12)
    rect(im, ox, 0, ox, 7, 12); rect(im, ox + 7, 0, ox + 7, 7, 12)
    # tile 5: door-open SEGMENT (dark opening + side jambs; tiles 2-wide x N-tall into a doorway)
    ox = 8 * 5
    rect(im, ox, 0, ox + 7, 7, 1)            # dark opening fill
    rect(im, ox, 0, ox, 7, 12); rect(im, ox + 7, 0, ox + 7, 7, 12)  # stone jambs (left/right)
    # tile 6: door-locked SEGMENT (dark + jambs + central gold bar -> barred look when tiled)
    ox = 8 * 6
    rect(im, ox, 0, ox + 7, 7, 1)
    rect(im, ox, 0, ox, 7, 12); rect(im, ox + 7, 0, ox + 7, 7, 12)
    rect(im, ox + 3, 0, ox + 4, 7, 6)        # gold bar
    # tile 7: vine gate (green tangle)
    ox = 8 * 7
    rect(im, ox, 0, ox + 7, 7, 5)            # dark green fill
    for yy in range(0, 8, 2):
        rect(im, ox, yy, ox + 7, yy, 4)      # lighter green strands
    px(im, ox + 2, 1, 4); px(im, ox + 5, 4, 4)
    # tile 8: ice gate (pale blue block)
    ox = 8 * 8
    rect(im, ox, 0, ox + 7, 7, 8)            # cyan fill
    rect(im, ox, 0, ox + 7, 0, 9); rect(im, ox + 1, 1, ox + 2, 2, 9)  # white glints
    # tile 11: cracked FLOOR (M8 Stone) — a brown ground slab (like tile 1) but with a dark fracture
    # line + chips so it READS as "pound here to break through" and is distinct from solid ground.
    ox = 8 * 11
    rect(im, ox, 0, ox + 7, 7, 10)        # brown ground body (matches tile 1)
    rect(im, ox, 0, ox + 7, 0, 11)        # dark top mortar (no grass -> reads as an interior floor)
    rect(im, ox, 7, ox + 7, 7, 11)        # dark bottom
    # jagged fracture running across the slab (near-black), with a couple of loosened chips
    px(im, ox + 1, 2, 1); px(im, ox + 2, 3, 1); px(im, ox + 3, 2, 1); px(im, ox + 4, 4, 1)
    px(im, ox + 5, 3, 1); px(im, ox + 6, 5, 1)
    px(im, ox + 2, 5, 14); px(im, ox + 4, 6, 14); px(im, ox + 5, 1, 14)  # chip shadows
    # tile 12: dark-veil barrier (M8 Light gate) — a shadowy purple-black curtain with a faint
    # rune sheen, so the closed Light gate READS as a distinct dark veil (not solid stone, not flame).
    ox = 8 * 12
    rect(im, ox, 0, ox + 7, 7, 14)        # dark shadow body (pal 14, near-black blue)
    rect(im, ox, 0, ox + 7, 0, 1)         # near-black top edge
    rect(im, ox, 7, ox + 7, 7, 1)         # near-black bottom edge
    # vertical veil folds (stone-purple pal 12) to suggest a hanging dark curtain
    for cx in (1, 4, 6):
        rect(im, ox + cx, 1, ox + cx, 6, 12)
    # a faint rune sheen (shadow pal 14 highlights + one white-ish glint) so it shimmers
    px(im, ox + 2, 2, 9); px(im, ox + 5, 4, 9)   # pale glints
    px(im, ox + 3, 5, 12); px(im, ox + 2, 4, 12)  # mid folds
    # tile 13: lava (red with orange/gold bubbles)
    ox = 8 * 13
    rect(im, ox, 0, ox + 7, 7, 13)          # red
    rect(im, ox, 0, ox + 7, 0, 6)           # gold crust line
    px(im, ox + 2, 4, 6); px(im, ox + 5, 5, 6); px(im, ox + 4, 2, 6)  # bubbles
    # tile 14: brazier unlit — tall GREY STONE bowl on a thin dark post (cold/ashen).
    # Deliberately grey + bowl-shaped so it never reads as the brown wooden crate.
    ox = 8 * 14
    rect(im, ox + 3, 4, ox + 4, 7, 1)       # thin dark post
    rect(im, ox + 1, 2, ox + 6, 3, 12)      # wide stone bowl
    rect(im, ox + 1, 2, ox + 6, 2, 9)       # pale stone rim
    px(im, ox + 3, 3, 14); px(im, ox + 4, 3, 14)  # cold ash inside
    # tile 15: brazier lit — same bowl + a tall bright flame
    ox = 8 * 15
    rect(im, ox + 3, 4, ox + 4, 7, 1)       # post
    rect(im, ox + 1, 2, ox + 6, 3, 12)      # bowl
    rect(im, ox + 1, 2, ox + 6, 2, 9)       # rim
    rect(im, ox + 2, 0, ox + 5, 2, 13)      # red flame body
    rect(im, ox + 3, 1, ox + 4, 2, 6)       # gold inner flame
    px(im, ox + 3, 0, 9)                    # white-hot tip
    # tile 17: pressure plate (recessed stone on the floor)
    ox = 8 * 17
    rect(im, ox, 6, ox + 7, 7, 11)          # floor base
    rect(im, ox + 1, 4, ox + 6, 5, 12)      # plate
    # tile 18: hidden button — a round RED switch (distinct from grey braziers/brown crate)
    ox = 8 * 18
    rect(im, ox, 6, ox + 7, 7, 11)          # floor base
    rect(im, ox + 2, 3, ox + 5, 5, 13)      # red dome
    rect(im, ox + 2, 3, ox + 5, 3, 6)       # gold rim
    px(im, ox + 3, 4, 7)                    # highlight
    # tile 9: Water gate — vertical waterfall (cyan streaks on a blue wall), cleared by Ice
    ox = 8 * 9
    rect(im, ox, 0, ox + 7, 7, 8)           # cyan fill
    rect(im, ox + 1, 0, ox + 1, 7, 9); rect(im, ox + 4, 0, ox + 4, 7, 9)  # white streaks
    rect(im, ox + 6, 0, ox + 6, 7, 9)
    # tile 10: fire-wall gate — a wall of flames (red body, gold tongues), Ice extinguishes it
    ox = 8 * 10
    rect(im, ox, 0, ox + 7, 7, 13)          # red fill
    rect(im, ox + 1, 1, ox + 2, 5, 6); rect(im, ox + 5, 0, ox + 6, 4, 6)  # gold flame tongues
    px(im, ox + 3, 2, 9); px(im, ox + 4, 5, 9)  # white-hot flecks
    # tile 16: water — damaging hazard pool (cyan with white glints), Ice freezes it
    ox = 8 * 16
    rect(im, ox, 0, ox + 7, 7, 8)           # cyan body
    rect(im, ox, 0, ox + 7, 0, 9)           # white surface line
    px(im, ox + 2, 3, 9); px(im, ox + 5, 5, 9)  # glints
    # tile 19: ice platform — frozen white/cyan slab (distinct from the ice GATE tile 8)
    ox = 8 * 19
    rect(im, ox, 0, ox + 7, 7, 9)           # white slab
    rect(im, ox, 6, ox + 7, 7, 8)           # cyan underside
    px(im, ox + 2, 2, 8); px(im, ox + 5, 3, 8)  # cyan cracks
    # tile 20: updraft — upward white chevrons on a faint cyan column (M5 wind)
    ox = 8 * 20
    rect(im, ox + 3, 0, ox + 4, 7, 8)       # faint cyan column
    px(im, ox+3,1,9); px(im, ox+4,1,9); px(im, ox+2,2,9); px(im, ox+5,2,9)  # chevron ^
    px(im, ox+3,4,9); px(im, ox+4,4,9); px(im, ox+2,5,9); px(im, ox+5,5,9)  # chevron ^
    # tile 21: wind-left — leftward white streaks (tips at the LEFT)
    ox = 8 * 21
    rect(im, ox, 1, ox + 5, 1, 9); rect(im, ox, 4, ox + 5, 4, 9); rect(im, ox, 6, ox + 4, 6, 9)
    px(im, ox, 0, 9); px(im, ox, 3, 9); px(im, ox, 5, 9)
    # tile 22: wind-right — rightward white streaks (tips at the RIGHT)
    ox = 8 * 22
    rect(im, ox + 2, 1, ox + 7, 1, 9); rect(im, ox + 2, 4, ox + 7, 4, 9); rect(im, ox + 3, 6, ox + 7, 6, 9)
    px(im, ox + 7, 0, 9); px(im, ox + 7, 3, 9); px(im, ox + 7, 5, 9)
    # tile 23: cracked wall (M6) — grey stone block with dark zig-zag crack lines (Dash smashes it)
    ox = 8 * 23
    rect(im, ox, 0, ox + 7, 7, 12)          # grey stone fill
    rect(im, ox, 0, ox + 7, 0, 1)           # dark top mortar line
    # jagged crack down the middle
    px(im, ox + 3, 1, 1); px(im, ox + 4, 2, 1); px(im, ox + 3, 3, 1); px(im, ox + 2, 4, 1)
    px(im, ox + 3, 5, 1); px(im, ox + 4, 6, 1); px(im, ox + 4, 7, 1)
    px(im, ox + 1, 2, 14); px(im, ox + 6, 5, 14)  # shadow speckle
    # tile 24: spikes (M6) — dark base with upward red/grey triangular spikes (damaging hazard)
    ox = 8 * 24
    rect(im, ox, 6, ox + 7, 7, 1)           # dark base
    for bx in (0, 3, 6):                     # three triangle spikes
        px(im, ox + bx + 1, 5, 12); rect(im, ox + bx + 1, 4, ox + bx + 1, 5, 12)
        px(im, ox + bx, 5, 13); px(im, ox + bx + 1, 1, 15)   # red flank + bright tip
    # tile 25: grapple anchor (M7) — green ring with crosshair (distinct from any flame tile)
    ox = 8 * 25
    # outer ring (dark green outline), inner ring (bright green), crosshair (white)
    for cx in range(1, 7):                   # top/bottom ring edges
        px(im, ox + cx, 1, 5); px(im, ox + cx, 6, 5)
    for cy in range(1, 7):                   # left/right ring edges
        px(im, ox + 1, cy, 5); px(im, ox + 6, cy, 5)
    for cx in range(2, 6):                   # inner fill (bright green)
        for cy in range(2, 6):
            px(im, ox + cx, cy, 4)
    # crosshair in white through the centre
    for cx in range(0, 8):
        px(im, ox + cx, 3, 9); px(im, ox + cx, 4, 9)
    for cy in range(0, 8):
        px(im, ox + 3, cy, 9); px(im, ox + 4, cy, 9)
    # tile 26: exit-to-hub portal door SEGMENT — a glowing cyan/white portal inside stone jambs.
    # Distinct from tile 5 (dark room-door opening) and tile 6 (gold-barred dungeon goal): here the
    # opening GLOWS cyan with a white swirl, reading as a magical "way out / hub portal" when tiled
    # 2-wide x 4-tall into an archway.
    ox = 8 * 26
    rect(im, ox, 0, ox + 7, 7, 8)               # cyan portal glow fill (pal 8 bolt-cyan)
    rect(im, ox, 0, ox, 7, 12); rect(im, ox + 7, 0, ox + 7, 7, 12)  # stone jambs (left/right)
    # white swirl/sheen down the centre so it shimmers like an active portal
    px(im, ox + 3, 1, 9); px(im, ox + 4, 2, 9); px(im, ox + 3, 3, 9)
    px(im, ox + 4, 4, 9); px(im, ox + 3, 5, 9); px(im, ox + 4, 6, 9)
    px(im, ox + 2, 2, 15); px(im, ox + 5, 5, 15)  # bright white glints
    write(im, "tiles", {"type": "regular_bg_tiles", "bpp_mode": "bpp_4"})

def gen_ember_sprites():
    """M3 sprites: fire projectile, fire enemy, pushable block, ability shrine."""
    # fire projectile 8x8 (orange/red orb)
    fp = new_img(8, 8)
    rect(fp, 2, 2, 5, 5, 6)                  # gold core
    rect(fp, 3, 3, 4, 4, 13)                 # red center
    px(fp, 1, 4, 6); px(fp, 6, 3, 6); px(fp, 4, 1, 6); px(fp, 3, 6, 6)
    write(fp, "fire_proj", {"type": "sprite"})
    # ice projectile 8x8 (cyan/white orb) — M4, the Ice spell's shot
    ip = new_img(8, 8)
    rect(ip, 2, 2, 5, 5, 8)                  # cyan core
    rect(ip, 3, 3, 4, 4, 9)                  # white center
    px(ip, 1, 4, 8); px(ip, 6, 3, 8); px(ip, 4, 1, 8); px(ip, 3, 6, 8)
    write(ip, "ice_proj", {"type": "sprite"})
    # fire enemy 16x16 (red variant of the green enemy)
    fe = new_img(16, 16)
    rect(fe, 3, 5, 12, 14, 13)               # red body
    rect(fe, 4, 11, 11, 14, 1)               # dark base
    px(fe, 6, 8, 9); px(fe, 9, 8, 9)         # white eyes
    rect(fe, 5, 4, 10, 4, 6)                 # gold brow
    write(fe, "fire_enemy", {"type": "sprite"})
    # pushable block 8x8 — wooden crate with diagonal X-bracing, so it reads as a
    # pushable crate, not a brazier or button.
    bl = new_img(8, 8)
    rect(bl, 0, 0, 7, 7, 11)                 # dark-brown frame
    rect(bl, 1, 1, 6, 6, 10)                 # brown body
    for d in range(1, 7):                    # X brace across the face
        px(bl, d, d, 11); px(bl, 7 - d, d, 11)
    write(bl, "block", {"type": "sprite"})
    # ability shrine 16x16 (glowing pedestal)
    sh = new_img(16, 16)
    rect(sh, 4, 11, 11, 15, 12)              # stone pedestal
    rect(sh, 6, 3, 9, 10, 6)                 # gold orb column
    rect(sh, 5, 1, 10, 2, 9)                 # white glow top
    px(sh, 3, 6, 9); px(sh, 12, 6, 9)        # sparkles
    write(sh, "shrine", {"type": "sprite"})

def gen_bg_palette():
    """A swatch image whose 16-colour palette becomes the shared background palette.
    Pixels are irrelevant; grit reads the palette table."""
    im = new_img(8, 8)
    for i in range(16):
        px(im, i % 8, i // 8, i)
    write(im, "bg_palette", {"type": "bg_palette", "bpp_mode": "bpp_4"})

def gen_hud():
    """HUD meter pips: a 7x7 filled square (1px gap). Health = red, magic = cyan."""
    for name, idx in (("hud_health", 13), ("hud_magic", 8)):
        im = new_img(8, 8)
        rect(im, 0, 0, 6, 6, idx)
        write(im, name, {"type": "sprite"})

def gen_hud_life():
    """HUD life-icon pip 8x8: a gold shield glyph — visually distinct from the red health
    pip (flat square) and the cyan magic pip (flat square). The shield reads as a 'life'
    token at a glance: rounded top, narrow V-bottom, gold body (pal 6) with dark outline (pal 1)
    and a rose highlight (pal 7) so it's legible at 8x8."""
    im = new_img(8, 8)
    # Shield outline: near-black (pal 1) forms the border
    # Top arc (row 0): full width
    rect(im, 1, 0, 5, 0, 1)
    # Left and right edges (rows 1-4)
    for y in range(1, 5):
        px(im, 0, y, 1); px(im, 6, y, 1)
    # Narrowing V toward tip (rows 5-6)
    px(im, 1, 5, 1); px(im, 5, 5, 1)
    px(im, 2, 6, 1); px(im, 4, 6, 1)
    px(im, 3, 7, 1)    # tip
    # Gold fill (pal 6 = spronk gold) inside the outline
    rect(im, 1, 1, 5, 4, 6)
    rect(im, 2, 5, 4, 5, 6)
    rect(im, 3, 6, 3, 6, 6)
    # Rose highlight glint (pal 7) top-left corner for depth
    px(im, 1, 1, 7); px(im, 2, 1, 7)
    write(im, "hud_life", {"type": "sprite"})

def gen_heart_container():
    """Heart-container pickup 16x16 — a red heart on the transparent backdrop (palette idx 0).
    Permanent max-HP upgrade. Red body (pal 13) with a rose-light highlight (pal 7) and a
    near-black outline (pal 1), so it reads as a classic heart distinct from every gate/flame tile."""
    im = new_img(16, 16)
    # Classic heart silhouette: two top lobes (rows 3-5) + a downward V to the tip (row 12).
    # Filled red body, drawn as a per-row span table (x0,x1 inclusive) over the 16x16 field.
    spans = {
        3:  [(3, 6), (9, 12)],     # two lobe tops
        4:  [(2, 7), (8, 13)],
        5:  [(2, 13)],             # lobes merge
        6:  [(2, 13)],
        7:  [(3, 12)],
        8:  [(4, 11)],
        9:  [(5, 10)],
        10: [(6, 9)],
        11: [(7, 8)],
        12: [(7, 8)],              # tip
    }
    for y, segs in spans.items():
        for x0, x1 in segs:
            rect(im, x0, y, x1, y, 13)        # red body
    # Rose-light highlight glint on the upper-left lobe.
    rect(im, 3, 4, 4, 5, 7)
    px(im, 4, 3, 7)
    # Near-black outline along the bottom V edges (gives the heart depth).
    px(im, 6, 11, 1); px(im, 9, 11, 1)
    px(im, 6, 12, 1); px(im, 9, 12, 1)
    px(im, 7, 13, 1); px(im, 8, 13, 1)
    write(im, "heart_container", {"type": "sprite"})

def gen_light_proj():
    """Light projectile 8x8 (M10) — a bright white/yellow bolt, distinct from the red/orange
    fire orb and the cyan ice orb. Gold halo (pal 6) around a pure-white core (pal 15), with
    four white sparks so it reads as a radiant Light shot at a glance."""
    im = new_img(8, 8)
    rect(im, 2, 2, 5, 5, 6)                  # gold halo
    rect(im, 3, 3, 4, 4, 15)                 # pure-white core
    px(im, 1, 4, 15); px(im, 6, 3, 15); px(im, 4, 1, 15); px(im, 3, 6, 15)  # white sparks
    write(im, "light_proj", {"type": "sprite"})

def gen_magic_crystal():
    """Magic crystal pickup 16x16 (M10) — a faceted cyan/blue gem, distinct from the red heart
    container and gold shrine. Full magic refill; respawns each attempt. Cyan body (pal 8) with
    a white facet glint (pal 15) and a near-black outline (pal 1) so it reads as a magic gem."""
    im = new_img(16, 16)
    # Diamond/crystal silhouette: narrow top, wide middle, point at the bottom (per-row spans).
    spans = {
        2:  [(7, 8)],
        3:  [(6, 9)],
        4:  [(5, 10)],
        5:  [(4, 11)],
        6:  [(3, 12)],
        7:  [(3, 12)],
        8:  [(4, 11)],
        9:  [(5, 10)],
        10: [(6, 9)],
        11: [(7, 8)],
    }
    for y, segs in spans.items():
        for x0, x1 in segs:
            rect(im, x0, y, x1, y, 8)        # cyan body
    # White facet glints down the left face + a bright top, so it sparkles.
    px(im, 7, 3, 15); px(im, 6, 5, 15); px(im, 5, 6, 15)
    px(im, 8, 2, 9)
    # Near-black outline along the lower V edges for depth.
    px(im, 3, 7, 1); px(im, 12, 7, 1)
    px(im, 6, 10, 1); px(im, 9, 10, 1)
    px(im, 7, 11, 1); px(im, 8, 11, 1)
    write(im, "magic_crystal", {"type": "sprite"})

def gen_king():
    """Nightmare King boss placeholder 32x32 (M11) — a tall, looming dark/violet figure with a
    horned crown, glowing red eyes, and a flowing shadow cloak. Built from the shared 16-colour
    palette: shadow-blue body (pal 14), stone-purple cloak folds (pal 12), near-black outline
    (pal 1), red eyes (pal 13), gold crown (pal 6), white glints (pal 15). 32x32 is a Butano-
    supported sprite size (4bpp/16-colour). Distinct, readable silhouette vs every other sprite."""
    im = new_img(32, 32)
    # ---- horned crown (gold) sitting atop the head ----
    rect(im, 11, 2, 20, 4, 6)            # gold crown band
    rect(im, 10, 0, 11, 3, 6); rect(im, 20, 0, 21, 3, 6)   # two outer horns
    rect(im, 15, 0, 16, 2, 6)            # centre horn
    px(im, 12, 3, 15); px(im, 19, 3, 15)  # white crown glints
    # ---- head (shadow-blue) with red glowing eyes ----
    rect(im, 11, 5, 20, 12, 14)          # head block
    rect(im, 11, 5, 20, 5, 1)            # near-black brow line
    rect(im, 13, 8, 14, 9, 13); rect(im, 17, 8, 18, 9, 13)  # red eyes
    px(im, 13, 8, 15); px(im, 17, 8, 15)  # eye glints
    # ---- shoulders + cloaked body (widening shadow cloak with purple folds) ----
    rect(im, 7, 13, 24, 16, 14)          # broad shoulders
    rect(im, 8, 17, 23, 27, 14)          # cloak body
    rect(im, 6, 20, 25, 29, 14)          # cloak flares wider toward the base
    # purple vertical cloak folds
    for cx in (10, 15, 20):
        rect(im, cx, 17, cx, 28, 12)
    rect(im, 12, 14, 19, 18, 12)         # chest plate (purple)
    px(im, 15, 15, 15); px(im, 16, 16, 15)  # chest glints
    # ---- near-black outline along the cloak hem (ragged bottom) ----
    for bx in range(6, 26, 3):
        px(im, bx, 29, 1); px(im, bx + 1, 28, 1)
    rect(im, 6, 30, 25, 30, 1)           # base shadow line
    write(im, "king", {"type": "sprite"})

def gen_king_hp():
    """King HP pip 8x8 (M11) — the boss-health unit, shown as a row at the bottom of the screen.
    A small violet orb with a red core + white glint, deliberately distinct from the player's red
    health / cyan magic bars and from the gold fire orb: stone-purple (pal 12) body, red (pal 13)
    core, white (pal 15) glint, near-black (pal 1) outline corners."""
    im = new_img(8, 8)
    # round-ish orb body (per-row spans)
    rect(im, 2, 1, 5, 1, 12)
    rect(im, 1, 2, 6, 5, 12)
    rect(im, 2, 6, 5, 6, 12)
    rect(im, 3, 3, 4, 4, 13)            # red core (boss menace)
    px(im, 2, 2, 15)                    # white glint, top-left
    px(im, 1, 1, 1); px(im, 6, 1, 1); px(im, 1, 6, 1); px(im, 6, 6, 1)  # outline corners
    write(im, "king_hp", {"type": "sprite"})

def gen_grapple_icon():
    """Grapple HUD icon 8x8 — a green hook glyph, clearly distinct from the cyan Ice orb.
    Uses bright green (pal 4) + dark green (pal 5) + near-black outline (pal 1).
    The Ice icon is cyan; this is GREEN so the player can always tell them apart."""
    im = new_img(8, 8)
    # Hook shape: a J-curve made of green pixels
    # Vertical shaft on the right (x=5), top half
    for y in range(1, 5):
        px(im, 5, y, 4)          # bright green shaft
    # Curved hook at the bottom: goes left then curves up
    rect(im, 2, 5, 5, 5, 4)     # horizontal base of hook
    px(im, 2, 4, 4)              # inner curve up-left
    # Hook tip pointing upward-left
    px(im, 2, 3, 4)
    # Dark green outline/shadow to give depth
    px(im, 6, 1, 5); px(im, 6, 2, 5); px(im, 6, 3, 5); px(im, 6, 4, 5)
    px(im, 1, 5, 5); px(im, 2, 6, 5); px(im, 3, 6, 5); px(im, 4, 6, 5); px(im, 5, 6, 5)
    # Highlight pixel (white) at shaft top
    px(im, 5, 1, 15)
    write(im, "grapple_icon", {"type": "sprite"})

if __name__ == "__main__":
    gen_laurel()
    gen_enemy()
    gen_spronk()
    gen_bolt()
    gen_tiles()
    gen_bg_palette()
    gen_hud()
    gen_hud_life()
    gen_ember_sprites()
    gen_light_proj()
    gen_magic_crystal()
    gen_grapple_icon()
    gen_king()
    gen_king_hp()
    gen_heart_container()
    print("placeholder sprites + bg tiles + hud + ember art generated.")
