#!/usr/bin/env python3
"""Generate procedural audio assets for Ghost Protocol GBA game.

Creates 8 MOD tracker music files and 18 WAV sound effects.
All audio is synthesized programmatically — no external dependencies.

MOD format: 4 channels, 31 instruments, 8-bit signed PCM samples.
WAV format: 8-bit unsigned PCM, 16384 Hz mono.

Usage:
    python3 tools/generate_audio.py
"""

import math
import os
import struct

# Output directories (relative to project root)
MUSIC_DIR = "audio/music"
SFX_DIR = "audio/sfx"

# WAV parameters
WAV_RATE = 16384
WAV_BITS = 8

# MOD parameters
MOD_RATE = 8363  # C-4 finetune 0 sample rate for MOD
MOD_ROWS = 64
MOD_CHANNELS = 4

# Note period table (MOD Amiga periods for octaves 1-3)
PERIODS = [
    # Octave 1
    856, 808, 762, 720, 678, 640, 604, 570, 538, 508, 480, 453,
    # Octave 2
    428, 404, 381, 360, 339, 320, 302, 285, 269, 254, 240, 226,
    # Octave 3
    214, 202, 190, 180, 170, 160, 151, 143, 135, 127, 120, 113,
]

# Note index shortcuts
C1, Cs1, D1, Ds1, E1, F1, Fs1, G1, Gs1, A1, As1, B1 = range(12)
C2, Cs2, D2, Ds2, E2, F2, Fs2, G2, Gs2, A2, As2, B2 = range(12, 24)
C3, Cs3, D3, Ds3, E3, F3, Fs3, G3, Gs3, A3, As3, B3 = range(24, 36)

# MOD instrument numbers (1-indexed in pattern data)
SMP_LEAD = 1
SMP_BASS = 2
SMP_PAD = 3
SMP_KICK = 4
SMP_HAT = 5
SMP_SINE = 6


# ─── WAV Generation ─────────────────────────────────────────────────────────

def make_wav(samples):
    """Pack 8-bit unsigned PCM samples into a WAV file."""
    data = bytes(max(0, min(255, s)) for s in samples)
    n = len(data)
    header = struct.pack('<4sI4s', b'RIFF', 36 + n, b'WAVE')
    fmt = struct.pack('<4sIHHIIHH', b'fmt ', 16, 1, 1, WAV_RATE, WAV_RATE, 1, 8)
    data_hdr = struct.pack('<4sI', b'data', n)
    return header + fmt + data_hdr + data


def sine_wave(freq, duration, volume=0.5):
    n = int(WAV_RATE * duration)
    return [int(128 + 127 * volume * math.sin(2 * math.pi * freq * i / WAV_RATE))
            for i in range(n)]


def triangle_wave(freq, duration, volume=0.5):
    n = int(WAV_RATE * duration)
    result = []
    for i in range(n):
        t = (freq * i / WAV_RATE) % 1.0
        if t < 0.25:
            val = 4.0 * t
        elif t < 0.75:
            val = 2.0 - 4.0 * t
        else:
            val = 4.0 * t - 4.0
        result.append(int(128 + 127 * volume * val))
    return result


def noise(duration, volume=0.3):
    """Low-pass filtered noise (much softer than raw white noise)."""
    import random
    random.seed(42)
    n = int(WAV_RATE * duration)
    raw = [random.random() * 2 - 1 for _ in range(n)]
    # Simple IIR low-pass filter (~2 kHz cutoff at 16384 Hz)
    filtered = [raw[0]]
    for i in range(1, n):
        filtered.append(filtered[-1] * 0.7 + raw[i] * 0.3)
    return [int(128 + 127 * volume * filtered[i]) for i in range(n)]


def envelope(samples, attack=0.01, decay=0.0, sustain=1.0, release=0.1):
    n = len(samples)
    a_len = int(WAV_RATE * attack)
    d_len = int(WAV_RATE * decay)
    r_len = int(WAV_RATE * release)
    r_start = max(0, n - r_len)
    result = []
    for i in range(n):
        if i < a_len:
            env = i / max(1, a_len)
        elif i < a_len + d_len:
            env = 1.0 - (1.0 - sustain) * (i - a_len) / max(1, d_len)
        elif i >= r_start:
            env = sustain * (1.0 - (i - r_start) / max(1, r_len))
        else:
            env = sustain
        val = 128 + (samples[i] - 128) * env
        result.append(int(max(0, min(255, val))))
    return result


def freq_sweep(f_start, f_end, duration, volume=0.5):
    """Exponential frequency sweep (sounds more natural than linear)."""
    n = int(WAV_RATE * duration)
    result = []
    phase = 0.0
    f_start = max(1, f_start)
    f_end = max(1, f_end)
    ratio = f_end / f_start
    for i in range(n):
        t = i / n
        freq = f_start * (ratio ** t)
        phase += 2 * math.pi * freq / WAV_RATE
        result.append(int(128 + 127 * volume * math.sin(phase)))
    return result


def mix_samples(*sample_lists):
    max_len = max(len(s) for s in sample_lists)
    result = []
    for i in range(max_len):
        total = 0.0
        count = 0
        for s in sample_lists:
            if i < len(s):
                total += (s[i] - 128)
                count += 1
        val = 128 + total / max(1, count)
        result.append(int(max(0, min(255, val))))
    return result


def generate_sfx():
    """Generate 18 sound effect WAV files — all sine-based, soft and clean."""
    sfx = {}

    # 01 SFX_SHOOT — clean laser blip
    s = freq_sweep(800, 300, 0.07, 0.45)
    sfx['01_shoot.wav'] = envelope(s, attack=0.003, decay=0.01, sustain=0.7, release=0.02)

    # 02 SFX_SHOOT_CHARGE — rising charge then release
    s1 = freq_sweep(150, 900, 0.2, 0.35)
    s2 = [128] * int(WAV_RATE * 0.2) + freq_sweep(900, 400, 0.08, 0.5)
    sfx['02_shoot_charge.wav'] = envelope(mix_samples(s1, s2),
                                          attack=0.01, decay=0.02, sustain=0.8, release=0.05)

    # 03 SFX_SHOOT_RAPID — short soft blip (triangle, not square)
    s = triangle_wave(700, 0.035, 0.4)
    sfx['03_shoot_rapid.wav'] = envelope(s, attack=0.002, release=0.01)

    # 04 SFX_ENEMY_HIT — soft impact thud
    lo = sine_wave(180, 0.05, 0.4)
    n = noise(0.05, 0.15)
    sfx['04_enemy_hit.wav'] = envelope(mix_samples(lo, n),
                                       attack=0.001, decay=0.01, sustain=0.5, release=0.015)

    # 05 SFX_ENEMY_DIE — descending digital dissolve
    s = freq_sweep(500, 60, 0.2, 0.4)
    n2 = noise(0.2, 0.12)
    sfx['05_enemy_die.wav'] = envelope(mix_samples(s, n2),
                                       attack=0.005, decay=0.05, sustain=0.6, release=0.06)

    # 06 SFX_PLAYER_HIT — impact with low rumble
    lo2 = sine_wave(100, 0.07, 0.45)
    n3 = noise(0.07, 0.15)
    sfx['06_player_hit.wav'] = envelope(mix_samples(lo2, n3),
                                        attack=0.001, decay=0.02, sustain=0.5, release=0.02)

    # 07 SFX_PLAYER_DIE — long descending fade
    s = freq_sweep(400, 40, 0.4, 0.4)
    n4 = noise(0.4, 0.1)
    sfx['07_player_die.wav'] = envelope(mix_samples(s, n4),
                                        attack=0.01, decay=0.1, sustain=0.5, release=0.15)

    # 08 SFX_JUMP — gentle rising chirp
    s = freq_sweep(250, 500, 0.05, 0.35)
    sfx['08_jump.wav'] = envelope(s, attack=0.003, release=0.015)

    # 09 SFX_WALL_JUMP — slightly higher chirp
    s = freq_sweep(300, 600, 0.06, 0.38)
    sfx['09_wall_jump.wav'] = envelope(s, attack=0.003, release=0.015)

    # 10 SFX_DASH — soft whoosh
    s = freq_sweep(350, 700, 0.1, 0.35)
    n5 = noise(0.1, 0.08)
    sfx['10_dash.wav'] = envelope(mix_samples(s, n5),
                                  attack=0.005, decay=0.02, sustain=0.7, release=0.03)

    # 11 SFX_PICKUP — bright ascending chime (G5 → C6)
    s1 = sine_wave(784, 0.06, 0.4)
    s2 = [128] * int(WAV_RATE * 0.06) + sine_wave(1047, 0.08, 0.45)
    sfx['11_pickup.wav'] = envelope(mix_samples(s1, s2), attack=0.003, release=0.04)

    # 12 SFX_MENU_SELECT — clean ascending two-note chime (C5 → E5)
    s1 = sine_wave(523, 0.04, 0.35)
    s2 = [128] * int(WAV_RATE * 0.04) + sine_wave(659, 0.05, 0.35)
    sfx['12_menu_select.wav'] = envelope(mix_samples(s1, s2), attack=0.003, release=0.02)

    # 13 SFX_MENU_BACK — gentle descending two-note (E5 → C5)
    s1 = sine_wave(659, 0.04, 0.3)
    s2 = [128] * int(WAV_RATE * 0.04) + sine_wave(523, 0.05, 0.3)
    sfx['13_menu_back.wav'] = envelope(mix_samples(s1, s2), attack=0.003, release=0.02)

    # 14 SFX_ABILITY — power activation shimmer
    dur = 0.18
    n_s = int(WAV_RATE * dur)
    s = [int(128 + 127 * 0.4 * math.sin(2 * math.pi * 523 * i / WAV_RATE)
         * (0.6 + 0.4 * math.sin(2 * math.pi * 8 * i / WAV_RATE)))
         for i in range(n_s)]
    sfx['14_ability.wav'] = envelope(s, attack=0.01, decay=0.03, sustain=0.7, release=0.05)

    # 15 SFX_BOSS_ROAR — deep rumble (sine pitch drop, not noise)
    s = freq_sweep(100, 30, 0.4, 0.5)
    n6 = noise(0.4, 0.12)
    sfx['15_boss_roar.wav'] = envelope(mix_samples(s, n6),
                                       attack=0.02, decay=0.1, sustain=0.6, release=0.12)

    # 16 SFX_LEVEL_DONE — ascending victory notes (C-E-G-C)
    notes_freqs = [523, 659, 784, 1047]
    s = []
    for freq in notes_freqs:
        s.extend(sine_wave(freq, 0.09, 0.4))
    sfx['16_level_done.wav'] = envelope(s, attack=0.005, release=0.08)

    # 17 SFX_SAVE — warm confirmation (A4 → E5)
    s1 = sine_wave(440, 0.08, 0.35)
    s2 = [128] * int(WAV_RATE * 0.08) + sine_wave(659, 0.1, 0.35)
    sfx['17_save.wav'] = envelope(mix_samples(s1, s2), attack=0.005, release=0.05)

    # 18 SFX_TRANSITION — gentle sweep
    s = freq_sweep(250, 800, 0.15, 0.35)
    n7 = noise(0.15, 0.06)
    sfx['18_transition.wav'] = envelope(mix_samples(s, n7), attack=0.01, release=0.05)

    return sfx


# ─── MOD Generation ──────────────────────────────────────────────────────────

def make_sample_data(waveform='sine', length=64, volume=48):
    """Generate a single-cycle waveform sample for MOD instruments."""
    if length % 2 != 0:
        length += 1
    data = []
    for i in range(length):
        t = i / length
        if waveform == 'pulse':
            # Bandlimited pulse: fundamental + weak odd harmonics
            # Much softer than a raw square wave
            raw = (math.sin(2 * math.pi * t)
                   + 0.25 * math.sin(2 * math.pi * 3 * t)
                   + 0.12 * math.sin(2 * math.pi * 5 * t))
            val = int(volume * raw / 1.37)
        elif waveform == 'sine':
            val = int(volume * math.sin(2 * math.pi * t))
        elif waveform == 'warm_sine':
            # Sine with slight 2nd harmonic for warmth
            raw = (math.sin(2 * math.pi * t)
                   + 0.15 * math.sin(2 * math.pi * 2 * t))
            val = int(volume * raw / 1.15)
        elif waveform == 'triangle':
            if t < 0.25:
                val = int(volume * 4.0 * t)
            elif t < 0.75:
                val = int(volume * (2.0 - 4.0 * t))
            else:
                val = int(volume * (4.0 * t - 4.0))
        else:
            val = 0
        data.append(max(-128, min(127, val)))
    return bytes(v & 0xFF for v in data)


def make_kick_sample(length=256, volume=50):
    """Bass drum: sine wave with pitch drop and amplitude decay."""
    data = []
    phase = 0.0
    for i in range(length):
        t = i / length
        # Phase increment drops exponentially (pitch drop effect)
        increment = 0.5 * math.exp(-4.0 * t) + 0.05
        phase += increment
        # Amplitude decays
        amp = volume * math.exp(-3.5 * t)
        val = int(amp * math.sin(2 * math.pi * phase))
        data.append(max(-128, min(127, val)))
    return bytes(v & 0xFF for v in data)


def make_hat_sample(length=128, volume=30):
    """Hi-hat: metallic ring with fast decay (not white noise)."""
    data = []
    for i in range(length):
        t = i / length
        amp = volume * math.exp(-10.0 * t)
        # Inharmonic frequency mix for metallic character
        val = amp * (0.6 * math.sin(i * 2.3) + 0.4 * math.sin(i * 3.7))
        data.append(max(-128, min(127, int(val))))
    return bytes(v & 0xFF for v in data)


def make_sample_header(name, length, volume=64, loop_start=0, loop_length=0, finetune=0):
    name_bytes = name.encode('ascii')[:22].ljust(22, b'\x00')
    return struct.pack('>22sHBBHH',
                       name_bytes,
                       length,
                       finetune & 0x0F,
                       min(64, volume),
                       loop_start,
                       loop_length)


def note_cell(period_idx, sample_num, effect=0, effect_param=0):
    if period_idx >= 0 and period_idx < len(PERIODS):
        period = PERIODS[period_idx]
    else:
        period = 0

    sample_hi = (sample_num >> 4) & 0x0F
    sample_lo = sample_num & 0x0F

    byte1 = (sample_hi << 4) | ((period >> 8) & 0x0F)
    byte2 = period & 0xFF
    byte3 = (sample_lo << 4) | (effect & 0x0F)
    byte4 = effect_param & 0xFF

    return bytes([byte1, byte2, byte3, byte4])


def empty_cell():
    return bytes([0, 0, 0, 0])


def build_pattern(rows_data):
    """Build a 64-row pattern from sparse row data.

    rows_data: list of (row, channel, period_idx, sample, effect, param)
    """
    pattern = bytearray(MOD_ROWS * MOD_CHANNELS * 4)
    for row, ch, period_idx, sample, effect, param in rows_data:
        if row >= MOD_ROWS or ch >= MOD_CHANNELS:
            continue
        offset = (row * MOD_CHANNELS + ch) * 4
        cell = note_cell(period_idx, sample, effect, param)
        pattern[offset:offset + 4] = cell
    return bytes(pattern)


def build_mod(title, samples, patterns, order, speed=6, bpm=125):
    """Build a complete MOD file."""
    title_bytes = title.encode('ascii')[:20].ljust(20, b'\x00')

    sample_headers = b''
    for i in range(31):
        if i < len(samples):
            sample_headers += samples[i][0]
        else:
            sample_headers += make_sample_header('', 0)

    song_length = len(order)
    restart = 0
    order_table = bytes(order + [0] * (128 - len(order)))
    tag = b'M.K.'

    # Inject BPM and speed into first pattern (separate rows to avoid mmutil issues)
    if len(patterns) > 0:
        pat0 = bytearray(patterns[0])
        for ch in range(MOD_CHANNELS):
            offset = ch * 4
            if (pat0[offset + 2] & 0x0F) == 0x0F:
                pat0[offset + 2] &= 0xF0
                pat0[offset + 3] = 0
        pat0[2] = (pat0[2] & 0xF0) | 0x0F
        pat0[3] = bpm & 0xFF
        row1_off = MOD_CHANNELS * 4
        if (pat0[row1_off + 2] & 0x0F) == 0x0F:
            pat0[row1_off + 2] &= 0xF0
            pat0[row1_off + 3] = 0
        pat0[row1_off + 2] = (pat0[row1_off + 2] & 0xF0) | 0x0F
        pat0[row1_off + 3] = speed & 0xFF
        patterns[0] = bytes(pat0)

    num_patterns = max(order) + 1 if order else 1

    mod_data = title_bytes + sample_headers
    mod_data += struct.pack('BB', song_length, restart)
    mod_data += order_table
    mod_data += tag

    for i in range(num_patterns):
        if i < len(patterns):
            mod_data += patterns[i]
        else:
            mod_data += bytes(MOD_ROWS * MOD_CHANNELS * 4)

    for i in range(len(samples)):
        mod_data += samples[i][1]

    return mod_data


# ─── MOD Instruments ─────────────────────────────────────────────────────────

def make_instruments():
    """Create instrument set — all soft waveforms, no harsh square/saw/noise."""
    # 1: Soft Pulse Lead — bandlimited pulse (fund + weak 3rd/5th harmonics)
    pulse_data = make_sample_data('pulse', 64, 48)
    pulse_hdr = make_sample_header('Soft Pulse', len(pulse_data) // 2, 48,
                                   0, len(pulse_data) // 2)

    # 2: Warm Bass — sine with slight 2nd harmonic
    bass_data = make_sample_data('warm_sine', 128, 50)
    bass_hdr = make_sample_header('Warm Bass', len(bass_data) // 2, 50,
                                  0, len(bass_data) // 2)

    # 3: Triangle Pad — inherently soft, warm pad
    pad_data = make_sample_data('triangle', 64, 40)
    pad_hdr = make_sample_header('Triangle Pad', len(pad_data) // 2, 40,
                                 0, len(pad_data) // 2)

    # 4: Sine Kick — pitch-dropping sine (not noise!), one-shot
    kick_data = make_kick_sample(256, 50)
    kick_hdr = make_sample_header('Sine Kick', len(kick_data) // 2, 50, 0, 0)

    # 5: Soft Hat — metallic ring with fast decay, one-shot
    hat_data = make_hat_sample(128, 30)
    hat_hdr = make_sample_header('Soft Hat', len(hat_data) // 2, 30, 0, 0)

    # 6: Clean Sine — pure sine for delicate tones
    sine_data = make_sample_data('sine', 64, 44)
    sine_hdr = make_sample_header('Clean Sine', len(sine_data) // 2, 44,
                                  0, len(sine_data) // 2)

    return [
        (pulse_hdr, pulse_data),
        (bass_hdr, bass_data),
        (pad_hdr, pad_data),
        (kick_hdr, kick_data),
        (hat_hdr, hat_data),
        (sine_hdr, sine_data),
    ]


# ─── Drum Patterns ───────────────────────────────────────────────────────────

def drums_sparse(rows, ch=3):
    """Minimal ambient percussion — just two soft kicks."""
    rows.append((0, ch, C1, SMP_KICK, 0x0C, 22))
    rows.append((32, ch, C1, SMP_KICK, 0x0C, 18))


def drums_light(rows, ch=3):
    """Light beat — kick on 1/3, hat on 2/4."""
    for r in range(0, 64, 16):
        rows.append((r, ch, C1, SMP_KICK, 0x0C, 28))
        rows.append((r + 4, ch, C2, SMP_HAT, 0x0C, 12))
        rows.append((r + 8, ch, C1, SMP_KICK, 0x0C, 22))
        rows.append((r + 12, ch, C2, SMP_HAT, 0x0C, 10))


def drums_steady(rows, ch=3):
    """Steady 4-on-the-floor."""
    for r in range(0, 64, 8):
        rows.append((r, ch, C1, SMP_KICK, 0x0C, 26))
        rows.append((r + 4, ch, C2, SMP_HAT, 0x0C, 12))


def drums_driving(rows, ch=3):
    """Driving beat with off-beat hats."""
    for r in range(0, 64, 16):
        rows.append((r, ch, C1, SMP_KICK, 0x0C, 30))
        rows.append((r + 4, ch, C2, SMP_HAT, 0x0C, 12))
        rows.append((r + 6, ch, C2, SMP_HAT, 0x0C, 8))
        rows.append((r + 8, ch, C1, SMP_KICK, 0x0C, 24))
        rows.append((r + 12, ch, C2, SMP_HAT, 0x0C, 10))


def drums_intense(rows, ch=3):
    """Intense boss/action drums."""
    for r in range(0, 64, 8):
        rows.append((r, ch, C1, SMP_KICK, 0x0C, 32))
        rows.append((r + 2, ch, C2, SMP_HAT, 0x0C, 10))
        rows.append((r + 4, ch, C1, SMP_KICK, 0x0C, 26))
        rows.append((r + 6, ch, C2, SMP_HAT, 0x0C, 8))


# ─── Music Tracks ────────────────────────────────────────────────────────────

def track_title():
    """00_title.mod — Atmospheric cyberpunk intro. Dark, moody, inviting."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Ambient intro — pad drone + sparse bass
    rows = []
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 16))
    rows.append((32, 2, Ds2, SMP_PAD, 0x0C, 14))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 22))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 20))
    drums_sparse(rows)
    patterns.append(build_pattern(rows))

    # Pattern 1: Melody enters — gentle sine lead
    rows = []
    melody = [E3, -1, G3, Gs3, G3, -1, E3, D3]
    for i, note in enumerate(melody):
        if note >= 0:
            rows.append((i * 8, 0, note, SMP_SINE, 0x0C, 20))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 22))
    rows.append((32, 1, Gs1, SMP_BASS, 0x0C, 20))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 16))
    rows.append((32, 2, Gs2, SMP_PAD, 0x0C, 14))
    drums_sparse(rows)
    patterns.append(build_pattern(rows))

    # Pattern 2: Melodic variation
    rows = []
    melody2 = [C3, -1, D3, Ds3, G3, -1, Ds3, C3]
    for i, note in enumerate(melody2):
        if note >= 0:
            rows.append((i * 8, 0, note, SMP_SINE, 0x0C, 18))
    rows.append((0, 1, Gs1, SMP_BASS, 0x0C, 20))
    rows.append((32, 1, F1, SMP_BASS, 0x0C, 18))
    rows.append((0, 2, Gs2, SMP_PAD, 0x0C, 14))
    rows.append((32, 2, F2, SMP_PAD, 0x0C, 14))
    drums_sparse(rows)
    patterns.append(build_pattern(rows))

    # Pattern 3: Build — light drums enter
    rows = []
    arp = [C3, Ds3, G3, C3, Ds3, G3, Gs3, G3]
    for i, note in enumerate(arp):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 18))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 24))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 22))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 18))
    rows.append((32, 2, G2, SMP_PAD, 0x0C, 16))
    drums_light(rows)
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 2, 1, 3, 0, 1]
    return build_mod('Ghost Protocol', instruments, patterns, order, speed=6, bpm=90)


def track_terminal():
    """01_terminal.mod — Calm ambient terminal hum. No drums."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Pure ambient drone
    rows = []
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 14))
    rows.append((32, 2, Ds2, SMP_PAD, 0x0C, 12))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 18))
    patterns.append(build_pattern(rows))

    # Pattern 1: Gentle blips enter
    rows = []
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 14))
    rows.append((16, 2, G2, SMP_PAD, 0x0C, 12))
    rows.append((32, 2, Ds2, SMP_PAD, 0x0C, 14))
    rows.append((48, 2, F2, SMP_PAD, 0x0C, 10))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 16))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 14))
    rows.append((8, 0, G3, SMP_SINE, 0x0C, 10))
    rows.append((40, 0, C3, SMP_SINE, 0x0C, 8))
    patterns.append(build_pattern(rows))

    # Pattern 2: Different key center
    rows = []
    rows.append((0, 2, Gs1, SMP_PAD, 0x0C, 14))
    rows.append((32, 2, F2, SMP_PAD, 0x0C, 12))
    rows.append((0, 1, Gs1, SMP_BASS, 0x0C, 16))
    rows.append((24, 0, Ds3, SMP_SINE, 0x0C, 10))
    rows.append((56, 0, Gs3, SMP_SINE, 0x0C, 8))
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 2, 1, 0]
    return build_mod('Terminal', instruments, patterns, order, speed=6, bpm=75)


def track_net_easy():
    """02_net_easy.mod — Upbeat electronic action (low tier)."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Main groove
    rows = []
    bass_seq = [C2, C2, Ds2, F2]
    for i, r in enumerate(range(0, 64, 16)):
        rows.append((r, 1, bass_seq[i], SMP_BASS, 0x0C, 28))
    lead = [G3, As3, G3, Ds3, F3, Ds3, C3, Ds3]
    for i, note in enumerate(lead):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 22))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 14))
    rows.append((32, 2, Ds2, SMP_PAD, 0x0C, 14))
    drums_light(rows)
    patterns.append(build_pattern(rows))

    # Pattern 1: Melodic variation
    rows = []
    bass_seq2 = [Gs1, Gs1, C2, D2]
    for i, r in enumerate(range(0, 64, 16)):
        rows.append((r, 1, bass_seq2[i], SMP_BASS, 0x0C, 26))
    lead2 = [Ds3, G3, Gs3, G3, Ds3, C3, Ds3, G3]
    for i, note in enumerate(lead2):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 20))
    rows.append((0, 2, Gs1, SMP_PAD, 0x0C, 14))
    rows.append((32, 2, C2, SMP_PAD, 0x0C, 14))
    drums_light(rows)
    patterns.append(build_pattern(rows))

    # Pattern 2: Breakdown
    rows = []
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 28))
    rows.append((32, 1, Ds1, SMP_BASS, 0x0C, 26))
    rows.append((0, 0, C3, SMP_SINE, 0x0C, 16))
    rows.append((16, 0, Ds3, SMP_SINE, 0x0C, 14))
    rows.append((32, 0, G3, SMP_SINE, 0x0C, 18))
    rows.append((48, 0, Ds3, SMP_SINE, 0x0C, 14))
    drums_sparse(rows)
    patterns.append(build_pattern(rows))

    # Pattern 3: Buildup
    rows = []
    for r in range(0, 64, 8):
        vol = 16 + r // 8
        note = G3 if r % 16 == 0 else Ds3
        rows.append((r, 0, note, SMP_LEAD, 0x0C, min(26, vol)))
    rows.append((0, 1, C2, SMP_BASS, 0x0C, 28))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 26))
    drums_steady(rows)
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 2, 0, 1, 3, 0]
    return build_mod('Net Easy', instruments, patterns, order, speed=6, bpm=120)


def track_net_hard():
    """03_net_hard.mod — Intense electronic action (high tier)."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Aggressive groove
    rows = []
    bass = [C2, C2, Ds2, D2]
    for i, r in enumerate(range(0, 64, 16)):
        rows.append((r, 1, bass[i], SMP_BASS, 0x0C, 30))
        rows.append((r + 8, 1, bass[i], SMP_BASS, 0x0C, 24))
    lead = [C3, Ds3, F3, G3, F3, Ds3, D3, C3]
    for i, note in enumerate(lead):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 24))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 16))
    drums_driving(rows)
    patterns.append(build_pattern(rows))

    # Pattern 1: Bridge
    rows = []
    rows.append((0, 1, Gs1, SMP_BASS, 0x0C, 28))
    rows.append((16, 1, As1, SMP_BASS, 0x0C, 26))
    rows.append((32, 1, C2, SMP_BASS, 0x0C, 30))
    rows.append((48, 1, D2, SMP_BASS, 0x0C, 28))
    melody = [Gs3, C3, Ds3, G3]
    for i, note in enumerate(melody):
        rows.append((i * 16, 0, note, SMP_LEAD, 0x0C, 22))
    rows.append((0, 2, Gs1, SMP_PAD, 0x0C, 14))
    drums_driving(rows)
    patterns.append(build_pattern(rows))

    # Pattern 2: Breakdown then surge
    rows = []
    rows.append((0, 0, C3, SMP_PAD, 0x0C, 18))
    rows.append((32, 0, C3, SMP_LEAD, 0x0C, 24))
    rows.append((40, 0, Ds3, SMP_LEAD, 0x0C, 22))
    rows.append((48, 0, G3, SMP_LEAD, 0x0C, 26))
    rows.append((56, 0, C3, SMP_LEAD, 0x0C, 22))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 30))
    rows.append((32, 1, C2, SMP_BASS, 0x0C, 28))
    drums_light(rows)
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 2, 0, 1, 0, 2]
    return build_mod('Net Hard', instruments, patterns, order, speed=5, bpm=130)


def track_net_final():
    """04_net_final.mod — Final tier net music. Dramatic and urgent."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Ominous opening — tritone tension
    rows = []
    rows.append((0, 0, C2, SMP_PAD, 0x0C, 18))
    rows.append((16, 0, Ds2, SMP_PAD, 0x0C, 16))
    rows.append((32, 0, Fs2, SMP_PAD, 0x0C, 20))
    rows.append((48, 0, G2, SMP_PAD, 0x0C, 16))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 28))
    rows.append((32, 1, Fs1, SMP_BASS, 0x0C, 26))
    drums_steady(rows)
    patterns.append(build_pattern(rows))

    # Pattern 1: Full intensity
    rows = []
    lead = [C3, Ds3, Fs3, G3, Fs3, Ds3, C3, B2]
    for i, note in enumerate(lead):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 24))
    bass = [C2, Fs1, G1, C2]
    for i, r in enumerate(range(0, 64, 16)):
        rows.append((r, 1, bass[i], SMP_BASS, 0x0C, 28))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 16))
    drums_driving(rows)
    patterns.append(build_pattern(rows))

    # Pattern 2: Climax
    rows = []
    for r in range(0, 64, 4):
        note = C3 if r % 8 == 0 else G3
        rows.append((r, 0, note, SMP_LEAD, 0x0C, 26))
    rows.append((0, 1, C2, SMP_BASS, 0x0C, 30))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 28))
    rows.append((0, 2, G2, SMP_PAD, 0x0C, 18))
    drums_driving(rows)
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 2, 1, 0, 1, 2]
    return build_mod('Net Final', instruments, patterns, order, speed=5, bpm=140)


def track_boss():
    """05_boss.mod — Intense boss battle. Dramatic but musical."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Menacing intro — low register
    rows = []
    rows.append((0, 0, C2, SMP_LEAD, 0x0C, 24))
    rows.append((8, 0, C2, SMP_LEAD, 0x0C, 22))
    rows.append((16, 0, Ds2, SMP_LEAD, 0x0C, 24))
    rows.append((24, 0, D2, SMP_LEAD, 0x0C, 22))
    rows.append((32, 0, C2, SMP_LEAD, 0x0C, 26))
    rows.append((40, 0, B1, SMP_LEAD, 0x0C, 22))
    rows.append((48, 0, C2, SMP_LEAD, 0x0C, 28))
    rows.append((56, 0, Ds2, SMP_LEAD, 0x0C, 24))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 30))
    rows.append((32, 1, B1, SMP_BASS, 0x0C, 28))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 16))
    drums_driving(rows)
    patterns.append(build_pattern(rows))

    # Pattern 1: Relentless attack
    rows = []
    attack = [C3, Ds3, F3, G3, F3, Ds3, D3, C3]
    for i, note in enumerate(attack):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 26))
    for r in range(0, 64, 16):
        bass_note = C2 if r < 32 else G1
        rows.append((r, 1, bass_note, SMP_BASS, 0x0C, 28))
    rows.append((0, 2, Ds2, SMP_PAD, 0x0C, 16))
    rows.append((32, 2, C2, SMP_PAD, 0x0C, 16))
    drums_intense(rows)
    patterns.append(build_pattern(rows))

    # Pattern 2: Tension bridge
    rows = []
    rows.append((0, 0, C2, SMP_PAD, 0x0C, 20))
    rows.append((16, 0, Ds2, SMP_PAD, 0x0C, 18))
    rows.append((32, 0, Fs2, SMP_PAD, 0x0C, 22))
    rows.append((48, 0, G2, SMP_PAD, 0x0C, 18))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 30))
    drums_steady(rows)
    patterns.append(build_pattern(rows))

    # Pattern 3: Climax
    rows = []
    climax = [C3, D3, Ds3, F3, G3, F3, Ds3, C3]
    for i, note in enumerate(climax):
        rows.append((i * 8, 0, note, SMP_LEAD, 0x0C, 28))
    rows.append((0, 1, C2, SMP_BASS, 0x0C, 32))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 30))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 18))
    drums_intense(rows)
    patterns.append(build_pattern(rows))

    order = [0, 1, 2, 1, 0, 3, 1, 3]
    return build_mod('Boss Battle', instruments, patterns, order, speed=4, bpm=145)


def track_victory():
    """06_victory.mod — Level complete fanfare. Bright and uplifting."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Triumphant C major fanfare
    rows = []
    fanfare = [C3, E3, G3, C3, E3, G3, A3, G3]
    for i, note in enumerate(fanfare):
        rows.append((i * 8, 0, note, SMP_SINE, 0x0C, 22))
    rows.append((0, 1, C2, SMP_BASS, 0x0C, 24))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 22))
    rows.append((0, 2, E2, SMP_PAD, 0x0C, 16))
    rows.append((32, 2, C2, SMP_PAD, 0x0C, 16))
    drums_light(rows)
    patterns.append(build_pattern(rows))

    # Pattern 1: Sustained resolution
    rows = []
    rows.append((0, 0, C3, SMP_PAD, 0x0C, 20))
    rows.append((32, 0, G2, SMP_PAD, 0x0C, 18))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 22))
    rows.append((32, 1, E1, SMP_BASS, 0x0C, 20))
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 1]
    return build_mod('Victory', instruments, patterns, order, speed=6, bpm=110)


def track_gameover():
    """07_gameover.mod — Somber game over. Quiet, slow, melancholy."""
    instruments = make_instruments()
    patterns = []

    # Pattern 0: Descending sadness
    rows = []
    desc = [G2, F2, Ds2, D2, C2, B1, Gs1, G1]
    for i, note in enumerate(desc):
        rows.append((i * 8, 0, note, SMP_SINE, 0x0C, 16))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 18))
    rows.append((32, 1, G1, SMP_BASS, 0x0C, 16))
    rows.append((0, 2, C2, SMP_PAD, 0x0C, 12))
    patterns.append(build_pattern(rows))

    # Pattern 1: Hollow echo — barely there
    rows = []
    rows.append((0, 0, C2, SMP_PAD, 0x0C, 14))
    rows.append((32, 0, G1, SMP_PAD, 0x0C, 12))
    rows.append((0, 1, C1, SMP_BASS, 0x0C, 16))
    patterns.append(build_pattern(rows))

    order = [0, 1, 0, 1]
    return build_mod('Game Over', instruments, patterns, order, speed=6, bpm=70)


# ─── Main ────────────────────────────────────────────────────────────────────

def main():
    os.makedirs(MUSIC_DIR, exist_ok=True)
    os.makedirs(SFX_DIR, exist_ok=True)

    # Generate music
    tracks = [
        ('00_title.mod', track_title),
        ('01_terminal.mod', track_terminal),
        ('02_net_easy.mod', track_net_easy),
        ('03_net_hard.mod', track_net_hard),
        ('04_net_final.mod', track_net_final),
        ('05_boss.mod', track_boss),
        ('06_victory.mod', track_victory),
        ('07_gameover.mod', track_gameover),
    ]

    for filename, gen_func in tracks:
        path = os.path.join(MUSIC_DIR, filename)
        data = gen_func()
        with open(path, 'wb') as f:
            f.write(data)
        print(f"  {path} ({len(data)} bytes)")

    # Generate SFX
    sfx = generate_sfx()
    for filename, data in sorted(sfx.items()):
        path = os.path.join(SFX_DIR, filename)
        wav = make_wav(data)
        with open(path, 'wb') as f:
            f.write(wav)
        print(f"  {path} ({len(wav)} bytes)")

    print(f"\nDone: {len(tracks)} music tracks, {len(sfx)} sound effects")


if __name__ == '__main__':
    main()
