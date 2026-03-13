#ifndef GAME_PARTICLE_H
#define GAME_PARTICLE_H

#include <tonc.h>

#define MAX_PARTICLES 32

/* Particle types */
#define PART_SPARK    0  /* Small impact spark */
#define PART_BURST    1  /* Death burst fragment */
#define PART_STAR     2  /* Loot sparkle / ability flash */
#define PART_SMOKE    3  /* Gray explosion residue */
#define PART_ELECTRIC 4  /* Cyan flicker (Tesla/EMP) */
#define PART_HEAL     5  /* Green glow (HP recovery) */

/* Initialize particle pool. */
void particle_init(void);

/* Spawn a single particle at world position with velocity and lifetime. */
void particle_spawn(s32 wx, s32 wy, s16 vx, s16 vy, int type, int lifetime);

/* Spawn a burst of N particles radiating outward from a point. */
void particle_burst(s32 wx, s32 wy, int count, int type, int speed, int lifetime);

/* Update all active particles (movement, gravity, lifetime). */
void particle_update(void);

/* Draw all active particles relative to camera. */
void particle_draw(s32 cam_x, s32 cam_y);

/* Clear all particles. */
void particle_clear(void);

#endif /* GAME_PARTICLE_H */
