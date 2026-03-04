#ifndef ENGINE_RNG_H
#define ENGINE_RNG_H

#include <tonc.h>

/* Seed RNG from hardware timers. Call on first user input. */
void rng_seed(void);

/* Get next pseudo-random 32-bit value. */
IWRAM_CODE u32 rand_next(void);

/* Get a random value in [0, max). */
u32 rand_range(u32 max);

#endif /* ENGINE_RNG_H */
