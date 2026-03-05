#ifndef GAME_BOSS_H
#define GAME_BOSS_H

#include <tonc.h>
#include "engine/entity.h"

/*
 * Ghost Protocol — Boss System
 *
 * 6 story bosses, one per act. Only one loaded at a time.
 */

/* Boss types */
enum {
    BOSS_FIREWALL = 0,   /* Tier 1: Barrier with spread shots */
    BOSS_BLACKOUT,       /* Tier 2: Teleporter */
    BOSS_WORM,           /* Tier 3: Multi-segment */
    BOSS_NEXUS_CORE,     /* Tier 4: Spawns minions */
    BOSS_ROOT_ACCESS,    /* Tier 5: Multi-phase humanoid */
    BOSS_DAEMON,         /* Tier 6: AXIOM backup (copies boss patterns) */
    BOSS_TYPE_COUNT
};

/* Boss phases */
enum {
    BPHASE_IDLE = 0,
    BPHASE_ATTACK1,
    BPHASE_ATTACK2,
    BPHASE_VULNERABLE,
    BPHASE_TRANSITION,
    BPHASE_DEAD,
};

/* Boss state */
typedef struct {
    u8  type;        /* BOSS_* */
    u8  phase;       /* BPHASE_* */
    s16 hp;
    s16 max_hp;
    s16 atk;
    u16 phase_timer;
    u8  pattern_idx; /* Current attack pattern step */
    u8  hit_count;   /* Hits taken this vulnerability window */
    u8  defeated;
    u8  active;
    u8  stage;       /* 1=P1(100-60%), 2=P2(60-30%), 3=P3(30-0%) */
    u8  stage_announced; /* 1 if stage transition announced */
    u8  enrage_announced; /* 1 if enrage (<10% HP) announced */
} BossState;

extern BossState boss_state;

/* Initialize boss system (call once). */
void boss_init(void);

/* Spawn a boss of given type. */
void boss_spawn(int boss_type, int tier);

/* Update boss AI. */
void boss_update(s32 player_x, s32 player_y);

/* Draw boss sprite. */
void boss_draw(s32 cam_x, s32 cam_y);

/* Check player attacks vs boss. Returns damage dealt. */
int boss_check_player_attack(Entity* player);

/* Deal damage to boss. */
void boss_damage(int dmg);

/* Check if boss is alive and active. */
int boss_is_active(void);

/* Get the boss entity pointer (NULL if no active boss). */
Entity* boss_get_entity(void);

/* Get boss display name. */
const char* boss_get_name(int boss_type);

#endif /* GAME_BOSS_H */
