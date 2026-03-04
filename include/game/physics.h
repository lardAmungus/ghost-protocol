#ifndef GAME_PHYSICS_H
#define GAME_PHYSICS_H

#include <tonc.h>
#include "engine/entity.h"
#include "game/common.h"

/*
 * Ghost Protocol — Platformer Physics
 *
 * Per-class physics tuning. Gravity, jumping, wall-jumping, dashing.
 * Uses 8.8 fixed-point throughout.
 */

typedef struct {
    s16 gravity;         /* Downward accel per frame (8.8) */
    s16 max_fall;        /* Terminal velocity (8.8) */
    s16 jump_vel;        /* Initial jump velocity (negative = up, 8.8) */
    s16 jump_cut;        /* Velocity set on jump release (8.8) */
    s16 wall_jump_vx;    /* Horizontal kick from wall jump (8.8) */
    s16 wall_jump_vy;    /* Vertical kick from wall jump (8.8) */
    s16 wall_slide_speed;/* Max fall speed when sliding wall (8.8) */
    s16 move_speed;      /* Max horizontal speed (8.8) */
    s16 accel;           /* Ground acceleration (8.8) */
    s16 decel;           /* Ground deceleration (8.8) */
    s16 air_accel;       /* Air acceleration (8.8) */
    s16 dash_speed;      /* Dash horizontal speed (8.8) */
    u8  dash_frames;     /* Dash duration in frames */
    u8  dash_cooldown;   /* Frames between dashes */
    u8  max_jumps;       /* 1 = single, 2 = double jump */
    u8  can_wall_jump;   /* 1 if wall-jump enabled */
    u8  can_dash;        /* 1 if air dash enabled */
    u8  pad;
} PhysicsParams;

/* Pre-defined physics params per class */
extern const PhysicsParams physics_class[CLASS_COUNT];

/* Apply gravity and physics to an entity.
 * Updates vy (gravity), resolves X then Y collision,
 * sets on_ground/on_wall flags. */
void physics_update(Entity* e, const PhysicsParams* params);

/* Check if entity is touching wall on the given side.
 * side: -1 = left, +1 = right. Returns 1 if wall detected. */
int physics_check_wall(const Entity* e, int side);

/* Resolve X-axis collision. Moves entity out of solid tiles. */
void physics_resolve_x(Entity* e);

/* Resolve Y-axis collision. Moves entity out of solid tiles.
 * Handles one-way platforms (TILE_PLATFORM). */
void physics_resolve_y(Entity* e);

#endif /* GAME_PHYSICS_H */
