#!/usr/bin/env python3
"""
Generate pixel-art plant-ghost sprites for all 10 DeskBuddy states.

Produces 48x48 PNG files (one per state) matching the chunky pixel-art
plant pot aesthetic. At 7x LVGL zoom these render as 336x336 on the AMOLED.

Usage:
    python generate_sprites.py --output-dir firmware/include/sprites
"""

import argparse
import os
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw
except ImportError:
    print("Install Pillow: pip install Pillow")
    sys.exit(1)

# ── Palette ──────────────────────────────────────────────────────────────────

BG           = (0,   0,   0,   0)    # transparent → AMOLED black
POT_MAIN     = (196, 232, 212, 255)  # mint green body
POT_STRIPE   = (170, 210, 188, 255)  # stripe (slightly darker)
POT_RIM      = (218, 244, 230, 255)  # lighter rim/collar
POT_DARK     = (148, 192, 168, 255)  # bottom/shadow
PLANT_MID    = (52,  172, 100, 255)  # plant mid-green
PLANT_DARK   = (36,  118, 68,  255)  # plant shadow
PLANT_LEAF   = (76,  192, 118, 255)  # leaf highlight
OUTLINE      = (22,  52,  38,  255)  # dark outline / pupils
EYE_W        = (236, 242, 238, 255)  # eye whites
BLUSH        = (226, 140, 166, 255)  # pink blush
HEART_PINK   = (212, 86,  122, 255)  # heart / loved eyes
GRAY_D       = (90,  95,  102, 255)  # headphone dark
GRAY_M       = (140, 145, 152, 255)  # headphone mid
GRAY_L       = (175, 180, 185, 255)  # headphone light
YELLOW       = (218, 190, 58,  255)  # sun/rooftop
SWEAT        = (100, 160, 220, 255)  # sweat / water drop
ZZZ          = (160, 168, 160, 255)  # sleeping Zs
FOCUS_MARK   = (218, 140, 60,  255)  # focus streaks
LIGHTNING    = (82,  200, 118, 255)  # coding lightning
WHITE        = (255, 255, 255, 255)

SIZE = 48

# ── Canvas / base helpers ─────────────────────────────────────────────────────

def new_canvas() -> Image.Image:
    return Image.new("RGBA", (SIZE, SIZE), BG)


def px(img: Image.Image, x: int, y: int, color: tuple):
    if 0 <= x < SIZE and 0 <= y < SIZE:
        img.putpixel((x, y), color)


def rect(img: Image.Image, x0: int, y0: int, x1: int, y1: int, color: tuple):
    d = ImageDraw.Draw(img)
    d.rectangle([x0, y0, x1, y1], fill=color)


def hline(img: Image.Image, y: int, x0: int, x1: int, color: tuple):
    d = ImageDraw.Draw(img)
    d.line([(x0, y), (x1, y)], fill=color)


# ── Character layout (all coords in 48x48 canvas) ────────────────────────────
#
#  Plant area  : y = 4 – 15   (centered stem at x=23-24)
#  Pot rim     : y = 15 – 16  (x = 9 – 38, 30px wide)
#  Pot body    : y = 17 – 36  (x = 12 – 35, 24px wide)
#  Ghost legs  : y = 37 – 42  (3 bumps)
#

RIM_X0, RIM_X1 = 9, 38
BODY_X0, BODY_X1 = 12, 35
RIM_Y0, RIM_Y1 = 15, 16
BODY_Y0, BODY_Y1 = 17, 36
LEG_Y0, LEG_Y1 = 37, 42
STEM_X = 23  # left edge of 2px stem

# Eye positions (relative to canvas)
EYE_L_X0, EYE_L_X1 = 14, 18   # left eye (5px wide)
EYE_R_X0, EYE_R_X1 = 25, 29   # right eye
EYE_Y0, EYE_Y1     = 21, 25   # eye row (5px tall)
# Mouth
MOUTH_Y = 31
MOUTH_X0, MOUTH_X1 = 17, 30


# ── POT BASE (shared by all states) ──────────────────────────────────────────

def draw_pot(img: Image.Image, dim: bool = False):
    """Draw the pot rim, body with horizontal stripes, and ghost legs."""
    alpha = 140 if dim else 255

    def _c(base: tuple) -> tuple:
        return base[:3] + (alpha,)

    # Rim
    rect(img, RIM_X0, RIM_Y0, RIM_X1, RIM_Y1, _c(POT_RIM))

    # Body with stripe pattern (alternate rows)
    for y in range(BODY_Y0, BODY_Y1 + 1):
        row_color = POT_STRIPE if (y - BODY_Y0) % 2 == 0 else POT_MAIN
        hline(img, y, BODY_X0, BODY_X1, _c(row_color))

    # Ghost legs (3 bumps)
    leg_xs = [(BODY_X0, BODY_X0 + 4), (BODY_X0 + 8, BODY_X0 + 12), (BODY_X0 + 16, BODY_X0 + 20)]
    for x0, x1 in leg_xs:
        rect(img, x0, LEG_Y0, x1, LEG_Y1, _c(POT_DARK))


# ── PLANT variants ────────────────────────────────────────────────────────────

def draw_plant_happy(img: Image.Image):
    """Two perky leaves left and right."""
    # Stem
    rect(img, STEM_X, 6, STEM_X + 1, 14, PLANT_DARK)
    # Left leaf block
    rect(img, STEM_X - 5, 7, STEM_X - 1, 11, PLANT_MID)
    px(img, STEM_X - 5, 7, PLANT_DARK)
    px(img, STEM_X - 5, 11, PLANT_DARK)
    # Right leaf block
    rect(img, STEM_X + 2, 6, STEM_X + 6, 10, PLANT_MID)
    px(img, STEM_X + 6, 6, PLANT_DARK)
    px(img, STEM_X + 6, 10, PLANT_DARK)


def draw_plant_loved(img: Image.Image):
    """Perky leaves + two small hearts above."""
    draw_plant_happy(img)
    # Left heart (2x2 pixel dot clusters)
    rect(img, 10, 4, 11, 5, HEART_PINK)
    rect(img, 9, 5, 12, 6, HEART_PINK)
    rect(img, 10, 7, 11, 7, HEART_PINK)
    # Right heart
    rect(img, 30, 3, 31, 4, HEART_PINK)
    rect(img, 29, 4, 32, 5, HEART_PINK)
    rect(img, 30, 6, 31, 6, HEART_PINK)


def draw_plant_coding(img: Image.Image):
    """Single upright stem with lightning bolt."""
    rect(img, STEM_X, 5, STEM_X + 1, 14, PLANT_DARK)
    # Lightning bolt (zig-zag 3px wide)
    pts = [(STEM_X + 3, 5), (STEM_X + 6, 5), (STEM_X + 3, 9),
           (STEM_X + 6, 9), (STEM_X + 3, 13)]
    d = ImageDraw.Draw(img)
    for i in range(len(pts) - 1):
        d.line([pts[i], pts[i + 1]], fill=LIGHTNING, width=2)


def draw_plant_wilted(img: Image.Image):
    """Drooping plant for thirsty/overwatered."""
    # Stem droops to the right
    rect(img, STEM_X, 9, STEM_X + 1, 14, PLANT_DARK)
    d = ImageDraw.Draw(img)
    d.line([(STEM_X + 1, 9), (STEM_X + 5, 5)], fill=PLANT_DARK, width=2)
    d.line([(STEM_X + 5, 5), (STEM_X + 9, 4)], fill=PLANT_MID, width=2)


def draw_plant_sleeping(img: Image.Image):
    """Small stub + ZZZ."""
    rect(img, STEM_X, 11, STEM_X + 1, 14, PLANT_DARK)
    # Zzz pixels (3x3 Z shapes)
    def z_shape(img, cx, cy, color):
        # top, diagonal, bottom
        hline(img, cy,     cx, cx + 3, color)
        px(img, cx + 2,    cy + 1, color)
        px(img, cx + 1,    cy + 2, color)
        hline(img, cy + 3, cx, cx + 3, color)

    z_shape(img, 27, 5, ZZZ)
    # smaller Z
    hline(img, 4, 31, 33, ZZZ)
    px(img, 33, 5, ZZZ)
    px(img, 32, 6, ZZZ)
    hline(img, 7, 31, 33, ZZZ)


def draw_plant_focus(img: Image.Image):
    """Upright plant + speed-lines."""
    rect(img, STEM_X, 5, STEM_X + 1, 14, PLANT_DARK)
    rect(img, STEM_X - 2, 6, STEM_X + 3, 10, PLANT_MID)
    # focus marks (short diagonal lines upper right)
    d = ImageDraw.Draw(img)
    d.line([(30, 5), (33, 3)], fill=FOCUS_MARK, width=1)
    d.line([(31, 8), (34, 6)], fill=FOCUS_MARK, width=1)


def draw_plant_rooftop(img: Image.Image):
    """Windswept plant with sun ray."""
    rect(img, STEM_X, 7, STEM_X + 1, 14, PLANT_DARK)
    # leaves blown left
    rect(img, STEM_X - 8, 5, STEM_X - 1, 9, PLANT_MID)
    px(img, STEM_X - 8, 5, PLANT_DARK)
    # sun dot top-right
    rect(img, 33, 4, 36, 7, YELLOW)
    px(img, 34, 4, (255, 220, 100, 255))


def draw_plant_meeting(img: Image.Image):
    """Normal plant + speech bubble dot."""
    draw_plant_happy(img)
    # speech bubble: small rounded rect upper right
    rect(img, 32, 5, 37, 9, WHITE)
    rect(img, 33, 10, 34, 11, WHITE)  # tail


# ── FACE variants ─────────────────────────────────────────────────────────────

def draw_eye_normal(img, x0, y0):
    """Oval eye: 5×5 white with 2×2 dark pupil."""
    rect(img, x0, y0, x0 + 4, y0 + 4, EYE_W)
    rect(img, x0 + 1, y0 + 1, x0 + 2, y0 + 2, OUTLINE)


def draw_eye_heart(img, x0, y0):
    """Heart-shaped pink eye."""
    # Two 2x2 dots on top, 3x2 in middle, 2x1 at bottom → heart shape
    rect(img, x0,     y0,     x0 + 1, y0 + 1, HEART_PINK)
    rect(img, x0 + 3, y0,     x0 + 4, y0 + 1, HEART_PINK)
    rect(img, x0,     y0 + 2, x0 + 4, y0 + 3, HEART_PINK)
    rect(img, x0 + 1, y0 + 4, x0 + 3, y0 + 4, HEART_PINK)
    px(img,   x0 + 2, y0 + 4, HEART_PINK)


def draw_eye_tired(img, x0, y0):
    """Half-closed (heavy eyelid) eye."""
    rect(img, x0, y0, x0 + 4, y0 + 4, EYE_W)
    rect(img, x0, y0, x0 + 4, y0 + 1, OUTLINE)   # heavy top lid
    rect(img, x0 + 1, y0 + 2, x0 + 2, y0 + 3, OUTLINE)  # pupil


def draw_eye_closed(img, x0, y0):
    """Closed eye (single line)."""
    hline(img, y0 + 2, x0, x0 + 4, OUTLINE)


def draw_eye_sad(img, x0, y0):
    """Droopy-bottom eye (pupils at bottom)."""
    rect(img, x0, y0, x0 + 4, y0 + 4, EYE_W)
    rect(img, x0 + 1, y0 + 2, x0 + 2, y0 + 3, OUTLINE)  # pupils low


def draw_eye_wide(img, x0, y0):
    """Wide/surprised eye (bigger)."""
    rect(img, x0 - 1, y0 - 1, x0 + 5, y0 + 5, EYE_W)
    rect(img, x0 + 1, y0 + 1, x0 + 2, y0 + 2, OUTLINE)


def draw_eye_determined(img, x0, y0):
    """Narrow determined eye with diagonal brow."""
    rect(img, x0, y0 + 1, x0 + 4, y0 + 3, EYE_W)
    rect(img, x0 + 1, y0 + 1, x0 + 2, y0 + 2, OUTLINE)
    # brow
    hline(img, y0 - 1, x0, x0 + 3, OUTLINE)


def draw_eye_bored(img, x0, y0, side='L'):
    """Sideways-glancing eye."""
    rect(img, x0, y0, x0 + 4, y0 + 4, EYE_W)
    px_x = x0 + 3 if side == 'L' else x0 + 1  # pupils off-center
    rect(img, px_x, y0 + 1, px_x + 1, y0 + 2, OUTLINE)


def draw_eye_squint(img, x0, y0):
    """Squinting/relaxed eye."""
    rect(img, x0, y0 + 1, x0 + 4, y0 + 3, EYE_W)
    rect(img, x0, y0,     x0 + 4, y0,     OUTLINE)  # top squint
    rect(img, x0 + 1, y0 + 2, x0 + 2, y0 + 2, OUTLINE)


def draw_mouth_smile(img):
    rect(img, MOUTH_X0, MOUTH_Y,     MOUTH_X0 + 1, MOUTH_Y,     OUTLINE)
    rect(img, MOUTH_X0 + 2, MOUTH_Y + 1, MOUTH_X1 - 2, MOUTH_Y + 1, OUTLINE)
    rect(img, MOUTH_X1 - 1, MOUTH_Y,     MOUTH_X1,     MOUTH_Y,     OUTLINE)


def draw_mouth_flat(img):
    rect(img, MOUTH_X0, MOUTH_Y, MOUTH_X1, MOUTH_Y, OUTLINE)


def draw_mouth_sad(img):
    rect(img, MOUTH_X0, MOUTH_Y + 1,     MOUTH_X0 + 1, MOUTH_Y + 1, OUTLINE)
    rect(img, MOUTH_X0 + 2, MOUTH_Y, MOUTH_X1 - 2, MOUTH_Y,     OUTLINE)
    rect(img, MOUTH_X1 - 1, MOUTH_Y + 1, MOUTH_X1,     MOUTH_Y + 1, OUTLINE)


def draw_mouth_open(img):
    """Small open mouth."""
    rect(img, MOUTH_X0 + 2, MOUTH_Y, MOUTH_X1 - 2, MOUTH_Y + 2, OUTLINE)
    rect(img, MOUTH_X0 + 3, MOUTH_Y + 1, MOUTH_X1 - 3, MOUTH_Y + 1, EYE_W)


def draw_blush(img):
    rect(img, EYE_L_X0 - 2, EYE_Y1 + 2, EYE_L_X0,     EYE_Y1 + 3, BLUSH)
    rect(img, EYE_R_X1,     EYE_Y1 + 2, EYE_R_X1 + 2, EYE_Y1 + 3, BLUSH)


# ── ACCESSORIES ───────────────────────────────────────────────────────────────

def draw_headphones(img: Image.Image):
    """Gray headphone cups on the sides of the pot."""
    # Left cup
    rect(img, 4,  BODY_Y0 + 3, 11, BODY_Y0 + 12, GRAY_D)
    rect(img, 5,  BODY_Y0 + 4, 10, BODY_Y0 + 11, GRAY_M)
    rect(img, 6,  BODY_Y0 + 5,  9, BODY_Y0 + 10, GRAY_L)
    # Right cup
    rect(img, 36, BODY_Y0 + 3, 43, BODY_Y0 + 12, GRAY_D)
    rect(img, 37, BODY_Y0 + 4, 42, BODY_Y0 + 11, GRAY_M)
    rect(img, 38, BODY_Y0 + 5, 41, BODY_Y0 + 10, GRAY_L)
    # Band (top connecting bar)
    rect(img, 10, RIM_Y0 - 2, 37, RIM_Y0 - 1, GRAY_D)


def draw_sweat_drop(img: Image.Image):
    """Blue teardrop on the side (thirsty)."""
    rect(img, 36, 28, 38, 30, SWEAT)
    px(img, 37, 31, SWEAT)


def draw_water_drops(img: Image.Image):
    """Multiple small blue drops (overwatered)."""
    for x, y in [(8, 30), (38, 26), (10, 38)]:
        rect(img, x, y, x + 2, y + 2, SWEAT)


def draw_tray(img: Image.Image):
    """Green tray/saucer under the pot (coding state)."""
    rect(img, BODY_X0 - 2, LEG_Y1 + 1, BODY_X1 + 2, LEG_Y1 + 3, PLANT_DARK)


# ── STATE BUILDERS ────────────────────────────────────────────────────────────

def build_state(state: str) -> Image.Image:
    img = new_canvas()
    dim = state == "sleeping"
    draw_pot(img, dim=dim)

    # Plant
    if state in ("happy", "idle"):
        draw_plant_happy(img)
    elif state == "loved":
        draw_plant_loved(img)
    elif state == "coding":
        draw_plant_coding(img)
    elif state in ("thirsty", "overwatered"):
        draw_plant_wilted(img)
    elif state == "sleeping":
        draw_plant_sleeping(img)
    elif state == "focus":
        draw_plant_focus(img)
    elif state == "rooftop":
        draw_plant_rooftop(img)
    elif state == "meeting":
        draw_plant_meeting(img)

    # Eyes
    el = (EYE_L_X0, EYE_Y0)
    er = (EYE_R_X0, EYE_Y0)

    if state == "happy":
        draw_eye_normal(img, *el);  draw_eye_normal(img, *er)
    elif state == "loved":
        draw_eye_heart(img, *el);   draw_eye_heart(img, *er)
    elif state == "coding":
        draw_eye_tired(img, *el);   draw_eye_tired(img, *er)
    elif state == "meeting":
        draw_eye_normal(img, *el);  draw_eye_normal(img, *er)
    elif state == "thirsty":
        draw_eye_sad(img, *el);     draw_eye_sad(img, *er)
    elif state == "overwatered":
        draw_eye_wide(img, *el);    draw_eye_wide(img, *er)
    elif state == "sleeping":
        draw_eye_closed(img, *el);  draw_eye_closed(img, *er)
    elif state == "focus":
        draw_eye_determined(img, *el); draw_eye_determined(img, *er)
    elif state == "rooftop":
        draw_eye_squint(img, *el);  draw_eye_squint(img, *er)
    elif state == "idle":
        draw_eye_bored(img, *el, 'L'); draw_eye_bored(img, *er, 'R')

    # Mouth
    if state in ("happy", "loved", "meeting", "rooftop"):
        draw_mouth_smile(img)
    elif state in ("coding", "focus", "idle"):
        draw_mouth_flat(img)
    elif state in ("thirsty", "sleeping"):
        draw_mouth_sad(img)
    elif state == "overwatered":
        draw_mouth_open(img)

    # Accessories / extras
    if state == "loved":
        draw_blush(img)
    elif state == "coding":
        draw_headphones(img)
        draw_tray(img)
    elif state == "thirsty":
        draw_sweat_drop(img)
    elif state == "overwatered":
        draw_water_drops(img)
    elif state == "meeting":
        draw_blush(img)

    return img


# ── Main ──────────────────────────────────────────────────────────────────────

STATES = [
    "happy", "coding", "meeting", "rooftop", "focus",
    "idle", "thirsty", "overwatered", "loved", "sleeping",
]


def main():
    parser = argparse.ArgumentParser(description="Generate DeskBuddy pixel-art sprites")
    parser.add_argument("--output-dir", default=".", help="Directory to write PNG files")
    parser.add_argument("--output-scale", type=int, default=5,
                        help="Integer scale factor for output sprites (default 5 = 240x240)")
    parser.add_argument("--preview-scale", type=int, default=0,
                        help="Extra scale for a side-by-side preview PNG (0 = skip)")
    args = parser.parse_args()

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    out_size = SIZE * args.output_scale

    for state in STATES:
        img = build_state(state)

        if args.output_scale > 1:
            img = img.resize((out_size, out_size), Image.NEAREST)

        if args.preview_scale > 1:
            preview = img.resize(
                (out_size * args.preview_scale, out_size * args.preview_scale),
                Image.NEAREST)
            preview.save(out_dir / f"sprite_{state}_preview.png")

        img.save(out_dir / f"sprite_{state}.png")
        print(f"  {state}: {out_dir}/sprite_{state}.png ({out_size}x{out_size})")

    print(f"\nGenerated {len(STATES)} sprites at {out_size}x{out_size} in {out_dir}/")
    print("\nNext: convert to C headers with:")
    print(f"  bash scripts/build_sprites.sh")


if __name__ == "__main__":
    main()
