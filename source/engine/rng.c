#include "engine/rng.h"

static u32 rng_state = 1;  /* Must never be zero */

void rng_seed(void) {
    rng_state = REG_TM0CNT_L ^ ((u32)REG_TM1CNT_L << 16);
    if (rng_state == 0) rng_state = 1;
}

IWRAM_CODE u32 rand_next(void) {
    u32 x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

u32 rand_range(u32 max) {
    if (max == 0) return 0;
    /* Multiply-and-shift to avoid expensive software division */
    u32 r = rand_next();
    return (u32)(((u64)r * max) >> 32);
}
