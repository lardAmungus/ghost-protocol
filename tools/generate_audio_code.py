#!/usr/bin/env python3
"""Auto-generate audio.h and audio.c from audio file listings.

Key insight: mmutil soundbank.h #defines names like SFX_39_AMBIENT_WIND.
If our audio.h enum uses the SAME name, the #define clobbers the enum.
Solution: named game-side enums (SFX_SHOOT) only for the original 38.
New SFX beyond 38 use numeric indices in sfx_map — no enum name needed.
"""
import os, re

SFX_DIR = "audio/sfx"
MUSIC_DIR = "audio/music"

# Original named SFX (indices 0-38, game code references by name)
NAMED_SFX = [
    (0,  "SFX_NONE"),
    (1,  "SFX_SHOOT"),
    (2,  "SFX_SHOOT_CHARGE"),
    (3,  "SFX_SHOOT_RAPID"),
    (4,  "SFX_ENEMY_HIT"),
    (5,  "SFX_ENEMY_DIE"),
    (6,  "SFX_PLAYER_HIT"),
    (7,  "SFX_PLAYER_DIE"),
    (8,  "SFX_JUMP"),
    (9,  "SFX_WALL_JUMP"),
    (10, "SFX_DASH"),
    (11, "SFX_PICKUP"),
    (12, "SFX_MENU_SELECT"),
    (13, "SFX_MENU_BACK"),
    (14, "SFX_ABILITY"),
    (15, "SFX_BOSS_ROAR"),
    (16, "SFX_LEVEL_DONE"),
    (17, "SFX_SAVE"),
    (18, "SFX_TRANSITION"),
    (19, "SFX_WALL_SLIDE"),
    (20, "SFX_LAND_HEAVY"),
    (21, "SFX_DOUBLE_JUMP"),
    (22, "SFX_BEAM_HUM"),
    (23, "SFX_SPREAD_BURST"),
    (24, "SFX_CHARGER_WHINE"),
    (25, "SFX_BOSS_PHASE"),
    (26, "SFX_BOSS_EXPLODE"),
    (27, "SFX_TESLA_ZAP"),
    (28, "SFX_CRAFT_SUCCESS"),
    (29, "SFX_EVOLVE"),
    (30, "SFX_ACHIEVEMENT"),
    (31, "SFX_BEAM_FIRE"),
    (32, "SFX_LASER_CRACK"),
    (33, "SFX_NOVA_WHOOSH"),
    (34, "SFX_HOMING_WHINE"),
    (35, "SFX_ENEMY_HIT_MECH"),
    (36, "SFX_ENEMY_DIE_MECH"),
    (37, "SFX_ENEMY_HIT_DIGI"),
    (38, "SFX_ENEMY_DIE_DIGI"),
]

# Original named MUS enums
NAMED_MUS = [
    (0,  "MUS_SILENCE"),
    (1,  "MUS_TITLE"),
    (2,  "MUS_TERMINAL"),
    (3,  "MUS_ACT1"),
    (4,  "MUS_ACT2"),
    (5,  "MUS_ACT3"),
    (6,  "MUS_ACT4"),
    (7,  "MUS_ACT5"),
    (8,  "MUS_ACT6"),
    (9,  "MUS_BOSS_CORP"),
    (10, "MUS_BOSS_GATE"),
    (11, "MUS_BOSS_DAEMON"),
    (12, "MUS_MINIBOSS"),
    (13, "MUS_VICTORY"),
    (14, "MUS_GAMEOVER"),
    (15, "MUS_CREDITS"),
    (16, "MUS_EVOLUTION"),
]


def fname_to_mmutil(fname, prefix):
    """Convert filename to mmutil constant name (e.g. '01_shoot.wav' -> 'SFX_01_SHOOT')."""
    stem = os.path.splitext(fname)[0]
    return prefix + "_" + re.sub(r'[^a-zA-Z0-9]', '_', stem).upper()


def scan_files(directory, ext):
    if not os.path.isdir(directory):
        return []
    return sorted(f for f in os.listdir(directory) if f.endswith(ext))


def generate_audio_h(sfx_files, mod_files):
    sfx_count = len(sfx_files) + 1  # +1 for SFX_NONE
    mus_count = len(mod_files) + 1  # +1 for MUS_SILENCE

    lines = []
    lines.append("#ifndef ENGINE_AUDIO_H")
    lines.append("#define ENGINE_AUDIO_H")
    lines.append("")
    lines.append("#include <tonc.h>")
    lines.append("")

    # Music enum — only named entries, plus MUS_COUNT
    lines.append(f"/* Ghost Protocol — Music track IDs ({len(mod_files)} tracks) */")
    lines.append("enum {")
    named_mus_ids = {idx for idx, _ in NAMED_MUS}
    for idx, name in NAMED_MUS:
        lines.append(f"    {name:30s} = {idx},")
    # New MUS entries beyond named — use numeric + descriptive suffix
    for i, fname in enumerate(mod_files):
        mus_idx = i + 1
        if mus_idx not in named_mus_ids:
            # Use MUS_EX_ prefix to avoid collision with MOD_ soundbank defines
            stem = os.path.splitext(fname)[0]
            # Strip leading digits
            clean = re.sub(r'^\d+_', '', stem).upper()
            clean = re.sub(r'[^A-Z0-9]', '_', clean)
            lines.append(f"    {'MUS_EX_' + clean:30s} = {mus_idx},")
    lines.append(f"    {'MUS_COUNT':30s}")
    lines.append("};")
    lines.append("")
    lines.append("#define MUS_NET_EASY  MUS_ACT1")
    lines.append("#define MUS_NET_HARD  MUS_ACT3")
    lines.append("#define MUS_NET_FINAL MUS_ACT5")
    lines.append("#define MUS_BOSS      MUS_BOSS_CORP")
    lines.append("")

    # SFX enum — only named entries + SFX_COUNT
    lines.append(f"/* Ghost Protocol — Sound effect IDs ({len(sfx_files)} effects) */")
    lines.append("enum {")
    for idx, name in NAMED_SFX:
        lines.append(f"    {name:30s} = {idx},")
    # No new enum names for expanded SFX — they use numeric indices
    # Just set the count
    lines.append(f"    {'SFX_COUNT':30s} = {sfx_count}")
    lines.append("};")
    lines.append("")

    lines.append("void audio_init(void);")
    lines.append("void audio_update(void);")
    lines.append("void audio_play_music(int module_id);")
    lines.append("void audio_stop_music(void);")
    lines.append("void audio_play_sfx(int sfx_id);")
    lines.append("void audio_fade_music(int frames);")
    lines.append("void audio_update_fade(void);")
    lines.append("/* Set combat intensity: 0=normal, 1=medium, 2=high — adjusts tempo */")
    lines.append("void audio_set_intensity(int level);")
    lines.append("")
    lines.append("#endif /* ENGINE_AUDIO_H */")
    lines.append("")
    return "\n".join(lines)


def generate_audio_c(sfx_files, mod_files):
    lines = []
    lines.append('#include "engine/audio.h"')
    lines.append("")
    lines.append("#ifdef HEADLESS_TEST")
    lines.append("void audio_init(void) {}")
    lines.append("void audio_update(void) {}")
    lines.append("void audio_play_music(int module_id) { (void)module_id; }")
    lines.append("void audio_stop_music(void) {}")
    lines.append("void audio_play_sfx(int sfx_id) { (void)sfx_id; }")
    lines.append("void audio_fade_music(int frames) { (void)frames; }")
    lines.append("void audio_update_fade(void) {}")
    lines.append("void audio_set_intensity(int level) { (void)level; }")
    lines.append("#else")
    lines.append("")
    lines.append("#include <maxmod.h>")
    lines.append('#include "soundbank.h"')
    lines.append('#include "soundbank_bin.h"')
    lines.append("")

    # Music map
    named_mus_ids = {idx for idx, _ in NAMED_MUS}
    lines.append("static const mm_word music_map[MUS_COUNT] = {")
    lines.append("    [MUS_SILENCE] = 0,")
    for i, fname in enumerate(mod_files):
        mmutil_const = fname_to_mmutil(fname, "MOD")
        mus_idx = i + 1
        # Find game-side enum name
        game_name = None
        for idx, name in NAMED_MUS:
            if idx == mus_idx:
                game_name = name
                break
        if game_name is None:
            stem = os.path.splitext(fname)[0]
            clean = re.sub(r'^\d+_', '', stem).upper()
            clean = re.sub(r'[^A-Z0-9]', '_', clean)
            game_name = "MUS_EX_" + clean
        lines.append(f"    [{game_name}] = {mmutil_const},")
    lines.append("};")
    lines.append("")

    # SFX map — use named enums for original 38, numeric indices for expanded
    named_sfx_map = {}
    for idx, name in NAMED_SFX:
        named_sfx_map[idx] = name

    lines.append("static const mm_word sfx_map[SFX_COUNT] = {")
    lines.append("    [SFX_NONE] = 0,")
    for i, fname in enumerate(sfx_files):
        mmutil_const = fname_to_mmutil(fname, "SFX")
        sfx_idx = i + 1  # game index (0 = SFX_NONE)
        game_name = named_sfx_map.get(sfx_idx)
        if game_name:
            lines.append(f"    [{game_name}] = {mmutil_const},")
        else:
            # Use numeric index to avoid #define collision
            lines.append(f"    [{sfx_idx}] = {mmutil_const},")
    lines.append("};")
    lines.append("")

    # Rest of audio.c (unchanged implementation)
    lines.append("static int fade_total = 0;")
    lines.append("static int fade_remaining = 0;")
    lines.append("")
    lines.append("void audio_init(void) {")
    lines.append("    mmInitDefault((mm_addr)soundbank_bin, 8);")
    lines.append("    fade_total = 0;")
    lines.append("    fade_remaining = 0;")
    lines.append("}")
    lines.append("")
    lines.append("void audio_update(void) { mmFrame(); }")
    lines.append("")
    lines.append("void audio_play_music(int module_id) {")
    lines.append("    fade_remaining = 0; fade_total = 0;")
    lines.append("    mmSetModuleVolume(1024);")
    lines.append("    if (module_id <= MUS_SILENCE || module_id >= MUS_COUNT) { mmStop(); return; }")
    lines.append("    mmStart((mm_word)music_map[module_id], MM_PLAY_LOOP);")
    lines.append("}")
    lines.append("")
    lines.append("void audio_stop_music(void) { mmStop(); }")
    lines.append("")
    lines.append("void audio_play_sfx(int sfx_id) {")
    lines.append("    if (sfx_id <= SFX_NONE || sfx_id >= SFX_COUNT) return;")
    lines.append("    mm_sfxhand h = mmEffect((mm_word)sfx_map[sfx_id]); (void)h;")
    lines.append("}")
    lines.append("")
    lines.append("void audio_set_intensity(int level) {")
    lines.append("    mm_word tempo;")
    lines.append("    switch (level) {")
    lines.append("    case 2:  tempo = 1200; break;")
    lines.append("    case 1:  tempo = 1100; break;")
    lines.append("    default: tempo = 1024; break;")
    lines.append("    }")
    lines.append("    mmSetModuleTempo(tempo);")
    lines.append("}")
    lines.append("")
    lines.append("void audio_fade_music(int frames) {")
    lines.append("    if (frames <= 0) { mmStop(); return; }")
    lines.append("    fade_total = frames; fade_remaining = frames;")
    lines.append("}")
    lines.append("")
    lines.append("void audio_update_fade(void) {")
    lines.append("    if (fade_remaining <= 0) return;")
    lines.append("    fade_remaining--;")
    lines.append("    if (fade_remaining <= 0) {")
    lines.append("        mmStop(); mmSetModuleVolume(1024); fade_total = 0;")
    lines.append("    } else if (fade_total <= 0) {")
    lines.append("        mmStop(); mmSetModuleVolume(1024);")
    lines.append("        fade_remaining = 0; fade_total = 0;")
    lines.append("    } else {")
    lines.append("        int vol = (fade_remaining * 1024) / fade_total;")
    lines.append("        mmSetModuleVolume((mm_word)vol);")
    lines.append("    }")
    lines.append("}")
    lines.append("")
    lines.append("#endif /* HEADLESS_TEST */")
    lines.append("")
    return "\n".join(lines)


def main():
    os.chdir(os.path.dirname(os.path.abspath(__file__)) + "/..")
    sfx_files = scan_files(SFX_DIR, ".wav")
    mod_files = scan_files(MUSIC_DIR, ".mod")
    print(f"Found {len(sfx_files)} SFX files, {len(mod_files)} MOD files")

    audio_h = generate_audio_h(sfx_files, mod_files)
    audio_c = generate_audio_c(sfx_files, mod_files)

    with open("include/engine/audio.h", "w") as f:
        f.write(audio_h)
    print(f"Wrote include/engine/audio.h ({len(audio_h)} bytes)")

    with open("source/engine/audio.c", "w") as f:
        f.write(audio_c)
    print(f"Wrote source/engine/audio.c ({len(audio_c)} bytes)")


if __name__ == "__main__":
    main()
