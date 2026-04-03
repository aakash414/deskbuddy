#!/usr/bin/env python3
"""Convert uint16_t PROGMEM sprite headers → scaled LVGL lv_img_dsc_t headers.

Usage: python3 scripts/convert_sprites.py
Reads  firmware/include/sprites/sprites/sprite_<name>.h
Writes firmware/include/sprites/sprite_<name>.h  (LVGL format, scaled)
"""

import re, os, sys

SCALE      = 6          # 48 * 6 = 288px
SRC_SIZE   = 48
DST_SIZE   = SRC_SIZE * SCALE
SRC_DIR    = "firmware/include/sprites/sprites"
DST_DIR    = "firmware/include/sprites"

STATES = ["rooftop", "loved", "sleeping", "coding", "meeting", "focus", "thirsty"]

def parse_frame(text):
    """Return list of uint16 pixel values from one PROGMEM array body."""
    vals = re.findall(r'0x([0-9A-Fa-f]{4})', text)
    pixels = [int(v, 16) for v in vals]
    assert len(pixels) == SRC_SIZE * SRC_SIZE, f"expected {SRC_SIZE*SRC_SIZE} pixels, got {len(pixels)}"
    return pixels

def scale_nearest(pixels, src_size, scale):
    dst = []
    for dy in range(src_size * scale):
        sy = dy // scale
        for dx in range(src_size * scale):
            sx = dx // scale
            dst.append(pixels[sy * src_size + sx])
    return dst

def pixels_to_lvgl_bytes(pixels):
    """uint16 RGB565 → uint8 pairs (little-endian, LV_COLOR_16_SWAP=0)."""
    out = []
    for p in pixels:
        out.append(p & 0xFF)        # low byte
        out.append((p >> 8) & 0xFF) # high byte
    return out

def write_header(name, frames_bytes, dst_size):
    path = os.path.join(DST_DIR, f"sprite_{name}.h")
    n = name.upper()
    num_frames = len(frames_bytes)
    data_size  = dst_size * dst_size * 2

    lines = []
    lines.append("#pragma once")
    lines.append("#include <lvgl.h>")
    lines.append("")

    for fi, data in enumerate(frames_bytes):
        sym = f"sprite_{name}_f{fi}_data"
        lines.append(f"static const uint8_t {sym}[] = {{")
        row_len = dst_size * 2  # bytes per row
        for i in range(0, len(data), row_len):
            row = data[i:i+row_len]
            lines.append("    " + ", ".join(f"0x{b:02X}" for b in row) + ",")
        lines.append("};")
        lines.append("")

        desc_sym = f"sprite_{name}_f{fi}"
        lines.append(f"static const lv_img_dsc_t {desc_sym} = {{")
        lines.append( "    .header = {")
        lines.append( "        .cf     = LV_IMG_CF_TRUE_COLOR,")
        lines.append(f"        .w      = {dst_size},")
        lines.append(f"        .h      = {dst_size},")
        lines.append( "    },")
        lines.append(f"    .data_size = {data_size},")
        lines.append(f"    .data      = {sym},")
        lines.append("};")
        lines.append("")

    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")

    print(f"  {name}: {num_frames} frame(s), {dst_size}x{dst_size} → {path}")

def process(name):
    src = os.path.join(SRC_DIR, f"sprite_{name}.h")
    if not os.path.exists(src):
        print(f"  SKIP {name}: {src} not found")
        return

    text = open(src).read()

    # Find all PROGMEM frame arrays (exclude the pointer array at the end)
    pattern = re.compile(
        r'static const uint16_t\s+sprite_\w+_f\d+\[\]\s+PROGMEM\s*=\s*\{([^}]+)\}',
        re.DOTALL
    )
    matches = pattern.findall(text)
    if not matches:
        print(f"  SKIP {name}: no frame arrays found")
        return

    frames_bytes = []
    for body in matches:
        pixels  = parse_frame(body)
        scaled  = scale_nearest(pixels, SRC_SIZE, SCALE)
        as_bytes = pixels_to_lvgl_bytes(scaled)
        frames_bytes.append(as_bytes)

    write_header(name, frames_bytes, DST_SIZE)

if __name__ == "__main__":
    os.makedirs(DST_DIR, exist_ok=True)
    print(f"Converting {len(STATES)} sprites  {SRC_SIZE}px → {DST_SIZE}px (scale {SCALE}x)...\n")
    for name in STATES:
        process(name)
    print("\nDone.")
