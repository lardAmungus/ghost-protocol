#ifndef GAME_PROJECTILE_H
#define GAME_PROJECTILE_H

#include <tonc.h>
#include "game/common.h"

/* ---- Projectile flags ---- */
#define PROJ_PIERCE   (1 << 0)  /* Pass through enemies */
#define PROJ_PHASE    (1 << 1)  /* Pass through walls */
#define PROJ_ENEMY    (1 << 2)  /* Fired by enemy */
#define PROJ_ACTIVE   (1 << 7)

#define MAX_PROJECTILES 16

typedef struct {
    s32 x, y;          /* 8.8 fixed-point position */
    s16 vx, vy;        /* Velocity (8.8) */
    s16 damage;
    u8  type;          /* SUBTYPE_PROJ_* */
    u8  flags;
    u8  lifetime;      /* Frames remaining */
    u8  owner;         /* Entity pool index of owner */
    u8  oam_index;     /* OAM sprite index */
    u8  pad;
    u32 hit_mask;      /* Bitmask: bits 0-23 = enemy slots, bit 31 = boss (piercing dedup) */
} Projectile;

/* Initialize the projectile pool. */
void projectile_init(void);

/* Spawn a projectile. Returns pointer or NULL if pool full. */
Projectile* projectile_spawn(s32 x, s32 y, s16 vx, s16 vy,
                             s16 damage, u8 type, u8 flags, u8 owner);

/* Update all active projectiles (movement, collision, lifetime). */
void projectile_update_all(void);

/* Draw all active projectiles. */
void projectile_draw_all(s32 cam_x, s32 cam_y);

/* Deactivate a single projectile (hide OAM sprite + free slot + clear flags).
 * Use this instead of manually setting p->flags = 0. */
void projectile_deactivate(Projectile* p);

/* Clear all projectiles. */
void projectile_clear_all(void);

/* Get projectile array for collision checks. */
Projectile* projectile_get_pool(void);

/* Get number of active projectiles. */
int projectile_active_count(void);

#endif /* GAME_PROJECTILE_H */
