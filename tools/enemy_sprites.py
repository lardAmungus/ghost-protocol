#!/usr/bin/env python3
"""Ghost Protocol — New Enemy Sprite Generator (6 types x 4 frames, 16x16)

Generates GBA 4bpp tile data for:
  DRONE     - Flying surveillance drone with spinning rotors
  TURRET    - Stationary gun platform on tripod
  MIMIC     - Loot chest that transforms into spider
  CORRUPTOR - Floating data corruption virus
  GHOST     - Spectral phasing entity
  BOMBER    - Heavy armored flying unit

Palette index convention:
  0 = transparent
  1 = dark outline (near-black)
  2 = dark body shade
  3 = medium body
  4 = light body / highlight
  5 = accent dark (eye/sensor dim)
  6 = accent mid (eye/sensor)
  7 = accent bright (sensor glow) — BRIGHTEST
  8 = secondary color dark
  9 = secondary mid
  A = secondary light
  B = tech accent dark
  C = tech accent bright
  D = AA edge / sub-outline
  E = detail
  F = white / flash
"""

# ============================================================
# Helper functions (from sprite_gen.py)
# ============================================================

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
        if len(px) != 16:
            raise ValueError(f"Row {y}: expected 16 pixels, got {len(px)}")
        left = row_to_u32(px[:8])
        right = row_to_u32(px[8:])
        if y < 8:
            tiles[0].append(left);  tiles[1].append(right)
        else:
            tiles[2].append(left);  tiles[3].append(right)
    return tiles

def fmt(val):
    return f"0x{val:08X}"

def tile_str(tile):
    return "{ " + ", ".join(fmt(v) for v in tile) + " }"

# ============================================================
# DRONE — Small flying surveillance drone with spinning rotors
#
# Compact metallic oval body (~8x6px center) with single sensor eye
# Two rotor arms extending left/right with blade tips
# 4 frames simulate rotor spin at 0, 45, 90, 135 degrees
#
# Palette: metallic grey (2-4), yellow sensor (5-7), dark arms (1-2)
# ============================================================

drone_frames = [
    # Frame 0: rotors horizontal (blades flat left/right)
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000000000000000",  # 2
        "0D21000000012D00",  # 3  rotor tips horizontal
        "01431111111134D0",  # 4  rotor arms connect to body
        "0000D133331D0000",  # 5  body top
        "0000135567310000",  # 6  body + sensor eye
        "0000134434310000",  # 7  body mid
        "0000D133331D0000",  # 8  body bottom
        "00000D1111D00000",  # 9  undercarriage
        "0000000000000000",  # 10
        "0000000000000000",  # 11
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 1: rotors 45 degrees (blades angled up-left / down-right)
    [
        "0000000000000000",  # 0
        "0D10000000000000",  # 1  top-left blade tip
        "00210000000000D0",  # 2  arm angles
        "000D1000001D0000",  # 3
        "00001D111D100000",  # 4  arms connect
        "0000D133331D0000",  # 5  body top
        "0000135567310000",  # 6  body + sensor
        "0000134434310000",  # 7  body mid
        "0000D133331D0000",  # 8  body bottom
        "00000D111D000000",  # 9  undercarriage
        "0000100001200000",  # 10 lower arm angles
        "0D000000000210D0",  # 11 bottom-right blade tip
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 2: rotors vertical (blades up/down)
    [
        "0000001210000000",  # 0  top blade
        "000000D1D0000000",  # 1
        "0000001D10000000",  # 2
        "0000001110000000",  # 3
        "00000D111D000000",  # 4  arms short vertical
        "0000D133331D0000",  # 5  body top
        "0000135567310000",  # 6  body + sensor
        "0000134434310000",  # 7  body mid
        "0000D133331D0000",  # 8  body bottom
        "00000D111D000000",  # 9  undercarriage
        "0000001110000000",  # 10
        "00000D1D10000000",  # 11
        "000000D1D0000000",  # 12 bottom blade
        "0000001210000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 3: rotors 135 degrees (blades angled up-right / down-left)
    [
        "0000000000000000",  # 0
        "00000000000001D0",  # 1  top-right blade tip
        "0D00000000012000",  # 2
        "0000D1000D100000",  # 3
        "000001D1D1000000",  # 4  arms connect
        "0000D133331D0000",  # 5  body top
        "0000135567310000",  # 6  body + sensor
        "0000134434310000",  # 7  body mid
        "0000D133331D0000",  # 8  body bottom
        "00000D111D000000",  # 9  undercarriage
        "0000210000100000",  # 10 lower arm angles
        "0D01000000000D00",  # 11 bottom-left blade tip
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
]

# ============================================================
# TURRET — Heavy stationary gun platform on tripod/pedestal
#
# Wide armored base (~12px), rotating gun barrel on top
# Small tracking sensor eye (5-7) above barrel
# Warning stripes on base (8-9)
# 4 frames: barrel straight, slight up, up, slight down
#
# Palette: industrial grey-blue (2-4), red warning (8-9), cyan sensor (5-7)
# ============================================================

turret_frames = [
    # Frame 0: barrel straight right
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000D1111D000000",  # 2  turret housing top
        "000013567D000000",  # 3  sensor eye row
        "000013443D000000",  # 4  housing mid
        "00001334311D0000",  # 5  barrel mount
        "000D133431BBD000",  # 6  barrel body
        "000D133431BCD000",  # 7  barrel tip
        "000D1334311D0000",  # 8  barrel mount lower
        "0000D1111D000000",  # 9  housing bottom
        "000D123321D00000",  # 10 pedestal neck
        "00D18989891D0000",  # 11 base top (warning stripes)
        "0D1898989891D000",  # 12 base mid
        "D189898989891D00",  # 13 base wide
        "D111111111111D00",  # 14 base bottom
        "0000000000000000",  # 15
    ],
    # Frame 1: barrel angled slightly up
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000D1111D000000",  # 2
        "000013567D000000",  # 3  sensor
        "000013443D0D0000",  # 4  barrel tip up
        "00001334311BD000",  # 5  barrel angled up
        "000D13343BBCD000",  # 6  barrel body angled
        "000D133431D00000",  # 7
        "000D1334311D0000",  # 8
        "0000D1111D000000",  # 9
        "000D123321D00000",  # 10
        "00D18989891D0000",  # 11
        "0D1898989891D000",  # 12
        "D189898989891D00",  # 13
        "D111111111111D00",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 2: barrel angled up
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000D1111D000000",  # 2
        "00001356BD000000",  # 3  sensor + barrel tip up high
        "000013443BCD0000",  # 4  barrel angled up high
        "0000133431BD0000",  # 5  barrel body
        "000D133431D00000",  # 6
        "000D133431D00000",  # 7
        "000D1334311D0000",  # 8
        "0000D1111D000000",  # 9
        "000D123321D00000",  # 10
        "00D18989891D0000",  # 11
        "0D1898989891D000",  # 12
        "D189898989891D00",  # 13
        "D111111111111D00",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 3: barrel angled slightly down
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000D1111D000000",  # 2
        "000013567D000000",  # 3  sensor
        "000013443D000000",  # 4
        "000013343D000000",  # 5
        "000D13343D000000",  # 6
        "000D133431BBD000",  # 7  barrel angled down
        "000D133431DBCD00",  # 8  barrel tip down
        "0000D1111D000000",  # 9
        "000D123321D00000",  # 10
        "00D18989891D0000",  # 11
        "0D1898989891D000",  # 12
        "D189898989891D00",  # 13
        "D111111111111D00",  # 14
        "0000000000000000",  # 15
    ],
]

# ============================================================
# MIMIC — Disguised as loot item, transforms into spider creature
#
# Frame 0: Innocent treasure chest/orb
# Frame 1: Chest cracking open, legs starting to emerge
# Frame 2: Full spider form — 8 legs, fangs, single eye
# Frame 3: Spider attack pose — rearing up, fangs extended
#
# Palette: Gold/brown treasure (2-4, C-D) shifting to dark spider (1-3), red eye (5-7)
# ============================================================

mimic_frames = [
    # Frame 0: loot chest (innocent)
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000000000000000",  # 2
        "0000000000000000",  # 3
        "0000111111110000",  # 4  chest lid top
        "000143434341D000",  # 5  chest lid
        "000143434341D000",  # 6  chest lid
        "000111C77C11D000",  # 7  clasp row (bright gem)
        "000134343431D000",  # 8  chest body
        "000134343431D000",  # 9  chest body
        "000123232321D000",  # 10 chest bottom
        "000111111111D000",  # 11 chest base
        "0000DDDDDDDD0000",  # 12 shadow
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 1: chest cracking, legs emerging
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000000000000000",  # 2
        "0001114411110000",  # 3  lid cracking open
        "0001434334310000",  # 4  lid tilted
        "000111C77C110000",  # 5  clasp visible
        "00D134355431D000",  # 6  eye peeking (5)
        "000134343431D000",  # 7  body
        "0D0123232321D0D0",  # 8  legs start emerging
        "0D0111111111D0D0",  # 9  base
        "D10DDDDDDDDDD01D",  # 10 legs out
        "1D00000000000D10",  # 11 legs extending
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 2: full spider form — 8 legs, fangs, eye
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "00000D1111D00000",  # 2  head top
        "0000D156731D0000",  # 3  head with eye (5-6-7)
        "0000D133331D0000",  # 4  head bottom
        "000D12333321D000",  # 5  thorax top
        "D10D123333210D1D",  # 6  thorax + front legs
        "01DD12333321DD10",  # 7  thorax + mid legs
        "001D12222221D100",  # 8  abdomen top + legs
        "0D1012222210D1D0",  # 9  abdomen + rear legs
        "D100D122221DD01D",  # 10 abdomen bottom
        "1D000D1111D000D1",  # 11 rear legs extending
        "01D000D00D000D10",  # 12 leg tips
        "001D0000000D1000",  # 13 leg tips far
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 3: spider attack pose (rearing up, fangs out)
    [
        "0000000000000000",  # 0
        "000000D11D000000",  # 1  fangs
        "00000D1571D00000",  # 2  head with bright eye
        "0000D156731D0000",  # 3  head + eye glow
        "0000D133331D0000",  # 4  head bottom
        "0D0D12333321D0D0",  # 5  thorax + raised legs
        "D10D123333210D1D",  # 6  thorax + legs wide
        "1D0D123333210D01",  # 7  mid legs spread
        "010D12222221D010",  # 8  abdomen
        "0D0D122222210D00",  # 9  abdomen + legs
        "D10D1222221DD01D",  # 10 abdomen
        "1D00D11111D00D10",  # 11 rear legs
        "0D000D000D000D00",  # 12 leg tips
        "00D00000000D0000",  # 13 tips far
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
]

# ============================================================
# CORRUPTOR — Floating data corruption virus
#
# Amorphous mass (~10x10px) with pseudopods/tendrils
# Bright pulsing center core (5-7) surrounded by dark mass
# 3-4 tendrils extending outward, shifting each frame
# Corruption pixel scatter around edges (D, 1)
#
# Palette: toxic purple (2-4), magenta core (5-7), corruption scatter (D)
# ============================================================

corruptor_frames = [
    # Frame 0: tendrils reaching up-right, core bright
    [
        "0000000D00D00000",  # 0  corruption scatter
        "000D0001D0000D00",  # 1  scatter + tendril tip
        "0000D1221D000000",  # 2  tendril
        "000D123321000000",  # 3  mass edge
        "00D1233431D00000",  # 4  mass
        "00D1345674310D00",  # 5  core row (bright 7)
        "0D12345673210000",  # 6  core bright center
        "0D12334563210000",  # 7  core lower
        "00D1233432100D00",  # 8  mass
        "00D12332210D0000",  # 9  mass edge
        "000D1221D0000000",  # 10 tendril down
        "0000D11D00000D00",  # 11 tendril tip
        "00D000D000000000",  # 12 scatter
        "0000000000D00000",  # 13 scatter
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 1: tendrils shift right-down, core pulses dimmer
    [
        "0000000000000000",  # 0
        "00000D00D00D0000",  # 1  scatter
        "000D0D12210D0000",  # 2  tendril tip
        "0000D1233210D000",  # 3  mass edge
        "000D12334321D000",  # 4  mass
        "000D13456531D000",  # 5  core row
        "00D123567632100D",  # 6  core + right tendril
        "00D12345563210D0",  # 7  core
        "000D1234432110D0",  # 8  mass + tendril
        "000D12332211D000",  # 9  mass
        "0000D122110D0D00",  # 10 tendril
        "00000D11D0D00000",  # 11 scatter
        "000000D000000000",  # 12
        "0000000000000D00",  # 13 scatter
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 2: tendrils reaching down-left, core brightest
    [
        "0000000000000000",  # 0
        "00D000000D000000",  # 1  scatter
        "0000D0D1221D0000",  # 2
        "000D12333210D000",  # 3  mass edge
        "00D123344321D000",  # 4  mass
        "00D134567431D000",  # 5  core row
        "0D1234577432100D",  # 6  core BRIGHTEST (double 7)
        "0D1233456321D000",  # 7  core lower
        "0D12233432110000",  # 8  mass + tendril left
        "00D12332110D0000",  # 9  mass
        "000D12210D0D0000",  # 10 tendril down-left
        "00D0D11D00000000",  # 11 tendril tip
        "0D000D0000000D00",  # 12 scatter
        "000000000D000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 3: tendrils reaching left-up, core pulse mid
    [
        "000D00000D000000",  # 0  scatter
        "0D00D0D12100D000",  # 1  tendril up-left
        "0000D122310D0000",  # 2  tendril
        "00D1233321D00000",  # 3  mass edge
        "00D1234432100D00",  # 4  mass
        "0D12345653210000",  # 5  core row
        "D1234567432100D0",  # 6  core + left tendril
        "0D123345632100D0",  # 7  core
        "00D12334321D0000",  # 8  mass
        "000D1233210D0000",  # 9  mass edge
        "0000D1221D000D00",  # 10 tendril
        "00000D1D0000D000",  # 11 tip
        "0000D000000D0000",  # 12 scatter
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
]

# ============================================================
# GHOST — Spectral phasing entity (phases through walls)
#
# Ethereal figure (~10x14px) with wispy lower body (no legs)
# Round head with two dim hollow eyes (5-6)
# Body fades to transparency — sparser pixels downward
# Spectral wisps below (1-2 px per row)
# 4 frames: float up/down cycle, wisps shift
#
# Palette: pale white-blue (2-4), dim blue eyes (5-6), near-transparent (D)
# ============================================================

ghost_frames = [
    # Frame 0: float position high
    [
        "00000D11D0000000",  # 0  head top
        "0000D1331D000000",  # 1  head
        "000D134431D00000",  # 2  head wide
        "000D156561D00000",  # 3  eyes (hollow 5-6)
        "000D134431D00000",  # 4  chin
        "0000D1331D000000",  # 5  neck
        "000D133331D00000",  # 6  upper body
        "000D123321D00000",  # 7  mid body
        "0000D1331D000000",  # 8  lower body
        "00000D33D0000000",  # 9  fading
        "000000D3D0000000",  # 10 fading
        "0000000D00000000",  # 11 wisp
        "000000D0D0000000",  # 12 wisp
        "00000D000D000000",  # 13 wisp trail
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 1: float position mid-high, wisps shift
    [
        "0000000000000000",  # 0
        "00000D11D0000000",  # 1  head top (shifted down 1)
        "0000D1331D000000",  # 2  head
        "000D134431D00000",  # 3  head wide
        "000D156561D00000",  # 4  eyes
        "000D134431D00000",  # 5  chin
        "0000D1331D000000",  # 6  neck
        "000D133331D00000",  # 7  upper body
        "000D123321D00000",  # 8  mid body
        "0000D1331D000000",  # 9  lower body
        "00000D33D0000000",  # 10 fading
        "0000000D00000000",  # 11 wisp
        "000000D000000000",  # 12 wisp shifted
        "0000000D0D000000",  # 13 wisp trail
        "00000D0000000000",  # 14 trail
        "0000000000000000",  # 15
    ],
    # Frame 2: float position low
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "00000D11D0000000",  # 2  head top (shifted down 2)
        "0000D1331D000000",  # 3  head
        "000D134431D00000",  # 4  head wide
        "000D156561D00000",  # 5  eyes
        "000D134431D00000",  # 6  chin
        "0000D1331D000000",  # 7  neck
        "000D133331D00000",  # 8  upper body
        "000D123321D00000",  # 9  mid body
        "0000D1331D000000",  # 10 lower body
        "00000D3D00000000",  # 11 fading
        "000000D000000000",  # 12 wisp
        "00000D00D0000000",  # 13 wisp trail
        "000000D000000000",  # 14 trail
        "0000000000000000",  # 15
    ],
    # Frame 3: float position mid-low, wisps shift opposite
    [
        "0000000000000000",  # 0
        "00000D11D0000000",  # 1  head top (back up 1)
        "0000D1331D000000",  # 2  head
        "000D134431D00000",  # 3  head wide
        "000D156561D00000",  # 4  eyes
        "000D134431D00000",  # 5  chin
        "0000D1331D000000",  # 6  neck
        "000D133331D00000",  # 7  upper body
        "000D123321D00000",  # 8  mid body
        "0000D1331D000000",  # 9  lower body
        "000000D3D0000000",  # 10 fading
        "00000D0D00000000",  # 11 wisp
        "000000D0D0000000",  # 12 wisp shifted
        "0000D00000000000",  # 13 wisp trail
        "0000000D00000000",  # 14 trail
        "0000000000000000",  # 15
    ],
]

# ============================================================
# BOMBER — Heavy armored flying unit that drops explosives
#
# Wide-body aircraft (~14x10px) with swept wings
# Chunky armored fuselage with cockpit window (5-7)
# Swept-back wings with weapon pylons
# 2 exhaust ports at rear (8-9 for heat glow)
# 4 frames: level, bank left, bank right, bomb drop
#
# Palette: dark military grey-green (2-4), orange engines (8-9), cockpit cyan (5-7)
# ============================================================

bomber_frames = [
    # Frame 0: level flight
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000000000000000",  # 2
        "0000001110000000",  # 3  nose
        "000001567D000000",  # 4  cockpit window (5-6-7)
        "00001134431D0000",  # 5  fuselage front
        "0D113344443111D0",  # 6  wings extend + fuselage
        "D1233344443321D0",  # 7  wings wide + body
        "01D1234443211D00",  # 8  wing trailing edge
        "001D1133311D1D00",  # 9  rear fuselage
        "0000D19D91D00000",  # 10 engines (heat glow 8-9)
        "00000D8D8D000000",  # 11 exhaust
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 1: banking left (left wing up, right wing down)
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0D10000000000000",  # 2  left wing tip up
        "01D1001110000000",  # 3  left wing angled + nose
        "001D01567D000000",  # 4  cockpit
        "000D1134431D0000",  # 5  fuselage
        "0001334444311D00",  # 6  body + right wing lower
        "0001234444321D00",  # 7  body
        "000D12344432D1D0",  # 8  right wing dips
        "0000D11331D0D1D0",  # 9  rear + right wing low
        "0000D19D91D00000",  # 10 engines
        "00000D8D8D000000",  # 11 exhaust
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 2: banking right (right wing up, left wing down)
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "00000000000001D0",  # 2  right wing tip up
        "0000001110001D00",  # 3  nose + right wing angled
        "000001567D01D000",  # 4  cockpit
        "00001134431D0000",  # 5  fuselage
        "001D133444431000",  # 6  left wing lower + body
        "001D123444431000",  # 7  body
        "0D1D12344321D000",  # 8  left wing dips
        "0D1D0D1331D00000",  # 9  rear + left wing low
        "00000D19D91D0000",  # 10 engines
        "000000D8D8D00000",  # 11 exhaust
        "0000000000000000",  # 12
        "0000000000000000",  # 13
        "0000000000000000",  # 14
        "0000000000000000",  # 15
    ],
    # Frame 3: bomb dropping (level + bomb beneath)
    [
        "0000000000000000",  # 0
        "0000000000000000",  # 1
        "0000000000000000",  # 2
        "0000001110000000",  # 3  nose
        "000001567D000000",  # 4  cockpit
        "00001134431D0000",  # 5  fuselage
        "0D113344443111D0",  # 6  wings + fuselage
        "D1233344443321D0",  # 7  wings wide
        "01D1234443211D00",  # 8  trailing edge
        "001D1133311D1D00",  # 9  rear
        "0000D19D91D00000",  # 10 engines
        "00000D8D8D000000",  # 11 exhaust
        "000000D11D000000",  # 12 bomb falling
        "0000000880000000",  # 13 bomb body
        "0000000110000000",  # 14 bomb tail
        "0000000000000000",  # 15
    ],
]

# ============================================================
# Validation — ensure all frames are 16 rows of 16 chars
# ============================================================

all_enemies = {
    "drone": drone_frames,
    "turret": turret_frames,
    "mimic": mimic_frames,
    "corruptor": corruptor_frames,
    "ghost": ghost_frames,
    "bomber": bomber_frames,
}

for ename, frames in all_enemies.items():
    for fi, f in enumerate(frames):
        assert len(f) == 16, f"{ename} frame {fi}: {len(f)} rows (expected 16)"
        for ri, r in enumerate(f):
            clean = r.replace(' ', '').replace('.', '0')
            assert len(clean) == 16, f"{ename} frame {fi} row {ri}: {len(clean)} chars (expected 16)"

# ============================================================
# PALETTES — 6 enemy palettes, 16 colors each as (r,g,b) tuples
# ============================================================

enemy_palettes = {
    # DRONE: metallic grey body, yellow-gold sensor glow
    "drone": [
        (0,0,0),       # 0: transparent
        (3,3,4),       # 1: outline (dark blue-grey)
        (8,9,10),      # 2: dark metallic
        (14,15,16),    # 3: mid metallic
        (20,21,22),    # 4: light metallic / highlight
        (22,18,4),     # 5: sensor dim (dark gold)
        (28,24,8),     # 6: sensor mid (gold)
        (31,30,14),    # 7: sensor bright (bright gold)
        (10,10,12),    # 8: secondary dark
        (16,16,18),    # 9: secondary mid
        (22,22,24),    # A: secondary light
        (6,8,10),      # B: tech dark
        (12,14,18),    # C: tech bright
        (5,5,6),       # D: AA edge
        (14,14,16),    # E: detail
        (31,31,31),    # F: white flash
    ],
    # TURRET: industrial grey-blue body, red warning stripes, cyan sensor
    "turret": [
        (0,0,0),       # 0: transparent
        (2,3,6),       # 1: outline (dark blue-black)
        (6,10,16),     # 2: dark grey-blue
        (12,16,22),    # 3: mid grey-blue
        (18,22,28),    # 4: light grey-blue
        (4,20,24),     # 5: sensor dim (dark cyan)
        (8,26,30),     # 6: sensor mid (cyan)
        (16,30,31),    # 7: sensor bright (bright cyan)
        (24,8,4),      # 8: warning dark (dark red)
        (30,16,8),     # 9: warning mid (red-orange)
        (31,24,12),    # A: warning light
        (4,8,14),      # B: barrel dark
        (8,14,22),     # C: barrel bright
        (3,5,10),      # D: AA edge
        (10,14,20),    # E: detail
        (31,31,31),    # F: white flash
    ],
    # MIMIC: gold/brown treasure → dark spider, red eye
    "mimic": [
        (0,0,0),       # 0: transparent
        (4,3,2),       # 1: outline (dark brown-black)
        (10,7,4),      # 2: dark body (brown/spider)
        (18,14,8),     # 3: mid body (tan/spider)
        (26,22,14),    # 4: light body (gold highlight)
        (24,4,4),      # 5: eye dim (dark red)
        (30,12,8),     # 6: eye mid (red)
        (31,24,16),    # 7: eye bright (bright amber)
        (14,10,6),     # 8: secondary dark
        (20,16,10),    # 9: secondary mid
        (26,22,16),    # A: secondary light
        (8,6,3),       # B: tech dark
        (24,20,10),    # C: clasp/gem bright (gold)
        (6,5,3),       # D: AA edge / shadow
        (16,12,8),     # E: detail
        (31,31,31),    # F: white flash
    ],
    # CORRUPTOR: toxic purple body, magenta core glow
    "corruptor": [
        (0,0,0),       # 0: transparent
        (4,2,6),       # 1: outline (dark purple)
        (10,4,14),     # 2: dark purple
        (16,8,20),     # 3: mid purple
        (22,14,26),    # 4: light purple
        (24,4,20),     # 5: core dim (dark magenta)
        (30,12,28),    # 6: core mid (magenta)
        (31,24,31),    # 7: core bright (white-magenta)
        (8,2,12),      # 8: secondary dark
        (14,6,18),     # 9: secondary mid
        (20,10,24),    # A: secondary light
        (6,2,10),      # B: tech dark
        (12,4,16),     # C: tech bright
        (5,3,8),       # D: AA edge / corruption scatter
        (14,8,18),     # E: detail
        (31,31,31),    # F: white flash
    ],
    # GHOST: pale white-blue body, dim blue eyes
    "ghost": [
        (0,0,0),       # 0: transparent
        (6,8,14),      # 1: outline (muted blue — not too dark for spectral)
        (12,16,24),    # 2: dark spectral blue
        (20,24,30),    # 3: mid spectral (pale blue-white)
        (26,28,31),    # 4: light spectral (near-white)
        (4,8,20),      # 5: eye dim (deep hollow blue)
        (8,14,28),     # 6: eye mid (blue glow)
        (16,22,31),    # 7: eye bright (unused — ghost eyes stay dim)
        (10,12,18),    # 8: secondary dark
        (16,18,24),    # 9: secondary mid
        (22,24,28),    # A: secondary light
        (8,10,16),     # B: tech dark
        (14,16,22),    # C: tech bright
        (8,10,18),     # D: AA edge / near-transparent wisps
        (18,20,26),    # E: detail
        (31,31,31),    # F: white flash
    ],
    # BOMBER: dark military grey-green body, orange engines, cyan cockpit
    "bomber": [
        (0,0,0),       # 0: transparent
        (3,4,3),       # 1: outline (dark grey-green)
        (8,10,8),      # 2: dark military green
        (14,16,14),    # 3: mid military green
        (20,22,20),    # 4: light military green
        (4,18,22),     # 5: cockpit dim (dark cyan)
        (8,24,28),     # 6: cockpit mid (cyan)
        (16,30,31),    # 7: cockpit bright (bright cyan)
        (24,12,4),     # 8: engine dark (dark orange)
        (30,20,8),     # 9: engine mid (orange glow)
        (31,26,14),    # A: engine light
        (6,8,6),       # B: tech dark
        (10,12,10),    # C: tech bright
        (5,6,5),       # D: AA edge
        (12,14,12),    # E: detail
        (31,31,31),    # F: white flash
    ],
}

# ============================================================
# OUTPUT: Tile data in C format
# ============================================================

enemy_data = [
    ("drone", drone_frames),
    ("turret", turret_frames),
    ("mimic", mimic_frames),
    ("corruptor", corruptor_frames),
    ("ghost", ghost_frames),
    ("bomber", bomber_frames),
]

print("/* ========== NEW ENEMY TILES (6 types x 4 frames) ========== */")
print("/* Generated by enemy_sprites.py */\n")

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

# ============================================================
# OUTPUT: Palette data
# ============================================================

print("/* ========== NEW ENEMY PALETTES ========== */\n")
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

print("/* ========== END NEW ENEMY SPRITES ========== */")
