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

if __name__ == "__main__":
    gen_laurel()
    gen_enemy()
    gen_spronk()
    gen_bolt()
    print("placeholder sprites generated.")
