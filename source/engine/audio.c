#include "engine/audio.h"

#ifdef HEADLESS_TEST
/* No audio hardware in headless test — stub everything */
void audio_init(void) {}
void audio_update(void) {}
void audio_play_music(int module_id) { (void)module_id; }
void audio_stop_music(void) {}
void audio_play_sfx(int sfx_id) { (void)sfx_id; }
void audio_fade_music(int frames) { (void)frames; }
void audio_update_fade(void) {}
#else

#include <maxmod.h>
#include "soundbank.h"
#include "soundbank_bin.h"

/*
 * Ghost Protocol — Maxmod audio implementation.
 *
 * Music files 00_title.mod through 07_gameover.mod: mmutil assigns
 * MOD_ constants 0-7 alphabetically. Maps to MUS_ enums (offset by 1).
 *
 * SFX files 01_shoot.wav through 18_transition.wav: mmutil assigns
 * SFX_ constants 0-17 alphabetically. Maps to SFX_ enums (offset by 1).
 */

static const mm_word music_map[MUS_COUNT] = {
    [MUS_SILENCE]   = 0,
    [MUS_TITLE]     = MOD_00_TITLE,
    [MUS_TERMINAL]  = MOD_01_TERMINAL,
    [MUS_NET_EASY]  = MOD_02_NET_EASY,
    [MUS_NET_HARD]  = MOD_03_NET_HARD,
    [MUS_NET_FINAL] = MOD_04_NET_FINAL,
    [MUS_BOSS]      = MOD_05_BOSS,
    [MUS_VICTORY]   = MOD_06_VICTORY,
    [MUS_GAMEOVER]  = MOD_07_GAMEOVER,
};

static const mm_word sfx_map[SFX_COUNT] = {
    [SFX_NONE]         = 0,
    [SFX_SHOOT]        = SFX_01_SHOOT,
    [SFX_SHOOT_CHARGE] = SFX_02_SHOOT_CHARGE,
    [SFX_SHOOT_RAPID]  = SFX_03_SHOOT_RAPID,
    [SFX_ENEMY_HIT]    = SFX_04_ENEMY_HIT,
    [SFX_ENEMY_DIE]    = SFX_05_ENEMY_DIE,
    [SFX_PLAYER_HIT]   = SFX_06_PLAYER_HIT,
    [SFX_PLAYER_DIE]   = SFX_07_PLAYER_DIE,
    [SFX_JUMP]         = SFX_08_JUMP,
    [SFX_WALL_JUMP]    = SFX_09_WALL_JUMP,
    [SFX_DASH]         = SFX_10_DASH,
    [SFX_PICKUP]       = SFX_11_PICKUP,
    [SFX_MENU_SELECT]  = SFX_12_MENU_SELECT,
    [SFX_MENU_BACK]    = SFX_13_MENU_BACK,
    [SFX_ABILITY]      = SFX_14_ABILITY,
    [SFX_BOSS_ROAR]    = SFX_15_BOSS_ROAR,
    [SFX_LEVEL_DONE]   = SFX_16_LEVEL_DONE,
    [SFX_SAVE]         = SFX_17_SAVE,
    [SFX_TRANSITION]   = SFX_18_TRANSITION,
};

static int fade_total = 0;
static int fade_remaining = 0;

void audio_init(void) {
    mmInitDefault((mm_addr)soundbank_bin, 8);
    fade_total = 0;
    fade_remaining = 0;
}

void audio_update(void) {
    mmFrame();
}

void audio_play_music(int module_id) {
    fade_remaining = 0;
    fade_total = 0;
    mmSetModuleVolume(1024);

    if (module_id <= MUS_SILENCE || module_id >= MUS_COUNT) {
        mmStop();
        return;
    }
    mmStart((mm_word)music_map[module_id], MM_PLAY_LOOP);
}

void audio_stop_music(void) {
    mmStop();
}

void audio_play_sfx(int sfx_id) {
    if (sfx_id <= SFX_NONE || sfx_id >= SFX_COUNT) {
        return;
    }
    mm_sfxhand h;
    h = mmEffect((mm_word)sfx_map[sfx_id]);
    (void)h;
}

void audio_fade_music(int frames) {
    if (frames <= 0) {
        mmStop();
        return;
    }
    fade_total = frames;
    fade_remaining = frames;
}

void audio_update_fade(void) {
    if (fade_remaining <= 0) return;
    fade_remaining--;
    if (fade_remaining <= 0) {
        mmStop();
        mmSetModuleVolume(1024);
        fade_total = 0;
    } else if (fade_total <= 0) {
        mmStop();
        mmSetModuleVolume(1024);
        fade_remaining = 0;
        fade_total = 0;
    } else {
        int vol = (fade_remaining * 1024) / fade_total;
        mmSetModuleVolume((mm_word)vol);
    }
}

#endif /* HEADLESS_TEST */
