/*
 * Ghost Protocol — Projectile System
 *
 * 16-slot static pool shared between player and enemy projectiles.
 * Each projectile has position, velocity, damage, type, lifetime, flags.
 */
#include "game/projectile.h"
#include "game/common.h"
#include "game/particle.h"
#include "engine/sprite.h"
#include "engine/entity.h"
#include "engine/collision.h"
#include "engine/audio.h"
#include "engine/rng.h"
#include "game/levelgen.h"
#include <string.h>

static Projectile pool[MAX_PROJECTILES];

/* ---- Projectile graphics: 10 tile variants (2 frames each for 5 types + enemy) ---- */

/* Buster: diamond-shaped energy bolt */
static const u32 proj_tile_buster[2][8] = {
    { /* Frame 0 */
        0x00030000, 0x00316D00, 0x03174100, 0x31274710,
        0x31274710, 0x03174100, 0x00316D00, 0x00030000,
    },
    { /* Frame 1: pulse outward */
        0x00030000, 0x00317D00, 0x0D174100, 0x31477710,
        0x31477710, 0x0D174100, 0x00317D00, 0x00030000,
    },
};
/* Rapid: sleek needle with motion trail */
static const u32 proj_tile_rapid[2][8] = {
    { /* Frame 0 */
        0x00000000, 0x00000000, 0x00051000, 0x05127410,
        0x05127410, 0x00051000, 0x00000000, 0x00000000,
    },
    { /* Frame 1: trail brightens */
        0x00000000, 0x00000000, 0x00061000, 0x06127410,
        0x06127410, 0x00061000, 0x00000000, 0x00000000,
    },
};
/* Spread: triple pellet cluster */
static const u32 proj_tile_spread[2][8] = {
    { /* Frame 0 */
        0x00012000, 0x00174000, 0x00012000, 0x00000000,
        0x01200000, 0x17400000, 0x01200000, 0x00000000,
    },
    { /* Frame 1: scatter slightly */
        0x00000120, 0x00001740, 0x00000120, 0x00000000,
        0x01200000, 0x17400000, 0x01200000, 0x00000000,
    },
};
/* Charge: large pulsing sphere with bright halo */
static const u32 proj_tile_charge[2][8] = {
    { /* Frame 0 */
        0x0D111D00, 0xD1274100, 0x12747410, 0x17477710,
        0x17477710, 0x12747410, 0xD1274100, 0x0D111D00,
    },
    { /* Frame 1: expand glow */
        0x0D161D00, 0xD1477100, 0x14777410, 0x47477710,
        0x47477710, 0x14777410, 0xD1477100, 0x0D161D00,
    },
};
/* Beam: sustained horizontal line with energy core */
static const u32 proj_tile_beam[2][8] = {
    { /* Frame 0 */
        0x00000000, 0x00000000, 0x55566655, 0x11274711,
        0x11274711, 0x55566655, 0x00000000, 0x00000000,
    },
    { /* Frame 1: ripple */
        0x00000000, 0x55500555, 0x00566600, 0x11474711,
        0x11474711, 0x00566600, 0x55500555, 0x00000000,
    },
};
/* Laser: continuous thin stream with interference bands */
static const u32 proj_tile_laser[2][8] = {
    { /* Frame 0 */
        0x00000000, 0x00000000, 0x66666666, 0x24747742,
        0x24747742, 0x66666666, 0x00000000, 0x00000000,
    },
    { /* Frame 1: flicker bands */
        0x00000000, 0x00000000, 0x55655656, 0x27474272,
        0x27474272, 0x55655656, 0x00000000, 0x00000000,
    },
};
/* Homing: rounded seeker with exhaust trail */
static const u32 proj_tile_homing[2][8] = {
    { /* Frame 0 */
        0x00031000, 0x00314D00, 0x03127100, 0x01274710,
        0x01274710, 0x03127100, 0x00531000, 0x05050000,
    },
    { /* Frame 1: exhaust flicker */
        0x00031000, 0x00314D00, 0x03127100, 0x01274710,
        0x01274710, 0x03127100, 0x00631000, 0x00060500,
    },
};
/* Nova: expanding ring burst */
static const u32 proj_tile_nova[2][8] = {
    { /* Frame 0: tight ring */
        0x00111000, 0x01000100, 0x10747010, 0x07000700,
        0x07000700, 0x10747010, 0x01000100, 0x00111000,
    },
    { /* Frame 1: expanded ring */
        0x01000010, 0x10000001, 0x00747700, 0x07477070,
        0x07477070, 0x00747700, 0x10000001, 0x01000010,
    },
};
/* Enemy: angular shard with menacing glow */
static const u32 proj_tile_enemy[2][8] = {
    { /* Frame 0 */
        0x00D10000, 0x00171000, 0x0D274100, 0x00274100,
        0x00127400, 0x00127410, 0x000D2100, 0x0000D000,
    },
    { /* Frame 1: pulse */
        0x00D10000, 0x00171000, 0x0D474100, 0x00474100,
        0x00147400, 0x00147410, 0x000D4100, 0x0000D000,
    },
};

/* Palette: 16-color player projectile (cyan energy ramp + accents) */
static const u16 proj_pal_player[16] = {
    0x0000,
    RGB15_C(0,28,31),  /* 1: bright cyan */
    RGB15_C(0,20,24),  /* 2: mid cyan */
    RGB15_C(0,12,16),  /* 3: dark cyan */
    RGB15_C(8,31,31),  /* 4: white-cyan glow */
    RGB15_C(0,8,12),   /* 5: dim trail */
    RGB15_C(4,24,28),  /* 6: mid-bright */
    RGB15_C(16,31,31), /* 7: core white */
    RGB15_C(2,6,10),   /* 8: faint outline */
    RGB15_C(12,28,31), /* 9: light cyan */
    RGB15_C(0,4,8),    /* A: very dim */
    RGB15_C(6,16,20),  /* B: mid-dim */
    RGB15_C(20,31,31), /* C: near-white glow */
    RGB15_C(0,14,18),  /* D: edge tint */
    RGB15_C(24,31,31), /* E: highlight */
    RGB15_C(31,31,31), /* F: pure white */
};
/* Palette: 16-color enemy projectile (red-orange menace + accents) */
static const u16 proj_pal_enemy[16] = {
    0x0000,
    RGB15_C(31,8,0),   /* 1: bright red-orange */
    RGB15_C(24,4,0),   /* 2: mid red */
    RGB15_C(16,2,0),   /* 3: dark red */
    RGB15_C(31,16,4),  /* 4: orange glow */
    RGB15_C(10,1,0),   /* 5: dim trail */
    RGB15_C(28,10,2),  /* 6: mid-bright */
    RGB15_C(31,24,12), /* 7: core yellow */
    RGB15_C(8,0,0),    /* 8: faint outline */
    RGB15_C(31,20,8),  /* 9: light orange */
    RGB15_C(6,0,0),    /* A: very dim */
    RGB15_C(18,6,0),   /* B: mid-dim */
    RGB15_C(31,28,16), /* C: near-white glow */
    RGB15_C(12,2,0),   /* D: edge tint */
    RGB15_C(31,31,20), /* E: highlight */
    RGB15_C(31,31,31), /* F: pure white */
};

static int gfx_loaded = 0;
static int anim_timer = 0;    /* global animation frame toggle for projectiles */
#define PROJ_TILE_BASE 272    /* OBJ tile index for projectile (after boss 240+32) */
#define PROJ_PAL_PLAYER 14
#define PROJ_PAL_ENEMY  15
/* Tile layout: 9 types × 2 frames = 18 tiles at PROJ_TILE_BASE + 0..17 */
#define PROJ_TILES_PER_TYPE 2

/* Map from projectile subtype to tile offset pair */
static const u8 proj_type_tile_ofs[] = {
    0,   /* SUBTYPE_PROJ_BUSTER  → tiles 0,1  */
    2,   /* SUBTYPE_PROJ_RAPID   → tiles 2,3  */
    4,   /* SUBTYPE_PROJ_SPREAD  → tiles 4,5  */
    6,   /* SUBTYPE_PROJ_CHARGE  → tiles 6,7  */
    8,   /* SUBTYPE_PROJ_BEAM    → tiles 8,9  */
    16,  /* SUBTYPE_PROJ_ENEMY   → tiles 16,17 */
    10,  /* SUBTYPE_PROJ_LASER   → tiles 10,11 */
    12,  /* SUBTYPE_PROJ_HOMING  → tiles 12,13 */
    14,  /* SUBTYPE_PROJ_NOVA    → tiles 14,15 */
};

static void load_gfx(void) {
    if (gfx_loaded) return;
    /* Load 9 projectile types × 2 frames = 18 tiles to OBJ VRAM */
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 0],  proj_tile_buster,  sizeof(proj_tile_buster) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 2],  proj_tile_rapid,   sizeof(proj_tile_rapid) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 4],  proj_tile_spread,  sizeof(proj_tile_spread) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 6],  proj_tile_charge,  sizeof(proj_tile_charge) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 8],  proj_tile_beam,    sizeof(proj_tile_beam) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 10], proj_tile_laser,   sizeof(proj_tile_laser) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 12], proj_tile_homing,  sizeof(proj_tile_homing) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 14], proj_tile_nova,    sizeof(proj_tile_nova) / 2);
    memcpy16(&tile_mem_obj[0][PROJ_TILE_BASE + 16], proj_tile_enemy,   sizeof(proj_tile_enemy) / 2);
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
            pool[i].hit_mask = 0; /* Clear piercing dedup mask from previous occupant */

            /* Configure OAM sprite — select tile variant by subtype */
            OBJ_ATTR* spr = sprite_get(oam);
            if (spr) {
                int pal = (flags & PROJ_ENEMY) ? PROJ_PAL_ENEMY : PROJ_PAL_PLAYER;
                int base_ofs = 0;
                if (type < (int)(sizeof(proj_type_tile_ofs) / sizeof(proj_type_tile_ofs[0]))) {
                    base_ofs = proj_type_tile_ofs[type];
                }
                spr->attr0 = ATTR0_SQUARE | ATTR0_4BPP;
                spr->attr1 = ATTR1_SIZE_8;
                spr->attr2 = (u16)(ATTR2_ID(PROJ_TILE_BASE + base_ofs) | ATTR2_PALBANK(pal));
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

/* Find nearest enemy entity position for homing projectiles */
static int find_nearest_enemy(s32 px, s32 py, s32* out_x, s32* out_y) {
    int hw = entity_get_high_water();
    int best_dist = 0x7FFFFFFF;
    int found = 0;
    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (!e || e->type != ENT_ENEMY || e->hp <= 0) continue;
        int dx = (int)((e->x - px) >> 8);
        int dy = (int)((e->y - py) >> 8);
        int dist = dx * dx + dy * dy;
        if (dist < best_dist) {
            best_dist = dist;
            *out_x = e->x + ((s32)e->width << 7);
            *out_y = e->y + ((s32)e->height << 7);
            found = 1;
        }
    }
    return found;
}

void projectile_update_all(void) {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!(pool[i].flags & PROJ_ACTIVE)) continue;
        Projectile* p = &pool[i];

        /* Homing AI: curve toward nearest enemy each frame */
        if (p->type == SUBTYPE_PROJ_HOMING && !(p->flags & PROJ_ENEMY)) {
            s32 tx, ty;
            if (find_nearest_enemy(p->x, p->y, &tx, &ty)) {
                int dx = (int)((tx - p->x) >> 8);
                int dy = (int)((ty - p->y) >> 8);
                /* Adjust velocity toward target (gradual curve, ±24/frame) */
                if (dx > 0 && p->vx < 448) p->vx += 24;
                else if (dx < 0 && p->vx > -448) p->vx -= 24;
                if (dy > 0 && p->vy < 448) p->vy += 24;
                else if (dy < 0 && p->vy > -448) p->vy -= 24;
            }
        }

        /* Nova AoE: short-lived expanding ring, passes through walls */
        if (p->type == SUBTYPE_PROJ_NOVA) {
            /* Nova doesn't move — just expands and damages everything nearby.
             * Already has PIERCE behavior. Reduce lifetime fast. */
            p->vx = 0;
            p->vy = 0;
        }

        /* Per-weapon velocity modifiers */
        if (!(p->flags & PROJ_ENEMY)) {
            switch (p->type) {
            case SUBTYPE_PROJ_RAPID:
                /* Slight deceleration — trails off at range */
                p->vx = (s16)(p->vx * 95 / 100);
                break;
            case SUBTYPE_PROJ_LASER:
                /* Accelerates to max speed quickly */
                if (p->vx > 0 && p->vx < 512) p->vx += 32;
                else if (p->vx < 0 && p->vx > -512) p->vx -= 32;
                break;
            case SUBTYPE_PROJ_HOMING:
                /* Stronger curve rate (handled above, but cap speed) */
                if (p->vx > 448) p->vx = 448;
                if (p->vx < -448) p->vx = -448;
                if (p->vy > 448) p->vy = 448;
                if (p->vy < -448) p->vy = -448;
                break;
            default:
                break;
            }
        }

        /* Move */
        p->x += p->vx;
        p->y += p->vy;

        /* Weapon-specific trail particles (player projectiles only) */
        if (!(p->flags & PROJ_ENEMY) && (p->lifetime & 3) == 0) {
            switch (p->type) {
            case SUBTYPE_PROJ_BEAM:
                /* Fading trail spark */
                particle_spawn(p->x, p->y, 0, 0, PART_SPARK, 6);
                break;
            case SUBTYPE_PROJ_LASER:
                /* Red spark trail */
                particle_spawn(p->x, p->y, (s16)(-(p->vx >> 3)), 0, PART_SPARK, 5);
                break;
            case SUBTYPE_PROJ_HOMING:
                /* Green exhaust */
                particle_spawn(p->x, p->y, (s16)(-(p->vx >> 2)), (s16)(-(p->vy >> 2)),
                               PART_BURST, 8);
                break;
            case SUBTYPE_PROJ_CHARGE:
                /* Pulsing glow */
                particle_spawn(p->x, p->y, 0, 0, PART_STAR, 8);
                break;
            case SUBTYPE_PROJ_SPREAD:
                /* Scatter trail */
                particle_spawn(p->x, p->y, (s16)(-(p->vx >> 3)), (s16)(-(p->vy >> 3)),
                               PART_SPARK, 4);
                break;
            case SUBTYPE_PROJ_NOVA:
                /* Expanding ring particles */
                particle_spawn(p->x + (s32)((int)rand_range(17) - 8) * 256,
                               p->y + (s32)((int)rand_range(17) - 8) * 256,
                               (s16)((int)rand_range(65) - 32), (s16)((int)rand_range(65) - 32),
                               PART_BURST, 6);
                break;
            default:
                break;
            }
        }

        /* Lifetime */
        if (p->lifetime > 0) {
            p->lifetime--;
        }
        if (p->lifetime == 0) {
            /* Impact spark on expiry (visual feedback) */
            if (p->type == SUBTYPE_PROJ_HOMING || p->type == SUBTYPE_PROJ_CHARGE) {
                particle_spawn(p->x, p->y, 0, -64, PART_SPARK, 8);
            }
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

        /* Wall collision (unless PHASE flag or NOVA/HOMING) */
        if (!(p->flags & PROJ_PHASE) && p->type != SUBTYPE_PROJ_NOVA) {
            int tile = collision_tile_at(px + 4, py + 4);
            if (tile == TILE_SOLID || tile == TILE_BREAKABLE) {
                /* Break breakable walls (player projectiles only) */
                if (tile == TILE_BREAKABLE && !(p->flags & PROJ_ENEMY)) {
                    int tx = (px + 4) >> 3;
                    int ty = (py + 4) >> 3;
                    levelgen_set_collision(tx, ty, TILE_EMPTY);
                    /* Extra sparks for wall break */
                    particle_burst(p->x, p->y, 6, PART_BURST, 200, 14);
                    audio_play_sfx(SFX_BOSS_EXPLODE);
                }
                /* Impact sparks: reverse direction bounce + scatter */
                s16 rvx = (s16)(-(p->vx >> 2));
                particle_spawn(p->x, p->y, rvx, -64, PART_SPARK, 10);
                particle_spawn(p->x, p->y, (s16)(rvx + 32), -96, PART_SPARK, 8);
                particle_spawn(p->x, p->y, (s16)(rvx - 32), -32, PART_SPARK, 8);
                despawn(p);
                continue;
            }
        }
    }
}

void projectile_draw_all(s32 cam_x, s32 cam_y) {
    int cx = (int)(cam_x >> 8);
    int cy = (int)(cam_y >> 8);

    /* Toggle animation frame every 6 frames for pulsing effect */
    anim_timer++;
    int anim_frame = (anim_timer >> 3) & 1; /* 0 or 1, toggles every 8 frames */

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
            /* GBA wraps negative coords via bitmask — no clamping needed */
            obj_unhide(spr, ATTR0_REG);
            spr->attr0 = (u16)((spr->attr0 & ~ATTR0_Y_MASK) | (sy & ATTR0_Y_MASK));
            spr->attr1 = (u16)((spr->attr1 & ~ATTR1_X_MASK) | (sx & ATTR1_X_MASK));

            /* Update tile ID for animation frame */
            int base_ofs = 0;
            if (p->type < (int)(sizeof(proj_type_tile_ofs) / sizeof(proj_type_tile_ofs[0]))) {
                base_ofs = proj_type_tile_ofs[p->type];
            }
            int pal = (p->flags & PROJ_ENEMY) ? PROJ_PAL_ENEMY : PROJ_PAL_PLAYER;
            spr->attr2 = (u16)(ATTR2_ID(PROJ_TILE_BASE + base_ofs + anim_frame) | ATTR2_PALBANK(pal));
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
