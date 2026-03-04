/*
 * Ghost Protocol — Projectile System
 *
 * 16-slot static pool shared between player and enemy projectiles.
 * Each projectile has position, velocity, damage, type, lifetime, flags.
 */
#include "game/projectile.h"
#include "game/common.h"
#include "engine/sprite.h"
#include "engine/collision.h"
#include <string.h>

static Projectile pool[MAX_PROJECTILES];

/* ---- Projectile graphics: 4 tile variants ---- */
/* Tile 0: Buster (medium energy bolt) */
static const u32 proj_tile_buster[8] = {
    0x00000000, 0x00011000, 0x00122100, 0x01233210,
    0x01233210, 0x00122100, 0x00011000, 0x00000000,
};
/* Tile 1: Rapid (small needle) */
static const u32 proj_tile_rapid[8] = {
    0x00000000, 0x00000000, 0x00011000, 0x00123100,
    0x00123100, 0x00011000, 0x00000000, 0x00000000,
};
/* Tile 2: Charge (large sphere with glow) */
static const u32 proj_tile_charge[8] = {
    0x00111000, 0x01232100, 0x12343210, 0x12344310,
    0x12344310, 0x12343210, 0x01232100, 0x00111000,
};
/* Tile 3: Enemy (angular red shard) */
static const u32 proj_tile_enemy[8] = {
    0x00010000, 0x00121000, 0x01232100, 0x00232100,
    0x00123200, 0x00123210, 0x00012100, 0x00001000,
};

/* Palette: full 16-color player (cyan ramp) */
static const u16 proj_pal_player[16] = {
    0x0000,
    RGB15_C(0,28,31),  /* 1: bright cyan */
    RGB15_C(0,20,24),  /* 2: mid cyan */
    RGB15_C(0,12,16),  /* 3: dark cyan */
    RGB15_C(8,31,31),  /* 4: white-cyan glow */
    RGB15_C(0,16,20),  /* 5: dim */
    RGB15_C(4,24,28),  /* 6: mid-bright */
    RGB15_C(16,31,31), /* 7: core white */
    0, 0, 0, 0, 0, 0, 0, 0,
};
/* Palette: full 16-color enemy (red-orange ramp) */
static const u16 proj_pal_enemy[16] = {
    0x0000,
    RGB15_C(31,8,0),   /* 1: bright red-orange */
    RGB15_C(24,4,0),   /* 2: mid red */
    RGB15_C(16,2,0),   /* 3: dark red */
    RGB15_C(31,16,4),  /* 4: orange glow */
    RGB15_C(20,4,0),   /* 5: dim */
    RGB15_C(28,10,2),  /* 6: mid-bright */
    RGB15_C(31,24,12), /* 7: core yellow */
    0, 0, 0, 0, 0, 0, 0, 0,
};

static int gfx_loaded = 0;
#define PROJ_TILE_BASE 176  /* OBJ tile index for projectile (after boss 144+32) */
#define PROJ_PAL_PLAYER 14
#define PROJ_PAL_ENEMY  15

static void load_gfx(void) {
    if (gfx_loaded) return;
    /* Load 4 projectile tile variants to OBJ VRAM (tiles 176-179) */
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 0], proj_tile_buster, sizeof(proj_tile_buster) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 1], proj_tile_rapid, sizeof(proj_tile_rapid) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 2], proj_tile_charge, sizeof(proj_tile_charge) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 3], proj_tile_enemy, sizeof(proj_tile_enemy) / 2);
    /* Load palettes */
    memcpy16(&pal_obj_mem[PROJ_PAL_PLAYER * 16], proj_pal_player, 16);
    memcpy16(&pal_obj_mem[PROJ_PAL_ENEMY * 16], proj_pal_enemy, 16);
    gfx_loaded = 1;
}

void projectile_init(void) {
    memset(pool, 0, sizeof(pool));
    /* Set all oam_index to OAM_NONE so despawn doesn't corrupt OAM slot 0 */
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        pool[i].oam_index = 0xFF;
    }
    gfx_loaded = 0;
    load_gfx();
}

int projectile_active_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (pool[i].flags & PROJ_ACTIVE) count++;
    }
    return count;
}

Projectile* projectile_spawn(s32 x, s32 y, s16 vx, s16 vy,
                             s16 damage, u8 type, u8 flags, u8 owner) {
    load_gfx();
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!(pool[i].flags & PROJ_ACTIVE)) {
            int oam = sprite_alloc();
            if (oam < 0) return NULL;

            pool[i].x = x;
            pool[i].y = y;
            pool[i].vx = vx;
            pool[i].vy = vy;
            pool[i].damage = damage;
            pool[i].type = type;
            pool[i].flags = (u8)(flags | PROJ_ACTIVE);
            pool[i].lifetime = 150; /* 2.5 seconds default */
            pool[i].owner = owner;
            pool[i].oam_index = (u8)oam;

            /* Configure OAM sprite — select tile variant by subtype */
            OBJ_ATTR* spr = sprite_get(oam);
            if (spr) {
                int pal = (flags & PROJ_ENEMY) ? PROJ_PAL_ENEMY : PROJ_PAL_PLAYER;
                int tile_ofs = 0; /* default: buster */
                if (flags & PROJ_ENEMY) {
                    tile_ofs = 3; /* enemy shard */
                } else if (type == SUBTYPE_PROJ_RAPID) {
                    tile_ofs = 1; /* needle */
                } else if (type == SUBTYPE_PROJ_CHARGE || type == SUBTYPE_PROJ_BEAM) {
                    tile_ofs = 2; /* large sphere */
                }
                spr->attr0 = ATTR0_SQUARE | ATTR0_4BPP;
                spr->attr1 = ATTR1_SIZE_8;
                spr->attr2 = (u16)(ATTR2_ID(PROJ_TILE_BASE + tile_ofs) | ATTR2_PALBANK(pal));
            }

            return &pool[i];
        }
    }
    return NULL;
}

static void despawn(Projectile* p) {
    if (p->oam_index != 0xFF) {
        OBJ_ATTR* spr = sprite_get(p->oam_index);
        if (spr) obj_hide(spr);
        sprite_free(p->oam_index);
    }
    p->flags = 0;
    p->lifetime = 0;
    p->oam_index = 0xFF;
}

void projectile_deactivate(Projectile* p) {
    despawn(p);
}

void projectile_update_all(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!(pool[i].flags & PROJ_ACTIVE)) continue;
        Projectile* p = &pool[i];

        /* Move */
        p->x += p->vx;
        p->y += p->vy;

        /* Lifetime */
        if (p->lifetime > 0) {
            p->lifetime--;
        }
        if (p->lifetime == 0) {
            despawn(p);
            continue;
        }

        /* Out of bounds check */
        int px = p->x >> 8;
        int py = p->y >> 8;
        if (px < -8 || px > NET_MAP_PX + 8 || py < -8 || py > NET_MAP_PY + 8) {
            despawn(p);
            continue;
        }

        /* Wall collision (unless PHASE flag) */
        if (!(p->flags & PROJ_PHASE)) {
            if (collision_point_solid(px + 4, py + 4)) {
                despawn(p);
                continue;
            }
        }
    }
}

void projectile_draw_all(s32 cam_x, s32 cam_y) {
    int cx = (int)(cam_x >> 8);
    int cy = (int)(cam_y >> 8);

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!(pool[i].flags & PROJ_ACTIVE)) continue;
        Projectile* p = &pool[i];

        int sx = (int)(p->x >> 8) - cx;
        int sy = (int)(p->y >> 8) - cy;

        OBJ_ATTR* spr = sprite_get(p->oam_index);
        if (!spr) continue;

        if (sx < -8 || sx > SCREEN_W || sy < -8 || sy > SCREEN_H) {
            obj_hide(spr);
        } else {
            /* Clamp to valid range before masking (negative wraps to far-right) */
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            obj_unhide(spr, ATTR0_REG);
            spr->attr0 = (u16)((spr->attr0 & ~ATTR0_Y_MASK) | (sy & ATTR0_Y_MASK));
            spr->attr1 = (u16)((spr->attr1 & ~ATTR1_X_MASK) | (sx & ATTR1_X_MASK));
        }
    }
}

void projectile_clear_all(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (pool[i].flags & PROJ_ACTIVE) {
            despawn(&pool[i]);
        }
    }
}

Projectile* projectile_get_pool(void) {
    return pool;
}
