#!/usr/bin/env python3
"""
Convert HD sprite PNGs to raw RGB565 binary files for LittleFS.

Input:  sprites_hd/png/<state>_<frame>.png   (e.g. coding_0.png)
Output: data/sprites/<state>_<frame>.bin      (raw RGB565, 2 bytes/pixel)

Usage:
    python3 scripts/build_sprites_hd.py [--size 300]
"""

import argparse, os, struct, sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Install Pillow: python3 -m pip install Pillow --break-system-packages")

STATES = ["coding", "meeting", "rooftop", "focus", "thirsty", "sleeping", "loved"]
FRAMES = 4
SRC_DIR = "sprites_hd/png"
DST_DIR = "data/sprites"

def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert(src_path, dst_path, size):
    img = Image.open(src_path).convert("RGBA")
    if img.size != (size, size):
        img = img.resize((size, size), Image.LANCZOS)

    out = bytearray()
    for y in range(size):
        for x in range(size):
            r, g, b, a = img.getpixel((x, y))
            px = 0x0000 if a < 128 else to_rgb565(r, g, b)
            out += struct.pack("<H", px)  # little-endian

    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
    with open(dst_path, "wb") as f:
        f.write(out)
    return len(out)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=300)
    args = ap.parse_args()

    total = 0
    missing = []
    print(f"Converting sprites  →  {args.size}×{args.size} RGB565 binary\n")

    for state in STATES:
        for fi in range(FRAMES):
            src = os.path.join(SRC_DIR, f"{state}_{fi}.png")
            dst = os.path.join(DST_DIR, f"{state}_{fi}.bin")
            if not os.path.exists(src):
                missing.append(src)
                continue
            n = convert(src, dst, args.size)
            total += n
            print(f"  {state}_{fi}.bin  {n//1024}KB")

    print(f"\nTotal: {total//1024}KB in {DST_DIR}/")
    if missing:
        print(f"\nMissing ({len(missing)} files):")
        for p in missing:
            print(f"  {p}")

if __name__ == "__main__":
    main()
