#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include <tonc.h>

/* Ghost Protocol — Music track IDs */
enum {
    MUS_SILENCE    = 0,
    MUS_TITLE      = 1,
    MUS_TERMINAL   = 2,
    MUS_NET_EASY   = 3,
    MUS_NET_HARD   = 4,
    MUS_NET_FINAL  = 5,
    MUS_BOSS       = 6,
    MUS_VICTORY    = 7,
    MUS_GAMEOVER   = 8,
    MUS_COUNT
};

/* Ghost Protocol — Sound effect IDs */
enum {
    SFX_NONE         = 0,
    SFX_SHOOT        = 1,
    SFX_SHOOT_CHARGE = 2,
    SFX_SHOOT_RAPID  = 3,
    SFX_ENEMY_HIT    = 4,
    SFX_ENEMY_DIE    = 5,
    SFX_PLAYER_HIT   = 6,
    SFX_PLAYER_DIE   = 7,
    SFX_JUMP         = 8,
    SFX_WALL_JUMP    = 9,
    SFX_DASH         = 10,
    SFX_PICKUP       = 11,
    SFX_MENU_SELECT  = 12,
    SFX_MENU_BACK    = 13,
    SFX_ABILITY      = 14,
    SFX_BOSS_ROAR    = 15,
    SFX_LEVEL_DONE   = 16,
    SFX_SAVE         = 17,
    SFX_TRANSITION   = 18,
    SFX_COUNT
};

void audio_init(void);
void audio_update(void);
void audio_play_music(int module_id);
void audio_stop_music(void);
void audio_play_sfx(int sfx_id);
void audio_fade_music(int frames);
void audio_update_fade(void);

#endif /* ENGINE_AUDIO_H */
