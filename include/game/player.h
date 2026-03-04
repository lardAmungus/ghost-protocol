#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <tonc.h>
#include "engine/entity.h"
#include "game/common.h"
#include "game/physics.h"

/* ---- Player state machine ---- */
enum {
    PSTATE_IDLE = 0,
    PSTATE_RUN,
    PSTATE_JUMP,
    PSTATE_FALL,
    PSTATE_WALL_SLIDE,
    PSTATE_DASH,
    PSTATE_SHOOT,
    PSTATE_HIT,
    PSTATE_DEAD,
};

/* ---- Ability bitmask flags ---- */
#define ABILITY_1  (1 << 0)  /* Unlocked at level 3 */
#define ABILITY_2  (1 << 1)  /* Unlocked at level 7 */
#define ABILITY_3  (1 << 2)  /* Unlocked at level 12 */
#define ABILITY_4  (1 << 3)  /* Unlocked at level 17 */

/* ---- XP curve ---- */
#define MAX_LEVEL 30
#define BASE_XP   90  /* Linear: BASE_XP * level */

/* ---- Player stats (persistent across levels) ---- */
typedef struct {
    u8  player_class;     /* CLASS_ASSAULT / INFILTRATOR / TECHNOMANCER */
    u8  level;            /* 1-20 */
    u16 xp;              /* Current XP */
    s16 hp;
    s16 max_hp;
    s16 atk;
    s16 def;
    s16 spd;
    s16 lck;
    u8  ability_unlocks;  /* Bitmask of ABILITY_1..4 */
    u8  jumps_remaining;  /* Multi-jump counter */
    u8  dash_timer;       /* Frames remaining in dash */
    u8  dash_cooldown;    /* Cooldown frames remaining */
    u8  shoot_cooldown;   /* Frames until next shot */
    u8  charge_timer;     /* Charged shot timer (Assault) */
    u8  invincible_timer; /* Iframes after hit */
    u8  state;            /* PSTATE_* */
    u16 credits;          /* Currency */
    u16 cooldown_ability[4]; /* Ability cooldown timers (up to 300) */
} PlayerState;

extern PlayerState player_state;

/* Initialize player entity + state for given class. */
void player_init(int player_class);

/* Process input and update player. Call once per frame. */
void player_update(void);

/* Render player sprite. */
void player_draw(s32 cam_x, s32 cam_y);

/* Get the player entity pointer. */
Entity* player_get(void);

/* Deal damage to the player. */
void player_take_damage(int dmg, s16 kb_vx, s16 kb_vy);

/* Add XP and handle level-ups. */
void player_add_xp(int amount);

/* Get XP needed for next level. */
int player_xp_to_next(void);

/* Check if player is alive. */
int player_is_alive(void);

/* Get base stat values per class per level */
void player_get_base_stats(int cls, int lvl, s16* hp, s16* atk, s16* def, s16* spd, s16* lck);

#endif /* GAME_PLAYER_H */
