#ifndef GAME_BUGBOUNTY_H
#define GAME_BUGBOUNTY_H

#include <tonc.h>

/*
 * Ghost Protocol — Bug Bounty Mode (End Game)
 *
 * Post-story high-difficulty replayable content.
 * Tiers 6-10 with scaling enemy stats, trace timer, guaranteed drops, scoring.
 */

/* Bug bounty tiers */
enum {
    BB_ZERO_DAY = 0,       /* Tier 6: x1.5 enemy stats */
    BB_KERNEL_PANIC,       /* Tier 7: x1.8 stats, faster enemies */
    BB_BUFFER_OVERFLOW,    /* Tier 8: x2.0 stats, more spawns */
    BB_STACK_SMASH,        /* Tier 9: x2.5 stats, elite enemies */
    BB_RING_ZERO,          /* Tier 10: x3.0 stats, all hunters */
    BB_TIER_COUNT
};

#define BB_BASE_TIER 6     /* Bug bounty starts at tier 6 */

/* Bug bounty state */
typedef struct {
    u8  active;            /* 1 if in bug bounty mode */
    u8  tier;              /* BB_ZERO_DAY through BB_RING_ZERO */
    u16 score;             /* Current run score */
    u16 high_scores[BB_TIER_COUNT]; /* Per-tier best scores */
    u16 trace_timer;       /* Frames remaining before trace (0=no timer) */
    u8  kills;             /* Kills this run */
    u8  rarity_floor;      /* Minimum drop rarity for this tier */
    u8  highest_unlocked;  /* Highest tier available (0=only Zero-Day) */
    u8  total_runs;        /* Completed runs counter */
    u8  run_complete;      /* 1 if run just finished (for results screen) */
} BugBountyState;

extern BugBountyState bb_state;

/* Initialize bug bounty system. */
void bugbounty_init(void);

/* Start a bug bounty run at given tier. */
void bugbounty_start(int tier);

/* Get tier name string. */
const char* bugbounty_tier_name(int tier);

/* Get enemy stat multiplier (x256 fixed-point). */
int bugbounty_stat_scale(int tier);

/* Get trace timer duration in frames for tier (0 = no timer). */
int bugbounty_trace_time(int tier);

/* Update trace timer. Returns 1 if traced (time up). */
int bugbounty_update_trace(void);

/* Add score for a kill. */
void bugbounty_add_kill_score(int enemy_tier, int rarity);

/* Complete run, update high score. */
void bugbounty_complete(void);

/* Check if bug bounty is unlocked (story complete). */
int bugbounty_unlocked(void);

/* Check if a tier is available (unlocked). */
int bugbounty_tier_available(int tier);

/* Get high score for a specific tier. */
u16 bugbounty_get_high_score(int tier);

/* Get rarity floor for a tier. */
int bugbounty_get_rarity_floor(int tier);

/* Restore persistent state from save data. */
void bugbounty_restore(const u16 hs[5], u8 unlocked, u8 runs);

/* Pack persistent state for saving. */
void bugbounty_pack(u16 hs_out[5], u8* unlocked_out, u8* runs_out);

#endif /* GAME_BUGBOUNTY_H */
