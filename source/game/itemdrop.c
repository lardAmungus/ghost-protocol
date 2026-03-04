/*
 * Ghost Protocol — Item Drop System
 *
 * Spawns floating pickup entities on enemy kills.
 */
#include "game/itemdrop.h"
#include "game/common.h"
#include "engine/entity.h"
#include "game/hud.h"
#include "engine/sprite.h"
#include "engine/audio.h"
#include "engine/rng.h"

typedef struct {
    s32 x, y;       /* World position (8.8) */
    LootItem item;
    u8 oam_index;
    u8 active;
    u8 bob_timer;
    u8 lifetime;     /* Frames remaining before despawn */
    u8 tile_variant; /* 0=gem, 1=chip, 2=orb */
} DropEntity;

static DropEntity drops[MAX_DROPS];

/* Item drop sprite tiles: 3 variants */
/* Tile 0: Gem (faceted, consumables/common) */
static const u32 spr_drop_gem[8] = {
    0x00044000, 0x00476400, 0x04677640, 0x46777764,
    0x04677640, 0x00476400, 0x00044000, 0x00000000,
};
/* Tile 1: Chip (data chip, gear drops) */
static const u32 spr_drop_chip[8] = {
    0x00000000, 0x04444440, 0x46777640, 0x47676740,
    0x47676740, 0x46777640, 0x04444440, 0x00000000,
};
/* Tile 2: Orb (energy sphere, rare+) */
static const u32 spr_drop_orb[8] = {
    0x00044000, 0x00477400, 0x04777740, 0x47F77F74,
    0x47777F74, 0x04777740, 0x00477400, 0x00044000,
};

/* Drop palette — full 16 colors with rarity + material ramps */
static const u16 pal_drop[16] = {
    0,
    RGB15_C(20, 20, 18),  /* 1: Common body (warm grey) */
    RGB15_C(10, 26, 10),  /* 2: Uncommon body (green) */
    RGB15_C(6, 16, 28),   /* 3: Rare body (blue) */
    RGB15_C(24, 10, 28),  /* 4: Epic body (purple) */
    RGB15_C(28, 24, 6),   /* 5: Legendary body (gold) */
    RGB15_C(14, 14, 12),  /* 6: gem shadow */
    RGB15_C(24, 24, 22),  /* 7: gem highlight */
    RGB15_C(8, 18, 8),    /* 8: chip shadow (green) */
    RGB15_C(18, 28, 18),  /* 9: chip highlight */
    RGB15_C(12, 8, 22),   /* A: orb shadow (purple) */
    RGB15_C(24, 18, 31),  /* B: orb highlight */
    RGB15_C(22, 18, 4),   /* C: accent dark */
    RGB15_C(31, 26, 8),   /* D: accent bright */
    RGB15_C(28, 28, 26),  /* E: warm bright */
    RGB15_C(31, 31, 28),  /* F: warm white */
};

/* Drop tiles at OBJ tile 180 (after 4 projectile tiles at 176-179) */
#define DROP_TILE_BASE 180
#define DROP_PAL_BANK  8
static int gfx_loaded;

static void load_drop_gfx(void) {
    if (gfx_loaded) return;
    /* Load 3 drop tile variants (tiles 180-182) */
    memcpy16(&tile_mem[4][DROP_TILE_BASE + 0], spr_drop_gem, sizeof(spr_drop_gem) / 2);
    memcpy16(&tile_mem[4][DROP_TILE_BASE + 1], spr_drop_chip, sizeof(spr_drop_chip) / 2);
    memcpy16(&tile_mem[4][DROP_TILE_BASE + 2], spr_drop_orb, sizeof(spr_drop_orb) / 2);
    memcpy16(&pal_obj_mem[DROP_PAL_BANK * 16], pal_drop, 16);
    gfx_loaded = 1;
}

void itemdrop_init(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        drops[i].active = 0;
    }
    gfx_loaded = 0;
}

void itemdrop_spawn(s32 x, s32 y, const LootItem* item) {
    load_drop_gfx();

    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) {
            drops[i].x = x;
            drops[i].y = y;
            drops[i].item = *item;
            drops[i].active = 1;
            drops[i].bob_timer = 0;
            drops[i].lifetime = 255; /* ~4.25 seconds */

            /* Select tile variant by rarity: common/uncommon=gem, rare/epic=chip, legendary=orb */
            if (item->rarity >= RARITY_LEGENDARY) {
                drops[i].tile_variant = 2; /* orb */
            } else if (item->rarity >= RARITY_RARE) {
                drops[i].tile_variant = 1; /* chip */
            } else {
                drops[i].tile_variant = 0; /* gem */
            }

            int oam = sprite_alloc();
            if (oam < 0) {
                /* OAM pool exhausted — cancel this drop */
                drops[i].active = 0;
                return;
            }
            drops[i].oam_index = (u8)oam;
            return;
        }
    }
}

void itemdrop_roll(s32 x, s32 y, int tier, int rarity_floor, int player_lck) {
    /* Regular enemies: 20% drop chance */
    int roll = (int)rand_range(100);
    if (roll >= 20) return;

    LootItem item;
    loot_generate(&item, tier, rarity_floor, player_lck);
    itemdrop_spawn(x, y, &item);
}

void itemdrop_update_all(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;

        drops[i].bob_timer++;
        drops[i].lifetime--;

        if (drops[i].lifetime == 0) {
            /* Despawn */
            if (drops[i].oam_index != OAM_NONE) {
                OBJ_ATTR* oam = sprite_get(drops[i].oam_index);
                if (oam) oam->attr0 = ATTR0_HIDE;
                sprite_free(drops[i].oam_index);
            }
            drops[i].active = 0;
        }
    }
}

void itemdrop_draw_all(s32 cam_x, s32 cam_y) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;
        if (drops[i].oam_index == OAM_NONE) continue;

        int sx = (int)((drops[i].x - cam_x) >> 8);
        int sy = (int)((drops[i].y - cam_y) >> 8);

        /* Bob up and down */
        int bob = (drops[i].bob_timer & 31);
        if (bob < 16) {
            sy -= bob / 4;
        } else {
            sy -= (31 - bob) / 4;
        }

        /* Off-screen culling */
        if (sx < -8 || sx > SCREEN_WIDTH + 8 || sy < -8 || sy > SCREEN_HEIGHT + 8) {
            OBJ_ATTR* oam = sprite_get(drops[i].oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            continue;
        }

        /* Blink when about to despawn */
        if (drops[i].lifetime < 60 && (drops[i].lifetime & 4)) {
            OBJ_ATTR* oam = sprite_get(drops[i].oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            continue;
        }

        int tile_id = DROP_TILE_BASE + drops[i].tile_variant;
        OBJ_ATTR* oam = sprite_get(drops[i].oam_index);
        if (!oam) continue;
        oam->attr0 = (u16)(ATTR0_SQUARE | ((u16)sy & 0xFF));
        oam->attr1 = (u16)(ATTR1_SIZE_8 | ((u16)sx & 0x1FF));
        oam->attr2 = (u16)(ATTR2_ID(tile_id) | ATTR2_PALBANK(DROP_PAL_BANK));
    }
}

int itemdrop_check_pickup(s32 player_x, s32 player_y) {
    int px = (int)(player_x >> 8);
    int py = (int)(player_y >> 8);

    for (int i = 0; i < MAX_DROPS; i++) {
        if (!drops[i].active) continue;

        int dx = (int)(drops[i].x >> 8) - px;
        int dy = (int)(drops[i].y >> 8) - py;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;

        if (dx < 12 && dy < 12) {
            /* Pickup! */
            if (inventory_add(&drops[i].item)) {
                audio_play_sfx(SFX_PICKUP);
                hud_notify("ITEM GET!", 30);

                /* Free OAM and deactivate */
                if (drops[i].oam_index != OAM_NONE) {
                    OBJ_ATTR* oam = sprite_get(drops[i].oam_index);
                    if (oam) oam->attr0 = ATTR0_HIDE;
                    sprite_free(drops[i].oam_index);
                }
                drops[i].active = 0;
                return 1;
            } else {
                hud_notify("INV FULL!", 30);
            }
        }
    }
    return 0;
}

void itemdrop_clear_all(void) {
    for (int i = 0; i < MAX_DROPS; i++) {
        if (drops[i].active && drops[i].oam_index != OAM_NONE) {
            OBJ_ATTR* oam = sprite_get(drops[i].oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            sprite_free(drops[i].oam_index);
        }
        drops[i].active = 0;
    }
}
