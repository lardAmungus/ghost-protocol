#ifndef GAME_COMMON_H
#define GAME_COMMON_H

#include <tonc.h>

/* ---- RGB15 compile-time constant (libtonc RGB15 is inline, not macro) ---- */
#define RGB15_C(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))

/* ---- Fixed-point helpers (8.8 format) ---- */
#define FP8(x)       ((s32)((x) * 256))
#define FP8_INT(fp)  ((fp) >> 8)
#define FP8_FRAC(fp) ((fp) & 0xFF)

/* ---- Gravity / physics constants ---- */
#define GRAVITY_ACCEL    FP8(0.15)   /* ~0.15 px/frame^2 */
#define MAX_FALL_SPEED   FP8(4.0)    /* Terminal velocity */

/* ---- Screen dimensions ---- */
#define SCREEN_W  240
#define SCREEN_H  160

/* ---- Map dimensions for Net levels ---- */
#define NET_MAP_W   256   /* tiles wide */
#define NET_MAP_H    32   /* tiles tall */
#define NET_MAP_PX  (NET_MAP_W * 8)
#define NET_MAP_PY  (NET_MAP_H * 8)

/* ---- Player classes ---- */
enum {
    CLASS_ASSAULT = 0,
    CLASS_INFILTRATOR,
    CLASS_TECHNOMANCER,
    CLASS_COUNT
};

/* ---- Entity subtypes (extend ENT_ENEMY) ---- */
enum {
    SUBTYPE_NONE = 0,
    /* Enemy subtypes */
    SUBTYPE_SENTRY,
    SUBTYPE_PATROL,
    SUBTYPE_FLYER,
    SUBTYPE_SHIELD,
    SUBTYPE_SPIKE,
    SUBTYPE_HUNTER,
    SUBTYPE_DRONE,
    SUBTYPE_TURRET,
    SUBTYPE_MIMIC,
    SUBTYPE_CORRUPTOR,
    SUBTYPE_GHOST,
    SUBTYPE_BOMBER,
    /* Boss subtypes */
    SUBTYPE_BOSS_FIREWALL,
    SUBTYPE_BOSS_BLACKOUT,
    SUBTYPE_BOSS_WORM,
    SUBTYPE_BOSS_NEXUS,
    SUBTYPE_BOSS_ROOT,
    SUBTYPE_BOSS_DAEMON,
    /* Projectile subtypes */
    SUBTYPE_PROJ_BUSTER,
    SUBTYPE_PROJ_RAPID,
    SUBTYPE_PROJ_SPREAD,
    SUBTYPE_PROJ_CHARGE,
    SUBTYPE_PROJ_BEAM,
    SUBTYPE_PROJ_ENEMY,
    SUBTYPE_PROJ_LASER,
    SUBTYPE_PROJ_HOMING,
    SUBTYPE_PROJ_NOVA,
};

/* ---- Class evolution types ---- */
enum {
    EVOLUTION_NONE = 0,
    /* Assault evolutions */
    EVOLUTION_VANGUARD = 1,   /* Tanky: HP+30%, DEF+20%, War Cry enhanced */
    EVOLUTION_COMMANDO = 2,   /* Damage: ATK+25%, crit+10%, Berserk enhanced */
    /* Infiltrator evolutions */
    EVOLUTION_PHANTOM = 3,    /* Stealth: invisibility on dash, Smoke enhanced */
    EVOLUTION_STRIKER = 4,    /* Speed: SPD+30%, double jump, Time Warp enhanced */
    /* Technomancer evolutions */
    EVOLUTION_ARCHITECT = 5,  /* Support: Nanobots enhanced, Firewall permanent */
    EVOLUTION_HACKER = 6,     /* Offense: Upload AoE, System Crash 2x */
    EVOLUTION_COUNT
};

/* ---- Loot rarity tiers ---- */
enum {
    RARITY_COMMON = 0,
    RARITY_UNCOMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY,
    RARITY_MYTHIC,
    RARITY_COUNT
};

/* ---- Game statistics (persisted in SaveData) ---- */
typedef struct {
    u32 play_time_frames;
    u16 total_kills;
    u16 total_deaths;
    u16 damage_dealt;
    u16 damage_taken;
    u16 items_found;
    u16 items_crafted;
    u8  highest_combo;
    u8  contracts_done;
    u8  ng_plus;             /* 0=normal, 1+=NG+ cycle */
    u8  endgame_unlocked;    /* 1=endless mode available */
    u16 bb_endgame_contracts;
    u8  bb_boss_contracts;
    u8  bb_threat_level;
    u8  bb_highest_level;
    u8  achievements[3];     /* 24 achievement bits */
    u8  codex_unlocks[32];   /* 256 codex entry bitfield */
    u8  choice_flags;        /* Story choice bits */
    /* Runtime-only (not saved) */
    u8  current_combo;       /* Kill streak this run */
} GameStats;

extern GameStats game_stats;

/* Achievement IDs (24 total) */
enum {
    ACH_FIRST_BLOOD = 0,     /* Kill 1 enemy */
    ACH_HUNTER,               /* Kill 100 enemies */
    ACH_EXTERMINATOR,          /* Kill 500 enemies */
    ACH_BOSS_SLAYER,           /* Defeat all 6 bosses */
    ACH_SPEED_RUN,             /* Clear mission in < 2 min */
    ACH_PACIFIST,              /* Clear RETRIEVAL with 0 kills */
    ACH_UNTOUCHABLE,           /* Clear mission without damage */
    ACH_LOOTER,                /* Find 50 items */
    ACH_LEGENDARY_FIND,        /* Find a Legendary item */
    ACH_FULL_SET,              /* Equip a complete set */
    ACH_CRAFTSMAN,             /* Craft 10 items */
    ACH_MASTER_CRAFTER,        /* Craft a Legendary via fusing */
    ACH_EVOLVED,               /* Reach class evolution */
    ACH_MAX_LEVEL,             /* Reach level 40 */
    ACH_COMPLETIONIST,         /* Complete all 30 story missions */
    ACH_BUG_HUNTER,            /* Complete all 5 bug bounty tiers */
    ACH_MILLIONAIRE,           /* Accumulate 10000 credits */
    ACH_SKILL_MASTER,          /* Fill one skill branch completely */
    ACH_NG_PLUS_CLEARED,       /* Complete NG+ */
    ACH_TRUE_GHOST,            /* Complete Act 5 undetected */
    ACH_ENDGAME,               /* Enter endless Bug Bounty mode */
    ACH_BOUNTY_HUNTER_25,      /* Complete 25 endgame contracts */
    ACH_MYTHIC_FIND,           /* Obtain a Mythic item */
    ACH_THREAT_LEVEL_5,        /* Reach Threat Level 5 */
    ACH_COUNT                  /* 24 */
};

/* Check/set achievement */
static inline int ach_unlocked(int id) {
    return (game_stats.achievements[id >> 3] >> (id & 7)) & 1;
}
static inline void ach_unlock(int id) {
    game_stats.achievements[id >> 3] |= (u8)(1 << (id & 7));
}

/* Unlock achievement with visual/audio celebration. Use this instead of ach_unlock when in gameplay. */
void ach_unlock_celebrate(int id);

/* Check/set codex entry */
static inline int codex_unlocked(int id) {
    return (game_stats.codex_unlocks[id >> 3] >> (id & 7)) & 1;
}
static inline void codex_unlock(int id) {
    game_stats.codex_unlocks[id >> 3] |= (u8)(1 << (id & 7));
}

/* Codex entry IDs: 0-11=enemy types, 12-17=bosses, 18-25=weapon types */
#define CODEX_ENEMY_BASE    0
#define CODEX_BOSS_BASE     12
#define CODEX_WEAPON_BASE   18

/* ---- Direction helpers ---- */
#define FACING_RIGHT 0
#define FACING_LEFT  1

/* ---- Min/max helpers ---- */
#define GP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define GP_CLAMP(x, lo, hi) GP_MIN(GP_MAX((x), (lo)), (hi))

#endif /* GAME_COMMON_H */
