#!/bin/bash
# Generate all DeskBuddy sprites and convert to LVGL C headers.
# Run from the repo root: bash scripts/build_sprites.sh

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SPRITE_DIR="$REPO_ROOT/firmware/include/sprites"

SPRITE_SCALE=5          # 48 * 5 = 240px per side
SPRITE_SIZE=$((48 * SPRITE_SCALE))  # 240

echo "=== Generating sprite PNGs (${SPRITE_SIZE}x${SPRITE_SIZE}) ==="
python3 "$SCRIPT_DIR/generate_sprites.py" \
    --output-dir "$SPRITE_DIR" \
    --output-scale "$SPRITE_SCALE"

echo ""
echo "=== Converting to LVGL C headers ==="
STATES="happy coding meeting rooftop focus idle thirsty overwatered loved sleeping"

for state in $STATES; do
    python3 "$SCRIPT_DIR/png_to_rgb565.py" \
        "$SPRITE_DIR/sprite_${state}.png" \
        --name "$state" \
        --size "$SPRITE_SIZE" \
        --frames 1 \
        --lvgl \
        --output "$SPRITE_DIR/sprite_${state}.h"
    echo "  $state → sprite_${state}.h"
done

echo ""
echo "Done! Headers written to $SPRITE_DIR/"
echo "Rebuild firmware: pio run --environment waveshare_amoled"
