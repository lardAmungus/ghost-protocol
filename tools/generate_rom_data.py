#!/usr/bin/env python3
"""
Generate large const data arrays for Ghost Protocol GBA ROM.

Produces ~4MB of C source data across 5 files:
  - content_cutscenes.c  (~2MB)  — 120 cutscene frames, 4bpp tile data
  - content_sprites.c    (~1MB)  — expanded sprite tile data
  - content_codex.c      (~300KB) — codex/bestiary/lore text
  - content_levels.c     (~500KB) — level section templates
  - content_data.h                — header with externs/structs
"""

import os
import math
import struct

# ---------------------------------------------------------------------------
# Deterministic PRNG (xorshift32)
# ---------------------------------------------------------------------------
class PRNG:
    def __init__(self, seed=0xDEADBEEF):
        self.state = seed & 0xFFFFFFFF
    def next(self):
        x = self.state
        x ^= (x << 13) & 0xFFFFFFFF
        x ^= (x >> 17)
        x ^= (x << 5) & 0xFFFFFFFF
        self.state = x & 0xFFFFFFFF
        return self.state
    def range(self, lo, hi):
        return lo + (self.next() % (hi - lo + 1))
    def byte(self):
        return self.next() & 0xFF

# ---------------------------------------------------------------------------
# Tile generation helpers (each tile = 8 u32, representing 8x8 4bpp pixels)
# A row of 8 pixels at 4bpp = one u32 (each pixel = 4 bits, LSB first)
# ---------------------------------------------------------------------------

def row_from_pixels(pixels):
    """Convert list of 8 pixel values (0-15) to a u32."""
    val = 0
    for i, p in enumerate(pixels[:8]):
        val |= (p & 0xF) << (i * 4)
    return val & 0xFFFFFFFF

def make_tile(row_func):
    """Make tile (8 u32) from a function(row_index) -> [8 pixels]."""
    return [row_from_pixels(row_func(r)) for r in range(8)]

def solid_tile(color):
    return make_tile(lambda r: [color]*8)

def gradient_h_tile(c1, c2):
    """Horizontal gradient across tile."""
    return make_tile(lambda r: [c1 + (c2-c1)*x//7 for x in range(8)])

def gradient_v_tile(c1, c2):
    """Vertical gradient down tile."""
    return make_tile(lambda r: [c1 + (c2-c1)*r//7]*8)

def checker_tile(c1, c2, size=1):
    def row(r):
        return [c1 if ((r//size + x//size) % 2 == 0) else c2 for x in range(8)]
    return make_tile(row)

def stripe_h_tile(c1, c2, width=2):
    return make_tile(lambda r: [c1 if (r % (width*2) < width) else c2]*8)

def stripe_v_tile(c1, c2, width=2):
    return make_tile(lambda r: [c1 if (x % (width*2) < width) else c2 for x in range(8)])

def dither_tile(c1, c2, rng):
    def row(r):
        return [c1 if rng.range(0,1)==0 else c2 for _ in range(8)]
    return make_tile(row)

def circle_tile(fg, bg, cx=3.5, cy=3.5, radius=3.5):
    def row(r):
        pixels = []
        for x in range(8):
            dist = math.sqrt((x - cx)**2 + (r - cy)**2)
            pixels.append(fg if dist <= radius else bg)
        return pixels
    return make_tile(row)

def circuit_tile(trace, node, bg, rng):
    """Circuit board pattern tile."""
    grid = [[bg]*8 for _ in range(8)]
    # horizontal trace
    hr = rng.range(2, 5)
    for x in range(8):
        grid[hr][x] = trace
    # vertical trace
    vc = rng.range(2, 5)
    for r in range(8):
        grid[r][vc] = trace
    # node at intersection
    grid[hr][vc] = node
    return [row_from_pixels(grid[r]) for r in range(8)]

def wave_tile(fg, bg, amplitude=2, phase=0):
    """Sine wave pattern."""
    def row(r):
        pixels = []
        for x in range(8):
            wave_y = 3.5 + amplitude * math.sin((x + phase) * math.pi / 4)
            pixels.append(fg if abs(r - wave_y) < 1.0 else bg)
        return pixels
    return make_tile(row)

def noise_tile(rng, max_color=15):
    return make_tile(lambda r: [rng.range(0, max_color) for _ in range(8)])

def diamond_tile(fg, bg):
    def row(r):
        pixels = []
        for x in range(8):
            dist = abs(x - 3.5) + abs(r - 3.5)
            pixels.append(fg if dist <= 3.5 else bg)
        return pixels
    return make_tile(row)

def triangle_tile(fg, bg):
    def row(r):
        half = r // 2
        return [fg if half <= x <= 7 - half else bg for x in range(8)]
    return make_tile(row)

def rect_tile(fg, bg, x0, y0, x1, y1):
    def row(r):
        return [fg if x0 <= x <= x1 and y0 <= r <= y1 else bg for x in range(8)]
    return make_tile(row)

# ---------------------------------------------------------------------------
# Cutscene frame generators — each frame = 600 tiles (240x160 px = 30x20 tiles)
# ---------------------------------------------------------------------------
TILES_PER_FRAME = 600  # 30 cols x 20 rows of 8x8 tiles

def gen_title_frame(frame_idx, rng):
    """Title sequence: glitch text + circuit patterns."""
    tiles = []
    for t in range(TILES_PER_FRAME):
        row = t // 30
        col = t % 30
        if 8 <= row <= 12:
            # Text band — glitch pattern
            if (frame_idx + col) % 3 == 0:
                tiles.extend(circuit_tile(10 + frame_idx % 5, 15, 1, rng))
            else:
                phase = frame_idx * 0.5 + col * 0.3
                tiles.extend(wave_tile(12, 1, 2, phase))
        elif row < 4:
            # Top — scrolling circuit traces
            tiles.extend(circuit_tile(6, 8, 0, rng))
        elif row > 16:
            # Bottom — gradient fade
            tiles.extend(gradient_v_tile(3, 0))
        else:
            # Mid — dither background
            tiles.extend(dither_tile(1, 2, rng))
    return tiles

def gen_act_intro_frame(act, sub_frame, rng):
    """Act intro cards: act number + geometric patterns."""
    tiles = []
    bg_color = 1 + act
    accent = 10 + act
    for t in range(TILES_PER_FRAME):
        row = t // 30
        col = t % 30
        # Border
        if row == 0 or row == 19 or col == 0 or col == 29:
            tiles.extend(solid_tile(accent))
        # Inner border
        elif row == 1 or row == 18 or col == 1 or col == 28:
            tiles.extend(stripe_h_tile(accent, bg_color, 1 + sub_frame))
        # Center title area
        elif 8 <= row <= 11 and 8 <= col <= 21:
            if sub_frame == 0:
                tiles.extend(gradient_h_tile(accent, bg_color))
            elif sub_frame == 1:
                tiles.extend(checker_tile(accent, bg_color, 2))
            else:
                tiles.extend(diamond_tile(accent, bg_color))
        # Geometric accents
        elif (row + col + act) % 7 == 0:
            tiles.extend(circle_tile(accent, bg_color))
        else:
            tiles.extend(solid_tile(bg_color))
    return tiles

def gen_boss_intro_frame(boss_idx, sub_frame, rng):
    """Boss intro: silhouettes + warning symbols."""
    tiles = []
    bg = 1
    silhouette = 0  # black
    warn = 14  # bright red
    for t in range(TILES_PER_FRAME):
        row = t // 30
        col = t % 30
        # Warning stripes at top/bottom
        if row < 3 or row > 16:
            if (col + sub_frame) % 4 < 2:
                tiles.extend(stripe_v_tile(warn, 8, 2))
            else:
                tiles.extend(solid_tile(8))
        # Boss silhouette zone (center)
        elif 6 <= row <= 14 and 10 <= col <= 19:
            cx, cy = 14.5, 10
            dist = math.sqrt((col - cx)**2 + (row - cy)**2)
            if dist < 4 + boss_idx * 0.3:
                tiles.extend(solid_tile(silhouette))
            elif dist < 5 + boss_idx * 0.3:
                tiles.extend(gradient_h_tile(silhouette, bg))
            else:
                tiles.extend(dither_tile(bg, bg + 1, rng))
        # Side warning indicators
        elif col < 4 or col > 25:
            if sub_frame >= 3:
                tiles.extend(circle_tile(warn, bg))
            else:
                tiles.extend(triangle_tile(warn, bg))
        else:
            tiles.extend(noise_tile(rng, 3))
    return tiles

def gen_story_frame(scene_idx, rng):
    """Story scenes: cityscapes, data streams, hacking."""
    tiles = []
    scene_type = scene_idx % 4
    for t in range(TILES_PER_FRAME):
        row = t // 30
        col = t % 30
        if scene_type == 0:
            # Cityscape
            if row > 14:
                tiles.extend(solid_tile(5))  # ground
            elif row > 10 - (col % 5):
                tiles.extend(rect_tile(7, 1, 1, 1, 6, 6))  # buildings
            else:
                tiles.extend(gradient_v_tile(2, 4))  # sky
        elif scene_type == 1:
            # Data stream
            if col % 3 == 0:
                tiles.extend(gradient_v_tile(10, 2))
            elif (row + scene_idx) % 4 == 0:
                tiles.extend(stripe_h_tile(11, 0, 1))
            else:
                tiles.extend(solid_tile(0))
        elif scene_type == 2:
            # Hacking terminal
            if 3 <= row <= 16 and 2 <= col <= 27:
                if row == 3 or row == 16:
                    tiles.extend(solid_tile(7))
                elif col == 2 or col == 27:
                    tiles.extend(solid_tile(7))
                else:
                    tiles.extend(circuit_tile(10, 12, 0, rng))
            else:
                tiles.extend(solid_tile(1))
        else:
            # Abstract network visualization
            cx, cy = 15, 10
            dist = abs(col - cx) + abs(row - cy)
            if dist < 3:
                tiles.extend(circle_tile(15, 0))
            elif dist < 8:
                tiles.extend(wave_tile(10, 0, 1, scene_idx + dist))
            else:
                tiles.extend(dither_tile(0, 1, rng))
    return tiles

def gen_ending_frame(frame_idx, rng):
    """Ending: victory celebration, credits backgrounds."""
    tiles = []
    for t in range(TILES_PER_FRAME):
        row = t // 30
        col = t % 30
        if frame_idx < 10:
            # Victory — expanding circles
            cx, cy = 15, 10
            dist = math.sqrt((col - cx)**2 + (row - cy)**2)
            ring = (frame_idx * 2 + 3)
            if abs(dist - ring) < 1.5:
                tiles.extend(solid_tile(14))
            elif dist < ring:
                tiles.extend(gradient_h_tile(9, 11))
            else:
                tiles.extend(solid_tile(1))
        else:
            # Credits background — scrolling stars
            if rng.range(0, 40) == 0:
                tiles.extend(circle_tile(15, 0, 3.5, 3.5, 1))
            else:
                tiles.extend(solid_tile(0))
    return tiles

def gen_environment_frame(frame_idx, rng):
    """Environment mood: rain, static, corruption."""
    tiles = []
    env_type = frame_idx % 4
    for t in range(TILES_PER_FRAME):
        row = t // 30
        col = t % 30
        if env_type == 0:
            # Rain
            if (col * 3 + row + frame_idx) % 7 == 0:
                tiles.extend(stripe_v_tile(9, 1, 1))
            else:
                tiles.extend(solid_tile(1))
        elif env_type == 1:
            # Static/noise
            tiles.extend(noise_tile(rng, 7))
        elif env_type == 2:
            # Corruption spreading from center
            cx, cy = 15, 10
            dist = math.sqrt((col - cx)**2 + (row - cy)**2)
            threshold = frame_idx * 0.8
            if dist < threshold:
                tiles.extend(dither_tile(5, 13, rng))
            else:
                tiles.extend(gradient_v_tile(3, 6))
        else:
            # Glitch bands
            band = (row + frame_idx) % 5
            if band == 0:
                tiles.extend(noise_tile(rng, 15))
            elif band == 1:
                tiles.extend(stripe_h_tile(0, 15, 1))
            else:
                tiles.extend(solid_tile(2))
    return tiles

# ---------------------------------------------------------------------------
# Sprite generation helpers
# ---------------------------------------------------------------------------

def gen_humanoid_tile(tx, ty, pose, variant, rng):
    """Generate a 8x8 sub-tile for a 32x32 humanoid sprite.
    tx, ty: tile position within 4x4 grid (0-3).
    pose: 0=stand, 1=walk1, 2=walk2, 3=jump, 4=attack, etc.
    variant: class index for color variation.
    """
    skin = 10 + variant
    body = 6 + variant * 2
    bg = 0
    def row(r):
        gy = ty * 8 + r  # global y in 32x32 sprite
        gx_base = tx * 8
        pixels = []
        for lx in range(8):
            gx = gx_base + lx
            p = bg
            # Head (rows 2-9)
            if 2 <= gy <= 9:
                cx, cy_h = 16, 5
                dist = math.sqrt((gx - cx)**2 + (gy - cy_h)**2 * 1.3)
                if dist < 5:
                    p = skin
                elif dist < 6:
                    p = skin - 2 if skin > 2 else 1
                # Eyes
                if gy == 5 and gx in (14, 18):
                    p = 15
            # Body (rows 10-21)
            elif 10 <= gy <= 21:
                if 12 <= gx <= 19:
                    p = body
                    # Belt
                    if gy == 15:
                        p = 8
                # Arms with pose
                if pose in (4, 5):  # attack poses
                    if 10 <= gy <= 16:
                        if 20 <= gx <= 25:
                            p = skin
                elif pose in (1, 2):  # walk
                    arm_swing = 2 if pose == 1 else -2
                    if 10 <= gy <= 16:
                        if gx in (11, 20):
                            p = skin
            # Legs (rows 22-30)
            elif 22 <= gy <= 30:
                if pose == 1:  # walk frame 1
                    if (10 <= gx <= 14) and gy <= 28:
                        p = body - 1
                    if (17 <= gx <= 21) and gy <= 30:
                        p = body - 1
                elif pose == 2:  # walk frame 2
                    if (11 <= gx <= 15) and gy <= 30:
                        p = body - 1
                    if (16 <= gx <= 20) and gy <= 28:
                        p = body - 1
                elif pose == 3:  # jump
                    if 11 <= gx <= 20 and gy <= 26:
                        p = body - 1
                else:  # standing
                    if (12 <= gx <= 14 or 17 <= gx <= 19) and gy <= 29:
                        p = body - 1
            pixels.append(p)
        return pixels
    return make_tile(row)

def gen_enemy_tile(tx, ty, enemy_type, frame, rng):
    """Generate an 8x8 sub-tile for a 32x32 enemy sprite."""
    bg = 0
    shapes = [
        (5, 3.5),   # Sentry — round
        (7, 2.5),   # Patrol — angular
        (9, 2.0),   # Flyer — diamond (wings)
        (11, 4.0),  # Shield — large round
        (13, 3.0),  # Spike — triangle
        (6, 2.5),   # Hunter — angular
        (8, 2.0),   # Drone — small round
        (10, 3.0),  # Turret — rectangular
        (12, 3.5),  # Mimic — boxy
        (14, 2.5),  # Corruptor — irregular
        (4, 1.5),   # Ghost — wispy
        (3, 3.0),   # Bomber — round with spikes
    ]
    color, spread = shapes[enemy_type % 12]
    def row(r):
        gy = ty * 8 + r
        gx_base = tx * 8
        pixels = []
        for lx in range(8):
            gx = gx_base + lx
            p = bg
            cx, cy = 16, 16
            if enemy_type % 3 == 0:
                # Round shapes
                dist = math.sqrt((gx-cx)**2 + (gy-cy)**2)
                if dist < 8 + spread:
                    p = color
                elif dist < 10 + spread:
                    p = max(1, color - 3)
            elif enemy_type % 3 == 1:
                # Angular/diamond shapes
                dist = abs(gx-cx) + abs(gy-cy)
                if dist < 8 + int(spread * 2):
                    p = color
                elif dist < 10 + int(spread * 2):
                    p = max(1, color - 2)
            else:
                # Rectangular shapes
                hw = int(5 + spread)
                hh = int(4 + spread)
                if abs(gx-cx) <= hw and abs(gy-cy) <= hh:
                    p = color
                    if abs(gx-cx) == hw or abs(gy-cy) == hh:
                        p = max(1, color - 2)
            # Eyes (all enemies)
            if gy in (12, 13) and gx in (13, 19):
                if p != bg:
                    p = 15
            # Animation wobble
            if frame % 2 == 1 and gy == 24 + (frame % 4):
                if p != bg:
                    p = max(1, p - 1)
            pixels.append(p)
        return pixels
    return make_tile(row)

def gen_boss_tile(tx, ty, boss_idx, frame, rng):
    """Generate an 8x8 sub-tile for a 64x64 boss sprite."""
    bg = 0
    boss_colors = [7, 5, 9, 11, 13, 14]  # per boss base color
    color = boss_colors[boss_idx % 6]
    def row(r):
        gy = ty * 8 + r
        gx_base = tx * 8
        pixels = []
        for lx in range(8):
            gx = gx_base + lx
            p = bg
            cx, cy = 32, 32
            # Large body shape
            dist = math.sqrt((gx-cx)**2 + (gy-cy)**2)
            if dist < 24:
                p = color
                # Inner detail: darker center
                if dist < 12:
                    p = min(15, color + 2)
                # Texture pattern
                if (gx + gy + boss_idx) % 5 == 0:
                    p = max(1, p - 1)
            elif dist < 27:
                p = max(1, color - 3)
            # Boss-specific features
            if boss_idx == 0:  # Microslop — blobby
                if dist < 26 and gy > 40:
                    p = color + 1 if color < 15 else 15
            elif boss_idx == 1:  # Gogol — eyes everywhere
                if (gx % 12 < 3) and (gy % 10 < 3) and dist < 22:
                    p = 15
            elif boss_idx == 2:  # Amazomb — mechanical arms
                if (gy == 32) and (gx < 8 or gx > 56):
                    p = 8
            elif boss_idx == 3:  # Crapple — sleek
                if abs(gy - 32) < 2 and dist < 25:
                    p = 15
            elif boss_idx == 4:  # Faceplant — organic tendrils
                if gy > 45 and (gx % 8 < 2) and dist < 30:
                    p = max(1, color - 1)
            elif boss_idx == 5:  # DAEMON — glowing core
                if dist < 6:
                    p = 15
                elif dist < 8:
                    p = 14
            # Frame animation (subtle shift)
            if frame % 4 >= 2 and gy < 10:
                if p != bg:
                    p = max(1, p - 1)
            pixels.append(p)
        return pixels
    return make_tile(row)

def gen_item_tile(tx, ty, item_idx, rng):
    """Generate 8x8 sub-tile for 16x16 item icon."""
    bg = 0
    # Different item shapes
    item_type = item_idx % 10
    colors = [15, 14, 12, 10, 9, 8, 7, 6, 11, 13]
    c = colors[item_type]
    def row(r):
        gy = ty * 8 + r
        gx_base = tx * 8
        pixels = []
        for lx in range(8):
            gx = gx_base + lx
            p = bg
            cx, cy = 8, 8
            if item_type == 0:  # Sword
                if abs(gx - 8) <= 1 and 2 <= gy <= 14:
                    p = c
                if 6 <= gx <= 10 and gy == 5:
                    p = c - 2
            elif item_type == 1:  # Gun
                if 4 <= gx <= 12 and 6 <= gy <= 10:
                    p = c
                if 12 <= gx <= 14 and gy == 8:
                    p = 15
            elif item_type == 2:  # Armor
                if 4 <= gx <= 12 and 3 <= gy <= 13:
                    if abs(gx - 8) + abs(gy - 4) > 5 or gy > 5:
                        p = c
            elif item_type == 3:  # Ring
                dist = math.sqrt((gx-cx)**2 + (gy-cy)**2)
                if 3 <= dist <= 5:
                    p = c
            elif item_type == 4:  # Potion
                if 5 <= gx <= 11 and 6 <= gy <= 14:
                    p = c
                if 6 <= gx <= 10 and 3 <= gy <= 6:
                    p = 7
            elif item_type == 5:  # Shield
                dist = math.sqrt((gx-cx)**2 + ((gy-cy)*0.8)**2)
                if dist < 6:
                    p = c
                    if dist < 3:
                        p = min(15, c + 3)
            elif item_type == 6:  # Key
                if abs(gx - 6) <= 1 and 4 <= gy <= 12:
                    p = c
                dist = math.sqrt((gx-8)**2 + (gy-4)**2)
                if dist < 3:
                    p = c
            elif item_type == 7:  # Gem
                dist = abs(gx-cx) + abs(gy-cy)
                if dist < 5:
                    p = c
                    if dist < 2:
                        p = 15
            elif item_type == 8:  # Scroll
                if 4 <= gx <= 12 and 2 <= gy <= 14:
                    p = c if (gy % 3 != 0) else max(1, c - 3)
            else:  # Chip/circuit
                if (gx == 8 or gy == 8) and 3 <= gx <= 13 and 3 <= gy <= 13:
                    p = c
                if 6 <= gx <= 10 and 6 <= gy <= 10:
                    p = 15
            pixels.append(p)
        return pixels
    return make_tile(row)

def gen_particle_tile(tx, ty, part_idx, rng):
    """Generate 8x8 sub-tile for 16x16 particle/effect."""
    bg = 0
    c = 9 + (part_idx % 7)
    def row(r):
        gy = ty * 8 + r
        gx_base = tx * 8
        pixels = []
        for lx in range(8):
            gx = gx_base + lx
            p = bg
            cx, cy = 8, 8
            dist = math.sqrt((gx-cx)**2 + (gy-cy)**2)
            # Particles are small glowing dots/bursts
            threshold = 2 + (part_idx % 5)
            if dist < threshold:
                p = c
                if dist < threshold / 2:
                    p = 15
            pixels.append(p)
        return pixels
    return make_tile(row)

# ---------------------------------------------------------------------------
# Codex / Bestiary / Lore text data
# ---------------------------------------------------------------------------

ACTS = ["The Glitch", "Traceback", "Deep Packet", "Zero Day", "Ghost Protocol", "Trace Route"]
BOSSES = ["Microslop", "Gogol", "Amazomb", "Crapple", "Faceplant", "DAEMON"]
ENEMIES = ["Sentry", "Patrol", "Flyer", "Shield", "Spike", "Hunter",
           "Drone", "Turret", "Mimic", "Corruptor", "Ghost", "Bomber"]
CLASSES = ["Assault", "Infiltrator", "Technomancer"]
WEAPON_TYPES = ["Blade", "Pistol", "Shotgun", "Rifle", "Laser", "Homing", "Nova", "Staff"]
RARITIES = ["Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic"]
AREAS = ["Sector Alpha", "Sector Beta", "Sector Gamma", "Sector Delta",
         "Sector Omega", "The Core", "Data Highway", "Trash Heap",
         "Server Farm", "Firewall Gate"]

def gen_codex_entries():
    """100 codex entries."""
    categories = ["History", "Technology", "Organizations", "People", "Locations",
                  "Weapons", "Threats", "Protocols", "Systems", "Lore"]
    entries = []
    rng = PRNG(0x12345)
    subjects = [
        "The Great Disconnect", "Neural Interface v2.0", "The Resistance Network",
        "Commander Wraith", "Neon District", "Plasma Cutter Mk.IV", "Virus Swarm Alpha",
        "Ghost Protocol Initiative", "Firewall Architecture", "The Digital Dark Age",
        "Memory Banks of Sector 7", "Quantum Encryption Keys", "The First Breach",
        "Cybernetic Augmentation", "Underground Data Markets", "Rogue AI Collective",
        "The Signal Tower", "Binary Code Monks", "Electromagnetic Pulse Weapons",
        "The Last Human Server", "Packet Storm Warning", "Digital Archaeology",
        "The Compiler Wars", "Silicon Valley Ruins", "Holographic Disguise Tech",
        "The Bandwidth Crisis", "Neural Worm Taxonomy", "Proxy Server Hideouts",
        "The Root Access Rebellion", "Deprecated Protocol Museum",
        "Kernel Panic Incident", "The Stack Overflow Catastrophe", "Zero-Day Market",
        "Memory Leak Plague", "Buffer Overflow Attacks", "The Recursion Cult",
        "Garbage Collector Drones", "Thread Pool Operators", "Deadlock Canyon",
        "Race Condition Hazards", "Null Pointer Wasteland", "Exception Handlers Guild",
        "The Segfault Massacre", "Heap Corruption Zones", "Cache Miss Desert",
        "Pipeline Stall Mountains", "Branch Prediction Oracle", "Instruction Set Cathedral",
        "Register File Vault", "Interrupt Vector Table",
    ]
    for i in range(100):
        cat = categories[i % len(categories)]
        if i < len(subjects):
            name = subjects[i]
        else:
            name = f"{categories[rng.range(0,9)]} Entry {i+1:03d}"
        # Build meaningful lore text
        text_parts = [
            f"Classification: {cat}. ",
            f"Threat Level: {'ALPHA BRAVO CHARLIE DELTA ECHO'.split()[rng.range(0,4)]}. ",
            f"First documented in cycle {rng.range(1000,9999)}. ",
            f"Located in {AREAS[rng.range(0, len(AREAS)-1)]}. ",
            f"Requires clearance level {rng.range(1,10)} to access. ",
            f"Related to {ACTS[rng.range(0, len(ACTS)-1)]} operations. ",
            f"Status: {'ACTIVE DORMANT CLASSIFIED REDACTED DESTROYED'.split()[rng.range(0,4)]}. ",
        ]
        text = "".join(text_parts)
        # Pad to ~500 bytes
        while len(text) < 480:
            text += f"Data fragment {rng.range(0,0xFFFF):04X}. "
        text = text[:499]
        entries.append((name, cat, text))
    return entries

def gen_bestiary_entries():
    """50 bestiary entries."""
    entries = []
    rng = PRNG(0xBE57)
    for i in range(50):
        if i < 12:
            name = ENEMIES[i]
            etype = "Standard"
        elif i < 18:
            name = BOSSES[i - 12]
            etype = "Boss"
        elif i < 30:
            name = f"{ENEMIES[i%12]} Mk.{i//12 + 1}"
            etype = "Elite"
        else:
            prefixes = ["Corrupted", "Overclocked", "Glitched", "Phantom", "Viral"]
            name = f"{prefixes[rng.range(0, 4)]} {ENEMIES[rng.range(0, 11)]}"
            etype = "Variant"
        lore = f"Type: {etype}. Habitat: {AREAS[rng.range(0, len(AREAS)-1)]}. "
        lore += f"First sighted during {ACTS[rng.range(0, len(ACTS)-1)]}. "
        lore += f"Threat assessment: {rng.range(1,100)}%. "
        lore += f"Known weaknesses: {'electrical thermal kinetic EMP none'.split()[rng.range(0,4)]}. "
        lore += f"Typical formation: {'solo pair pack swarm'.split()[rng.range(0,3)]}. "
        while len(lore) < 480:
            lore += f"Behavioral note #{rng.range(0,999):03d}: pattern {rng.range(0,0xFFFF):04X}. "
        lore = lore[:499]
        stats = f"HP:{rng.range(5,200)} ATK:{rng.range(1,50)} DEF:{rng.range(0,30)} SPD:{rng.range(1,10)} XP:{rng.range(5,500)}"
        while len(stats) < 80:
            stats += f" R{rng.range(0,99):02d}"
        stats = stats[:99]
        entries.append((name, etype, lore, stats))
    return entries

def gen_lore_fragments():
    """100 lore fragments."""
    fragments = []
    templates = [
        "In the time before the Disconnect, data flowed freely through the networks of {}.",
        "The {} sector was the first to fall when the firewalls collapsed.",
        "Commander Wraith's logs mention a hidden cache beneath {}.",
        "Warning: {} contamination detected in buffer zone {}.",
        "Cycle {}: The last transmission from {} read only 'GHOST PROTOCOL ACTIVE'.",
        "The {} were built to protect, but corruption turned them into weapons.",
        "Deep within {}, the old servers still hum with forgotten data.",
        "Legends speak of a {} that could restore the network to its former state.",
        "The {} protocol was deprecated after the incident at Sector {}.",
        "Field report: {} activity increasing near {}. Recommend immediate extraction.",
        "Encrypted message decoded: 'The {} holds the key to {}.'",
        "Historical record: {} was founded in cycle {} by the original architects.",
        "Tactical analysis: {} forces are weakest during {} phase transitions.",
        "Recovery log: Salvaged {} from the ruins of {}. Integrity: {}%.",
        "Observation: The {} phenomenon occurs every {} cycles without fail.",
    ]
    rng = PRNG(0x10FE)
    for i in range(100):
        tmpl = templates[i % len(templates)]
        # Fill in placeholders
        text = tmpl
        count = text.count("{}")
        replacements = []
        for _ in range(count):
            rtype = rng.range(0, 3)
            if rtype == 0:
                replacements.append(AREAS[rng.range(0, len(AREAS)-1)])
            elif rtype == 1:
                replacements.append(ENEMIES[rng.range(0, 11)])
            elif rtype == 2:
                replacements.append(str(rng.range(100, 9999)))
            else:
                replacements.append(ACTS[rng.range(0, len(ACTS)-1)])
        text = text.format(*replacements)
        while len(text) < 180:
            text += f" [{rng.range(0,0xFFFF):04X}]"
        text = text[:199]
        fragments.append(text)
    return fragments

def gen_item_descriptions():
    """200 item descriptions."""
    items = []
    rng = PRNG(0x1734)
    prefixes = ["Rusty", "Polished", "Cracked", "Glowing", "Ancient", "Modified",
                "Prototype", "Standard", "Enhanced", "Corrupted", "Quantum", "Plasma",
                "Neural", "Hardened", "Volatile", "Stealth", "Overclocked", "Depleted"]
    suffixes = ["of the Circuit", "of Disruption", "of Protection", "of Speed",
                "of the Firewall", "of Extraction", "of the Root", "of Corruption",
                "of the Signal", "of the Void", "Prime", "Zero", "Omega", "Alpha"]
    base_names = [
        "Blade", "Pistol", "Shotgun", "Rifle", "Staff", "Shield", "Armor", "Helm",
        "Ring", "Amulet", "Chip", "Module", "Core", "Cell", "Injector", "Grenade",
        "Mine", "Turret Kit", "Drone Core", "Data Shard",
    ]
    for i in range(200):
        prefix = prefixes[rng.range(0, len(prefixes)-1)]
        base = base_names[rng.range(0, len(base_names)-1)]
        suffix = suffixes[rng.range(0, len(suffixes)-1)] if rng.range(0, 2) > 0 else ""
        name = f"{prefix} {base}"
        if suffix:
            name += f" {suffix}"
        name = name[:63]
        flavor_parts = [
            f"Rarity: {RARITIES[rng.range(0, 5)]}. ",
            f"Origin: {AREAS[rng.range(0, len(AREAS)-1)]}. ",
        ]
        flavor = "".join(flavor_parts)
        descs = [
            "Hums with residual energy from a forgotten age.",
            "The circuitry within pulses with an eerie glow.",
            "Salvaged from the wreckage of a fallen guardian.",
            "Bears the mark of the original network architects.",
            "Emits a faint signal on deprecated frequencies.",
            "Its surface is etched with ancient binary patterns.",
            "Warm to the touch despite the surrounding cold.",
            "Contains fragments of corrupted but powerful code.",
            "Once belonged to a high-ranking operative.",
            "The craftsmanship suggests pre-Disconnect manufacture.",
        ]
        flavor += descs[rng.range(0, len(descs)-1)]
        while len(flavor) < 130:
            flavor += f" +{rng.range(1,10)}"
        flavor = flavor[:149]
        items.append((name, flavor))
    return items

def gen_achievement_descriptions():
    """50 achievement descriptions."""
    achievements = []
    names = [
        "First Steps", "Bug Squasher", "Ghost in the Machine", "Root Access",
        "Firewall Breaker", "Data Miner", "Protocol Override", "Zero Day Hero",
        "Full Circuit", "Master Hacker", "Speed Runner", "Collector",
        "Untouchable", "Overkill", "Pacifist Run", "No Herbs Challenge",
        "Boss Rush", "Mythic Hunter", "Completionist", "True Ghost",
        "Lv10 Assault", "Lv10 Infiltrator", "Lv10 Technomancer",
        "Lv20 Evolution", "Lv40 Max", "All Skills Unlocked", "Full Bestiary",
        "Full Codex", "100 Enemies", "500 Enemies", "1000 Enemies",
        "10 Bosses", "50 Bosses", "All Acts Cleared", "Bug Bounty Tier 1",
        "Bug Bounty Tier 3", "Bug Bounty Tier 5", "Bug Bounty Tier 7",
        "Iron Mode Clear", "Swarm Mode Clear", "Craft 10 Items", "Craft 50 Items",
        "Forge Mythic", "Find All Lore", "Secret Room", "Hidden Boss",
        "Speedkill Boss", "Deathless Act", "The Long Road", "Ghost Protocol",
    ]
    rng = PRNG(0xACE1)
    for i in range(50):
        name = names[i] if i < len(names) else f"Achievement {i+1}"
        texts = [
            f"Complete the objective: {name}.",
            f"Reward: {rng.range(10,500)} XP bonus.",
            f"Difficulty: {'Easy Normal Hard Expert Legendary'.split()[rng.range(0,4)]}.",
        ]
        text = " ".join(texts)
        while len(text) < 180:
            text += f" [{rng.range(0,0xFF):02X}]"
        text = text[:199]
        achievements.append((name, text))
    return achievements

# ---------------------------------------------------------------------------
# Level section template generators
# ---------------------------------------------------------------------------

def gen_section_template(idx, rng):
    """16x32 tile section template (tile data + collision)."""
    tiles = []
    collision = []
    section_type = idx % 14
    for y in range(32):
        for x in range(16):
            if section_type == 0:  # Flat ground
                if y >= 28:
                    tiles.append(rng.range(1, 3))
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 1:  # Platforms
                if y >= 28 or (y == 20 and 4 <= x <= 12) or (y == 14 and 2 <= x <= 8):
                    tiles.append(rng.range(4, 6))
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 2:  # Corridor
                if y <= 4 or y >= 28 or x <= 1 or x >= 14:
                    tiles.append(rng.range(7, 9))
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 3:  # Zigzag
                if y >= 28:
                    tiles.append(2)
                    collision.append(1)
                elif (y // 4 + x // 4) % 2 == 0 and (y % 8 >= 6):
                    tiles.append(3)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 4:  # Waterfall
                if y >= 28:
                    tiles.append(10)
                    collision.append(1)
                elif 6 <= x <= 9:
                    tiles.append(11)  # water
                    collision.append(2)  # hazard
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 5:  # Transit
                if y >= 28 or (y == 16 and x % 4 != 0):
                    tiles.append(rng.range(12, 14))
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 6:  # Security
                if y >= 28 or y <= 2:
                    tiles.append(15)
                    collision.append(1)
                elif x == 8 and y % 6 == 0:
                    tiles.append(16)  # laser
                    collision.append(2)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 7:  # Cache
                if y >= 28:
                    tiles.append(1)
                    collision.append(1)
                elif 4 <= x <= 12 and 12 <= y <= 20:
                    tiles.append(17 if x == 4 or x == 12 or y == 12 or y == 20 else 0)
                    collision.append(1 if (x == 4 or x == 12 or y == 12 or y == 20) else 0)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 8:  # Network
                if y >= 28:
                    tiles.append(2)
                    collision.append(1)
                elif x % 4 == 0 or y % 8 == 0:
                    tiles.append(18)
                    collision.append(0)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 9:  # Gauntlet
                if y >= 28:
                    tiles.append(1)
                    collision.append(1)
                elif y % 4 == 0 and rng.range(0, 2) == 0:
                    tiles.append(19)
                    collision.append(2)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 10:  # Vertical shaft
                if x <= 2 or x >= 13:
                    tiles.append(rng.range(7, 9))
                    collision.append(1)
                elif y % 10 == 0 and 4 <= x <= 11:
                    tiles.append(5)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 11:  # Spikes
                if y >= 28:
                    tiles.append(1)
                    collision.append(1)
                elif y >= 26 and x % 3 == 0:
                    tiles.append(20)
                    collision.append(2)
                elif y == 10 and 3 <= x <= 12:
                    tiles.append(4)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif section_type == 12:  # Pillars
                if y >= 28:
                    tiles.append(2)
                    collision.append(1)
                elif x % 5 == 2 and 8 <= y <= 27:
                    tiles.append(21)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            else:  # Open arena
                if y >= 28 or y <= 2 or x <= 0 or x >= 15:
                    tiles.append(rng.range(1, 3))
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
    return tiles, collision

def gen_boss_arena(idx, rng):
    """32x32 boss arena layout."""
    tiles = []
    collision = []
    for y in range(32):
        for x in range(32):
            # Border walls
            if y <= 1 or y >= 30 or x <= 1 or x >= 30:
                tiles.append(rng.range(22, 25))
                collision.append(1)
            # Arena-specific features
            elif idx % 6 == 0:  # Open arena with pillars
                if (x in (8, 24) and 8 <= y <= 24):
                    tiles.append(21)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif idx % 6 == 1:  # Pit arena
                if 12 <= x <= 20 and 14 <= y <= 18:
                    tiles.append(11)
                    collision.append(2)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif idx % 6 == 2:  # Platform arena
                if y == 20 and (4 <= x <= 12 or 20 <= x <= 28):
                    tiles.append(5)
                    collision.append(1)
                elif y == 12 and 10 <= x <= 22:
                    tiles.append(5)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif idx % 6 == 3:  # Corridor boss
                if (x <= 4 or x >= 28) and 8 <= y <= 24:
                    tiles.append(rng.range(7, 9))
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
            elif idx % 6 == 4:  # Hazard arena
                if (x + y) % 8 == 0 and 4 <= x <= 28 and 4 <= y <= 28:
                    tiles.append(19)
                    collision.append(2)
                else:
                    tiles.append(0)
                    collision.append(0)
            else:  # Symmetric arena
                cx, cy = 16, 16
                dist = abs(x - cx) + abs(y - cy)
                if dist == 10:
                    tiles.append(21)
                    collision.append(1)
                else:
                    tiles.append(0)
                    collision.append(0)
    return tiles, collision

def gen_bg_theme(idx, rng):
    """256 background tiles (each 8x8 = 8 u32)."""
    tiles = []
    theme = idx % 10
    for t in range(256):
        if theme == 0:  # Grass
            tiles.extend(dither_tile(4, 5, rng))
        elif theme == 1:  # Stone
            tiles.extend(checker_tile(7, 8, 2))
        elif theme == 2:  # Circuit
            tiles.extend(circuit_tile(10, 12, 1, rng))
        elif theme == 3:  # Water
            tiles.extend(wave_tile(9, 11, 1, t * 0.5))
        elif theme == 4:  # Lava
            tiles.extend(dither_tile(14, 3, rng))
        elif theme == 5:  # Ice
            tiles.extend(gradient_h_tile(9, 11))
        elif theme == 6:  # Dark
            tiles.extend(noise_tile(rng, 3))
        elif theme == 7:  # Neon
            if t % 4 == 0:
                tiles.extend(solid_tile(10 + t % 6))
            else:
                tiles.extend(solid_tile(0))
        elif theme == 8:  # Ruins
            tiles.extend(rect_tile(7, 2, 1, 1, 6, 6))
        else:  # Stars
            if rng.range(0, 15) == 0:
                tiles.extend(circle_tile(15, 0, 3.5, 3.5, 1))
            else:
                tiles.extend(solid_tile(0))
    return tiles

# ---------------------------------------------------------------------------
# C code formatting helpers
# ---------------------------------------------------------------------------

def format_u32_array(data, values_per_line=10):
    """Format list of u32 values as C array initializer lines."""
    lines = []
    for i in range(0, len(data), values_per_line):
        chunk = data[i:i+values_per_line]
        vals = ", ".join(f"0x{v:08X}" for v in chunk)
        lines.append(f"    {vals},")
    return "\n".join(lines)

def format_u8_array(data, values_per_line=16):
    """Format list of u8 values as C array initializer lines."""
    lines = []
    for i in range(0, len(data), values_per_line):
        chunk = data[i:i+values_per_line]
        vals = ", ".join(f"0x{v:02X}" for v in chunk)
        lines.append(f"    {vals},")
    return "\n".join(lines)

def c_string_literal(s, max_len=None):
    """Escape a string for C literal."""
    if max_len:
        s = s[:max_len]
    s = s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{s}"'

# ---------------------------------------------------------------------------
# File generators
# ---------------------------------------------------------------------------

def generate_cutscenes(out_path):
    """Generate content_cutscenes.c (~2MB)."""
    print("  Generating cutscenes...")
    rng = PRNG(0xC073CE)
    frame_names = []
    frame_data = []

    # 10 title frames
    for i in range(10):
        name = f"cutscene_title_{i:03d}"
        data = gen_title_frame(i, rng)
        frame_names.append(name)
        frame_data.append(data)

    # 20 act intro frames (6 acts x 3-4 frames)
    frames_per_act = [4, 3, 3, 4, 3, 3]
    for act in range(6):
        for sub in range(frames_per_act[act]):
            name = f"cutscene_act{act+1}_{sub:02d}"
            data = gen_act_intro_frame(act, sub, rng)
            frame_names.append(name)
            frame_data.append(data)

    # 30 boss intro frames (6 bosses x 5 frames)
    for boss in range(6):
        for sub in range(5):
            name = f"cutscene_boss{boss+1}_{sub:02d}"
            data = gen_boss_intro_frame(boss, sub, rng)
            frame_names.append(name)
            frame_data.append(data)

    # 20 story scene frames
    for i in range(20):
        name = f"cutscene_story_{i:03d}"
        data = gen_story_frame(i, rng)
        frame_names.append(name)
        frame_data.append(data)

    # 20 ending frames
    for i in range(20):
        name = f"cutscene_ending_{i:03d}"
        data = gen_ending_frame(i, rng)
        frame_names.append(name)
        frame_data.append(data)

    # 20 environment mood frames
    for i in range(20):
        name = f"cutscene_env_{i:03d}"
        data = gen_environment_frame(i, rng)
        frame_names.append(name)
        frame_data.append(data)

    with open(out_path, "w") as f:
        f.write("/*\n * Ghost Protocol — Cutscene Frame Data\n *\n")
        f.write(f" * {len(frame_names)} cutscene frames, {TILES_PER_FRAME} tiles each (240x160 4bpp)\n")
        f.write(" * Auto-generated by generate_rom_data.py\n */\n")
        f.write('#include <tonc.h>\n\n')

        for i, (name, data) in enumerate(zip(frame_names, frame_data)):
            f.write(f"/* Frame {i}: {name} */\n")
            f.write(f"const u32 {name}[{TILES_PER_FRAME * 8}] __attribute__((used)) = {{\n")
            f.write(format_u32_array(data))
            f.write("\n};\n\n")

        # Force-link function
        f.write("/* Prevent LTO from stripping data */\n")
        f.write("const void* cutscene_force_link(void) {\n")
        f.write(f"    return (const void*){frame_names[0]};\n")
        f.write("}\n")

    return frame_names


def generate_sprites(out_path):
    """Generate content_sprites.c (~1MB)."""
    print("  Generating sprites...")
    rng = PRNG(0x5F173E)
    arrays = []  # (name, size_in_u32, data)

    # 3 player classes x 24 animation frames x 16 tiles (32x32)
    for cls in range(3):
        for frame in range(24):
            name = f"spr_player_c{cls}_f{frame:02d}"
            data = []
            pose = frame % 6  # 6 distinct poses cycled
            for ty in range(4):
                for tx in range(4):
                    data.extend(gen_humanoid_tile(tx, ty, pose, cls, rng))
            arrays.append((name, len(data), data))

    # 12 enemy types x 8 animation frames x 16 tiles (32x32)
    for etype in range(12):
        for frame in range(8):
            name = f"spr_enemy_t{etype:02d}_f{frame:02d}"
            data = []
            for ty in range(4):
                for tx in range(4):
                    data.extend(gen_enemy_tile(tx, ty, etype, frame, rng))
            arrays.append((name, len(data), data))

    # 6 bosses x 16 animation frames x 64 tiles (64x64)
    for boss in range(6):
        for frame in range(16):
            name = f"spr_boss_b{boss}_f{frame:02d}"
            data = []
            for ty in range(8):
                for tx in range(8):
                    data.extend(gen_boss_tile(tx, ty, boss, frame, rng))
            arrays.append((name, len(data), data))

    # 50 item icons x 4 tiles (16x16)
    for item in range(50):
        name = f"spr_item_{item:03d}"
        data = []
        for ty in range(2):
            for tx in range(2):
                data.extend(gen_item_tile(tx, ty, item, rng))
        arrays.append((name, len(data), data))

    # 20 particle effects x 4 tiles (16x16)
    for part in range(20):
        name = f"spr_particle_{part:03d}"
        data = []
        for ty in range(2):
            for tx in range(2):
                data.extend(gen_particle_tile(tx, ty, part, rng))
        arrays.append((name, len(data), data))

    with open(out_path, "w") as f:
        f.write("/*\n * Ghost Protocol — Expanded Sprite Tile Data\n *\n")
        f.write(f" * {len(arrays)} sprite arrays\n")
        f.write(" * Auto-generated by generate_rom_data.py\n */\n")
        f.write('#include <tonc.h>\n\n')

        for name, size, data in arrays:
            f.write(f"const u32 {name}[{size}] __attribute__((used)) = {{\n")
            f.write(format_u32_array(data))
            f.write("\n};\n\n")

        f.write("const void* sprite_force_link(void) {\n")
        f.write(f"    return (const void*){arrays[0][0]};\n")
        f.write("}\n")

    return [(name, size) for name, size, _ in arrays]


def generate_codex(out_path):
    """Generate content_codex.c (~300KB)."""
    print("  Generating codex...")

    codex = gen_codex_entries()
    bestiary = gen_bestiary_entries()
    lore = gen_lore_fragments()
    items = gen_item_descriptions()
    achievements = gen_achievement_descriptions()

    with open(out_path, "w") as f:
        f.write("/*\n * Ghost Protocol — Codex, Bestiary, and Lore Data\n *\n")
        f.write(" * Auto-generated by generate_rom_data.py\n */\n")
        f.write('#include <tonc.h>\n')
        f.write('#include "game/content_data.h"\n\n')

        # Codex entries
        f.write(f"const CodexEntry codex_entries[{len(codex)}] __attribute__((used)) = {{\n")
        for name, cat, text in codex:
            f.write(f"    {{ {c_string_literal(name, 63)}, {c_string_literal(cat, 31)}, {c_string_literal(text, 499)} }},\n")
        f.write("};\n\n")

        # Bestiary entries
        f.write(f"const BestiaryEntry bestiary_entries[{len(bestiary)}] __attribute__((used)) = {{\n")
        for name, etype, lore_text, stats in bestiary:
            f.write(f"    {{ {c_string_literal(name, 63)}, {c_string_literal(etype, 31)}, "
                    f"{c_string_literal(lore_text, 499)}, {c_string_literal(stats, 99)} }},\n")
        f.write("};\n\n")

        # Lore fragments
        f.write(f"const char* const lore_fragments[{len(lore)}] __attribute__((used)) = {{\n")
        for text in lore:
            f.write(f"    {c_string_literal(text, 199)},\n")
        f.write("};\n\n")

        # Item descriptions
        f.write(f"const ItemDescription item_descriptions[{len(items)}] __attribute__((used)) = {{\n")
        for name, flavor in items:
            f.write(f"    {{ {c_string_literal(name, 63)}, {c_string_literal(flavor, 149)} }},\n")
        f.write("};\n\n")

        # Achievement descriptions
        f.write(f"const AchievementDescription achievement_descriptions[{len(achievements)}] __attribute__((used)) = {{\n")
        for name, text in achievements:
            f.write(f"    {{ {c_string_literal(name, 63)}, {c_string_literal(text, 199)} }},\n")
        f.write("};\n\n")

        f.write("const void* codex_force_link(void) {\n")
        f.write("    return (const void*)codex_entries;\n")
        f.write("}\n")

    return {
        "codex": len(codex),
        "bestiary": len(bestiary),
        "lore": len(lore),
        "items": len(items),
        "achievements": len(achievements),
    }


def generate_levels(out_path):
    """Generate content_levels.c (~500KB)."""
    print("  Generating levels...")
    rng = PRNG(0x1EE1)

    section_templates = []
    for i in range(100):
        tiles, coll = gen_section_template(i, rng)
        section_templates.append((f"section_tiles_{i:03d}", f"section_coll_{i:03d}", tiles, coll))

    boss_arenas = []
    for i in range(30):
        tiles, coll = gen_boss_arena(i, rng)
        boss_arenas.append((f"arena_tiles_{i:03d}", f"arena_coll_{i:03d}", tiles, coll))

    bg_themes = []
    for i in range(50):
        data = gen_bg_theme(i, rng)
        bg_themes.append((f"bg_theme_{i:03d}", data))

    with open(out_path, "w") as f:
        f.write("/*\n * Ghost Protocol — Level Section Templates\n *\n")
        f.write(f" * {len(section_templates)} section templates, {len(boss_arenas)} boss arenas, "
                f"{len(bg_themes)} background themes\n")
        f.write(" * Auto-generated by generate_rom_data.py\n */\n")
        f.write('#include <tonc.h>\n\n')

        # Section templates (tile data as u8, collision as u8)
        for tname, cname, tiles, coll in section_templates:
            f.write(f"const u8 {tname}[{len(tiles)}] __attribute__((used)) = {{\n")
            f.write(format_u8_array(tiles))
            f.write("\n};\n\n")
            f.write(f"const u8 {cname}[{len(coll)}] __attribute__((used)) = {{\n")
            f.write(format_u8_array(coll))
            f.write("\n};\n\n")

        # Boss arenas
        for tname, cname, tiles, coll in boss_arenas:
            f.write(f"const u8 {tname}[{len(tiles)}] __attribute__((used)) = {{\n")
            f.write(format_u8_array(tiles))
            f.write("\n};\n\n")
            f.write(f"const u8 {cname}[{len(coll)}] __attribute__((used)) = {{\n")
            f.write(format_u8_array(coll))
            f.write("\n};\n\n")

        # Background themes (u32 tile data)
        for name, data in bg_themes:
            f.write(f"const u32 {name}[{len(data)}] __attribute__((used)) = {{\n")
            f.write(format_u32_array(data))
            f.write("\n};\n\n")

        f.write("const void* levels_force_link(void) {\n")
        f.write(f"    return (const void*){section_templates[0][0]};\n")
        f.write("}\n")

    return {
        "sections": len(section_templates),
        "arenas": len(boss_arenas),
        "themes": len(bg_themes),
    }


def generate_header(out_path, cutscene_names, sprite_arrays, codex_counts, level_counts):
    """Generate content_data.h."""
    print("  Generating header...")
    with open(out_path, "w") as f:
        f.write("/*\n * Ghost Protocol — Content Data Header\n *\n")
        f.write(" * Extern declarations, struct definitions, and size constants.\n")
        f.write(" * Auto-generated by generate_rom_data.py\n */\n")
        f.write("#ifndef GAME_CONTENT_DATA_H\n")
        f.write("#define GAME_CONTENT_DATA_H\n\n")
        f.write("#include <tonc.h>\n\n")

        # Size constants
        f.write("/* ---- Size Constants ---- */\n")
        f.write(f"#define CUTSCENE_FRAME_COUNT    {len(cutscene_names)}\n")
        f.write(f"#define CUTSCENE_TILES_PER_FRAME {TILES_PER_FRAME}\n")
        f.write(f"#define CUTSCENE_U32_PER_FRAME  {TILES_PER_FRAME * 8}\n\n")

        f.write(f"#define SPRITE_PLAYER_CLASSES   3\n")
        f.write(f"#define SPRITE_PLAYER_FRAMES    24\n")
        f.write(f"#define SPRITE_ENEMY_TYPES      12\n")
        f.write(f"#define SPRITE_ENEMY_FRAMES     8\n")
        f.write(f"#define SPRITE_BOSS_COUNT       6\n")
        f.write(f"#define SPRITE_BOSS_FRAMES      16\n")
        f.write(f"#define SPRITE_ITEM_COUNT       50\n")
        f.write(f"#define SPRITE_PARTICLE_COUNT   20\n\n")

        f.write(f"#define CODEX_ENTRY_COUNT       {codex_counts['codex']}\n")
        f.write(f"#define BESTIARY_ENTRY_COUNT    {codex_counts['bestiary']}\n")
        f.write(f"#define LORE_FRAGMENT_COUNT     {codex_counts['lore']}\n")
        f.write(f"#define ITEM_DESC_COUNT         {codex_counts['items']}\n")
        f.write(f"#define ACHIEVEMENT_COUNT       {codex_counts['achievements']}\n\n")

        f.write(f"#define SECTION_TEMPLATE_COUNT  {level_counts['sections']}\n")
        f.write(f"#define BOSS_ARENA_COUNT        {level_counts['arenas']}\n")
        f.write(f"#define BG_THEME_COUNT          {level_counts['themes']}\n\n")

        # Struct definitions
        f.write("/* ---- Struct Definitions ---- */\n")
        f.write("typedef struct {\n")
        f.write("    const char name[64];\n")
        f.write("    const char category[32];\n")
        f.write("    const char text[500];\n")
        f.write("} CodexEntry;\n\n")

        f.write("typedef struct {\n")
        f.write("    const char name[64];\n")
        f.write("    const char type[32];\n")
        f.write("    const char lore[500];\n")
        f.write("    const char stats[100];\n")
        f.write("} BestiaryEntry;\n\n")

        f.write("typedef struct {\n")
        f.write("    const char name[64];\n")
        f.write("    const char flavor[150];\n")
        f.write("} ItemDescription;\n\n")

        f.write("typedef struct {\n")
        f.write("    const char name[64];\n")
        f.write("    const char text[200];\n")
        f.write("} AchievementDescription;\n\n")

        # Cutscene externs
        f.write("/* ---- Cutscene Frame Externs ---- */\n")
        for name in cutscene_names:
            f.write(f"extern const u32 {name}[{TILES_PER_FRAME * 8}];\n")
        f.write("\n")

        # Sprite externs
        f.write("/* ---- Sprite Tile Externs ---- */\n")
        for name, size in sprite_arrays:
            f.write(f"extern const u32 {name}[{size}];\n")
        f.write("\n")

        # Codex externs
        f.write("/* ---- Codex/Bestiary Externs ---- */\n")
        f.write(f"extern const CodexEntry codex_entries[{codex_counts['codex']}];\n")
        f.write(f"extern const BestiaryEntry bestiary_entries[{codex_counts['bestiary']}];\n")
        f.write(f"extern const char* const lore_fragments[{codex_counts['lore']}];\n")
        f.write(f"extern const ItemDescription item_descriptions[{codex_counts['items']}];\n")
        f.write(f"extern const AchievementDescription achievement_descriptions[{codex_counts['achievements']}];\n\n")

        # Level externs
        f.write("/* ---- Level Template Externs ---- */\n")
        for i in range(level_counts['sections']):
            f.write(f"extern const u8 section_tiles_{i:03d}[512];\n")
            f.write(f"extern const u8 section_coll_{i:03d}[512];\n")
        f.write("\n")
        for i in range(level_counts['arenas']):
            f.write(f"extern const u8 arena_tiles_{i:03d}[1024];\n")
            f.write(f"extern const u8 arena_coll_{i:03d}[1024];\n")
        f.write("\n")
        for i in range(level_counts['themes']):
            f.write(f"extern const u32 bg_theme_{i:03d}[2048];\n")
        f.write("\n")

        # Force-link declarations
        f.write("/* ---- Force-link functions (prevent LTO stripping) ---- */\n")
        f.write("const void* cutscene_force_link(void);\n")
        f.write("const void* sprite_force_link(void);\n")
        f.write("const void* codex_force_link(void);\n")
        f.write("const void* levels_force_link(void);\n\n")

        f.write("/* Call this from main to ensure all content data is linked */\n")
        f.write("static inline void content_force_link_all(void) {\n")
        f.write("    volatile const void* p;\n")
        f.write("    p = cutscene_force_link();\n")
        f.write("    p = sprite_force_link();\n")
        f.write("    p = codex_force_link();\n")
        f.write("    p = levels_force_link();\n")
        f.write("    (void)p;\n")
        f.write("}\n\n")

        f.write("#endif /* GAME_CONTENT_DATA_H */\n")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    base = os.path.dirname(os.path.abspath(__file__))
    src_dir = os.path.dirname(base)
    source_game = os.path.join(src_dir, "source", "game")
    include_game = os.path.join(src_dir, "include", "game")

    os.makedirs(source_game, exist_ok=True)
    os.makedirs(include_game, exist_ok=True)

    print("Ghost Protocol ROM Data Generator")
    print("=" * 50)

    cutscene_path = os.path.join(source_game, "content_cutscenes.c")
    sprite_path = os.path.join(source_game, "content_sprites.c")
    codex_path = os.path.join(source_game, "content_codex.c")
    levels_path = os.path.join(source_game, "content_levels.c")
    header_path = os.path.join(include_game, "content_data.h")

    cutscene_names = generate_cutscenes(cutscene_path)
    sprite_arrays = generate_sprites(sprite_path)
    codex_counts = generate_codex(codex_path)
    level_counts = generate_levels(levels_path)
    generate_header(header_path, cutscene_names, sprite_arrays, codex_counts, level_counts)

    print("=" * 50)
    print("Generation complete! Files:")
    for path in [cutscene_path, sprite_path, codex_path, levels_path, header_path]:
        size = os.path.getsize(path)
        if size >= 1024 * 1024:
            print(f"  {path}: {size / (1024*1024):.2f} MB")
        else:
            print(f"  {path}: {size / 1024:.1f} KB")

    total = sum(os.path.getsize(p) for p in [cutscene_path, sprite_path, codex_path, levels_path, header_path])
    print(f"\n  Total: {total / (1024*1024):.2f} MB")


if __name__ == "__main__":
    main()
