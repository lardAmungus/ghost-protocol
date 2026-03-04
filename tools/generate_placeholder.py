#!/usr/bin/env python3
"""
Generate placeholder indexed PNGs for GBA development.
Outputs 4bpp-compatible images (max 16 colors, index 0 = transparent).
Use grit to convert to C arrays for inclusion in the build.

Usage:
    python3 generate_placeholder.py sprite 16 16 output.png
    python3 generate_placeholder.py tile 8 8 output.png
"""

import sys
from PIL import Image


# Default 16-color palette: index 0 = magenta (transparent)
DEFAULT_PALETTE = [
    (255,   0, 255),  # 0: transparent (magenta)
    (  0,   0,   0),  # 1: black
    (255, 255, 255),  # 2: white
    (128, 128, 128),  # 3: gray
    (255,   0,   0),  # 4: red
    (  0, 255,   0),  # 5: green
    (  0,   0, 255),  # 6: blue
    (255, 255,   0),  # 7: yellow
    (255, 128,   0),  # 8: orange
    (128,   0, 255),  # 9: purple
    (  0, 255, 255),  # 10: cyan
    (128,  64,   0),  # 11: brown
    (255, 192, 203),  # 12: pink
    ( 64, 128,  64),  # 13: dark green
    ( 64,  64, 128),  # 14: dark blue
    (192, 192, 192),  # 15: light gray
]


def create_indexed_png(width, height, palette_rgb, pixel_data, output_path):
    """Create a 4bpp-compatible indexed PNG for grit conversion."""
    img = Image.new('P', (width, height))

    flat_palette = []
    for r, g, b in palette_rgb:
        flat_palette.extend([r, g, b])
    flat_palette.extend([0] * (768 - len(flat_palette)))
    img.putpalette(flat_palette)

    pixels = img.load()
    for y in range(height):
        for x in range(width):
            pixels[x, y] = pixel_data[y][x]

    img.save(output_path)
    print(f"Created {output_path} ({width}x{height}, {len(palette_rgb)} colors)")


def generate_sprite(width, height):
    """Generate a simple character placeholder (colored rectangle with outline)."""
    data = []
    for y in range(height):
        row = []
        for x in range(width):
            if x == 0 or x == width - 1 or y == 0 or y == height - 1:
                row.append(1)   # Black outline
            elif y < height // 4:
                row.append(12)  # Pink head area
            elif y < height * 3 // 4:
                row.append(6)   # Blue body
            else:
                row.append(11)  # Brown legs
            row[-1] if (x > 0 and x < width - 1 and y > 0 and y < height - 1) else row[-1]
        data.append(row)
    return data


def generate_tile(width, height):
    """Generate a simple terrain tile placeholder."""
    data = []
    for y in range(height):
        row = []
        for x in range(width):
            if (x + y) % 4 == 0:
                row.append(13)  # Dark green accent
            else:
                row.append(5)   # Green base
        data.append(row)
    return data


def main():
    if len(sys.argv) < 5:
        print(f"Usage: {sys.argv[0]} <sprite|tile> <width> <height> <output.png>")
        sys.exit(1)

    kind = sys.argv[1]
    width = int(sys.argv[2])
    height = int(sys.argv[3])
    output = sys.argv[4]

    if kind == "sprite":
        data = generate_sprite(width, height)
    elif kind == "tile":
        data = generate_tile(width, height)
    else:
        print(f"Unknown type: {kind}. Use 'sprite' or 'tile'.")
        sys.exit(1)

    create_indexed_png(width, height, DEFAULT_PALETTE, data, output)


if __name__ == "__main__":
    main()
