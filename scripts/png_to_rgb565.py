#!/usr/bin/env python3
"""
Convert a PNG sprite sheet to a C header file with RGB565 pixel data.

Usage:
    python png_to_rgb565.py sprites/happy.png --name happy --frames 4

Input: PNG file (single frame or horizontal strip of frames)
Output: C header file with PROGMEM array

Each frame should be SPRITE_SIZE x SPRITE_SIZE pixels.
A horizontal strip of N frames is (SPRITE_SIZE * N) x SPRITE_SIZE.
"""

import argparse
import struct
import sys

try:
    from PIL import Image
except ImportError:
    print("Install Pillow: pip install Pillow")
    sys.exit(1)


def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def convert_frame(img, x_offset, size):
    """Extract one frame and convert to RGB565 array."""
    pixels = []
    for y in range(size):
        for x in range(size):
            px = img.getpixel((x_offset + x, y))
            r, g, b = px[0], px[1], px[2]
            a = px[3] if len(px) > 3 else 255

            if a < 128:
                # Transparent pixel -> black (AMOLED shows nothing)
                pixels.append(0x0000)
            else:
                pixels.append(rgb888_to_rgb565(r, g, b))
    return pixels


def generate_header(name, frames_data, sprite_size, frame_count, lvgl=False):
    """Generate C header file content."""
    lines = []
    lines.append(f"#pragma once")
    lines.append(f"")
    if lvgl:
        lines.append(f"#include <lvgl.h>")
    else:
        lines.append(f"#include <Arduino.h>")
    lines.append(f"")
    lines.append(f"#define SPRITE_{name.upper()}_FRAMES {frame_count}")
    lines.append(f"#define SPRITE_{name.upper()}_SIZE {sprite_size}")
    lines.append(f"")

    for i, frame in enumerate(frames_data):
        if lvgl:
            # LVGL needs uint8_t array (2 bytes per pixel, little-endian RGB565)
            lines.append(f"static const uint8_t sprite_{name}_f{i}_data[] = {{")
            # Flatten uint16_t → 2 bytes each (little-endian)
            byte_pairs = []
            for v in frame:
                byte_pairs.append(v & 0xFF)
                byte_pairs.append((v >> 8) & 0xFF)
            for row_start in range(0, len(byte_pairs), 16):
                chunk = byte_pairs[row_start:row_start + 16]
                hex_vals = ", ".join(f"0x{v:02X}" for v in chunk)
                comma = "," if row_start + 16 < len(byte_pairs) else ""
                lines.append(f"    {hex_vals}{comma}")
            lines.append(f"}};")
            lines.append(f"")
            lines.append(f"static const lv_img_dsc_t sprite_{name}_f{i} = {{")
            lines.append(f"    .header = {{")
            lines.append(f"        .cf = LV_IMG_CF_TRUE_COLOR,")
            lines.append(f"        .always_zero = 0,")
            lines.append(f"        .reserved = 0,")
            lines.append(f"        .w = {sprite_size},")
            lines.append(f"        .h = {sprite_size},")
            lines.append(f"    }},")
            lines.append(f"    .data_size = {sprite_size} * {sprite_size} * 2,")
            lines.append(f"    .data = sprite_{name}_f{i}_data,")
            lines.append(f"}};")
            lines.append(f"")
        else:
            lines.append(f"static const uint16_t sprite_{name}_f{i}[] PROGMEM = {{")
            for row_start in range(0, len(frame), 16):
                chunk = frame[row_start:row_start + 16]
                hex_vals = ", ".join(f"0x{v:04X}" for v in chunk)
                comma = "," if row_start + 16 < len(frame) else ""
                lines.append(f"    {hex_vals}{comma}")
            lines.append(f"}};")
            lines.append(f"")

    if lvgl:
        # Array of pointers to lv_img_dsc_t
        lines.append(f"static const lv_img_dsc_t* const sprite_{name}[{frame_count}] = {{")
        ptrs = ", ".join(f"&sprite_{name}_f{i}" for i in range(frame_count))
        lines.append(f"    {ptrs}")
        lines.append(f"}};")
    else:
        lines.append(f"static const uint16_t* const sprite_{name}[] PROGMEM = {{")
        ptrs = ", ".join(f"sprite_{name}_f{i}" for i in range(frame_count))
        lines.append(f"    {ptrs}")
        lines.append(f"}};")
    lines.append(f"")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Convert PNG sprites to RGB565 C headers")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("--name", required=True, help="State name (e.g. happy, coding)")
    parser.add_argument("--frames", type=int, default=1, help="Number of frames in strip")
    parser.add_argument("--size", type=int, default=300, help="Target sprite size in pixels (default: 300)")
    parser.add_argument("--resize", action="store_true",
                        help="Resize input to --size using LANCZOS (otherwise error if size mismatch)")
    parser.add_argument("--output", help="Output .h file (default: sprite_<name>.h)")
    parser.add_argument("--lvgl", action="store_true",
                        help="Output LVGL lv_img_dsc_t format instead of raw uint16_t")
    args = parser.parse_args()

    img = Image.open(args.input).convert("RGBA")
    expected_width = args.size * args.frames
    expected_height = args.size

    if img.width != expected_width or img.height != expected_height:
        if args.resize:
            img = img.resize((expected_width, expected_height), Image.LANCZOS)
            print(f"  Resized to {expected_width}x{expected_height}")
        else:
            print(f"Warning: expected {expected_width}x{expected_height}, got {img.width}x{img.height}")
            if img.width < expected_width or img.height < expected_height:
                print("Image too small — use --resize to auto-scale")
                sys.exit(1)

    frames = []
    for i in range(args.frames):
        x_offset = i * args.size
        frame_data = convert_frame(img, x_offset, args.size)
        frames.append(frame_data)
        print(f"  Frame {i}: {len(frame_data)} pixels ({len(frame_data) * 2} bytes)")

    header = generate_header(args.name, frames, args.size, args.frames, lvgl=args.lvgl)

    out_path = args.output or f"sprite_{args.name}.h"
    with open(out_path, "w") as f:
        f.write(header)

    total_bytes = sum(len(f) * 2 for f in frames)
    print(f"\nWrote {out_path}: {args.frames} frames, {total_bytes} bytes total")


if __name__ == "__main__":
    main()
