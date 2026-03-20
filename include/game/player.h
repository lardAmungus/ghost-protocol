#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <tonc.h>
#include "engine/entity.h"
#include "game/common.h"
#include "game/skills.h"
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
    PSTATE_CHARGE,       /* Charge rush (Trojan tier skill) */
    PSTATE_TETHER,       /* Tether (Technomancer tier skill) */
    PSTATE_TETHER_HOLD,  /* Tether (Technomancer tier skill) */
};

/* ---- XP curve ---- */
/* MAX_LEVEL_BASE and MAX_LEVEL_ENDGAME defined in common.h */
#define BASE_XP   90  /* Linear: BASE_XP * level */

/* ---- Player stats ----
 *
 * LAYOUT: The struct is divided into two sections separated by _level_scope_start.
 *   - PERSISTENT fields (above the marker): never zeroed after character creation.
 *     These include class, level, XP, stats, inventory, equipment, quest, achievements, etc.
 *   - LEVEL-SCOPE fields (below the marker): zeroed on every level entry via player_init_level().
 *     These include position, velocity, movement state, timers, cooldowns, etc.
 *
 * This prevents the recurring bug where player_init() zeroes the entire struct and
 * state_net_enter() must manually save/restore a growing list of persistent fields.
 *
 * IMPORTANT: When adding new persistent fields (inventory, quest, etc.), add them
 * ABOVE _level_scope_start. When adding new per-level fields (timers, state), add
 * them BELOW _level_scope_start.
 */
typedef struct {
    /* === PERSISTENT (never zeroed after character creation) === */
    u8  player_class;     /* CLASS_TROJAN / INFILTRATOR / TECHNOMANCER (0xFF = classless) */
    u8  tier_choices[3];  /* [0]=T1 class, [1]=T2 SPEC_*, [2]=T3 SPEC3_* (0xFF=unchosen) */
    u16 level;            /* 1-40+ (widened from u8 for NG+ levels) */
    u32 xp;              /* Current XP (widened from u16 for endgame) */
    /* INTEGRATION NOTE: xp widened to u32 — other files assigning to u16 SaveData fields
     * need explicit (u16) casts or widened SaveData fields. */
    u32 credits;          /* Currency (widened from u16 for endgame economy) */
    /* INTEGRATION NOTE: credits widened to u32 — same as xp above. */
    s16 max_hp;
    s16 atk;
    s16 def;
    s16 spd;
    s16 lck;
    u8  skill_ranks[MAX_PLAYER_SKILLS]; /* Rank 0-10 per unlocked skill */
    u8  skill_points;     /* Unspent skill points (1/level, banked pre-T1) */
    u8  slotted_skill;    /* Index into unlocked skills array (which skill R fires, 0xFF=none) */
    u8  suit_color;       /* 0-23 palette preset */
    u8  visor_color;      /* 0-23 palette preset */
    u8  save_slot;        /* Active save slot */
    u16 craft_shards;     /* Crafting currency */
    u8  armor_flags;       /* Active armor passive bitflags */
    u8  armor_regen_timer; /* Frames until next regen tick */
    /* === LEVEL-SCOPE (zeroed on every level entry via player_init_level) === */
    u8  _level_scope_start; /* Marker: everything from here down gets zeroed */
    s32 last_safe_x;       /* Last safe ground position (8.8 FP) — level-scope */
    s32 last_safe_y;       /* Last safe ground position (8.8 FP) — level-scope */
    s32 x, y;              /* Position (used by entity, mirrored here for scope) */
    s16 vx, vy;
    u8  facing;
    u8  on_ground, on_wall, wall_dir;
    u8  on_platform;
    u8  state;             /* PSTATE_* */
    u8  jumps_remaining;   /* Multi-jump counter */
    u8  coyote_timer;
    u8  jump_buffer;       /* Frames remaining in jump buffer window */
    s16 hp;
    u8  shoot_cooldown;    /* Frames until next shot */
    u8  charge_timer;      /* Charged shot timer (Assault) */
    u8  invincible_timer;  /* Iframes after hit */
    /* Skill system */
    u16 skill_cooldown;       /* Cooldown frames remaining for slotted skill */
    u8  skill_wheel_open;     /* 1=skill wheel overlay active */
    u8  skill_wheel_cursor;   /* Cursor position in wheel */
    u16 overclock_timer;
    u16 iron_skin_timer;
    u16 berserk_timer;
    u16 smoke_timer;
    u16 backstab_timer;
    u16 time_warp_timer;
    u16 data_shield_timer;
    u16 nanobots_timer;
    u16 firewall_timer;
    u16 overclock_plus_timer;
    u16 upload_timer;
    u8  regen_counter;
    /* Assault: Charge Rush */
    u8  charge_rush_timer;
    u8  charge_rush_cooldown;
    s16 charge_vx, charge_vy;
    /* Technomancer: Tether Dash */
    u8  tether_timer;
    u8  tether_cooldown;
    u8  tether_hold;
    u8  tether_hold_timer;
    s32 tether_target_x, tether_target_y;
    /* Infiltrator: Dash (moved from old locations) */
    u8  dash_timer;        /* Frames remaining in dash */
    u8  dash_cooldown;     /* Cooldown frames remaining */
} PlayerState;

#define AFLAG_HAZARD_RESIST  (1 << 0)
#define AFLAG_HP_REGEN       (1 << 1)
#define AFLAG_DAMAGE_RESIST  (1 << 2)
#define AFLAG_DODGE_CHANCE   (1 << 3)
#define AFLAG_REFLECT        (1 << 4)
#define AFLAG_KB_RESIST      (1 << 5)

extern PlayerState player_state;

/* Initialize player for a NEW character (full zeroed state). Only call for character creation. */
void player_init(int player_class);

/* Zero level-scope fields only. Does NOT create entity or sprite. */
void player_init_level(void);

/* Full level entry: creates entity, sprite, loads graphics, resets level-scope fields.
 * Call from state_net_enter() instead of player_init() for existing characters. */
void player_enter_level(void);

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

/* Recompute all stats from base + equipment + skills. Call after equip changes. */
void player_recompute_stats(void);

/* Check if player has chosen T1 class yet. */
int player_has_class(void);

/* Get number of unlocked skills for current build path. */
int player_get_skill_count(void);

/* Get the skill ID at position idx in the player's unlocked skill list. */
int player_get_skill_id(int idx);

/* Get the player's current tier name string. */
const char* player_get_tier_name(void);

/* INTEGRATION NOTE: Stream 2 (enemy.c) should call this during Charge Rush (PSTATE_CHARGE)
 * to apply contact damage to enemies hit by the charge. Returns damage dealt. */
int player_charge_rush_damage(void);

#endif /* GAME_PLAYER_H */
