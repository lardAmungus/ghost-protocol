#!/usr/bin/env python3
"""Ghost Protocol — Complete Sprite Art Generator
Converts readable pixel grids to GBA 4bpp tile data.

Palette index convention:
  0 = transparent
  1 = dark outline (near-black)
  2 = dark body shade
  3 = medium body
  4 = light body / highlight
  5 = accent dark (visor/eye dim)
  6 = accent mid (visor/eye)
  7 = accent bright (visor/eye core) — BRIGHTEST on sprite
  8 = secondary color (skin/claws/shield/warning)
  9 = secondary mid
  A = secondary light
  B = tech accent dark
  C = tech accent bright
  D = AA edge
  F = white / flash
"""

def row_to_u32(pixels):
    """Convert 8 pixels (ints 0-15) to GBA 4bpp u32. LSN = leftmost."""
    v = 0
    for i, p in enumerate(pixels):
        v |= (p & 0xF) << (i * 4)
    return v

def parse_row(s):
    """Parse a hex string row like '00113300' into list of ints."""
    s = s.replace(' ', '').replace('.', '0')
    return [int(c, 16) for c in s]

def grid16_to_tiles(grid):
    """16x16 grid -> 4 tiles [TL, TR, BL, BR], each tile = list of 8 u32."""
    tiles = [[], [], [], []]
    for y in range(16):
        px = parse_row(grid[y])
        assert len(px) == 16, f"Row {y}: expected 16 pixels, got {len(px)}"
        left = row_to_u32(px[:8])
        right = row_to_u32(px[8:])
        if y < 8:
            tiles[0].append(left);  tiles[1].append(right)
        else:
            tiles[2].append(left);  tiles[3].append(right)
    return tiles

def grid32_to_tiles(grid):
    """32x32 grid -> 16 tiles in row-major order, each = list of 8 u32."""
    tiles = [[] for _ in range(16)]
    for y in range(32):
        px = parse_row(grid[y])
        assert len(px) == 32, f"Row {y}: expected 32 pixels, got {len(px)}"
        tr = y // 8  # tile row 0-3
        pr = y % 8   # pixel row within tile
        for tc in range(4):
            chunk = px[tc*8:(tc+1)*8]
            tiles[tr*4 + tc].append(row_to_u32(chunk))
    return tiles

def grid8_to_tile(grid):
    """8x8 grid -> 1 tile = list of 8 u32."""
    tile = []
    for y in range(8):
        px = parse_row(grid[y])
        assert len(px) == 8
        tile.append(row_to_u32(px))
    return tile

def fmt(val):
    return f"0x{val:08X}"

def tile_str(tile):
    return "{ " + ", ".join(fmt(v) for v in tile) + " }"

# ============================================================
# PLAYER SPRITES — 12 frames, 16x16 each
# Samus-inspired cyberpunk netrunner facing RIGHT
# Dominant round helmet, full-face glowing visor,
# pauldron shoulder plate, 4px arm cannon, thick legs
# ============================================================
#
# Layout (facing right):
#   Rows  0-5 : large round helmet (7px wide, cols 3-9)
#   Row   6   : chin guard / neck
#   Rows  7-9 : torso + shoulder pauldron + arm cannon (cols 10-13)
#   Row  10   : waist / belt
#   Rows 11-15: legs (left: cols 2-5, right: cols 7-10)
#
# Palette indices used:
# 1=outline, 2=dark armor, 3=armor, 4=bright armor
# 5=visor dim, 6=visor mid, 7=visor bright
# 8=secondary armor dark, 9=secondary armor mid
# B=cannon dark teal, C=cannon bright cyan
# D=AA edge, F=muzzle flash white

player_frames = []

# Shared top-half (rows 0-6): helmet + chin guard — same across most frames
# Row 0: "0000011111100000"  helmet top (6 wide at cols 5-10)
# Row 1: "0001344444310000"  helmet dome
# Row 2: "0001366666310000"  visor band (dim)
# Row 3: "0001377777310000"  visor band (BRIGHT — most visible row)
# Row 4: "0001366666310000"  visor band (dim)
# Row 5: "0001344444310000"  helmet lower dome
# Row 6: "0001111111110000"  helmet rim / chin guard

# Frame 0: idle0 — standing, cannon at mid-height
player_frames.append([
    "0000011111100000",  # helmet top
    "0001344444310000",  # helmet dome
    "0001366666310000",  # visor dim
    "0001377777310000",  # visor BRIGHT
    "0001366666310000",  # visor dim
    "0001344444310000",  # helmet lower
    "0001111111110000",  # chin guard
    "0013444321BCCB10",  # shoulder + torso upper + arm cannon
    "0013444321BCCB10",  # torso mid + cannon
    "0013432100000000",  # torso lower (cannon ends)
    "0001333100000000",  # waist / belt
    "0013310013310000",  # upper legs
    "0013310013310000",  # mid legs
    "0013210013210000",  # lower legs
    "0011110011110000",  # boots
    "0000000000000000",  # empty
])

# Frame 1: idle1 — breathing (cannon tilts 1px down)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321000000",  # shoulder + torso (cannon absent row 7)
    "0013444321BCCB10",  # cannon shifted to row 8
    "0013433321BCCB10",  # cannon carries to row 9
    "0001333100000000",  # waist
    "0013310013310000",
    "0013310013310000",
    "0013210013210000",
    "0011110011110000",
    "0000000000000000",
])

# Frame 2: run0 — left leg back, right leg forward (stride A)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCB10",
    "0013444321BCCB10",
    "0013432100000000",
    "0001333100000000",
    "0133100001331000",  # left back (cols 0-4), right forward (cols 7-11)
    "1133100001331000",  # left extends back
    "0113100001321000",  # left shin trailing
    "0011100001111000",  # left foot back, right boot
    "0000000000000000",
])

# Frame 3: run1 — passing (both legs under body)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCB10",
    "0013444321BCCB10",
    "0013432100000000",
    "0001333100000000",
    "0013310013310000",  # both under body (same as idle)
    "0013310013310000",
    "0013210013210000",
    "0011110011110000",
    "0000000000000000",
])

# Frame 4: run2 — right leg back, left leg forward (stride B)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCB10",
    "0013444321BCCB10",
    "0013432100000000",
    "0001333100000000",
    "0013310001331000",  # left forward (cols 2-5), right back (cols 8-11)
    "0013310001331000",
    "0013210001330000",  # right shin trailing
    "0011110001100000",  # right foot back
    "0000000000000000",
])

# Frame 5: run3 — passing return (same as run1, slight variation)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCB10",
    "0013444321BCCB10",
    "0013432100000000",
    "0001333100000000",
    "0013320013320000",  # slight shading variation (index 2)
    "0013310013310000",
    "0013210013210000",
    "0011110011110000",
    "0000000000000000",
])

# Frame 6: jump — legs tucked up, body crouches
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCB10",
    "0013444321BCCB10",
    "0001333100000000",  # waist rises (legs tucking)
    "0013332100000000",  # thighs tuck (single mass)
    "0013321000000000",  # shins fold in
    "0011210000000000",  # feet tuck
    "0000000000000000",
    "0000000000000000",
    "0000000000000000",
])

# Frame 7: fall — legs extended straight down
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCB10",
    "0013444321BCCB10",
    "0013432100000000",
    "0001333100000000",
    "0013310013310000",
    "0013310013310000",
    "0013310013310000",  # extra extension (legs fully straight)
    "0013210013210000",
    "0011110011110000",
])

# Frame 8: shoot0 — cannon fires (muzzle flash F at tip)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCF10",  # F = muzzle flash at cannon tip
    "0013444321BCCF10",  # flash on both cannon rows
    "0013432100000000",
    "0001333100000000",
    "0013310013310000",
    "0013310013310000",
    "0013210013210000",
    "0011110011110000",
    "0000000000000000",
])

# Frame 9: shoot1 — cannon recoil (pulled back, fading flash D)
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444321BCCD10",  # D = dimming flash (recoil)
    "0013444321BCCB10",
    "0013432100000000",
    "0001333100000000",
    "0013310013310000",
    "0013310013310000",
    "0013210013210000",
    "0011110011110000",
    "0000000000000000",
])

# Frame 10: wall_slide — pressed against right wall, legs bent/crouching
player_frames.append([
    "0000011111100000",
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",
    "0001366666310000",
    "0001344444310000",
    "0001111111110000",
    "0013444310000000",  # cannon hidden (arm pressed to wall)
    "0013444310000000",
    "0013432100000000",
    "0001333100000000",
    "0013321000000000",  # crouching: legs bent under body
    "0133321000000000",
    "0133210000000000",
    "0111100000000000",
    "0000000000000000",
])

# Frame 11: dash — body low and horizontal (lean forward)
player_frames.append([
    "0000000000000000",
    "0000011111100000",  # helmet shifted down 1 row
    "0001344444310000",
    "0001366666310000",
    "0001377777310000",  # visor bright
    "0001344444310000",
    "0001111111110000",
    "1344444321BCCF10",  # full-width lean + cannon + muzzle flash
    "0134444321BCCB10",
    "0000133210000000",  # trailing legs (flat)
    "0001332100000000",
    "0013321000000000",
    "0111100000000000",
    "0000000000000000",
    "0000000000000000",
    "0000000000000000",
])

# Validate all frames have 16 rows of 16 chars
for fi, f in enumerate(player_frames):
    assert len(f) == 16, f"Player frame {fi}: {len(f)} rows"
    for ri, r in enumerate(f):
        clean = r.replace(' ','').replace('.','0')
        assert len(clean) == 16, f"Player frame {fi} row {ri}: {len(clean)} chars"

# ============================================================
# PLAYER PALETTES — 3 classes
# ============================================================

# RGB15_C(r,g,b) values
player_palettes = {
    "assault": [
        (0,0,0),      # 0: transparent
        (2,3,8),      # 1: outline (very dark blue-black)
        (6,10,18),    # 2: dark armor
        (10,16,24),   # 3: medium armor
        (16,22,28),   # 4: bright armor
        (24,4,2),     # 5: visor dim (dark red)
        (30,10,6),    # 6: visor mid (red-orange)
        (31,20,10),   # 7: visor bright (bright orange)
        (14,10,8),    # 8: skin shadow
        (22,18,14),   # 9: skin
        (28,24,18),   # A: skin light
        (2,10,16),    # B: cannon dark teal
        (6,22,28),    # C: cannon bright cyan
        (4,6,14),     # D: AA edge
        (14,18,26),   # E: unused
        (31,31,31),   # F: white flash
    ],
    "infiltrator": [
        (0,0,0),
        (3,3,4),      # 1: outline (near-black)
        (6,7,8),      # 2: dark grey
        (10,11,12),   # 3: medium grey
        (16,17,18),   # 4: light grey
        (2,14,4),     # 5: visor dim (dark green)
        (4,24,8),     # 6: visor mid (green)
        (8,31,14),    # 7: visor bright (bright green)
        (14,10,8),    # 8: skin shadow
        (22,18,14),   # 9: skin
        (28,24,18),   # A: skin light
        (2,8,6),      # B: tech dark
        (4,16,12),    # C: tech mid
        (4,5,6),      # D: AA edge
        (12,13,14),   # E: unused
        (31,31,31),   # F: white flash
    ],
    "technomancer": [
        (0,0,0),
        (6,3,2),      # 1: outline (dark brown-black)
        (12,8,4),     # 2: dark brown
        (18,14,8),    # 3: medium brown
        (24,20,14),   # 4: light brown/tan
        (20,14,2),    # 5: visor dim (dark amber)
        (28,22,6),    # 6: visor mid (amber)
        (31,30,12),   # 7: visor bright (bright gold)
        (14,10,8),    # 8: skin shadow
        (22,18,14),   # 9: skin
        (28,24,18),   # A: skin light
        (14,10,2),    # B: tech dark
        (24,18,4),    # C: tech mid
        (8,5,3),      # D: AA edge
        (18,14,10),   # E: unused
        (31,31,31),   # F: white flash
    ],
}

# ============================================================
# OUTPUT: Player tile data
# ============================================================

print("/* ========== PLAYER TILES ========== */")
print("static const u32 player_tiles[48][8] = {")

for fi, frame in enumerate(player_frames):
    tiles = grid16_to_tiles(frame)
    names = ["TL", "TR", "BL", "BR"]
    for ti, tile in enumerate(tiles):
        idx = fi * 4 + ti
        label = f"/* Frame {fi} {names[ti]} */"
        print(f"    {tile_str(tile)}, {label}")

print("};")
print()

# Output palettes
for cls_name, pal in player_palettes.items():
    print(f"/* {cls_name} palette */")
    parts = []
    for i, (r, g, b) in enumerate(pal):
        if i == 0:
            parts.append("0x0000")
        else:
            parts.append(f"RGB15_C({r},{g},{b})")
    # Print in groups of 4
    print("{ " + ", ".join(parts) + " },")
    print()

print("\n/* ========== END PLAYER ========== */\n")

# ============================================================
# ENEMY SPRITES — 6 types, 16x16 each
# Each has clear silhouette, bright eye/sensor, dark outline
#
# Palette convention per enemy (each gets own 16-color palette):
#   0=transparent, 1=outline(near-black), 2=dark body, 3=body,
#   4=highlight, 5=eye dim, 6=eye mid, 7=eye bright,
#   8=accent/detail, 9=accent2, B=tech, C=tech bright
# ============================================================

# ---- SENTRY: Turret with sensor eye + barrel ----
# 4 frames: idle, scan (eye shifts), fire (barrel flash), damaged
sentry_frames = [
    # Frame 0: idle
    [
        "0000000000000000",
        "0000000000000000",
        "0000111110000000",
        "0001567631000000",
        "0001344310000000",
        "0001344310000000",
        "0000111110000000",
        "0000131111110000",
        "0000143334410000",
        "0000131111110000",
        "0000111110000000",
        "0001234321000000",
        "0012344321000000",
        "0011111111000000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 1: scanning (eye wider)
    [
        "0000000000000000",
        "0000000000000000",
        "0000111110000000",
        "0001577731000000",
        "0001344310000000",
        "0001344310000000",
        "0000111110000000",
        "0000131111110000",
        "0000143334410000",
        "0000131111110000",
        "0000111110000000",
        "0001234321000000",
        "0012344321000000",
        "0011111111000000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 2: firing (barrel flash)
    [
        "0000000000000000",
        "0000000000000000",
        "0000111110000000",
        "0001567631000000",
        "0001344310000000",
        "0001344310000000",
        "0000111110000000",
        "0000131111111F00",
        "000014333441FF00",
        "0000131111111F00",
        "0000111110000000",
        "0001234321000000",
        "0012344321000000",
        "0011111111000000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 3: damaged (flicker)
    [
        "0000000000000000",
        "0000000000000000",
        "0000011110000000",
        "0001567630000000",
        "0001344310000000",
        "0001344310000000",
        "0000111100000000",
        "0000131111100000",
        "0000143334410000",
        "0000131111110000",
        "0000111110000000",
        "0001234321000000",
        "0012344321000000",
        "0011111111000000",
        "0000000000000000",
        "0000000000000000",
    ],
]

# ---- PATROL: Walking humanoid guard robot ----
# 4 frames: idle, walk1, walk2, attack
patrol_frames = [
    # Frame 0: idle (wide stance)
    [
        "0000D111D0000000",
        "000134443D000000",
        "001156665D000000",
        "000134443D000000",
        "0000D111D0000000",
        "00D1234321D00000",
        "0D123434321D0000",
        "0D123434321D0000",
        "00D1234321D00000",
        "000D13431D000000",
        "000D31D31D000000",
        "000D31D31D000000",
        "00D121D121D00000",
        "00D131D131D00000",
        "00D111D111D00000",
        "0000000000000000",
    ],
    # Frame 1: walk — left leg forward
    [
        "0000D111D0000000",
        "000134443D000000",
        "001156665D000000",
        "000134443D000000",
        "0000D111D0000000",
        "00D1234321D00000",
        "0D123434321D0000",
        "0D123434321D0000",
        "00D1234321D00000",
        "000D1D431D000000",
        "00D31D0D31D00000",
        "0D31D00D31D00000",
        "0D21D0D121D00000",
        "0D11D0D131D00000",
        "00DD00D111D00000",
        "0000000000000000",
    ],
    # Frame 2: walk — right leg forward
    [
        "0000D111D0000000",
        "000134443D000000",
        "001156665D000000",
        "000134443D000000",
        "0000D111D0000000",
        "00D1234321D00000",
        "0D123434321D0000",
        "0D123434321D0000",
        "00D1234321D00000",
        "000D134D1D000000",
        "000D31D0D31D0000",
        "000D31D00D31D000",
        "00D121D00D21D000",
        "00D131D00D11D000",
        "00D111D000DD0000",
        "0000000000000000",
    ],
    # Frame 3: attack (arm extended)
    [
        "0000D111D0000000",
        "000134443D000000",
        "001156665D000000",
        "000134443D000000",
        "0000D111D0000000",
        "00D1234321D00000",
        "0D12343432111000",
        "0D12343432134100",
        "00D123432111F100",
        "000D13431D000000",
        "000D31D31D000000",
        "000D31D31D000000",
        "00D121D121D00000",
        "00D131D131D00000",
        "00D111D111D00000",
        "0000000000000000",
    ],
]

# ---- FLYER: Drone with rotors ----
# 4 frames: rotor up, rotor mid, rotor down, damaged
flyer_frames = [
    # Frame 0: rotors up
    [
        "0000000000000000",
        "0D41000000014D00",
        "01431000000134D0",
        "0000D111D0000000",
        "0001134311000000",
        "0001567651000000",
        "0001134311000000",
        "0000D131D0000000",
        "0000D111D0000000",
        "00000D1D00000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 1: rotors mid
    [
        "0000000000000000",
        "0000000000000000",
        "0D41D111D014D000",
        "01431344310134D0",
        "0001567651000000",
        "0001134311000000",
        "0000D131D0000000",
        "0000D111D0000000",
        "00000D1D00000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 2: rotors down
    [
        "0000000000000000",
        "0000000000000000",
        "0000D111D0000000",
        "0001134311000000",
        "0001567651000000",
        "0D411343110014D0",
        "01431D131D0134D0",
        "0000D111D0000000",
        "00000D1D00000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 3: damaged
    [
        "0000000000000000",
        "0D41000000014D00",
        "01431000000034D0",
        "0000D111D0000000",
        "0001134311000000",
        "0001567651000000",
        "0001134311000000",
        "0000D131D0000000",
        "0000D1D1D0000000",
        "00000D0D00000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
    ],
]

# ---- SHIELD: Heavy mech ----
# 4 frames: idle, blocking, charging, stagger
shield_frames = [
    # Frame 0: idle
    [
        "00D11111111D0000",
        "0D134444431D0000",
        "0D156777651D0000",
        "0D134444431D0000",
        "00D11111111D0000",
        "0D1234343210D000",
        "D12343434321D000",
        "D12343434321D000",
        "0D1234343210D000",
        "00D12344321D0000",
        "000D1341431D0000",
        "000D1341431D0000",
        "00D1231D1321D000",
        "00D1341D1341D000",
        "00D1111D1111D000",
        "0000000000000000",
    ],
    # Frame 1: blocking (shield up on left)
    [
        "00D11111111D0000",
        "0D134444431D0000",
        "0D156777651D0000",
        "0D134444431D0000",
        "00D11111111D0000",
        "881234343210D000",
        "881234343421D000",
        "881234343421D000",
        "881234343210D000",
        "88D12344321D0000",
        "000D1341431D0000",
        "000D1341431D0000",
        "00D1231D1321D000",
        "00D1341D1341D000",
        "00D1111D1111D000",
        "0000000000000000",
    ],
    # Frame 2: charging forward
    [
        "00D11111111D0000",
        "0D134444431D0000",
        "0D156777651D0000",
        "0D134444431D0000",
        "00D11111111D0000",
        "0D1234343210D000",
        "D12343434321D000",
        "D12343434321D000",
        "0D1234343210D000",
        "00D12344321D0000",
        "000D134D431D0000",
        "00D1341D0431D000",
        "0D1231D001321D00",
        "0D1341D001341D00",
        "0D1111D001111D00",
        "0000000000000000",
    ],
    # Frame 3: stagger (leaning back)
    [
        "000D11111111D000",
        "00D134444431D000",
        "00D156777651D000",
        "00D134444431D000",
        "000D11111111D000",
        "00D1234343210D00",
        "0D12343434321D00",
        "0D12343434321D00",
        "00D1234343210D00",
        "000D12344321D000",
        "000D1341431D0000",
        "000D1341431D0000",
        "00D1231D1321D000",
        "00D1341D1341D000",
        "00D1111D1111D000",
        "0000000000000000",
    ],
]

# ---- SPIKE: Hazard column ----
# 3 frames: retracted, extending, extended
spike_frames = [
    # Frame 0: retracted (no spike visible)
    [
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
        "0000D11D00000000",
        "0000131000000000",
        "0000181000000000",
        "0000131000000000",
        "0000181000000000",
        "0000131000000000",
        "0000141000000000",
        "0000131000000000",
        "0000181000000000",
        "000D1311D0000000",
        "00D111111D000000",
        "0000000000000000",
    ],
    # Frame 1: extending (spike emerging)
    [
        "0000000000000000",
        "0000000000000000",
        "0000070000000000",
        "0000151000000000",
        "0000D11D00000000",
        "0000131000000000",
        "0000181000000000",
        "0000131000000000",
        "0000181000000000",
        "0000131000000000",
        "0000141000000000",
        "0000131000000000",
        "0000181000000000",
        "000D1311D0000000",
        "00D111111D000000",
        "0000000000000000",
    ],
    # Frame 2: extended (full spike)
    [
        "0000070000000000",
        "0000171000000000",
        "0000151000000000",
        "0000171000000000",
        "0000151000000000",
        "0000D11D00000000",
        "0000131000000000",
        "0000181000000000",
        "0000131000000000",
        "0000181000000000",
        "0000131000000000",
        "0000141000000000",
        "0000181000000000",
        "000D1311D0000000",
        "00D111111D000000",
        "0000000000000000",
    ],
]

# ---- HUNTER: Lean predator ----
# 4 frames: crouch, run1, run2, pounce
hunter_frames = [
    # Frame 0: crouch (ready stance)
    [
        "00000D11D0000000",
        "0000D1341D000000",
        "0000D1571D000000",
        "0000D1341D000000",
        "00000D11D0000000",
        "000D123210000000",
        "000D183810000000",
        "000D123210000000",
        "0000D131D0000000",
        "0000D131D0000000",
        "000D31D31D000000",
        "000D31D31D000000",
        "00D131D131D00000",
        "00D131D131D00000",
        "00D111D111D00000",
        "0000000000000000",
    ],
    # Frame 1: run — left forward
    [
        "00000D11D0000000",
        "0000D1341D000000",
        "0000D1571D000000",
        "0000D1341D000000",
        "00000D11D0000000",
        "000D123210000000",
        "000D183810000000",
        "000D123210000000",
        "0000D131D0000000",
        "000D31D01D000000",
        "00D31D0D31D00000",
        "0D31D00D31D00000",
        "0D11D0D131D00000",
        "00DD00D111D00000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 2: run — right forward
    [
        "00000D11D0000000",
        "0000D1341D000000",
        "0000D1571D000000",
        "0000D1341D000000",
        "00000D11D0000000",
        "000D123210000000",
        "000D183810000000",
        "000D123210000000",
        "0000D131D0000000",
        "0000D1D31D000000",
        "000D31D0D31D0000",
        "000D31D00D31D000",
        "00D131D00D11D000",
        "00D111D000DD0000",
        "0000000000000000",
        "0000000000000000",
    ],
    # Frame 3: pounce (leaping)
    [
        "00000D11D0000000",
        "0000D1341D000000",
        "0000D1571D000000",
        "0000D1341D000000",
        "00000D11D0000000",
        "000D12321D000000",
        "000D18381D000000",
        "000D12321D000000",
        "0000D131D0000000",
        "000D31D31D000000",
        "00D31DD31D000000",
        "00D111D111D00000",
        "000D1D0D1D000000",
        "0000000000000000",
        "0000000000000000",
        "0000000000000000",
    ],
]

# Enemy palettes (high contrast, each type distinct)
enemy_palettes = {
    "sentry": [
        (0,0,0), (2,4,6), (8,14,18), (14,20,24), (20,26,30),
        (24,4,4), (31,10,8), (31,22,16), (6,18,24), (10,24,28),
        (0,0,0), (4,12,16), (8,22,28), (4,8,12), (0,0,0), (31,31,31),
    ],
    "patrol": [
        (0,0,0), (4,2,2), (16,8,4), (22,14,6), (28,20,10),
        (24,4,2), (30,12,4), (31,24,8), (14,10,8), (22,18,14),
        (0,0,0), (12,6,2), (20,12,4), (6,4,3), (0,0,0), (31,31,31),
    ],
    "flyer": [
        (0,0,0), (2,4,2), (4,12,6), (8,18,10), (14,24,16),
        (20,20,4), (28,28,8), (31,31,16), (6,14,8), (10,20,12),
        (0,0,0), (4,10,6), (8,18,10), (3,6,3), (0,0,0), (31,31,31),
    ],
    "shield": [
        (0,0,0), (3,3,4), (8,10,12), (14,16,18), (20,22,24),
        (4,8,24), (8,16,30), (16,24,31), (6,6,8), (10,10,12),
        (0,0,0), (6,8,12), (12,14,18), (5,5,6), (0,0,0), (31,31,31),
    ],
    "spike": [
        (0,0,0), (4,2,2), (18,6,2), (24,10,4), (28,16,6),
        (28,20,4), (31,26,8), (31,31,16), (20,4,4), (24,8,4),
        (0,0,0), (14,4,2), (22,8,4), (6,3,2), (0,0,0), (31,31,31),
    ],
    "hunter": [
        (0,0,0), (4,2,6), (12,6,16), (18,10,22), (24,16,28),
        (24,4,20), (31,10,28), (31,20,31), (16,8,12), (22,12,18),
        (0,0,0), (10,4,14), (18,8,22), (6,3,8), (0,0,0), (31,31,31),
    ],
}

# ---- OUTPUT ENEMY TILES ----
enemy_data = [
    ("sentry", sentry_frames),
    ("patrol", patrol_frames),
    ("flyer", flyer_frames),
    ("shield", shield_frames),
    ("spike", spike_frames),
    ("hunter", hunter_frames),
]

print("\n/* ========== ENEMY TILES ========== */\n")

for ename, frames in enemy_data:
    nf = len(frames)
    print(f"static const u32 spr_{ename}[{nf}][4][8] = {{")
    for fi, frame in enumerate(frames):
        tiles = grid16_to_tiles(frame)
        print(f"  {{ /* frame {fi} */")
        for ti, tile in enumerate(tiles):
            print(f"    {tile_str(tile)},")
        print(f"  }},")
    print(f"}};")
    print()

# Output enemy palettes
print("/* Enemy palettes */")
for ename, pal in enemy_palettes.items():
    parts = []
    for i, (r, g, b) in enumerate(pal):
        if r == 0 and g == 0 and b == 0:
            parts.append("0x0000")
        else:
            parts.append(f"RGB15_C({r},{g},{b})")
    print(f"/* pal_{ename} */")
    print("{ " + ", ".join(parts) + " },")
print()

# ============================================================
# PROJECTILE SPRITES — 4 variants, 8x8 each
# ============================================================

proj_buster = [
    "00000000",
    "00011000",
    "00122100",
    "01233210",
    "01233210",
    "00122100",
    "00011000",
    "00000000",
]

proj_rapid = [
    "00000000",
    "00000000",
    "00011000",
    "00123100",
    "00123100",
    "00011000",
    "00000000",
    "00000000",
]

proj_charge = [
    "00111000",
    "01232100",
    "12343210",
    "12344310",
    "12344310",
    "12343210",
    "01232100",
    "00111000",
]

proj_enemy = [
    "00010000",
    "00121000",
    "01232100",
    "00232100",
    "00123200",
    "00123210",
    "00012100",
    "00001000",
]

print("\n/* ========== PROJECTILE TILES ========== */\n")
for name, grid in [("buster", proj_buster), ("rapid", proj_rapid),
                   ("charge", proj_charge), ("enemy", proj_enemy)]:
    tile = grid8_to_tile(grid)
    print(f"static const u32 proj_tile_{name}[8] = {tile_str(tile)};")
print()

# Projectile palettes (keep existing — they work well)
print("/* Projectile palettes unchanged — cyan player, red enemy */")
print()

# ============================================================
# ITEM DROP SPRITES — 3 variants, 8x8 each
# ============================================================

drop_gem = [
    "00011000",
    "00122100",
    "01233210",
    "12344321",
    "01233210",
    "00122100",
    "00011000",
    "00000000",
]

drop_chip = [
    "00000000",
    "01111110",
    "01234310",
    "01343410",
    "01343410",
    "01234310",
    "01111110",
    "00000000",
]

drop_orb = [
    "00011000",
    "00133100",
    "01344310",
    "13F44F31",
    "13444431",
    "01344310",
    "00133100",
    "00011000",
]

print("\n/* ========== ITEM DROP TILES ========== */\n")
for name, grid in [("gem", drop_gem), ("chip", drop_chip), ("orb", drop_orb)]:
    tile = grid8_to_tile(grid)
    print(f"static const u32 spr_drop_{name}[8] = {tile_str(tile)};")
print()

print("\n/* ========== END ALL SPRITES ========== */\n")
