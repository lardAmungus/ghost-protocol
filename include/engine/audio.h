#ifndef ENGINE_AUDIO_H
#define ENGINE_AUDIO_H

#include <tonc.h>

/* Ghost Protocol — Music track IDs (40 tracks) */
enum {
    MUS_SILENCE                    = 0,
    MUS_TITLE                      = 1,
    MUS_TERMINAL                   = 2,
    MUS_ACT1                       = 3,
    MUS_ACT2                       = 4,
    MUS_ACT3                       = 5,
    MUS_ACT4                       = 6,
    MUS_ACT5                       = 7,
    MUS_ACT6                       = 8,
    MUS_BOSS_CORP                  = 9,
    MUS_BOSS_GATE                  = 10,
    MUS_BOSS_DAEMON                = 11,
    MUS_MINIBOSS                   = 12,
    MUS_VICTORY                    = 13,
    MUS_GAMEOVER                   = 14,
    MUS_CREDITS                    = 15,
    MUS_EVOLUTION                  = 16,
    MUS_EX_AMBIENT_HUB             = 17,
    MUS_EX_AMBIENT_SHOP            = 18,
    MUS_EX_AMBIENT_CODEX           = 19,
    MUS_EX_TENSION_01              = 20,
    MUS_EX_TENSION_02              = 21,
    MUS_EX_TENSION_03              = 22,
    MUS_EX_TENSION_04              = 23,
    MUS_EX_BOSS_PHASE2             = 24,
    MUS_EX_BOSS_PHASE3             = 25,
    MUS_EX_CUTSCENE_INTRO          = 26,
    MUS_EX_CUTSCENE_MID            = 27,
    MUS_EX_CUTSCENE_END            = 28,
    MUS_EX_ENDGAME_01              = 29,
    MUS_EX_ENDGAME_02              = 30,
    MUS_EX_BUGBOUNTY_01            = 31,
    MUS_EX_BUGBOUNTY_02            = 32,
    MUS_EX_ACHIEVE_JINGLE          = 33,
    MUS_EX_LEVELUP_JINGLE          = 34,
    MUS_EX_RAREDROP_JINGLE         = 35,
    MUS_EX_CRAFTING                = 36,
    MUS_EX_STEALTH_01              = 37,
    MUS_EX_STEALTH_02              = 38,
    MUS_EX_CHASE_01                = 39,
    MUS_EX_CHASE_02                = 40,
    MUS_COUNT                     
};

#define MUS_NET_EASY  MUS_ACT1
#define MUS_NET_HARD  MUS_ACT3
#define MUS_NET_FINAL MUS_ACT5
#define MUS_BOSS      MUS_BOSS_CORP

/* Ghost Protocol — Sound effect IDs (380 effects) */
enum {
    SFX_NONE                       = 0,
    SFX_SHOOT                      = 1,
    SFX_SHOOT_CHARGE               = 2,
    SFX_SHOOT_RAPID                = 3,
    SFX_ENEMY_HIT                  = 4,
    SFX_ENEMY_DIE                  = 5,
    SFX_PLAYER_HIT                 = 6,
    SFX_PLAYER_DIE                 = 7,
    SFX_JUMP                       = 8,
    SFX_WALL_JUMP                  = 9,
    SFX_DASH                       = 10,
    SFX_PICKUP                     = 11,
    SFX_MENU_SELECT                = 12,
    SFX_MENU_BACK                  = 13,
    SFX_ABILITY                    = 14,
    SFX_BOSS_ROAR                  = 15,
    SFX_LEVEL_DONE                 = 16,
    SFX_SAVE                       = 17,
    SFX_TRANSITION                 = 18,
    SFX_WALL_SLIDE                 = 19,
    SFX_LAND_HEAVY                 = 20,
    SFX_DOUBLE_JUMP                = 21,
    SFX_BEAM_HUM                   = 22,
    SFX_SPREAD_BURST               = 23,
    SFX_CHARGER_WHINE              = 24,
    SFX_BOSS_PHASE                 = 25,
    SFX_BOSS_EXPLODE               = 26,
    SFX_TESLA_ZAP                  = 27,
    SFX_CRAFT_SUCCESS              = 28,
    SFX_EVOLVE                     = 29,
    SFX_ACHIEVEMENT                = 30,
    SFX_BEAM_FIRE                  = 31,
    SFX_LASER_CRACK                = 32,
    SFX_NOVA_WHOOSH                = 33,
    SFX_HOMING_WHINE               = 34,
    SFX_ENEMY_HIT_MECH             = 35,
    SFX_ENEMY_DIE_MECH             = 36,
    SFX_ENEMY_HIT_DIGI             = 37,
    SFX_ENEMY_DIE_DIGI             = 38,
    SFX_COUNT                      = 381
};

/* SFX volume categories (0-255 scale for mmEffectEx) */
#define VOL_SFX_LOUD     150   /* weapons, explosions */
#define VOL_SFX_NORMAL   200   /* combat, movement */
#define VOL_SFX_QUIET    230   /* UI, ambient */
#define VOL_SFX_SUBTLE   100   /* footsteps, particles */

void audio_init(void);
void audio_update(void);
void audio_play_music(int module_id);
void audio_stop_music(void);
void audio_play_sfx(int sfx_id);
void audio_play_sfx_vol(u16 sfx_id, u8 volume);
void audio_fade_music(int frames);
void audio_update_fade(void);
/* Set combat intensity: 0=normal, 1=medium, 2=high — adjusts tempo */
void audio_set_intensity(int level);

#endif /* ENGINE_AUDIO_H */
