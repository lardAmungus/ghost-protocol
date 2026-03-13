/*
 * Ghost Protocol — Particle Effects System
 *
 * Lightweight particle pool using spare OAM entries for visual juice:
 * impact sparks, death bursts, loot sparkles, ability flashes.
 * 16-particle pool, recycled oldest when full.
 */
#include "game/particle.h"
#include "game/common.h"
#include "engine/sprite.h"
#include "engine/rng.h"
#include <string.h>

typedef struct {
    s32 x, y;       /* 8.8 fixed-point world position */
    s16 vx, vy;     /* velocity */
    u8  type;       /* PART_* */
    u8  lifetime;   /* frames remaining */
    u8  oam_index;  /* OAM slot */
    u8  active;
} Particle;

static Particle pool[MAX_PARTICLES];
static int next_slot = 0;
static int gfx_loaded = 0;

#define PART_TILE_BASE 293  /* After drop tiles at 290-292 */
#define PART_PAL_BANK  14   /* Shares with player projectile palette (cyan) */

/* 3 particle tile variants (8x8 each) */
/* Spark: tiny bright cross */
static const u32 part_tile_spark[8] = {
    0x00000000, 0x00010000, 0x00171000, 0x01747100,
    0x00171000, 0x00010000, 0x00000000, 0x00000000,
};
/* Burst: small fragment/debris */
static const u32 part_tile_burst[8] = {
    0x00000000, 0x00000000, 0x00120000, 0x01241000,
    0x00121000, 0x00010000, 0x00000000, 0x00000000,
};
/* Star: loot sparkle diamond */
static const u32 part_tile_star[8] = {
    0x00000000, 0x00040000, 0x00474000, 0x04747400,
    0x00474000, 0x00040000, 0x00000000, 0x00000000,
};
/* Smoke: soft gray puff */
static const u32 part_tile_smoke[8] = {
    0x00000000, 0x00560000, 0x05665000, 0x56776500,
    0x56776500, 0x05665000, 0x00560000, 0x00000000,
};
/* Electric: cyan zigzag */
static const u32 part_tile_electric[8] = {
    0x00000000, 0x03000000, 0x00310000, 0x00031000,
    0x00310000, 0x03100000, 0x31000000, 0x00000000,
};
/* Heal: green cross glow */
static const u32 part_tile_heal[8] = {
    0x00000000, 0x00020000, 0x00242000, 0x02424200,
    0x00242000, 0x00020000, 0x00000000, 0x00000000,
};

static void load_gfx(void) {
    if (gfx_loaded) return;
    memcpy16(&tile_mem_obj[0][PART_TILE_BASE + 0], part_tile_spark, sizeof(part_tile_spark) / 2);
    memcpy16(&tile_mem_obj[0][PART_TILE_BASE + 1], part_tile_burst, sizeof(part_tile_burst) / 2);
    memcpy16(&tile_mem_obj[0][PART_TILE_BASE + 2], part_tile_star,  sizeof(part_tile_star) / 2);
    memcpy16(&tile_mem_obj[0][PART_TILE_BASE + 3], part_tile_smoke, sizeof(part_tile_smoke) / 2);
    memcpy16(&tile_mem_obj[0][PART_TILE_BASE + 4], part_tile_electric, sizeof(part_tile_electric) / 2);
    memcpy16(&tile_mem_obj[0][PART_TILE_BASE + 5], part_tile_heal,  sizeof(part_tile_heal) / 2);
    gfx_loaded = 1;
}

void particle_init(void) {
    memset(pool, 0, sizeof(pool));
    for (int i = 0; i < MAX_PARTICLES; i++) {
        pool[i].oam_index = 0xFF;
    }
    next_slot = 0;
    gfx_loaded = 0;
    load_gfx();
}

static void despawn_particle(Particle* p) {
    if (p->oam_index != 0xFF) {
        OBJ_ATTR* spr = sprite_get(p->oam_index);
        if (spr) obj_hide(spr);
        sprite_free(p->oam_index);
    }
    p->active = 0;
    p->oam_index = 0xFF;
}

void particle_spawn(s32 wx, s32 wy, s16 vx, s16 vy, int type, int lifetime) {
    load_gfx();

    /* Find a free slot, or recycle oldest (round-robin) */
    Particle* p = &pool[next_slot];
    if (p->active) {
        despawn_particle(p);
    }

    int oam = sprite_alloc();
    if (oam < 0) {
        next_slot = (next_slot + 1) % MAX_PARTICLES;
        return;
    }

    p->x = wx;
    p->y = wy;
    p->vx = vx;
    p->vy = vy;
    p->type = (u8)type;
    p->lifetime = (u8)(lifetime > 255 ? 255 : lifetime);
    p->oam_index = (u8)oam;
    p->active = 1;

    /* Configure OAM sprite */
    OBJ_ATTR* spr = sprite_get(oam);
    if (spr) {
        int tile_ofs = type;
        if (tile_ofs > 5) tile_ofs = 0;
        spr->attr0 = ATTR0_SQUARE | ATTR0_4BPP;
        spr->attr1 = ATTR1_SIZE_8;
        spr->attr2 = (u16)(ATTR2_ID(PART_TILE_BASE + tile_ofs) | ATTR2_PALBANK(PART_PAL_BANK));
    }

    next_slot = (next_slot + 1) % MAX_PARTICLES;
}

void particle_burst(s32 wx, s32 wy, int count, int type, int speed, int lifetime) {
    if (count > MAX_PARTICLES) count = MAX_PARTICLES;
    for (int i = 0; i < count; i++) {
        /* Spread particles in roughly even directions with some randomness */
        s16 vx = (s16)((int)rand_range((u32)(speed * 2 + 1)) - speed);
        s16 vy = (s16)((int)rand_range((u32)(speed * 2 + 1)) - speed);
        /* Ensure minimum velocity so particles actually move */
        if (vx == 0 && vy == 0) vx = (s16)speed;
        int lt = lifetime + (int)rand_range((u32)(lifetime / 2 + 1));
        particle_spawn(wx, wy, vx, vy, type, lt);
    }
}

void particle_update(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pool[i].active) continue;
        Particle* p = &pool[i];

        /* Move */
        p->x += p->vx;
        p->y += p->vy;

        /* Gravity (subtle downward pull for burst/smoke fragments) */
        if (p->type == PART_BURST || p->type == PART_SMOKE) {
            p->vy += 8; /* light gravity */
        }

        /* Heal particles float upward */
        if (p->type == PART_HEAL) {
            p->vy -= 4;
        }

        /* Electric particles jitter randomly */
        if (p->type == PART_ELECTRIC && (p->lifetime & 1)) {
            p->vx = (s16)((int)rand_range(65) - 32);
            p->vy = (s16)((int)rand_range(65) - 32);
        }

        /* Friction (slow down sparks/smoke) */
        if (p->type == PART_SPARK || p->type == PART_SMOKE) {
            if (p->vx > 0) p->vx--;
            else if (p->vx < 0) p->vx++;
            if (p->vy > 0) p->vy--;
            else if (p->vy < 0) p->vy++;
        }

        /* Lifetime */
        if (p->lifetime > 0) {
            p->lifetime--;
        }
        if (p->lifetime == 0) {
            despawn_particle(p);
        }
    }
}

void particle_draw(s32 cam_x, s32 cam_y) {
    int cx = (int)(cam_x >> 8);
    int cy = (int)(cam_y >> 8);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!pool[i].active) continue;
        Particle* p = &pool[i];

        int sx = (int)(p->x >> 8) - cx;
        int sy = (int)(p->y >> 8) - cy;

        OBJ_ATTR* spr = sprite_get(p->oam_index);
        if (!spr) continue;

        if (sx < -8 || sx > SCREEN_W || sy < -8 || sy > SCREEN_H) {
            obj_hide(spr);
        } else {
            /* GBA wraps negative coords via bitmask — no clamping needed */

            /* Blink in final frames for fade-out effect */
            if (p->lifetime < 6 && (p->lifetime & 1)) {
                obj_hide(spr);
                continue;
            }

            obj_unhide(spr, ATTR0_REG);
            spr->attr0 = (u16)((spr->attr0 & ~ATTR0_Y_MASK) | (sy & ATTR0_Y_MASK));
            spr->attr1 = (u16)((spr->attr1 & ~ATTR1_X_MASK) | (sx & ATTR1_X_MASK));
        }
    }
}

void particle_clear(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (pool[i].active) {
            despawn_particle(&pool[i]);
        }
    }
    next_slot = 0;
}
