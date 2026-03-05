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
#define ABILITY_2  (1 << 1)  /* Unlocked at level 5 */
#define ABILITY_3  (1 << 2)  /* Unlocked at level 8 */
#define ABILITY_4  (1 << 3)  /* Unlocked at level 10 */
#define ABILITY_5  (1 << 4)  /* Unlocked at level 14 */
#define ABILITY_6  (1 << 5)  /* Unlocked at level 18 */
#define ABILITY_7  (1 << 6)  /* Unlocked at level 22 */
#define ABILITY_8  (1 << 7)  /* Unlocked at level 26 */

/* ---- Skill tree ---- */
#define SKILL_BRANCHES   3   /* Offense, Defense, Utility */
#define SKILLS_PER_BRANCH 4
#define SKILL_TREE_SIZE  12  /* 3 branches x 4 skills */
#define SKILL_MAX_RANK    3  /* Each skill has ranks 0-3 */
#define SKILL_POINTS_PER_2_LEVELS 1  /* 1 SP per 2 levels */

/* ---- XP curve ---- */
#define MAX_LEVEL 40
#define BASE_XP   90  /* Linear: BASE_XP * level */
#define EVOLUTION_LEVEL 20  /* Level at which class evolution is offered */

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
    u8  ability_unlocks;  /* Bitmask of ABILITY_1..8 */
    u8  jumps_remaining;  /* Multi-jump counter */
    u8  dash_timer;       /* Frames remaining in dash */
    u8  dash_cooldown;    /* Cooldown frames remaining */
    u8  shoot_cooldown;   /* Frames until next shot */
    u8  charge_timer;     /* Charged shot timer (Assault) */
    u8  invincible_timer; /* Iframes after hit */
    u8  state;            /* PSTATE_* */
    u16 credits;          /* Currency */
    u16 cooldown_ability[8]; /* Ability cooldown timers (up to 300), AB_SLOT_COUNT */
    u8  skill_tree[SKILL_TREE_SIZE]; /* Skill ranks (0-3 per skill) */
    u8  evolution;        /* EVOLUTION_NONE or EVOLUTION_* */
    u8  skill_points;     /* Unspent skill points */
    u8  evolution_pending; /* 1 if evolution choice available but not yet made */
    u16 craft_shards;     /* Crafting currency */
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

/* Get total skill points earned at given level */
int player_skill_points_earned(int level);

/* Try to allocate a skill point into skill_tree[index]. Returns 1 on success. */
int player_skill_allocate(int index);

/* Get passive bonus from skill tree for a given stat.
 * branch: 0=offense, 1=defense, 2=utility */
int player_get_skill_bonus(int branch, int skill_idx);

/* Apply evolution choice (1 or 2 for the class). Returns 1 on success. */
int player_apply_evolution(int choice);

/* Get evolution name string for the player's current evolution. */
const char* player_get_evolution_name(void);

#endif /* GAME_PLAYER_H */
