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
    """Background tileset: 19 tiles of 8x8 in a horizontal strip. Index order:
    0 blank, 1 ground, 2 one-way, 3 gate(closed wall), 4 cage, 5 door-open, 6 door-locked,
    7 vine, 8 ice, 9-12 reserved (D3+ gates), 13 lava, 14 brazier-unlit, 15 brazier-lit,
    16 free, 17 plate, 18 button. (block + shrine are SPRITES, not bg tiles.)"""
    im = new_img(8 * 19, 8)
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
    # tile 13: lava (red with orange/gold bubbles)
    ox = 8 * 13
    rect(im, ox, 0, ox + 7, 7, 13)          # red
    rect(im, ox, 0, ox + 7, 0, 6)           # gold crust line
    px(im, ox + 2, 4, 6); px(im, ox + 5, 5, 6); px(im, ox + 4, 2, 6)  # bubbles
    # tile 14: brazier unlit (brown bowl on a post)
    ox = 8 * 14
    rect(im, ox + 2, 5, ox + 5, 7, 11)      # post
    rect(im, ox + 1, 3, ox + 6, 4, 12)      # stone bowl
    # tile 15: brazier lit (bowl + flame)
    ox = 8 * 15
    rect(im, ox + 2, 5, ox + 5, 7, 11)      # post
    rect(im, ox + 1, 3, ox + 6, 4, 12)      # bowl
    rect(im, ox + 2, 0, ox + 5, 3, 6)       # gold flame
    px(im, ox + 3, 0, 13); px(im, ox + 4, 1, 13)  # red flame tips
    # tile 17: pressure plate (recessed stone on the floor)
    ox = 8 * 17
    rect(im, ox, 6, ox + 7, 7, 11)          # floor base
    rect(im, ox + 1, 4, ox + 6, 5, 12)      # plate
    # tile 18: hidden button (small floor switch)
    ox = 8 * 18
    rect(im, ox, 6, ox + 7, 7, 11)          # floor base
    rect(im, ox + 3, 4, ox + 4, 5, 6)       # gold button
    write(im, "tiles", {"type": "regular_bg_tiles", "bpp_mode": "bpp_4"})

def gen_ember_sprites():
    """M3 sprites: fire projectile, fire enemy, pushable block, ability shrine."""
    # fire projectile 8x8 (orange/red orb)
    fp = new_img(8, 8)
    rect(fp, 2, 2, 5, 5, 6)                  # gold core
    rect(fp, 3, 3, 4, 4, 13)                 # red center
    px(fp, 1, 4, 6); px(fp, 6, 3, 6); px(fp, 4, 1, 6); px(fp, 3, 6, 6)
    write(fp, "fire_proj", {"type": "sprite"})
    # fire enemy 16x16 (red variant of the green enemy)
    fe = new_img(16, 16)
    rect(fe, 3, 5, 12, 14, 13)               # red body
    rect(fe, 4, 11, 11, 14, 1)               # dark base
    px(fe, 6, 8, 9); px(fe, 9, 8, 9)         # white eyes
    rect(fe, 5, 4, 10, 4, 6)                 # gold brow
    write(fe, "fire_enemy", {"type": "sprite"})
    # pushable block 8x8 (brown crate, 1-tile to match its grid collision)
    bl = new_img(8, 8)
    rect(bl, 0, 0, 7, 7, 11)                 # dark-brown border
    rect(bl, 1, 1, 6, 6, 10)                 # brown body
    px(bl, 3, 3, 11); px(bl, 5, 5, 11)       # grain
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

if __name__ == "__main__":
    gen_laurel()
    gen_enemy()
    gen_spronk()
    gen_bolt()
    gen_tiles()
    gen_bg_palette()
    gen_hud()
    gen_ember_sprites()
    print("placeholder sprites + bg tiles + hud + ember art generated.")
