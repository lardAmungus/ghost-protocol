#include "game/colors.h"
#include "game/common.h"

/*
 * Suit color presets — 3 shades each: dark / medium / bright
 * Stored at player OBJ palette bank 0, indices 2 / 3 / 4.
 * GBA RGB15: 5 bits per channel (0-31), packed as R | G<<5 | B<<10.
 */
static const u16 suit_presets[COLOR_PRESET_COUNT][3] = {
    /* 0: Red        — dark maroon / mid red / bright red */
    { RGB15_C(12,  1,  1), RGB15_C(22,  2,  2), RGB15_C(31,  6,  6) },
    /* 1: Crimson    — deep burgundy / crimson / vivid crimson */
    { RGB15_C(14,  0,  4), RGB15_C(24,  2,  6), RGB15_C(31,  4, 10) },
    /* 2: Orange     — dark burnt orange / mid orange / bright orange */
    { RGB15_C(14,  5,  0), RGB15_C(24, 10,  0), RGB15_C(31, 16,  2) },
    /* 3: Amber      — dark amber / warm amber / bright amber */
    { RGB15_C(14,  8,  0), RGB15_C(24, 14,  0), RGB15_C(31, 20,  2) },
    /* 4: Gold       — dark gold / mid gold / bright gold */
    { RGB15_C(14, 11,  0), RGB15_C(24, 18,  0), RGB15_C(31, 26,  2) },
    /* 5: Yellow     — dark yellow / mid yellow / bright yellow */
    { RGB15_C(12, 12,  0), RGB15_C(22, 22,  0), RGB15_C(31, 31,  4) },
    /* 6: Lime       — dark lime / mid lime / bright lime */
    { RGB15_C( 6, 14,  0), RGB15_C(10, 24,  2), RGB15_C(14, 31,  4) },
    /* 7: Green      — dark forest / mid green / bright green */
    { RGB15_C( 1, 12,  1), RGB15_C( 2, 22,  2), RGB15_C( 4, 31,  4) },
    /* 8: Emerald    — dark emerald / mid emerald / bright emerald */
    { RGB15_C( 0, 12,  6), RGB15_C( 1, 22, 10), RGB15_C( 2, 31, 14) },
    /* 9: Teal       — dark teal / mid teal / bright teal */
    { RGB15_C( 0, 10, 10), RGB15_C( 1, 18, 18), RGB15_C( 2, 26, 26) },
    /* 10: Cyan      — dark cyan / mid cyan / bright cyan */
    { RGB15_C( 0, 12, 14), RGB15_C( 0, 20, 24), RGB15_C( 2, 28, 31) },
    /* 11: Sky       — dark sky / mid sky / bright sky */
    { RGB15_C( 2,  8, 16), RGB15_C( 4, 16, 26), RGB15_C( 6, 22, 31) },
    /* 12: Blue      — dark navy / mid blue / bright blue */
    { RGB15_C( 1,  2, 14), RGB15_C( 2,  4, 24), RGB15_C( 4,  8, 31) },
    /* 13: Indigo    — dark indigo / mid indigo / bright indigo */
    { RGB15_C( 4,  1, 14), RGB15_C( 8,  2, 24), RGB15_C(12,  4, 31) },
    /* 14: Violet    — dark violet / mid violet / bright violet */
    { RGB15_C( 8,  0, 14), RGB15_C(14,  2, 24), RGB15_C(20,  4, 31) },
    /* 15: Purple    — dark purple / mid purple / bright purple */
    { RGB15_C(10,  0, 10), RGB15_C(18,  2, 18), RGB15_C(26,  4, 26) },
    /* 16: Magenta   — dark magenta / mid magenta / bright magenta */
    { RGB15_C(12,  0,  8), RGB15_C(22,  2, 16), RGB15_C(31,  4, 22) },
    /* 17: Pink      — dark rose / mid pink / bright pink */
    { RGB15_C(14,  2,  8), RGB15_C(24,  6, 14), RGB15_C(31, 12, 20) },
    /* 18: White     — light gray / mid gray / near-white */
    { RGB15_C(14, 14, 14), RGB15_C(22, 22, 22), RGB15_C(29, 29, 29) },
    /* 19: Silver    — blue-tinted dark / silver-blue / bright silver */
    { RGB15_C(10, 10, 13), RGB15_C(18, 18, 22), RGB15_C(26, 26, 30) },
    /* 20: Gunmetal  — dark gunmetal / mid gunmetal / bright gunmetal */
    { RGB15_C( 5,  6,  7), RGB15_C(10, 12, 13), RGB15_C(16, 18, 20) },
    /* 21: Charcoal  — very dark / dark gray / charcoal mid */
    { RGB15_C( 4,  4,  4), RGB15_C( 8,  8,  8), RGB15_C(13, 13, 13) },
    /* 22: Brown     — dark brown / mid brown / tan */
    { RGB15_C(10,  5,  1), RGB15_C(18,  9,  3), RGB15_C(26, 14,  6) },
    /* 23: Olive     — dark olive / mid olive / bright olive */
    { RGB15_C( 7,  8,  0), RGB15_C(13, 14,  1), RGB15_C(20, 20,  4) },
};

/*
 * Visor color presets — 3 shades each: dim / mid / bright
 * Stored at player OBJ palette bank 0, indices 5 / 6 / 7.
 * Visor colors have a "glowing" quality — more saturated, with a near-white
 * bright shade to simulate emissive lens glow.
 */
static const u16 visor_presets[COLOR_PRESET_COUNT][3] = {
    /* 0: Red        — dim red glow / vivid red / white-red flare */
    { RGB15_C(14,  0,  0), RGB15_C(28,  2,  2), RGB15_C(31, 18, 18) },
    /* 1: Crimson    — dim crimson / vivid crimson / white-pink flare */
    { RGB15_C(14,  0,  4), RGB15_C(28,  2,  8), RGB15_C(31, 18, 22) },
    /* 2: Orange     — dim orange / vivid orange / white-orange flare */
    { RGB15_C(16,  6,  0), RGB15_C(30, 14,  2), RGB15_C(31, 26, 16) },
    /* 3: Amber      — dim amber / vivid amber / white-amber flare */
    { RGB15_C(16, 10,  0), RGB15_C(30, 20,  0), RGB15_C(31, 29, 16) },
    /* 4: Gold       — dim gold / vivid gold / white-gold flare */
    { RGB15_C(16, 14,  0), RGB15_C(30, 26,  0), RGB15_C(31, 31, 18) },
    /* 5: Yellow     — dim yellow / vivid yellow / white-yellow flare */
    { RGB15_C(14, 14,  0), RGB15_C(28, 28,  2), RGB15_C(31, 31, 20) },
    /* 6: Lime       — dim lime / vivid lime / white-lime flare */
    { RGB15_C( 6, 16,  0), RGB15_C(12, 30,  2), RGB15_C(20, 31, 18) },
    /* 7: Green      — dim green / vivid green / white-green flare */
    { RGB15_C( 0, 14,  0), RGB15_C( 2, 28,  2), RGB15_C(16, 31, 16) },
    /* 8: Emerald    — dim emerald / vivid emerald / white-emerald flare */
    { RGB15_C( 0, 14,  6), RGB15_C( 2, 28, 12), RGB15_C(16, 31, 22) },
    /* 9: Teal       — dim teal / vivid teal / white-teal flare */
    { RGB15_C( 0, 12, 12), RGB15_C( 2, 24, 24), RGB15_C(16, 31, 31) },
    /* 10: Cyan      — dim cyan / vivid cyan / white-cyan flare */
    { RGB15_C( 0, 14, 16), RGB15_C( 2, 26, 30), RGB15_C(16, 31, 31) },
    /* 11: Sky       — dim sky / vivid sky / white-sky flare */
    { RGB15_C( 2, 10, 18), RGB15_C( 4, 20, 30), RGB15_C(18, 26, 31) },
    /* 12: Blue      — dim blue / vivid blue / white-blue flare */
    { RGB15_C( 0,  2, 16), RGB15_C( 2,  6, 30), RGB15_C(16, 20, 31) },
    /* 13: Indigo    — dim indigo / vivid indigo / white-indigo flare */
    { RGB15_C( 4,  0, 16), RGB15_C(10,  2, 30), RGB15_C(22, 16, 31) },
    /* 14: Violet    — dim violet / vivid violet / white-violet flare */
    { RGB15_C( 8,  0, 16), RGB15_C(16,  2, 30), RGB15_C(26, 16, 31) },
    /* 15: Purple    — dim purple / vivid purple / white-purple flare */
    { RGB15_C(10,  0, 12), RGB15_C(20,  2, 24), RGB15_C(29, 16, 31) },
    /* 16: Magenta   — dim magenta / vivid magenta / white-magenta flare */
    { RGB15_C(14,  0, 10), RGB15_C(28,  2, 20), RGB15_C(31, 18, 29) },
    /* 17: Pink      — dim pink / vivid pink / white-pink flare */
    { RGB15_C(16,  4, 10), RGB15_C(28,  8, 20), RGB15_C(31, 22, 29) },
    /* 18: White     — soft white / bright white / pure white */
    { RGB15_C(20, 20, 20), RGB15_C(27, 27, 27), RGB15_C(31, 31, 31) },
    /* 19: Silver    — dim blue-silver / bright blue-silver / white-silver */
    { RGB15_C(12, 12, 16), RGB15_C(22, 22, 28), RGB15_C(30, 30, 31) },
    /* 20: Gunmetal  — dim gunmetal glow / mid / bright steel */
    { RGB15_C( 8,  9, 11), RGB15_C(16, 18, 20), RGB15_C(24, 26, 28) },
    /* 21: Charcoal  — dark glow / dim gray / mid gray */
    { RGB15_C( 6,  6,  6), RGB15_C(12, 12, 12), RGB15_C(20, 20, 20) },
    /* 22: Brown     — dim amber-brown / warm glow / white-tan flare */
    { RGB15_C(14,  7,  1), RGB15_C(26, 14,  4), RGB15_C(31, 24, 16) },
    /* 23: Olive     — dim olive / vivid olive / white-olive flare */
    { RGB15_C( 9, 10,  0), RGB15_C(18, 20,  2), RGB15_C(28, 29, 16) },
};

void colors_apply_player_palette(int suit_idx, int visor_idx) {
    if (suit_idx < 0 || suit_idx >= COLOR_PRESET_COUNT) suit_idx = 0;
    if (visor_idx < 0 || visor_idx >= COLOR_PRESET_COUNT) visor_idx = 0;

    /* Write suit colors to palette bank 0, indices 2-4 */
    /* pal_obj_mem is u16* in libtonc — direct assignment is a 16-bit write */
    pal_obj_mem[0 * 16 + 2] = suit_presets[suit_idx][0];
    pal_obj_mem[0 * 16 + 3] = suit_presets[suit_idx][1];
    pal_obj_mem[0 * 16 + 4] = suit_presets[suit_idx][2];

    /* Write visor colors to palette bank 0, indices 5-7 */
    pal_obj_mem[0 * 16 + 5] = visor_presets[visor_idx][0];
    pal_obj_mem[0 * 16 + 6] = visor_presets[visor_idx][1];
    pal_obj_mem[0 * 16 + 7] = visor_presets[visor_idx][2];
}

u16 colors_get_suit_preview(int idx) {
    if (idx < 0 || idx >= COLOR_PRESET_COUNT) return 0;
    return suit_presets[idx][1];  /* Return middle shade */
}

u16 colors_get_visor_preview(int idx) {
    if (idx < 0 || idx >= COLOR_PRESET_COUNT) return 0;
    return visor_presets[idx][2];  /* Return brightest shade */
}
