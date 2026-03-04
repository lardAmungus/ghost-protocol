/*
 * Ghost Protocol — Enemy System
 *
 * 6 enemy types (ICE security programs) for Net levels.
 */
#include "game/enemy.h"
#include "game/common.h"
#include "game/projectile.h"
#include "game/physics.h"
#include "engine/sprite.h"
#include "engine/entity.h"
#include "engine/collision.h"
#include "engine/audio.h"
#include "engine/rng.h"
#include "game/itemdrop.h"
#include "game/player.h"
#include "game/bugbounty.h"
#include "game/hud.h"

/* Enemy info table (base stats, scaled by tier at spawn) */
const EnemyInfo enemy_info[ENEMY_TYPE_COUNT] = {
    /* SENTRY:  low HP, ranged, stationary — longest detect (turret) */
    { 6, 3, 12, 12, 5, 128 },
    /* PATROL:  moderate HP, melee, walks */
    { 10, 4, 14, 14, 8, 64 },
    /* FLYER:   low HP, aerial */
    { 5, 3, 12, 12, 6, 96 },
    /* SHIELD:  high HP, frontal block — moderate detect for telegraphing */
    { 14, 5, 14, 14, 10, 72 },
    /* SPIKE:   low HP, hazard, extending */
    { 4, 6, 8, 16, 4, 0 },
    /* HUNTER:  moderate HP, aggressive */
    { 8, 5, 14, 14, 12, 112 },
};

/* Per-enemy AI state (indexed by entity pool slot) */
typedef struct {
    u8 state;         /* ESTATE_* */
    u8 state_timer;
    u8 shoot_timer;
    u8 tier;          /* Difficulty scaling */
    s16 home_x;       /* Patrol home position (8.8) */
    s16 patrol_dir;   /* +1 or -1 */
    s16 scaled_atk;   /* ATK after tier/bounty scaling */
} EnemyAI;

#define MAX_ENEMY_SLOTS 24
static EnemyAI enemy_ai[MAX_ENEMY_SLOTS];
static int total_kills;

/* Sprite tile data for enemies (8x8 tiles, 4 tiles per enemy = 16x16) */
/* Each enemy type uses 8 OBJ tiles (2 frames x 4 tiles) */

/* Enemy sprites: 4 frames per type, 4 tiles per frame = 16 tiles each
 * Palette indices: 0=transparent, 1=body deep shadow, 2=body shadow,
 *   3=body base, 4=body highlight, 5=lens/glow dim, 6=lens/glow mid,
 *   7=lens/glow bright, 8=panel dark, 9=panel mid, A=panel highlight,
 *   B=accent dark, C=accent bright, D=AA edge dark, E=AA edge light,
 *   F=white highlight */

/* Sentry: Turret with sensor eye + barrel on base */
static const u32 spr_sentry[4][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x11110000, 0x67651000, 0x34431000, 0x34431000, 0x11110000, 0x11310000 },
    { 0x00000000, 0x00000000, 0x00000001, 0x00000013, 0x00000001, 0x00000001, 0x00000001, 0x00001111 },
    { 0x33410000, 0x11310000, 0x11110000, 0x34321000, 0x34432100, 0x11111100, 0x00000000, 0x00000000 },
    { 0x00001443, 0x00001111, 0x00000001, 0x00000012, 0x00000012, 0x00000011, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x11110000, 0x77751000, 0x34431000, 0x34431000, 0x11110000, 0x11310000 },
    { 0x00000000, 0x00000000, 0x00000001, 0x00000013, 0x00000001, 0x00000001, 0x00000001, 0x00001111 },
    { 0x33410000, 0x11310000, 0x11110000, 0x34321000, 0x34432100, 0x11111100, 0x00000000, 0x00000000 },
    { 0x00001443, 0x00001111, 0x00000001, 0x00000012, 0x00000012, 0x00000011, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000000, 0x11110000, 0x67651000, 0x34431000, 0x34431000, 0x11110000, 0x11310000 },
    { 0x00000000, 0x00000000, 0x00000001, 0x00000013, 0x00000001, 0x00000001, 0x00000001, 0x00F11111 },
    { 0x33410000, 0x11310000, 0x11110000, 0x34321000, 0x34432100, 0x11111100, 0x00000000, 0x00000000 },
    { 0x00FF1443, 0x00F11111, 0x00000001, 0x00000012, 0x00000012, 0x00000011, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x00000000, 0x11100000, 0x67651000, 0x34431000, 0x34431000, 0x11110000, 0x11310000 },
    { 0x00000000, 0x00000000, 0x00000001, 0x00000003, 0x00000001, 0x00000001, 0x00000000, 0x00000111 },
    { 0x33410000, 0x11310000, 0x11110000, 0x34321000, 0x34432100, 0x11111100, 0x00000000, 0x00000000 },
    { 0x00001443, 0x00001111, 0x00000001, 0x00000012, 0x00000012, 0x00000011, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_patrol[4][4][8] = {
  { /* frame 0 */
    { 0x111D0000, 0x44431000, 0x66651100, 0x44431000, 0x111D0000, 0x34321D00, 0x434321D0, 0x434321D0 },
    { 0x0000000D, 0x000000D3, 0x000000D5, 0x000000D3, 0x0000000D, 0x00000D12, 0x0000D123, 0x0000D123 },
    { 0x34321D00, 0x3431D000, 0x3D13D000, 0x3D13D000, 0x1D121D00, 0x1D131D00, 0x1D111D00, 0x00000000 },
    { 0x00000D12, 0x000000D1, 0x000000D1, 0x000000D1, 0x00000D12, 0x00000D13, 0x00000D11, 0x00000000 },
  },
  { /* frame 1 */
    { 0x111D0000, 0x44431000, 0x66651100, 0x44431000, 0x111D0000, 0x34321D00, 0x434321D0, 0x434321D0 },
    { 0x0000000D, 0x000000D3, 0x000000D5, 0x000000D3, 0x0000000D, 0x00000D12, 0x0000D123, 0x0000D123 },
    { 0x34321D00, 0x34D1D000, 0xD0D13D00, 0xD00D13D0, 0x1D0D12D0, 0x1D0D11D0, 0x1D00DD00, 0x00000000 },
    { 0x00000D12, 0x000000D1, 0x00000D13, 0x00000D13, 0x00000D12, 0x00000D13, 0x00000D11, 0x00000000 },
  },
  { /* frame 2 */
    { 0x111D0000, 0x44431000, 0x66651100, 0x44431000, 0x111D0000, 0x34321D00, 0x434321D0, 0x434321D0 },
    { 0x0000000D, 0x000000D3, 0x000000D5, 0x000000D3, 0x0000000D, 0x00000D12, 0x0000D123, 0x0000D123 },
    { 0x34321D00, 0xD431D000, 0x0D13D000, 0x0D13D000, 0x0D121D00, 0x0D131D00, 0x0D111D00, 0x00000000 },
    { 0x00000D12, 0x000000D1, 0x0000D13D, 0x000D13D0, 0x000D12D0, 0x000D11D0, 0x0000DD00, 0x00000000 },
  },
  { /* frame 3 */
    { 0x111D0000, 0x44431000, 0x66651100, 0x44431000, 0x111D0000, 0x34321D00, 0x434321D0, 0x434321D0 },
    { 0x0000000D, 0x000000D3, 0x000000D5, 0x000000D3, 0x0000000D, 0x00000D12, 0x00011123, 0x00143123 },
    { 0x34321D00, 0x3431D000, 0x3D13D000, 0x3D13D000, 0x1D121D00, 0x1D131D00, 0x1D111D00, 0x00000000 },
    { 0x001F1112, 0x000000D1, 0x000000D1, 0x000000D1, 0x00000D12, 0x00000D13, 0x00000D11, 0x00000000 },
  },
};

static const u32 spr_flyer[4][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x000014D0, 0x00013410, 0x111D0000, 0x34311000, 0x67651000, 0x34311000, 0x131D0000 },
    { 0x00000000, 0x00D41000, 0x0D431000, 0x0000000D, 0x00000011, 0x00000015, 0x00000011, 0x0000000D },
    { 0x111D0000, 0xD1D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x111D14D0, 0x44313410, 0x67651000, 0x34311000, 0x131D0000, 0x111D0000 },
    { 0x00000000, 0x00000000, 0x000D410D, 0x0D431013, 0x00000015, 0x00000011, 0x0000000D, 0x0000000D },
    { 0xD1D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000000, 0x111D0000, 0x34311000, 0x67651000, 0x343114D0, 0x31D13410, 0x111D0000 },
    { 0x00000000, 0x00000000, 0x0000000D, 0x00000011, 0x00000015, 0x0D410011, 0x0D4310D1, 0x0000000D },
    { 0xD1D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x000014D0, 0x00013410, 0x111D0000, 0x34311000, 0x67651000, 0x34311000, 0x131D0000 },
    { 0x00000000, 0x00D41000, 0x0D430000, 0x0000000D, 0x00000011, 0x00000015, 0x00000011, 0x0000000D },
    { 0x1D1D0000, 0xD0D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_shield[4][4][8] = {
  { /* frame 0 */
    { 0x11111D00, 0x444431D0, 0x777651D0, 0x444431D0, 0x11111D00, 0x434321D0, 0x3434321D, 0x3434321D },
    { 0x0000D111, 0x0000D134, 0x0000D156, 0x0000D134, 0x0000D111, 0x000D0123, 0x000D1234, 0x000D1234 },
    { 0x434321D0, 0x44321D00, 0x1431D000, 0x1431D000, 0xD1321D00, 0xD1431D00, 0xD1111D00, 0x00000000 },
    { 0x000D0123, 0x0000D123, 0x0000D134, 0x0000D134, 0x000D1231, 0x000D1431, 0x000D1111, 0x00000000 },
  },
  { /* frame 1 */
    { 0x11111D00, 0x444431D0, 0x777651D0, 0x444431D0, 0x11111D00, 0x43432188, 0x43432188, 0x43432188 },
    { 0x0000D111, 0x0000D134, 0x0000D156, 0x0000D134, 0x0000D111, 0x000D0123, 0x000D1243, 0x000D1243 },
    { 0x43432188, 0x44321D88, 0x1431D000, 0x1431D000, 0xD1321D00, 0xD1431D00, 0xD1111D00, 0x00000000 },
    { 0x000D0123, 0x0000D123, 0x0000D134, 0x0000D134, 0x000D1231, 0x000D1431, 0x000D1111, 0x00000000 },
  },
  { /* frame 2 */
    { 0x11111D00, 0x444431D0, 0x777651D0, 0x444431D0, 0x11111D00, 0x434321D0, 0x3434321D, 0x3434321D },
    { 0x0000D111, 0x0000D134, 0x0000D156, 0x0000D134, 0x0000D111, 0x000D0123, 0x000D1234, 0x000D1234 },
    { 0x434321D0, 0x44321D00, 0xD431D000, 0xD1431D00, 0x0D1321D0, 0x0D1431D0, 0x0D1111D0, 0x00000000 },
    { 0x000D0123, 0x0000D123, 0x0000D134, 0x000D1340, 0x00D12310, 0x00D14310, 0x00D11110, 0x00000000 },
  },
  { /* frame 3 */
    { 0x1111D000, 0x44431D00, 0x77651D00, 0x44431D00, 0x1111D000, 0x34321D00, 0x434321D0, 0x434321D0 },
    { 0x000D1111, 0x000D1344, 0x000D1567, 0x000D1344, 0x000D1111, 0x00D01234, 0x00D12343, 0x00D12343 },
    { 0x34321D00, 0x4321D000, 0x1431D000, 0x1431D000, 0xD1321D00, 0xD1431D00, 0xD1111D00, 0x00000000 },
    { 0x00D01234, 0x000D1234, 0x0000D134, 0x0000D134, 0x000D1231, 0x000D1431, 0x000D1111, 0x00000000 },
  },
};

static const u32 spr_spike[3][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xD11D0000, 0x01310000, 0x01810000, 0x01310000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x01810000, 0x01310000, 0x01410000, 0x01310000, 0x01810000, 0x1131D000, 0x11111D00, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D, 0x000000D1, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x00700000, 0x01510000, 0xD11D0000, 0x01310000, 0x01810000, 0x01310000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x01810000, 0x01310000, 0x01410000, 0x01310000, 0x01810000, 0x1131D000, 0x11111D00, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D, 0x000000D1, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00700000, 0x01710000, 0x01510000, 0x01710000, 0x01510000, 0xD11D0000, 0x01310000, 0x01810000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x01310000, 0x01810000, 0x01310000, 0x01410000, 0x01810000, 0x1131D000, 0x11111D00, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D, 0x000000D1, 0x00000000 },
  },
};

static const u32 spr_hunter[4][4][8] = {
  { /* frame 0 */
    { 0x11D00000, 0x431D0000, 0x751D0000, 0x431D0000, 0x11D00000, 0x2321D000, 0x8381D000, 0x2321D000 },
    { 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1, 0x0000000D, 0x00000001, 0x00000001, 0x00000001 },
    { 0x131D0000, 0x131D0000, 0x3D13D000, 0x3D13D000, 0x1D131D00, 0x1D131D00, 0x1D111D00, 0x00000000 },
    { 0x0000000D, 0x0000000D, 0x000000D1, 0x000000D1, 0x00000D13, 0x00000D13, 0x00000D11, 0x00000000 },
  },
  { /* frame 1 */
    { 0x11D00000, 0x431D0000, 0x751D0000, 0x431D0000, 0x11D00000, 0x2321D000, 0x8381D000, 0x2321D000 },
    { 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1, 0x0000000D, 0x00000001, 0x00000001, 0x00000001 },
    { 0x131D0000, 0x0D13D000, 0xD0D13D00, 0xD00D13D0, 0x1D0D11D0, 0x1D00DD00, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x000000D1, 0x00000D13, 0x00000D13, 0x00000D13, 0x00000D11, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x11D00000, 0x431D0000, 0x751D0000, 0x431D0000, 0x11D00000, 0x2321D000, 0x8381D000, 0x2321D000 },
    { 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1, 0x0000000D, 0x00000001, 0x00000001, 0x00000001 },
    { 0x131D0000, 0x3D1D0000, 0x0D13D000, 0x0D13D000, 0x0D131D00, 0x0D111D00, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x000000D1, 0x0000D13D, 0x000D13D0, 0x000D11D0, 0x0000DD00, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x11D00000, 0x431D0000, 0x751D0000, 0x431D0000, 0x11D00000, 0x2321D000, 0x8381D000, 0x2321D000 },
    { 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1, 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1 },
    { 0x131D0000, 0x3D13D000, 0x3DD13D00, 0x1D111D00, 0xD0D1D000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x000000D1, 0x000000D1, 0x00000D11, 0x000000D1, 0x00000000, 0x00000000, 0x00000000 },
  },
};

/* Palette data per enemy type — full 16-color material ramps
 * 0=transparent, 1=outline, 2-4=body gradient, 5-7=eye glow,
 * 8-9=accent, B-C=tech, D=AA edge, F=white */
static const u16 pal_sentry[16] = {
    0,
    RGB15_C(2,4,6), /* 1: outline */
    RGB15_C(8,14,18), /* 2: dark body */
    RGB15_C(14,20,24), /* 3: body */
    RGB15_C(20,26,30), /* 4: highlight */
    RGB15_C(24,4,4), /* 5: eye dim */
    RGB15_C(31,10,8), /* 6: eye mid */
    RGB15_C(31,22,16), /* 7: eye bright */
    RGB15_C(6,18,24), /* 8: accent */
    RGB15_C(10,24,28), /* 9: accent2 */
    0, /* 10: unused (unused) */
    RGB15_C(4,12,16), /* B: tech dark */
    RGB15_C(8,22,28), /* C: tech bright */
    RGB15_C(4,8,12), /* D: AA edge */
    0, /* 14: unused (unused) */
    RGB15_C(31,31,31), /* F: white */
};
static const u16 pal_patrol[16] = {
    0,
    RGB15_C(4,2,2), /* 1: outline */
    RGB15_C(16,8,4), /* 2: dark body */
    RGB15_C(22,14,6), /* 3: body */
    RGB15_C(28,20,10), /* 4: highlight */
    RGB15_C(24,4,2), /* 5: eye dim */
    RGB15_C(30,12,4), /* 6: eye mid */
    RGB15_C(31,24,8), /* 7: eye bright */
    RGB15_C(14,10,8), /* 8: accent */
    RGB15_C(22,18,14), /* 9: accent2 */
    0, /* 10: unused (unused) */
    RGB15_C(12,6,2), /* B: tech dark */
    RGB15_C(20,12,4), /* C: tech bright */
    RGB15_C(6,4,3), /* D: AA edge */
    0, /* 14: unused (unused) */
    RGB15_C(31,31,31), /* F: white */
};
static const u16 pal_flyer[16] = {
    0,
    RGB15_C(2,4,2), /* 1: outline */
    RGB15_C(4,12,6), /* 2: dark body */
    RGB15_C(8,18,10), /* 3: body */
    RGB15_C(14,24,16), /* 4: highlight */
    RGB15_C(20,20,4), /* 5: eye dim */
    RGB15_C(28,28,8), /* 6: eye mid */
    RGB15_C(31,31,16), /* 7: eye bright */
    RGB15_C(6,14,8), /* 8: accent */
    RGB15_C(10,20,12), /* 9: accent2 */
    0, /* 10: unused (unused) */
    RGB15_C(4,10,6), /* B: tech dark */
    RGB15_C(8,18,10), /* C: tech bright */
    RGB15_C(3,6,3), /* D: AA edge */
    0, /* 14: unused (unused) */
    RGB15_C(31,31,31), /* F: white */
};
static const u16 pal_shield[16] = {
    0,
    RGB15_C(3,3,4), /* 1: outline */
    RGB15_C(8,10,12), /* 2: dark body */
    RGB15_C(14,16,18), /* 3: body */
    RGB15_C(20,22,24), /* 4: highlight */
    RGB15_C(4,8,24), /* 5: eye dim */
    RGB15_C(8,16,30), /* 6: eye mid */
    RGB15_C(16,24,31), /* 7: eye bright */
    RGB15_C(6,6,8), /* 8: accent */
    RGB15_C(10,10,12), /* 9: accent2 */
    0, /* 10: unused (unused) */
    RGB15_C(6,8,12), /* B: tech dark */
    RGB15_C(12,14,18), /* C: tech bright */
    RGB15_C(5,5,6), /* D: AA edge */
    0, /* 14: unused (unused) */
    RGB15_C(31,31,31), /* F: white */
};
static const u16 pal_spike[16] = {
    0,
    RGB15_C(4,2,2), /* 1: outline */
    RGB15_C(18,6,2), /* 2: dark body */
    RGB15_C(24,10,4), /* 3: body */
    RGB15_C(28,16,6), /* 4: highlight */
    RGB15_C(28,20,4), /* 5: eye dim */
    RGB15_C(31,26,8), /* 6: eye mid */
    RGB15_C(31,31,16), /* 7: eye bright */
    RGB15_C(20,4,4), /* 8: accent */
    RGB15_C(24,8,4), /* 9: accent2 */
    0, /* 10: unused (unused) */
    RGB15_C(14,4,2), /* B: tech dark */
    RGB15_C(22,8,4), /* C: tech bright */
    RGB15_C(6,3,2), /* D: AA edge */
    0, /* 14: unused (unused) */
    RGB15_C(31,31,31), /* F: white */
};
static const u16 pal_hunter[16] = {
    0,
    RGB15_C(4,2,6), /* 1: outline */
    RGB15_C(12,6,16), /* 2: dark body */
    RGB15_C(18,10,22), /* 3: body */
    RGB15_C(24,16,28), /* 4: highlight */
    RGB15_C(24,4,20), /* 5: eye dim */
    RGB15_C(31,10,28), /* 6: eye mid */
    RGB15_C(31,20,31), /* 7: eye bright */
    RGB15_C(16,8,12), /* 8: accent */
    RGB15_C(22,12,18), /* 9: accent2 */
    0, /* 10: unused (unused) */
    RGB15_C(10,4,14), /* B: tech dark */
    RGB15_C(18,8,22), /* C: tech bright */
    RGB15_C(6,3,8), /* D: AA edge */
    0, /* 14: unused (unused) */
    RGB15_C(31,31,31), /* F: white */
};


/* Sprite frame counts per type (spike has 3, others have 4) */
static const u8 enemy_frame_count[ENEMY_TYPE_COUNT] = { 4, 4, 4, 4, 3, 4 };

/* Sprite data pointers per type */
static const u32* const enemy_sprites[ENEMY_TYPE_COUNT] = {
    (const u32*)spr_sentry,
    (const u32*)spr_patrol,
    (const u32*)spr_flyer,
    (const u32*)spr_shield,
    (const u32*)spr_spike,
    (const u32*)spr_hunter,
};
static const u16* const enemy_palettes[ENEMY_TYPE_COUNT] = {
    pal_sentry, pal_patrol, pal_flyer, pal_shield, pal_spike, pal_hunter,
};

/* OBJ tile base per enemy type (loaded dynamically) */
/* Enemy tiles start at OBJ tile 48 (after player's 48 tiles) */
#define ENEMY_TILE_BASE 48
/* Each enemy type: 4 frames x 4 tiles = 16 tiles */
/* OBJ palette banks: 1-6 for enemy types */
#define ENEMY_PAL_BASE 1

static int slot_to_entity[MAX_ENEMY_SLOTS]; /* Entity pool index per enemy slot */

void enemy_init(void) {
    total_kills = 0;
    for (int i = 0; i < MAX_ENEMY_SLOTS; i++) {
        slot_to_entity[i] = -1;
        enemy_ai[i].state = ESTATE_IDLE;
    }
}

static int find_free_slot(void) {
    for (int i = 0; i < MAX_ENEMY_SLOTS; i++) {
        if (slot_to_entity[i] < 0) return i;
    }
    return -1;
}

static int entity_to_slot(Entity* e) {
    int idx = (int)(e - entity_get(0));
    for (int i = 0; i < MAX_ENEMY_SLOTS; i++) {
        if (slot_to_entity[i] == idx) return i;
    }
    return -1;
}

Entity* enemy_spawn(int subtype, int tile_x, int tile_y, int tier) {
    int slot = find_free_slot();
    if (slot < 0) return NULL;

    Entity* e = entity_spawn(ENT_ENEMY);
    if (!e) return NULL;

    e->subtype = (u8)subtype;
    e->x = tile_x * 8 * 256; /* tile -> 8.8 px */
    e->y = tile_y * 8 * 256;
    e->width = enemy_info[subtype].width;
    e->height = enemy_info[subtype].height;
    e->hp = (s16)(enemy_info[subtype].hp + tier * 2);
    e->facing = 1; /* Face left by default (toward player) */
    e->on_ground = 0;

    /* Allocate OAM sprite — if pool exhausted, despawn and bail */
    int oam = sprite_alloc();
    if (oam < 0) {
        entity_despawn(e);
        return NULL;
    }
    e->oam_index = (u8)oam;

    /* Load tiles for this enemy type into OBJ VRAM (16 tiles per type) */
    int tile_id = ENEMY_TILE_BASE + subtype * 16;
    int num_frames = enemy_frame_count[subtype];
    memcpy16(&tile_mem[4][tile_id], enemy_sprites[subtype],
             (u32)(num_frames * 4 * 8) * (u32)sizeof(u32) / 2);

    /* Load palette */
    int pal_bank = ENEMY_PAL_BASE + subtype;
    if (pal_bank < 16) {
        memcpy16(&pal_obj_mem[pal_bank * 16], enemy_palettes[subtype], 16);
    }

    /* Setup AI */
    int ent_idx = (int)(e - entity_get(0));
    slot_to_entity[slot] = ent_idx;
    enemy_ai[slot].state = ESTATE_IDLE;
    enemy_ai[slot].state_timer = 0;
    enemy_ai[slot].shoot_timer = 0;
    enemy_ai[slot].tier = (u8)tier;
    enemy_ai[slot].home_x = (s16)(e->x >> 8);
    enemy_ai[slot].patrol_dir = 1;
    enemy_ai[slot].scaled_atk = (s16)(enemy_info[subtype].atk + tier); /* Scale ATK with tier */

    return e;
}

static void ai_sentry(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Stationary turret — face player, shoot aimed projectiles */
    int dx = (int)((player_x - e->x) >> 8);
    e->facing = (u8)((dx < 0) ? 1 : 0);

    int dist = dx < 0 ? -dx : dx;
    if (dist < enemy_info[ENEMY_SENTRY].detection_range) {
        ai->shoot_timer++;
        if (ai->shoot_timer >= 80) {
            ai->shoot_timer = 0;
            int ent_idx = (int)(e - entity_get(0));
            /* Aimed shot: compute velocity toward player */
            int dy = (int)((player_y - e->y) >> 8);
            int adx = dx < 0 ? -dx : dx;
            int ady = dy < 0 ? -dy : dy;
            int total = adx + ady;
            if (total < 1) total = 1;
            s16 pvx = (s16)(dx * 384 / total);
            s16 pvy = (s16)(dy * 384 / total);
            projectile_spawn(e->x, e->y, pvx, pvy,
                             ai->scaled_atk, SUBTYPE_PROJ_ENEMY,
                             PROJ_ENEMY, (u8)ent_idx);
            /* Tier 3+: fire a second spread projectile */
            if (ai->tier >= 3) {
                s16 spread_vy = (s16)(pvy + (pvy > 0 ? -96 : 96));
                projectile_spawn(e->x, e->y, pvx, spread_vy,
                                 ai->scaled_atk, SUBTYPE_PROJ_ENEMY,
                                 PROJ_ENEMY, (u8)ent_idx);
            }
            audio_play_sfx(SFX_SHOOT);
        }
    }
}

static void ai_patrol(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    int dx = (int)((player_x - e->x) >> 8);
    int dist = dx < 0 ? -dx : dx;

    if (dist < enemy_info[ENEMY_PATROL].detection_range) {
        /* Chase player */
        ai->state = ESTATE_CHASE;
        e->facing = (u8)((dx < 0) ? 1 : 0);

        /* Charge rush when close: burst of speed for 20 frames */
        if (dist < 24 && e->on_ground && ai->shoot_timer == 0) {
            e->vx = e->facing ? -256 : 256; /* Double speed charge */
            ai->shoot_timer = 60; /* Charge cooldown */
            ai->state = ESTATE_ATTACK;
        } else if (ai->shoot_timer > 40) {
            /* Still in charge (first 20 frames of cooldown) */
            e->vx = e->facing ? -256 : 256;
            ai->state = ESTATE_ATTACK;
        } else {
            e->vx = e->facing ? -128 : 128; /* 0.5 px/frame normal chase */
        }
    } else {
        /* Patrol back and forth */
        ai->state = ESTATE_PATROL;
        int home_dist = (int)(e->x >> 8) - ai->home_x;
        if (home_dist > 40) ai->patrol_dir = -1;
        if (home_dist < -40) ai->patrol_dir = 1;
        e->vx = (s16)(ai->patrol_dir * 64); /* 0.25 px/frame */
        e->facing = (u8)((ai->patrol_dir < 0) ? 1 : 0);
    }

    /* Apply gravity */
    e->vy += 32; /* gravity */
    if (e->vy > 512) e->vy = 512;

    /* Charge cooldown tick */
    if (ai->shoot_timer > 0) ai->shoot_timer--;

    (void)player_y;
}

static void ai_flyer(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Sine-wave flight, moves toward player, fires aimed shots */
    int dx = (int)((player_x - e->x) >> 8);
    e->facing = (u8)((dx < 0) ? 1 : 0);

    int dist = dx < 0 ? -dx : dx;
    if (dist < enemy_info[ENEMY_FLYER].detection_range) {
        ai->state = ESTATE_CHASE;
        e->vx = e->facing ? -96 : 96; /* 0.375 px/frame */

        /* Fire aimed projectile every ~100 frames while chasing */
        ai->shoot_timer++;
        if (ai->shoot_timer >= 100) {
            ai->shoot_timer = 0;
            int ent_idx = (int)(e - entity_get(0));
            int dy = (int)((player_y - e->y) >> 8);
            int adx = dx < 0 ? -dx : dx;
            int ady = dy < 0 ? -dy : dy;
            int total = adx + ady;
            if (total < 1) total = 1;
            s16 pvx = (s16)(dx * 320 / total);
            s16 pvy = (s16)(dy * 320 / total);
            projectile_spawn(e->x, e->y, pvx, pvy,
                             ai->scaled_atk, SUBTYPE_PROJ_ENEMY,
                             PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT);
        }
    } else {
        e->vx = 0;
        ai->shoot_timer = 0;
    }

    /* Sine wave vertical movement */
    ai->state_timer++;
    /* Use a simple triangle wave instead of sine lookup */
    int phase = ai->state_timer & 63;
    if (phase < 16) {
        e->vy = -32;
    } else if (phase < 48) {
        e->vy = 32;
    } else {
        e->vy = -32;
    }
}

static void ai_shield(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Blocks frontal attacks. Walks slowly toward player. */
    int dx = (int)((player_x - e->x) >> 8);
    e->facing = (u8)((dx < 0) ? 1 : 0);

    int dist = dx < 0 ? -dx : dx;
    if (dist < enemy_info[ENEMY_SHIELD].detection_range) {
        ai->state = ESTATE_CHASE;
        /* Retreat at <30% HP: walk backwards slowly (still facing player) */
        int max_hp = enemy_info[ENEMY_SHIELD].hp + ai->tier * 2;
        if (e->hp * 10 < max_hp * 3) {
            e->vx = e->facing ? 32 : -32; /* Walk away */
        } else {
            e->vx = e->facing ? -48 : 48; /* Normal approach */
        }
    } else {
        ai->state = ESTATE_IDLE;
        e->vx = 0;
    }

    /* Gravity */
    e->vy += 32;
    if (e->vy > 512) e->vy = 512;

    (void)player_y;
}

static void ai_spike(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Extends periodically — toggle between frames */
    ai->state_timer++;
    if (ai->state_timer >= 120) ai->state_timer = 0;

    /* Extended for first 60 frames, retracted for next 60 */
    if (ai->state_timer < 60) {
        ai->state = ESTATE_ATTACK;
        e->height = 16;
    } else {
        ai->state = ESTATE_IDLE;
        e->height = 8; /* Retracted — smaller hitbox */
    }

    /* No movement */
    e->vx = 0;
    e->vy = 0;

    (void)player_x;
    (void)player_y;
}

static void ai_hunter(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Aggressive chaser — runs fast, can jump, lunges when close */
    int dx = (int)((player_x - e->x) >> 8);
    int dy = (int)((player_y - e->y) >> 8);
    e->facing = (u8)((dx < 0) ? 1 : 0);

    int dist = dx < 0 ? -dx : dx;
    if (dist < enemy_info[ENEMY_HUNTER].detection_range) {
        ai->state = ESTATE_CHASE;

        /* Frenzy at <50% HP: faster lunge cooldown + faster chase */
        int max_hp = enemy_info[ENEMY_HUNTER].hp + ai->tier * 2;
        int frenzied = (e->hp * 2 < max_hp);
        int lunge_cd = frenzied ? 37 : 75;
        int chase_spd = frenzied ? 256 : 192;

        /* Lunge when close (< 32px): burst of speed + small jump */
        if (dist < 32 && e->on_ground && ai->shoot_timer == 0) {
            e->vx = e->facing ? -384 : 384; /* Lunge speed */
            e->vy = -256; /* Small hop */
            e->on_ground = 0;
            ai->shoot_timer = (u8)lunge_cd;
        } else {
            e->vx = e->facing ? (s16)-chase_spd : (s16)chase_spd;
        }

        /* Jump if player is above and we're on ground */
        if (dy < -24 && e->on_ground) {
            e->vy = -512; /* Jump */
            e->on_ground = 0;
        }
    } else {
        ai->state = ESTATE_PATROL;
        /* Patrol slowly */
        int home_dist = (int)(e->x >> 8) - ai->home_x;
        if (home_dist > 32) ai->patrol_dir = -1;
        if (home_dist < -32) ai->patrol_dir = 1;
        e->vx = (s16)(ai->patrol_dir * 64);
        e->facing = (u8)((ai->patrol_dir < 0) ? 1 : 0);
    }

    /* Lunge cooldown tick */
    if (ai->shoot_timer > 0) ai->shoot_timer--;

    /* Gravity */
    e->vy += 32;
    if (e->vy > 512) e->vy = 512;
}

IWRAM_CODE void enemy_update_all(s32 player_x, s32 player_y) {
    int hw = entity_get_high_water();
    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (!e || e->type != ENT_ENEMY) continue;

        int slot = entity_to_slot(e);
        if (slot < 0) continue;
        EnemyAI* ai = &enemy_ai[slot];

        /* Dead check */
        if (e->hp <= 0) {
            ai->state = ESTATE_DEAD;
            ai->state_timer++;
            if (ai->state_timer >= 30) {
                /* Despawn — entity_despawn handles sprite_free + oam_index clearing */
                entity_despawn(e);
                slot_to_entity[slot] = -1;
                total_kills++;
            }
            continue;
        }

        /* Hit stun */
        if (ai->state == ESTATE_HIT) {
            ai->state_timer++;
            if (ai->state_timer >= 10) {
                ai->state = ESTATE_IDLE;
                ai->state_timer = 0;
            }
            /* Apply knockback with collision resolution */
            e->x += e->vx;
            physics_resolve_x(e);
            e->y += e->vy;
            physics_resolve_y(e);
            continue;
        }

        /* AI per type */
        switch (e->subtype) {
        case ENEMY_SENTRY: ai_sentry(e, ai, player_x, player_y); break;
        case ENEMY_PATROL: ai_patrol(e, ai, player_x, player_y); break;
        case ENEMY_FLYER:  ai_flyer(e, ai, player_x, player_y); break;
        case ENEMY_SHIELD: ai_shield(e, ai, player_x, player_y); break;
        case ENEMY_SPIKE:  ai_spike(e, ai, player_x, player_y); break;
        case ENEMY_HUNTER: ai_hunter(e, ai, player_x, player_y); break;
        }

        /* Apply velocity with tile collision resolution */
        e->on_ground = 0;
        e->x += e->vx;
        physics_resolve_x(e);
        e->y += e->vy;
        physics_resolve_y(e);

        /* World bounds */
        if (e->x < 0) e->x = 0;
        if (e->y < 0) e->y = 0;
        int max_x = (NET_MAP_W * 8 - e->width) * 256;
        int max_y = (NET_MAP_H * 8 - e->height) * 256;
        if (e->x > max_x) e->x = max_x;
        if (e->y > max_y) e->y = max_y;

        /* Animation timer — 4-frame cycle driven by AI state */
        e->anim_timer++;
        if (e->anim_timer >= 8) {
            e->anim_timer = 0;
            int max_f = (int)enemy_frame_count[e->subtype];
            /* State-driven frame override for attack/damaged */
            if (ai->state == ESTATE_ATTACK && max_f > 2) {
                e->anim_frame = (u8)(max_f - 1); /* Last frame = attack/damaged */
            } else {
                e->anim_frame++;
                if (e->anim_frame >= max_f) e->anim_frame = 0;
            }
        }
    }
}

void enemy_draw_all(s32 cam_x, s32 cam_y) {
    int hw = entity_get_high_water();
    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (!e || e->type != ENT_ENEMY) continue;
        if (e->oam_index == OAM_NONE) continue;

        int slot = entity_to_slot(e);
        if (slot < 0) continue;

        int sx = (int)((e->x - cam_x) >> 8);
        int sy = (int)((e->y - cam_y) >> 8);

        /* Off-screen culling */
        if (sx < -16 || sx > SCREEN_WIDTH + 16 || sy < -16 || sy > SCREEN_HEIGHT + 16) {
            OBJ_ATTR* oam = sprite_get(e->oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            continue;
        }

        int tile_id = ENEMY_TILE_BASE + e->subtype * 16 + e->anim_frame * 4;
        int pal_bank = ENEMY_PAL_BASE + e->subtype;
        u16 attr1_flip = e->facing ? ATTR1_HFLIP : 0;  /* sprite faces RIGHT natively */

        /* Death flicker */
        EnemyAI* ai = &enemy_ai[slot];
        if (ai->state == ESTATE_DEAD) {
            if (ai->state_timer & 2) {
                OBJ_ATTR* oam = sprite_get(e->oam_index);
                if (oam) oam->attr0 = ATTR0_HIDE;
                continue;
            }
        }

        /* Hit flash: fast flicker on alternating frames during ESTATE_HIT */
        if (ai->state == ESTATE_HIT && (ai->state_timer & 1)) {
            OBJ_ATTR* oam = sprite_get(e->oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            continue;
        }

        OBJ_ATTR* oam = sprite_get(e->oam_index);
        if (!oam) continue;
        oam->attr0 = (u16)(ATTR0_SQUARE | ((u16)sy & 0xFF));
        oam->attr1 = (u16)(ATTR1_SIZE_16 | attr1_flip | ((u16)sx & 0x1FF));
        oam->attr2 = (u16)(ATTR2_ID(tile_id) | ATTR2_PALBANK(pal_bank));
    }
}

IWRAM_CODE int enemy_check_player_attack(Entity* player) {
    if (!player) return 0;

    Projectile* pool = projectile_get_pool();
    int total_dmg = 0;
    int hw = entity_get_high_water();

    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (!e || e->type != ENT_ENEMY || e->hp <= 0) continue;

        /* Check player projectiles vs this enemy */
        for (int j = 0; j < MAX_PROJECTILES; j++) {
            Projectile* p = &pool[j];
            if (!(p->flags & PROJ_ACTIVE)) continue;
            if (p->flags & PROJ_ENEMY) continue; /* Skip enemy projectiles */

            int px = (int)(p->x >> 8);
            int py = (int)(p->y >> 8);
            int ex = (int)(e->x >> 8);
            int ey = (int)(e->y >> 8);

            /* AABB: 8x8 projectile box vs enemy box */
            if (px + 8 > ex && px < ex + (int)e->width &&
                py + 8 > ey && py < ey + (int)e->height) {

                /* Piercing dedup: skip enemies already hit by this projectile */
                if (p->flags & PROJ_PIERCE) {
                    u16 bit = (u16)(1 << (i & 15));
                    if (p->hit_mask & bit) continue;
                    p->hit_mask |= bit;
                }

                /* Shield: block frontal attacks + push player back */
                if (e->subtype == ENEMY_SHIELD) {
                    int from_front = (e->facing && px > ex) || (!e->facing && px < ex);
                    if (from_front) {
                        /* Blocked — deflect projectile and stagger player */
                        audio_play_sfx(SFX_MENU_BACK);
                        projectile_deactivate(p);
                        /* Push player away from shield */
                        if (player) {
                            s16 kb = (player->x > e->x) ? 96 : -96;
                            player->vx = kb;
                        }
                        continue;
                    }
                }

                /* Hit! Apply damage + directional knockback from projectile */
                enemy_damage(e, p->damage);
                e->vx = (p->vx > 0) ? 128 : -128;
                total_dmg += p->damage;

                /* Remove projectile unless piercing */
                if (!(p->flags & PROJ_PIERCE)) {
                    projectile_deactivate(p);
                }
            }
        }
    }
    return total_dmg;
}

void enemy_damage(Entity* e, int dmg) {
    e->hp -= (s16)dmg;
    audio_play_sfx(SFX_ENEMY_HIT);

    /* Knockback */
    int slot = entity_to_slot(e);
    if (slot >= 0) {
        enemy_ai[slot].state = ESTATE_HIT;
        enemy_ai[slot].state_timer = 0;
    }
    /* Push away from damage source */
    e->vx = e->facing ? 128 : -128;
    e->vy = -64;

    if (e->hp <= 0) {
        audio_play_sfx(SFX_ENEMY_DIE);
        if (slot >= 0) {
            enemy_ai[slot].state = ESTATE_DEAD;
            enemy_ai[slot].state_timer = 0;

            /* XP reward — scales with enemy tier */
            {
                int base_xp = enemy_info[e->subtype].xp_reward;
                int tier_bonus = enemy_ai[slot].tier;
                player_add_xp(base_xp + tier_bonus * 3);
            }

            /* Credit drop — scales with enemy type and tier */
            {
                static const u8 base_credits[ENEMY_TYPE_COUNT] = {
                    2, 3, 2, 4, 1, 5
                };
                int cr = base_credits[e->subtype] + enemy_ai[slot].tier;
                u32 total = (u32)player_state.credits + (u32)cr;
                player_state.credits = (total > 0xFFFF) ? (u16)0xFFFF : (u16)total;
            }

            /* Bug bounty scoring */
            if (bb_state.active) {
                bugbounty_add_kill_score(enemy_ai[slot].tier,
                                         bb_state.rarity_floor);
            }

            /* Small health drop chance (15%) — restores 5 HP */
            if (player_state.hp < player_state.max_hp && rand_range(100) < 15) {
                player_state.hp += 5;
                if (player_state.hp > player_state.max_hp) {
                    player_state.hp = player_state.max_hp;
                }
                hud_notify("+5 HP!", 20);
            }

            /* Roll for loot drop (use bounty rarity floor if active) */
            int rarity_floor = bb_state.active ? bb_state.rarity_floor
                                               : RARITY_COMMON;
            itemdrop_roll(e->x, e->y, enemy_ai[slot].tier,
                          rarity_floor, player_state.lck);
        }
    }
}

int enemy_count_alive(void) {
    int count = 0;
    int hw = entity_get_high_water();
    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (e && e->type == ENT_ENEMY && e->hp > 0) count++;
    }
    return count;
}

int enemy_get_kills(void) {
    return total_kills;
}

int enemy_get_atk(Entity* e) {
    int slot = entity_to_slot(e);
    if (slot < 0) return enemy_info[e->subtype].atk;
    return enemy_ai[slot].scaled_atk;
}

void enemy_scale_atk(Entity* e, int scale256) {
    int slot = entity_to_slot(e);
    if (slot < 0) return;
    enemy_ai[slot].scaled_atk = (s16)((enemy_ai[slot].scaled_atk * scale256) >> 8);
}

void enemy_stun_all(int damage) {
    int hw = entity_get_high_water();
    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (!e || e->type != ENT_ENEMY || e->hp <= 0) continue;
        int slot = entity_to_slot(e);
        if (slot < 0) continue;

        /* Stun: put into hit state */
        enemy_ai[slot].state = ESTATE_HIT;
        enemy_ai[slot].state_timer = 0;

        /* Deal damage */
        if (damage > 0) {
            enemy_damage(e, damage);
        }
    }
}
