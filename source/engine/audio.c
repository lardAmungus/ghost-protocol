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
void audio_set_intensity(int level) { (void)level; }
#else

#include <maxmod.h>
#include "soundbank.h"
#include "soundbank_bin.h"

/*
 * Ghost Protocol — Maxmod audio implementation.
 *
 * Music files 00_title.mod through 15_evolution.mod: mmutil assigns
 * MOD_ constants 0-15 alphabetically. Maps to MUS_ enums (offset by 1).
 *
 * SFX files 01_shoot.wav through 30_achievement.wav: mmutil assigns
 * SFX_ constants 0-29 alphabetically. Maps to SFX_ enums (offset by 1).
 */

static const mm_word music_map[MUS_COUNT] = {
    [MUS_SILENCE]     = 0,
    [MUS_TITLE]       = MOD_00_TITLE,
    [MUS_TERMINAL]    = MOD_01_TERMINAL,
    [MUS_ACT1]        = MOD_02_ACT1,
    [MUS_ACT2]        = MOD_03_ACT2,
    [MUS_ACT3]        = MOD_04_ACT3,
    [MUS_ACT4]        = MOD_05_ACT4,
    [MUS_ACT5]        = MOD_06_ACT5,
    [MUS_ACT6]        = MOD_07_ACT6,
    [MUS_BOSS_CORP]   = MOD_08_BOSS_CORP,
    [MUS_BOSS_GATE]   = MOD_09_BOSS_GATE,
    [MUS_BOSS_DAEMON] = MOD_10_BOSS_DAEMON,
    [MUS_MINIBOSS]    = MOD_11_MINIBOSS,
    [MUS_VICTORY]     = MOD_12_VICTORY,
    [MUS_GAMEOVER]    = MOD_13_GAMEOVER,
    [MUS_CREDITS]     = MOD_14_CREDITS,
    [MUS_EVOLUTION]   = MOD_15_EVOLUTION,
};

static const mm_word sfx_map[SFX_COUNT] = {
    [SFX_NONE]           = 0,
    [SFX_SHOOT]          = SFX_01_SHOOT,
    [SFX_SHOOT_CHARGE]   = SFX_02_SHOOT_CHARGE,
    [SFX_SHOOT_RAPID]    = SFX_03_SHOOT_RAPID,
    [SFX_ENEMY_HIT]      = SFX_04_ENEMY_HIT,
    [SFX_ENEMY_DIE]      = SFX_05_ENEMY_DIE,
    [SFX_PLAYER_HIT]     = SFX_06_PLAYER_HIT,
    [SFX_PLAYER_DIE]     = SFX_07_PLAYER_DIE,
    [SFX_JUMP]           = SFX_08_JUMP,
    [SFX_WALL_JUMP]      = SFX_09_WALL_JUMP,
    [SFX_DASH]           = SFX_10_DASH,
    [SFX_PICKUP]         = SFX_11_PICKUP,
    [SFX_MENU_SELECT]    = SFX_12_MENU_SELECT,
    [SFX_MENU_BACK]      = SFX_13_MENU_BACK,
    [SFX_ABILITY]        = SFX_14_ABILITY,
    [SFX_BOSS_ROAR]      = SFX_15_BOSS_ROAR,
    [SFX_LEVEL_DONE]     = SFX_16_LEVEL_DONE,
    [SFX_SAVE]           = SFX_17_SAVE,
    [SFX_TRANSITION]     = SFX_18_TRANSITION,
    [SFX_WALL_SLIDE]     = SFX_19_WALL_SLIDE,
    [SFX_LAND_HEAVY]     = SFX_20_LAND_HEAVY,
    [SFX_DOUBLE_JUMP]    = SFX_21_DOUBLE_JUMP,
    [SFX_BEAM_HUM]       = SFX_22_BEAM_HUM,
    [SFX_SPREAD_BURST]   = SFX_23_SPREAD_BURST,
    [SFX_CHARGER_WHINE]  = SFX_24_CHARGER_WHINE,
    [SFX_BOSS_PHASE]     = SFX_25_BOSS_PHASE,
    [SFX_BOSS_EXPLODE]   = SFX_26_BOSS_EXPLODE,
    [SFX_TESLA_ZAP]      = SFX_27_TESLA_ZAP,
    [SFX_CRAFT_SUCCESS]  = SFX_28_CRAFT_SUCCESS,
    [SFX_EVOLVE]         = SFX_29_EVOLVE,
    [SFX_ACHIEVEMENT]    = SFX_30_ACHIEVEMENT,
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

void audio_set_intensity(int level) {
    /* Maxmod tempo: 1024 = normal speed */
    mm_word tempo;
    switch (level) {
    case 2:  tempo = 1200; break; /* +17% BPM (boss low HP / many enemies) */
    case 1:  tempo = 1100; break; /* +7% BPM (moderate threat) */
    default: tempo = 1024; break; /* Normal */
    }
    mmSetModuleTempo(tempo);
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
