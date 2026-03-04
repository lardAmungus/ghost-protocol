/*
 * Ghost Protocol — Bug Bounty Mode (End Game)
 *
 * Post-story high-difficulty replayable content.
 * Each tier scales enemy HP/ATK, adds trace countdown, improves loot.
 */
#include "game/bugbounty.h"
#include "game/quest.h"
#include "game/common.h"
#include <string.h>

BugBountyState bb_state;

static const char* const tier_names[BB_TIER_COUNT] = {
    "Zero-Day",
    "Kernel Panic",
    "Buffer Overflow",
    "Stack Smash",
    "Ring Zero",
};

/* Stat multipliers (x256 fixed-point): 1.5x, 1.8x, 2.0x, 2.5x, 3.0x */
static const u16 stat_scales[BB_TIER_COUNT] = {
    384, 460, 512, 640, 768,
};

/* Trace timer in seconds per tier (0 = no timer for tier 6) */
static const u8 trace_seconds[BB_TIER_COUNT] = {
    0, 120, 90, 75, 60,
};

/* Minimum rarity floor per tier */
static const u8 rarity_floors[BB_TIER_COUNT] = {
    RARITY_UNCOMMON,  /* Zero-Day */
    RARITY_UNCOMMON,  /* Kernel Panic */
    RARITY_RARE,      /* Buffer Overflow */
    RARITY_RARE,      /* Stack Smash */
    RARITY_EPIC,      /* Ring Zero */
};

void bugbounty_init(void) {
    /* Zero only transient fields — persistent fields (high_scores,
       highest_unlocked, total_runs) survive across terminal visits */
    bb_state.active = 0;
    bb_state.tier = 0;
    bb_state.score = 0;
    bb_state.trace_timer = 0;
    bb_state.kills = 0;
    bb_state.rarity_floor = 0;
    bb_state.run_complete = 0;
}

void bugbounty_start(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) tier = 0;

    bb_state.active = 1;
    bb_state.tier = (u8)tier;
    bb_state.score = 0;
    bb_state.kills = 0;
    bb_state.rarity_floor = rarity_floors[tier];

    int seconds = trace_seconds[tier];
    bb_state.trace_timer = (u16)(seconds * 60); /* frames */
}

const char* bugbounty_tier_name(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) return "???";
    return tier_names[tier];
}

int bugbounty_stat_scale(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) return 256;
    return stat_scales[tier];
}

int bugbounty_trace_time(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) return 0;
    return trace_seconds[tier] * 60;
}

int bugbounty_update_trace(void) {
    if (!bb_state.active) return 0;
    if (bb_state.trace_timer == 0) return 0; /* No timer this tier */

    bb_state.trace_timer--;
    if (bb_state.trace_timer == 0) {
        /* Traced! Run over */
        return 1;
    }
    return 0;
}

void bugbounty_add_kill_score(int enemy_tier, int rarity) {
    if (!bb_state.active) return;

    /* Base score: 10 per kill, scaled by tier and rarity */
    int base = 10 + enemy_tier * 5;
    int rarity_bonus = rarity * 10;
    int total = base + rarity_bonus;

    /* Clamp to u16 — use u32 accumulator to avoid signed/unsigned ambiguity */
    u32 new_score = (u32)bb_state.score + (u32)total;
    if (new_score > 0xFFFFU) new_score = 0xFFFFU;
    bb_state.score = (u16)new_score;

    if (bb_state.kills < 255) bb_state.kills++;
}

void bugbounty_complete(void) {
    if (!bb_state.active) return;

    /* Completion bonus: 100 * (tier + 1) — u32 accumulator for safe clamp */
    u32 bonus = (u32)(100 * (bb_state.tier + 1));
    u32 new_score = (u32)bb_state.score + bonus;
    if (new_score > 0xFFFFU) new_score = 0xFFFFU;
    bb_state.score = (u16)new_score;

    /* Update per-tier high score */
    int t = bb_state.tier;
    if (t >= 0 && t < BB_TIER_COUNT) {
        if (bb_state.score > bb_state.high_scores[t]) {
            bb_state.high_scores[t] = bb_state.score;
        }
    }

    /* Unlock next tier */
    if (t >= bb_state.highest_unlocked && t + 1 < BB_TIER_COUNT) {
        bb_state.highest_unlocked = (u8)(t + 1);
    }

    /* Track total runs */
    if (bb_state.total_runs < 255) bb_state.total_runs++;

    bb_state.run_complete = 1;
    bb_state.active = 0;
}

int bugbounty_unlocked(void) {
    return quest_is_story_complete();
}

int bugbounty_tier_available(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) return 0;
    return tier <= bb_state.highest_unlocked;
}

u16 bugbounty_get_high_score(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) return 0;
    return bb_state.high_scores[tier];
}

int bugbounty_get_rarity_floor(int tier) {
    if (tier < 0 || tier >= BB_TIER_COUNT) return 0;
    return rarity_floors[tier];
}

void bugbounty_restore(const u16 hs[5], u8 unlocked, u8 runs) {
    for (int i = 0; i < BB_TIER_COUNT; i++) {
        bb_state.high_scores[i] = hs[i];
    }
    bb_state.highest_unlocked = unlocked;
    bb_state.total_runs = runs;
}

void bugbounty_pack(u16 hs_out[5], u8* unlocked_out, u8* runs_out) {
    for (int i = 0; i < BB_TIER_COUNT; i++) {
        hs_out[i] = bb_state.high_scores[i];
    }
    *unlocked_out = bb_state.highest_unlocked;
    *runs_out = bb_state.total_runs;
}
