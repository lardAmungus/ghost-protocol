#ifndef GAME_ENEMY_H
#define GAME_ENEMY_H

#include <tonc.h>
#include "engine/entity.h"

/*
 * Ghost Protocol — Enemy System
 *
 * 6 enemy types (ICE programs) that spawn in procedural levels.
 */

/* Enemy subtypes (stored in Entity.subtype) */
enum {
    ENEMY_SENTRY = 0,   /* Stationary turret */
    ENEMY_PATROL,        /* Walking + charge */
    ENEMY_FLYER,         /* Sine-wave flight */
    ENEMY_SHIELD,        /* Frontal block */
    ENEMY_SPIKE,         /* Extending hazard */
    ENEMY_HUNTER,        /* Aggressive chaser */
    ENEMY_DRONE,         /* Small swarm unit */
    ENEMY_TURRET,        /* Stationary aimed laser */
    ENEMY_MIMIC,         /* Disguised as item drop */
    ENEMY_CORRUPTOR,     /* Ranged corruption shots */
    ENEMY_GHOST,         /* Phases through walls */
    ENEMY_BOMBER,        /* Aerial bomb dropper */
    ENEMY_TYPE_COUNT     /* 12 */
};

/* Enemy states (stored in Entity.anim_frame high bits via enemy local state) */
enum {
    ESTATE_SPAWN = 0,  /* Materializing in — flicker for 20 frames */
    ESTATE_IDLE,
    ESTATE_PATROL,
    ESTATE_CHASE,
    ESTATE_ATTACK,
    ESTATE_HIT,
    ESTATE_DEAD,
};

/* Enemy info table entry */
typedef struct {
    s16 hp;
    s16 atk;
    u8  width, height;
    u8  xp_reward;
    u8  detection_range;  /* Pixels */
} EnemyInfo;

extern const EnemyInfo enemy_info[ENEMY_TYPE_COUNT];

/* Initialize enemy system. */
void enemy_init(void);

/* Spawn an enemy of given subtype at tile position. Returns entity or NULL. */
Entity* enemy_spawn(int subtype, int tile_x, int tile_y, int tier);

/* Update all enemies (AI, movement, attacks). */
void enemy_update_all(s32 player_x, s32 player_y);

/* Draw all enemies. */
void enemy_draw_all(s32 cam_x, s32 cam_y);

/* Check player attacks vs enemies. Returns total damage dealt. */
int enemy_check_player_attack(Entity* player);

/* Get number of living enemies. */
int enemy_count_alive(void);

/* Get total kills this level. */
int enemy_get_kills(void);

/* Deal damage to a specific enemy entity. */
void enemy_damage(Entity* e, int dmg);

/* Get an enemy's effective ATK (after tier/bounty scaling). */
int enemy_get_atk(Entity* e);

/* Scale an enemy's ATK by a fixed-point factor (256 = 1.0x). */
void enemy_scale_atk(Entity* e, int scale256);

/* Stun all alive enemies and deal damage to each. */
void enemy_stun_all(int damage);

/* Get/reset chase detection counter (for stealth achievements). */
int enemy_get_chase_count(void);
void enemy_reset_chase_count(void);

/* Set 60-frame grace period — enemies won't chase after area load. */
void enemy_reset_grace_timer(void);

#endif /* GAME_ENEMY_H */
