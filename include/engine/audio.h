#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include <tonc.h>

/* Ghost Protocol — Music track IDs (16 tracks) */
enum {
    MUS_SILENCE     = 0,
    MUS_TITLE       = 1,
    MUS_TERMINAL    = 2,
    MUS_ACT1        = 3,  /* Act 1: The Glitch */
    MUS_ACT2        = 4,  /* Act 2: Traceback */
    MUS_ACT3        = 5,  /* Act 3: Deep Packet */
    MUS_ACT4        = 6,  /* Act 4: Zero Day */
    MUS_ACT5        = 7,  /* Act 5: Ghost Protocol */
    MUS_ACT6        = 8,  /* Act 6: Trace Route */
    MUS_BOSS_CORP   = 9,  /* Corp bosses (Acts 1-3) */
    MUS_BOSS_GATE   = 10, /* Gate bosses (Acts 4-5) */
    MUS_BOSS_DAEMON = 11, /* DAEMON boss (Act 6) */
    MUS_MINIBOSS    = 12, /* Mini-boss encounters */
    MUS_VICTORY     = 13,
    MUS_GAMEOVER    = 14,
    MUS_CREDITS     = 15,
    MUS_EVOLUTION   = 16,
    MUS_COUNT
};

/* Backwards compat aliases for old music names */
#define MUS_NET_EASY  MUS_ACT1
#define MUS_NET_HARD  MUS_ACT3
#define MUS_NET_FINAL MUS_ACT5
#define MUS_BOSS      MUS_BOSS_CORP

/* Ghost Protocol — Sound effect IDs (30 effects) */
enum {
    SFX_NONE           = 0,
    SFX_SHOOT          = 1,
    SFX_SHOOT_CHARGE   = 2,
    SFX_SHOOT_RAPID    = 3,
    SFX_ENEMY_HIT      = 4,
    SFX_ENEMY_DIE      = 5,
    SFX_PLAYER_HIT     = 6,
    SFX_PLAYER_DIE     = 7,
    SFX_JUMP           = 8,
    SFX_WALL_JUMP      = 9,
    SFX_DASH           = 10,
    SFX_PICKUP         = 11,
    SFX_MENU_SELECT    = 12,
    SFX_MENU_BACK      = 13,
    SFX_ABILITY        = 14,
    SFX_BOSS_ROAR      = 15,
    SFX_LEVEL_DONE     = 16,
    SFX_SAVE           = 17,
    SFX_TRANSITION     = 18,
    /* New SFX (Phase 4) */
    SFX_WALL_SLIDE     = 19,
    SFX_LAND_HEAVY     = 20,
    SFX_DOUBLE_JUMP    = 21,
    SFX_BEAM_HUM       = 22,
    SFX_SPREAD_BURST   = 23,
    SFX_CHARGER_WHINE  = 24,
    SFX_BOSS_PHASE     = 25,
    SFX_BOSS_EXPLODE   = 26,
    SFX_TESLA_ZAP      = 27,
    SFX_CRAFT_SUCCESS  = 28,
    SFX_EVOLVE         = 29,
    SFX_ACHIEVEMENT    = 30,
    SFX_COUNT
};

void audio_init(void);
void audio_update(void);
void audio_play_music(int module_id);
void audio_stop_music(void);
void audio_play_sfx(int sfx_id);
void audio_fade_music(int frames);
void audio_update_fade(void);
/* Set combat intensity: 0=normal, 1=medium, 2=high — adjusts tempo */
void audio_set_intensity(int level);

#endif /* ENGINE_AUDIO_H */
