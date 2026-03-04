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
#define NET_MAP_W   128   /* tiles wide */
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
    /* Boss subtypes */
    SUBTYPE_BOSS_FIREWALL,
    SUBTYPE_BOSS_BLACKOUT,
    SUBTYPE_BOSS_WORM,
    SUBTYPE_BOSS_NEXUS,
    SUBTYPE_BOSS_ROOT,
    /* Projectile subtypes */
    SUBTYPE_PROJ_BUSTER,
    SUBTYPE_PROJ_RAPID,
    SUBTYPE_PROJ_SPREAD,
    SUBTYPE_PROJ_CHARGE,
    SUBTYPE_PROJ_BEAM,
    SUBTYPE_PROJ_ENEMY,
};

/* ---- Loot rarity tiers ---- */
enum {
    RARITY_COMMON = 0,
    RARITY_UNCOMMON,
    RARITY_RARE,
    RARITY_EPIC,
    RARITY_LEGENDARY,
    RARITY_COUNT
};

/* ---- Direction helpers ---- */
#define FACING_RIGHT 0
#define FACING_LEFT  1

/* ---- Min/max helpers ---- */
#define GP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define GP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define GP_CLAMP(x, lo, hi) GP_MIN(GP_MAX((x), (lo)), (hi))

#endif /* GAME_COMMON_H */
