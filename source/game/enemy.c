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
#include "engine/video.h"
#include "engine/rng.h"
#include "game/itemdrop.h"
#include "game/player.h"
#include "game/bugbounty.h"
#include "game/hud.h"
#include "game/particle.h"
#include "game/abilities.h"
#include "game/quest.h"

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
    /* DRONE:   low HP, fast swarm */
    { 4, 3, 8, 8, 3, 80 },
    /* TURRET:  high ATK, stationary aimed */
    { 8, 8, 12, 12, 7, 128 },
    /* MIMIC:   medium HP, disguised */
    { 10, 6, 14, 14, 10, 16 },
    /* CORRUPTOR: ranged, corruption shots */
    { 8, 4, 12, 12, 8, 96 },
    /* GHOST:   low HP, phases through walls */
    { 5, 4, 12, 12, 6, 80 },
    /* BOMBER:  medium HP, aerial bombs */
    { 7, 5, 12, 12, 8, 96 },
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
    s16 max_hp;       /* Actual max HP at spawn (after NG+/bounty scaling) */
} EnemyAI;

#define MAX_ENEMY_SLOTS 24
static EnemyAI enemy_ai[MAX_ENEMY_SLOTS];
static int slot_to_entity[MAX_ENEMY_SLOTS]; /* Entity pool index per enemy slot */
static s8 entity_slot_map[MAX_ENTITIES];    /* Reverse lookup: entity index → slot (-1 if none) */
static int total_kills;
static int chase_transitions;
static int spawn_grace_timer; /* Frames until enemies can chase after area load */

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
 * 0=transparent, 1=outline/deep shadow, 2=body shadow, 3=body mid,
 * 4=body highlight, 5=glow/eye dim, 6=glow/eye mid, 7=glow/eye bright,
 * 8=panel/accent dark, 9=panel/accent mid, A=panel/accent bright,
 * B=tech dark, C=tech bright, D=AA edge, E=secondary highlight, F=white
 *
 * Palette bank mapping (avoids collisions per act composition table):
 *   Sentry→1, Patrol→2, Flyer→3, Shield→4, Spike→5, Hunter→6,
 *   Drone→9, Turret→10, Mimic→11, Corruptor→12, Ghost→1(shares w/Sentry,
 *   never co-appear), Bomber→13 */
static const u8 enemy_pal_bank[ENEMY_TYPE_COUNT] = {
    1,  /* SENTRY */
    2,  /* PATROL */
    3,  /* FLYER */
    4,  /* SHIELD */
    5,  /* SPIKE */
    6,  /* HUNTER */
    9,  /* DRONE */
    10, /* TURRET */
    11, /* MIMIC */
    12, /* CORRUPTOR */
    1,  /* GHOST — shares bank 1 with Sentry (never coexist) */
    13, /* BOMBER */
};

/* Sentry: Military olive-green + menacing red targeting laser */
static const u16 pal_sentry[16] = {
    0,
    RGB15_C(2,4,2),    /* 1: deep olive outline */
    RGB15_C(6,12,6),   /* 2: dark olive body */
    RGB15_C(10,18,10), /* 3: mid olive body */
    RGB15_C(16,24,16), /* 4: bright olive highlight */
    RGB15_C(24,4,4),   /* 5: targeting dim red */
    RGB15_C(31,10,6),  /* 6: targeting mid red */
    RGB15_C(31,22,14), /* 7: targeting bright */
    RGB15_C(4,8,4),    /* 8: panel dark */
    RGB15_C(8,16,8),   /* 9: panel mid */
    RGB15_C(12,20,12), /* A: panel bright */
    RGB15_C(3,10,4),   /* B: tech dark */
    RGB15_C(8,22,10),  /* C: tech bright */
    RGB15_C(3,6,3),    /* D: AA edge */
    RGB15_C(20,28,20), /* E: secondary highlight */
    RGB15_C(31,31,31), /* F: white */
};
/* Patrol: Bronze/copper armored guard + amber visor */
static const u16 pal_patrol[16] = {
    0,
    RGB15_C(4,2,1),    /* 1: dark bronze outline */
    RGB15_C(14,8,3),   /* 2: dark copper */
    RGB15_C(22,14,5),  /* 3: mid bronze */
    RGB15_C(28,20,8),  /* 4: bright bronze highlight */
    RGB15_C(24,16,2),  /* 5: amber visor dim */
    RGB15_C(30,24,6),  /* 6: amber visor mid */
    RGB15_C(31,30,14), /* 7: amber visor bright */
    RGB15_C(10,6,2),   /* 8: armor joint dark */
    RGB15_C(18,12,4),  /* 9: armor joint mid */
    RGB15_C(24,18,8),  /* A: armor joint highlight */
    RGB15_C(10,5,1),   /* B: undersuit dark */
    RGB15_C(18,10,3),  /* C: undersuit bright */
    RGB15_C(6,3,1),    /* D: AA edge */
    RGB15_C(26,22,12), /* E: warm highlight */
    RGB15_C(31,31,31), /* F: white */
};
/* Flyer: Teal-green wings + vivid yellow-green bioluminescent eyes */
static const u16 pal_flyer[16] = {
    0,
    RGB15_C(1,4,3),    /* 1: deep teal outline */
    RGB15_C(4,12,8),   /* 2: dark teal wing */
    RGB15_C(8,20,14),  /* 3: mid teal wing */
    RGB15_C(14,26,20), /* 4: bright teal highlight */
    RGB15_C(18,24,4),  /* 5: bio-eye dim yellow-green */
    RGB15_C(26,30,8),  /* 6: bio-eye mid */
    RGB15_C(31,31,16), /* 7: bio-eye bright */
    RGB15_C(3,10,6),   /* 8: wing membrane dark */
    RGB15_C(6,16,10),  /* 9: wing membrane mid */
    RGB15_C(10,22,16), /* A: wing membrane highlight */
    RGB15_C(2,8,5),    /* B: vein dark */
    RGB15_C(6,16,10),  /* C: vein bright */
    RGB15_C(2,6,4),    /* D: AA edge */
    RGB15_C(20,28,24), /* E: specular highlight */
    RGB15_C(31,31,31), /* F: white */
};
/* Shield: Heavy steel-blue armor + electric blue energy barrier */
static const u16 pal_shield[16] = {
    0,
    RGB15_C(2,3,6),    /* 1: dark steel outline */
    RGB15_C(6,8,14),   /* 2: dark steel body */
    RGB15_C(12,14,22), /* 3: mid steel body */
    RGB15_C(20,22,28), /* 4: bright steel highlight */
    RGB15_C(4,10,28),  /* 5: barrier dim blue */
    RGB15_C(8,18,31),  /* 6: barrier mid blue */
    RGB15_C(18,26,31), /* 7: barrier bright */
    RGB15_C(4,5,10),   /* 8: armor plate dark */
    RGB15_C(8,10,16),  /* 9: armor plate mid */
    RGB15_C(14,16,22), /* A: armor plate highlight */
    RGB15_C(3,6,12),   /* B: rivet dark */
    RGB15_C(10,14,24), /* C: rivet bright */
    RGB15_C(3,4,8),    /* D: AA edge */
    RGB15_C(24,26,30), /* E: polished highlight */
    RGB15_C(31,31,31), /* F: white */
};
/* Spike: Hot orange hazard spines + vivid yellow warning glow */
static const u16 pal_spike[16] = {
    0,
    RGB15_C(6,2,0),    /* 1: deep red-brown outline */
    RGB15_C(18,6,0),   /* 2: dark orange */
    RGB15_C(26,12,2),  /* 3: mid hot orange */
    RGB15_C(31,18,4),  /* 4: bright orange highlight */
    RGB15_C(28,24,4),  /* 5: warning yellow dim */
    RGB15_C(31,28,10), /* 6: warning yellow mid */
    RGB15_C(31,31,18), /* 7: warning yellow bright */
    RGB15_C(14,4,0),   /* 8: spine shadow */
    RGB15_C(22,8,0),   /* 9: spine mid */
    RGB15_C(28,14,2),  /* A: spine highlight */
    RGB15_C(12,3,0),   /* B: base dark */
    RGB15_C(20,6,0),   /* C: base bright */
    RGB15_C(8,2,0),    /* D: AA edge */
    RGB15_C(31,24,8),  /* E: hot glow */
    RGB15_C(31,31,31), /* F: white */
};
/* Hunter: Dark violet predator + blazing magenta tracking eye */
static const u16 pal_hunter[16] = {
    0,
    RGB15_C(4,1,6),    /* 1: deep purple outline */
    RGB15_C(10,4,16),  /* 2: dark violet body */
    RGB15_C(16,8,24),  /* 3: mid violet body */
    RGB15_C(24,14,30), /* 4: bright violet highlight */
    RGB15_C(28,4,20),  /* 5: magenta eye dim */
    RGB15_C(31,12,28), /* 6: magenta eye mid */
    RGB15_C(31,22,31), /* 7: magenta eye bright */
    RGB15_C(8,3,12),   /* 8: cloak shadow */
    RGB15_C(14,6,20),  /* 9: cloak mid */
    RGB15_C(20,10,26), /* A: cloak highlight */
    RGB15_C(6,2,10),   /* B: blade dark */
    RGB15_C(14,6,22),  /* C: blade bright */
    RGB15_C(5,2,8),    /* D: AA edge */
    RGB15_C(28,18,31), /* E: energy trail */
    RGB15_C(31,31,31), /* F: white */
};


/* ---- New enemy type palettes ---- */

static const u16 pal_drone[16] = { 0x0000, RGB15_C(3,3,4), RGB15_C(8,9,10), RGB15_C(14,15,16), RGB15_C(20,21,22), RGB15_C(22,18,4), RGB15_C(28,24,8), RGB15_C(31,30,14), RGB15_C(10,10,12), RGB15_C(16,16,18), RGB15_C(22,22,24), RGB15_C(6,8,10), RGB15_C(12,14,18), RGB15_C(5,5,6), RGB15_C(14,14,16), RGB15_C(31,31,31) };
static const u16 pal_turret[16] = { 0x0000, RGB15_C(2,3,6), RGB15_C(6,10,16), RGB15_C(12,16,22), RGB15_C(18,22,28), RGB15_C(4,20,24), RGB15_C(8,26,30), RGB15_C(16,30,31), RGB15_C(24,8,4), RGB15_C(30,16,8), RGB15_C(31,24,12), RGB15_C(4,8,14), RGB15_C(8,14,22), RGB15_C(3,5,10), RGB15_C(10,14,20), RGB15_C(31,31,31) };
static const u16 pal_mimic[16] = { 0x0000, RGB15_C(4,3,2), RGB15_C(10,7,4), RGB15_C(18,14,8), RGB15_C(26,22,14), RGB15_C(24,4,4), RGB15_C(30,12,8), RGB15_C(31,24,16), RGB15_C(14,10,6), RGB15_C(20,16,10), RGB15_C(26,22,16), RGB15_C(8,6,3), RGB15_C(24,20,10), RGB15_C(6,5,3), RGB15_C(16,12,8), RGB15_C(31,31,31) };
static const u16 pal_corruptor[16] = { 0x0000, RGB15_C(4,2,6), RGB15_C(10,4,14), RGB15_C(16,8,20), RGB15_C(22,14,26), RGB15_C(24,4,20), RGB15_C(30,12,28), RGB15_C(31,24,31), RGB15_C(8,2,12), RGB15_C(14,6,18), RGB15_C(20,10,24), RGB15_C(6,2,10), RGB15_C(12,4,16), RGB15_C(5,3,8), RGB15_C(14,8,18), RGB15_C(31,31,31) };
static const u16 pal_ghost[16] = { 0x0000, RGB15_C(6,8,14), RGB15_C(12,16,24), RGB15_C(20,24,30), RGB15_C(26,28,31), RGB15_C(4,8,20), RGB15_C(8,14,28), RGB15_C(16,22,31), RGB15_C(10,12,18), RGB15_C(16,18,24), RGB15_C(22,24,28), RGB15_C(8,10,16), RGB15_C(14,16,22), RGB15_C(8,10,18), RGB15_C(18,20,26), RGB15_C(31,31,31) };
static const u16 pal_bomber[16] = { 0x0000, RGB15_C(3,4,3), RGB15_C(8,10,8), RGB15_C(14,16,14), RGB15_C(20,22,20), RGB15_C(4,18,22), RGB15_C(8,24,28), RGB15_C(16,30,31), RGB15_C(24,12,4), RGB15_C(30,20,8), RGB15_C(31,26,14), RGB15_C(6,8,6), RGB15_C(10,12,10), RGB15_C(5,6,5), RGB15_C(12,14,12), RGB15_C(31,31,31) };

/* ---- New enemy sprites (4 frames each, 4 tiles per frame) ---- */

/* ========== NEW ENEMY TILES (6 types x 4 frames) ========== */
/* Generated by enemy_sprites.py */

static const u32 spr_drone[4][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x000012D0, 0x11113410, 0x331D0000, 0x55310000, 0x44310000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00D21000, 0x0D431111, 0x0000D133, 0x00001376, 0x00001343 },
    { 0x331D0000, 0x11D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000D133, 0x00000D11, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x000001D0, 0x00001200, 0x0001D000, 0x11D10000, 0x331D0000, 0x55310000, 0x44310000 },
    { 0x00000000, 0x00000000, 0x0D000000, 0x0000D100, 0x000001D1, 0x0000D133, 0x00001376, 0x00001343 },
    { 0x331D0000, 0x11D00000, 0x00010000, 0x000000D0, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000D133, 0x000000D1, 0x00000210, 0x0D012000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x21000000, 0x1D000000, 0xD1000000, 0x11000000, 0x11D00000, 0x331D0000, 0x55310000, 0x44310000 },
    { 0x00000001, 0x0000000D, 0x00000001, 0x00000001, 0x000000D1, 0x0000D133, 0x00001376, 0x00001343 },
    { 0x331D0000, 0x11D00000, 0x11000000, 0xD1D00000, 0x1D000000, 0x21000000, 0x00000000, 0x00000000 },
    { 0x0000D133, 0x000000D1, 0x00000001, 0x00000001, 0x0000000D, 0x00000001, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x00000000, 0x000000D0, 0x001D0000, 0x1D100000, 0x331D0000, 0x55310000, 0x44310000 },
    { 0x00000000, 0x0D100000, 0x00021000, 0x000001D0, 0x0000001D, 0x0000D133, 0x00001376, 0x00001343 },
    { 0x331D0000, 0x11D00000, 0x00120000, 0x000010D0, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000D133, 0x000000D1, 0x00000100, 0x00D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_turret[4][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x111D0000, 0x65310000, 0x44310000, 0x43310000, 0x4331D000, 0x4331D000 },
    { 0x00000000, 0x00000000, 0x000000D1, 0x000000D7, 0x000000D3, 0x0000D113, 0x000DBB13, 0x000DCB13 },
    { 0x4331D000, 0x111D0000, 0x3321D000, 0x98981D00, 0x898981D0, 0x9898981D, 0x1111111D, 0x00000000 },
    { 0x0000D113, 0x000000D1, 0x00000D12, 0x0000D198, 0x000D1989, 0x00D19898, 0x00D11111, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x111D0000, 0x65310000, 0x44310000, 0x43310000, 0x4331D000, 0x4331D000 },
    { 0x00000000, 0x00000000, 0x000000D1, 0x000000D7, 0x0000D0D3, 0x000DB113, 0x000DCBB3, 0x00000D13 },
    { 0x4331D000, 0x111D0000, 0x3321D000, 0x98981D00, 0x898981D0, 0x9898981D, 0x1111111D, 0x00000000 },
    { 0x0000D113, 0x000000D1, 0x00000D12, 0x0000D198, 0x000D1989, 0x00D19898, 0x00D11111, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000000, 0x111D0000, 0x65310000, 0x44310000, 0x43310000, 0x4331D000, 0x4331D000 },
    { 0x00000000, 0x00000000, 0x000000D1, 0x000000DB, 0x0000DCB3, 0x0000DB13, 0x00000D13, 0x00000D13 },
    { 0x4331D000, 0x111D0000, 0x3321D000, 0x98981D00, 0x898981D0, 0x9898981D, 0x1111111D, 0x00000000 },
    { 0x0000D113, 0x000000D1, 0x00000D12, 0x0000D198, 0x000D1989, 0x00D19898, 0x00D11111, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x00000000, 0x111D0000, 0x65310000, 0x44310000, 0x43310000, 0x4331D000, 0x4331D000 },
    { 0x00000000, 0x00000000, 0x000000D1, 0x000000D7, 0x000000D3, 0x000000D3, 0x000000D3, 0x000DBB13 },
    { 0x4331D000, 0x111D0000, 0x3321D000, 0x98981D00, 0x898981D0, 0x9898981D, 0x1111111D, 0x00000000 },
    { 0x00DCBD13, 0x000000D1, 0x00000D12, 0x0000D198, 0x000D1989, 0x00D19898, 0x00D11111, 0x00000000 },
  },
};

static const u32 spr_mimic[4][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11110000, 0x34341000, 0x34341000, 0x7C111000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00001111, 0x000D1434, 0x000D1434, 0x000D11C7 },
    { 0x43431000, 0x43431000, 0x32321000, 0x11111000, 0xDDDD0000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x000D1343, 0x000D1343, 0x000D1232, 0x000D1111, 0x0000DDDD, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x00000000, 0x44111000, 0x34341000, 0x7C111000, 0x53431D00, 0x43431000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00001111, 0x00001343, 0x000011C7, 0x000D1345, 0x000D1343 },
    { 0x323210D0, 0x111110D0, 0xDDDDD01D, 0x000000D1, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0D0D1232, 0x0D0D1111, 0xD10DDDDD, 0x01D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000000, 0x11D00000, 0x651D0000, 0x331D0000, 0x3321D000, 0x3321D01D, 0x3321DD10 },
    { 0x00000000, 0x00000000, 0x00000D11, 0x0000D137, 0x0000D133, 0x000D1233, 0xD1D01233, 0x01DD1233 },
    { 0x2221D100, 0x222101D0, 0x221D001D, 0x11D000D1, 0x0D000D10, 0x0000D100, 0x00000000, 0x00000000 },
    { 0x001D1222, 0x0D1D0122, 0xD10DD122, 0x1D000D11, 0x01D000D0, 0x0001D000, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x1D000000, 0x51D00000, 0x651D0000, 0x331D0000, 0x3321D0D0, 0x3321D01D, 0x3321D0D1 },
    { 0x00000000, 0x000000D1, 0x00000D17, 0x0000D137, 0x0000D133, 0x0D0D1233, 0xD1D01233, 0x10D01233 },
    { 0x2221D010, 0x2221D0D0, 0x2221D01D, 0x111D00D1, 0x00D000D0, 0x00000D00, 0x00000000, 0x00000000 },
    { 0x010D1222, 0x00D01222, 0xD10DD122, 0x01D00D11, 0x00D000D0, 0x0000D000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_corruptor[4][4][8] = {
  { /* frame 0 */
    { 0xD0000000, 0x1000D000, 0x221D0000, 0x3321D000, 0x43321D00, 0x65431D00, 0x654321D0, 0x543321D0 },
    { 0x00000D00, 0x00D0000D, 0x000000D1, 0x00000012, 0x00000D13, 0x00D01347, 0x00001237, 0x00001236 },
    { 0x43321D00, 0x23321D00, 0x1221D000, 0xD11D0000, 0x0D000D00, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00D00123, 0x0000D012, 0x0000000D, 0x00D00000, 0x00000000, 0x00000D00, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00D00000, 0x21D0D000, 0x321D0000, 0x3321D000, 0x5431D000, 0x65321D00, 0x54321D00 },
    { 0x00000000, 0x0000D00D, 0x0000D012, 0x000D0123, 0x000D1234, 0x000D1356, 0xD0012367, 0x0D012365 },
    { 0x4321D000, 0x3321D000, 0x221D0000, 0x11D00000, 0x0D000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0D011234, 0x000D1122, 0x00D0D011, 0x00000D0D, 0x00000000, 0x00D00000, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000D00, 0x1D0D0000, 0x3321D000, 0x43321D00, 0x65431D00, 0x754321D0, 0x543321D0 },
    { 0x00000000, 0x000000D0, 0x0000D122, 0x000D0123, 0x000D1234, 0x000D1347, 0xD0012347, 0x000D1236 },
    { 0x433221D0, 0x23321D00, 0x1221D000, 0xD11D0D00, 0x00D000D0, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00001123, 0x0000D011, 0x0000D0D0, 0x00000000, 0x00D00000, 0x000000D0, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x0000D000, 0x1D0D00D0, 0x221D0000, 0x33321D00, 0x44321D00, 0x654321D0, 0x7654321D, 0x543321D0 },
    { 0x000000D0, 0x000D0012, 0x0000D013, 0x00000D12, 0x00D00123, 0x00001235, 0x0D001234, 0x0D001236 },
    { 0x43321D00, 0x3321D000, 0x221D0000, 0xD1D00000, 0x000D0000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000D123, 0x0000D012, 0x00D000D1, 0x000D0000, 0x0000D000, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_ghost[4][4][8] = {
  { /* frame 0 */
    { 0x11D00000, 0x331D0000, 0x4431D000, 0x5651D000, 0x4431D000, 0x331D0000, 0x3331D000, 0x3321D000 },
    { 0x0000000D, 0x000000D1, 0x00000D13, 0x00000D16, 0x00000D13, 0x000000D1, 0x00000D13, 0x00000D12 },
    { 0x331D0000, 0x33D00000, 0x3D000000, 0xD0000000, 0x0D000000, 0x00D00000, 0x00000000, 0x00000000 },
    { 0x000000D1, 0x0000000D, 0x0000000D, 0x00000000, 0x0000000D, 0x000000D0, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x11D00000, 0x331D0000, 0x4431D000, 0x5651D000, 0x4431D000, 0x331D0000, 0x3331D000 },
    { 0x00000000, 0x0000000D, 0x000000D1, 0x00000D13, 0x00000D16, 0x00000D13, 0x000000D1, 0x00000D13 },
    { 0x3321D000, 0x331D0000, 0x33D00000, 0xD0000000, 0x0D000000, 0xD0000000, 0x00D00000, 0x00000000 },
    { 0x00000D12, 0x000000D1, 0x0000000D, 0x00000000, 0x00000000, 0x000000D0, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000000, 0x11D00000, 0x331D0000, 0x4431D000, 0x5651D000, 0x4431D000, 0x331D0000 },
    { 0x00000000, 0x00000000, 0x0000000D, 0x000000D1, 0x00000D13, 0x00000D16, 0x00000D13, 0x000000D1 },
    { 0x3331D000, 0x3321D000, 0x331D0000, 0xD3D00000, 0x0D000000, 0x00D00000, 0x0D000000, 0x00000000 },
    { 0x00000D13, 0x00000D12, 0x000000D1, 0x00000000, 0x00000000, 0x0000000D, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x11D00000, 0x331D0000, 0x4431D000, 0x5651D000, 0x4431D000, 0x331D0000, 0x3331D000 },
    { 0x00000000, 0x0000000D, 0x000000D1, 0x00000D13, 0x00000D16, 0x00000D13, 0x000000D1, 0x00000D13 },
    { 0x3321D000, 0x331D0000, 0x3D000000, 0xD0D00000, 0x0D000000, 0x000D0000, 0xD0000000, 0x00000000 },
    { 0x00000D12, 0x000000D1, 0x0000000D, 0x00000000, 0x0000000D, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_bomber[4][4][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0x65100000, 0x43110000, 0x443311D0, 0x4433321D },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x000000D7, 0x0000D134, 0x0D111344, 0x0D123344 },
    { 0x44321D10, 0x3311D100, 0xD91D0000, 0xD8D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00D11234, 0x00D1D113, 0x00000D19, 0x000000D8, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x000001D0, 0x11001D10, 0x6510D100, 0x4311D000, 0x44331000, 0x44321000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x000000D7, 0x0000D134, 0x00D11344, 0x00D12344 },
    { 0x4321D000, 0x311D0000, 0xD91D0000, 0xD8D00000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0D1D2344, 0x0D1D0D13, 0x00000D19, 0x000000D8, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 2 */
    { 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0x65100000, 0x43110000, 0x4331D100, 0x4321D100 },
    { 0x00000000, 0x00000000, 0x0D100000, 0x00D10001, 0x000D10D7, 0x0000D134, 0x00013444, 0x00013444 },
    { 0x4321D1D0, 0x31D0D1D0, 0x91D00000, 0x8D000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x000D1234, 0x00000D13, 0x0000D19D, 0x00000D8D, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 3 */
    { 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0x65100000, 0x43110000, 0x443311D0, 0x4433321D },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x000000D7, 0x0000D134, 0x0D111344, 0x0D123344 },
    { 0x44321D10, 0x3311D100, 0xD91D0000, 0xD8D00000, 0x1D000000, 0x80000000, 0x10000000, 0x00000000 },
    { 0x00D11234, 0x00D1D113, 0x00000D19, 0x000000D8, 0x000000D1, 0x00000008, 0x00000001, 0x00000000 },
  },
};


/* Sprite frame counts per type (spike has 3, all others have 4) */
static const u8 enemy_frame_count[ENEMY_TYPE_COUNT] = {
    4, 4, 4, 4, 3, 4,  /* Original 6 */
    4, 4, 4, 4, 4, 4,  /* New 6 */
};

/* Sprite data pointers per type */
static const u32* const enemy_sprites[ENEMY_TYPE_COUNT] = {
    (const u32*)spr_sentry,
    (const u32*)spr_patrol,
    (const u32*)spr_flyer,
    (const u32*)spr_shield,
    (const u32*)spr_spike,
    (const u32*)spr_hunter,
    (const u32*)spr_drone,
    (const u32*)spr_turret,
    (const u32*)spr_mimic,
    (const u32*)spr_corruptor,
    (const u32*)spr_ghost,
    (const u32*)spr_bomber,
};
static const u16* const enemy_palettes[ENEMY_TYPE_COUNT] = {
    pal_sentry, pal_patrol, pal_flyer, pal_shield, pal_spike, pal_hunter,
    pal_drone, pal_turret, pal_mimic, pal_corruptor, pal_ghost, pal_bomber,
};

/* OBJ tile base per enemy type (loaded dynamically) */
/* Enemy tiles start at OBJ tile 48 (after player's 48 tiles) */
#define ENEMY_TILE_BASE 48
/* Each enemy type: 4 frames x 4 tiles = 16 tiles */
/* OBJ palette banks: per-type via enemy_pal_bank[] lookup table */

void enemy_init(void) {
    total_kills = 0;
    chase_transitions = 0;
    spawn_grace_timer = 0;
    for (int i = 0; i < MAX_ENEMY_SLOTS; i++) {
        slot_to_entity[i] = -1;
        enemy_ai[i].state = ESTATE_IDLE;
    }
    for (int i = 0; i < MAX_ENTITIES; i++) {
        entity_slot_map[i] = -1;
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
    if (idx >= 0 && idx < MAX_ENTITIES) return (int)entity_slot_map[idx];
    return -1;
}

Entity* enemy_spawn(int subtype, int tile_x, int tile_y, int tier) {
    if (subtype < 0 || subtype >= ENEMY_TYPE_COUNT) return NULL;

    int slot = find_free_slot();
    if (slot < 0) return NULL;

    Entity* e = entity_spawn(ENT_ENEMY);
    if (!e) return NULL;

    e->subtype = (u8)subtype;
    e->x = tile_x * 8 * 256; /* tile -> 8.8 px */
    e->y = tile_y * 8 * 256;
    e->width = enemy_info[subtype].width;
    e->height = enemy_info[subtype].height;
    {
        int base_hp = enemy_info[subtype].hp + tier * 3;
        /* NG+ scaling: +10%/+25%/+40% per NG+ cycle */
        if (game_stats.ng_plus == 1) base_hp = base_hp * 11 / 10;
        else if (game_stats.ng_plus == 2) base_hp = base_hp * 5 / 4;
        else if (game_stats.ng_plus >= 3) base_hp = base_hp * 7 / 5;
        /* Threat level scaling: +10% HP per threat level */
        if (game_stats.bb_threat_level > 0)
            base_hp = base_hp * (10 + game_stats.bb_threat_level) / 10;
        e->hp = (s16)base_hp;
    }
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

    /* Load palette into type-specific bank, tinted per act */
    int pal_bank = enemy_pal_bank[subtype];
    memcpy16(&pal_obj_mem[pal_bank * 16], enemy_palettes[subtype], 16);
    /* Per-act color shift — subtle tint on palette entries 1-5 */
    {
        int act = (int)quest_state.current_act;
        int r_shift = 0, g_shift = 0, b_shift = 0;
        switch (act) {
        case 0: r_shift = 0; g_shift = 1; b_shift = 2; break; /* Cyan */
        case 1: r_shift = 0; g_shift = 0; b_shift = 2; break; /* Blue */
        case 2: r_shift = 0; g_shift = 2; b_shift = 0; break; /* Green */
        case 3: r_shift = 2; g_shift = 1; b_shift = 0; break; /* Orange */
        case 4: r_shift = 1; g_shift = 0; b_shift = 1; break; /* Purple */
        case 5: r_shift = 2; g_shift = 0; b_shift = 0; break; /* Red */
        default: break;
        }
        for (int c = 1; c < 6; c++) {
            u16 col = pal_obj_mem[pal_bank * 16 + c];
            int r = (col & 0x1F) + r_shift;
            int g = ((col >> 5) & 0x1F) + g_shift;
            int b = ((col >> 10) & 0x1F) + b_shift;
            if (r > 31) r = 31;
            if (g > 31) g = 31;
            if (b > 31) b = 31;
            pal_obj_mem[pal_bank * 16 + c] = (u16)(r | (g << 5) | (b << 10));
        }
    }

    /* Setup AI */
    int ent_idx = (int)(e - entity_get(0));
    slot_to_entity[slot] = ent_idx;
    if (ent_idx >= 0 && ent_idx < MAX_ENTITIES)
        entity_slot_map[ent_idx] = (s8)slot;
    enemy_ai[slot].state = ESTATE_SPAWN;
    enemy_ai[slot].state_timer = 0;
    enemy_ai[slot].shoot_timer = 0;
    enemy_ai[slot].tier = (u8)tier;
    enemy_ai[slot].home_x = (s16)(e->x >> 8);
    enemy_ai[slot].patrol_dir = 1;
    enemy_ai[slot].scaled_atk = (s16)(enemy_info[subtype].atk + tier); /* Scale ATK with tier */
    enemy_ai[slot].max_hp = e->hp; /* Snapshot max HP before further scaling */

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
        if (e->hp * 10 < ai->max_hp * 3) {
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
        int frenzied = (e->hp * 2 < ai->max_hp);
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

/* ---- New enemy AI functions ---- */

static void ai_drone(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Fast erratic swarm movement — sine wave approach */
    int dx = (int)((player_x - e->x) >> 8);
    int dy = (int)((player_y - e->y) >> 8);
    e->facing = (u8)((dx < 0) ? 1 : 0);
    int dist = dx < 0 ? -dx : dx;

    if (dist < enemy_info[ENEMY_DRONE].detection_range) {
        ai->state = ESTATE_CHASE;
        /* Erratic: oscillate vertically while pursuing horizontally */
        e->vx = (s16)(dx > 0 ? 192 : -192);
        ai->state_timer++;
        int sine_ofs = ((ai->state_timer & 16) > 8) ? 128 : -128;
        e->vy = (s16)(sine_ofs + (dy > 0 ? 64 : -64));
    } else {
        ai->state = ESTATE_IDLE;
        e->vx = 0;
        e->vy = 0;
    }
}

static void ai_turret_enemy(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    (void)player_y;
    /* Stationary, fires aimed projectile */
    int dx = (int)((player_x - e->x) >> 8);
    int dist = dx < 0 ? -dx : dx;
    e->facing = (u8)((dx < 0) ? 1 : 0);
    e->vx = 0;

    if (dist < enemy_info[ENEMY_TURRET].detection_range) {
        ai->state = ESTATE_ATTACK;
        ai->shoot_timer++;
        if (ai->shoot_timer >= 90) {
            ai->shoot_timer = 0;
            /* Fire aimed shot */
            int ent_idx = (int)(e - entity_get(0));
            int proj_vx = e->facing ? -512 : 512;
            projectile_spawn(
                e->x + (e->facing ? -8 * 256 : 8 * 256),
                e->y,
                (s16)proj_vx, 0, (s16)ai->scaled_atk, SUBTYPE_PROJ_ENEMY,
                PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT);
        }
    } else {
        ai->state = ESTATE_IDLE;
    }
    /* Gravity */
    e->vy += 32;
    if (e->vy > 512) e->vy = 512;
}

static void ai_mimic(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Disguised until player gets close, then attacks */
    int dx = (int)((player_x - e->x) >> 8);
    int dy = (int)((player_y - e->y) >> 8);
    int dist = dx < 0 ? -dx : dx;

    if (ai->state == ESTATE_IDLE && dist < 16 && (dy < 16 && dy > -16)) {
        /* Reveal! Surprise transform with dramatic effects */
        ai->state = ESTATE_CHASE;
        audio_play_sfx(SFX_BOSS_PHASE);
        video_shake(3, 1);
        /* Shatter-transform burst */
        particle_burst(e->x + ((s32)e->width << 7), e->y + ((s32)e->height << 7),
                       3, PART_BURST, 200, 12);
        hud_notify("MIMIC!", 30);
    }

    if (ai->state == ESTATE_CHASE) {
        e->facing = (u8)((dx < 0) ? 1 : 0);
        e->vx = e->facing ? -192 : 192;
        /* Jump toward player */
        if (e->on_ground && ai->shoot_timer == 0) {
            e->vy = -384;
            e->on_ground = 0;
            ai->shoot_timer = 45;
        }
    } else {
        e->vx = 0;
    }

    if (ai->shoot_timer > 0) ai->shoot_timer--;
    e->vy += 32;
    if (e->vy > 512) e->vy = 512;
}

static void ai_corruptor(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    (void)player_y;
    /* Ranged attacker that fires corrupting projectiles */
    int dx = (int)((player_x - e->x) >> 8);
    int dist = dx < 0 ? -dx : dx;
    e->facing = (u8)((dx < 0) ? 1 : 0);

    if (dist < enemy_info[ENEMY_CORRUPTOR].detection_range) {
        ai->state = ESTATE_ATTACK;
        /* Retreat if too close */
        if (dist < 24) {
            e->vx = e->facing ? 128 : -128; /* Run away */
        } else {
            e->vx = 0;
        }
        /* Fire every 60 frames */
        ai->shoot_timer++;
        if (ai->shoot_timer >= 60) {
            ai->shoot_timer = 0;
            int ent_idx = (int)(e - entity_get(0));
            int proj_vx = e->facing ? -384 : 384;
            projectile_spawn(
                e->x + (e->facing ? -6 * 256 : 6 * 256),
                e->y,
                (s16)proj_vx, 0, (s16)ai->scaled_atk, SUBTYPE_PROJ_ENEMY,
                PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT);
        }
    } else {
        ai->state = ESTATE_IDLE;
        e->vx = 0;
    }
    e->vy += 32;
    if (e->vy > 512) e->vy = 512;
}

static void ai_ghost_enemy(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    /* Phases through walls, sine-wave approach toward player */
    int dx = (int)((player_x - e->x) >> 8);
    int dy = (int)((player_y - e->y) >> 8);
    int dist = dx < 0 ? -dx : dx;
    e->facing = (u8)((dx < 0) ? 1 : 0);

    if (dist < enemy_info[ENEMY_GHOST].detection_range) {
        ai->state = ESTATE_CHASE;
        /* Move directly toward player (ignoring walls via no collision) */
        e->vx = (s16)(dx > 0 ? 128 : -128);
        e->vy = (s16)(dy > 0 ? 96 : -96);
    } else {
        ai->state = ESTATE_IDLE;
        /* Float in place with slight bob */
        ai->state_timer++;
        e->vx = 0;
        e->vy = (s16)(((ai->state_timer & 32) > 16) ? 32 : -32);
    }
    /* Note: ghost skips physics_resolve so it passes through walls */
}

static void ai_bomber(Entity* e, EnemyAI* ai, s32 player_x, s32 player_y) {
    (void)player_y;
    /* Flies above player, drops projectiles downward */
    int dx = (int)((player_x - e->x) >> 8);
    int dist = dx < 0 ? -dx : dx;
    e->facing = (u8)((dx < 0) ? 1 : 0);

    if (dist < enemy_info[ENEMY_BOMBER].detection_range) {
        ai->state = ESTATE_ATTACK;
        /* Fly horizontally toward player but stay above */
        e->vx = (s16)(dx > 0 ? 160 : -160);
        e->vy = -32; /* Float upward slowly */

        /* Drop bomb every 90 frames when roughly above player */
        ai->shoot_timer++;
        if (ai->shoot_timer >= 90 && dist < 16) {
            ai->shoot_timer = 0;
            int ent_idx = (int)(e - entity_get(0));
            projectile_spawn(
                e->x,
                e->y + 8 * 256,
                0, 256, (s16)ai->scaled_atk, SUBTYPE_PROJ_ENEMY,
                PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT);
        }
    } else {
        ai->state = ESTATE_IDLE;
        e->vx = 0;
        e->vy = 0;
    }
}

IWRAM_CODE void enemy_update_all(s32 player_x, s32 player_y) {
    if (spawn_grace_timer > 0) spawn_grace_timer--;
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
                /* Save subtype before despawn (entity cleared on despawn) */
                int dead_subtype = e->subtype;
                /* Despawn — entity_despawn handles sprite_free + oam_index clearing */
                int dead_ent_idx = (int)(e - entity_get(0));
                entity_despawn(e);
                slot_to_entity[slot] = -1;
                if (dead_ent_idx >= 0 && dead_ent_idx < MAX_ENTITIES)
                    entity_slot_map[dead_ent_idx] = -1;
                total_kills++;
                /* Track global stats */
                if (game_stats.total_kills < 65535) game_stats.total_kills++;
                game_stats.current_combo++;
                if (game_stats.current_combo > game_stats.highest_combo) {
                    game_stats.highest_combo = game_stats.current_combo;
                }
                /* Codex: unlock enemy entry on first kill */
                codex_unlock(CODEX_ENEMY_BASE + dead_subtype);
                /* Achievement: First Blood */
                ach_unlock_celebrate(ACH_FIRST_BLOOD);
                if (game_stats.total_kills >= 100) ach_unlock_celebrate(ACH_HUNTER);
                if (game_stats.total_kills >= 500) ach_unlock_celebrate(ACH_EXTERMINATOR);
            }
            continue;
        }

        /* Hit stun */
        if (ai->state == ESTATE_HIT) {
            /* Heavy-hit freeze: state_timer starts at 254, count down to 252 first */
            if (ai->state_timer >= 252) {
                ai->state_timer--;
                if (ai->state_timer < 252) ai->state_timer = 0; /* freeze done, begin normal hitstun */
            } else {
                ai->state_timer++;
            }
            if (ai->state_timer > 0 && ai->state_timer < 252 && ai->state_timer >= 12) {
                /* Mimics stay aggressive after reveal (IDLE = disguised) */
                ai->state = (e->subtype == ENEMY_MIMIC) ? ESTATE_CHASE : ESTATE_IDLE;
                ai->state_timer = 0;
            }
            /* Apply gravity during hit stun (prevents floating) */
            e->vy += 32;
            if (e->vy > 512) e->vy = 512;
            /* Apply knockback with collision resolution */
            e->x += e->vx;
            physics_resolve_x(e);
            e->y += e->vy;
            physics_resolve_y(e);
            continue;
        }

        /* Spawn-in materialization (20-frame flicker) */
        if (ai->state == ESTATE_SPAWN) {
            ai->state_timer++;
            if (ai->state_timer >= 20) {
                ai->state = ESTATE_IDLE;
                ai->state_timer = 0;
                /* Spawn-in complete: sparkle burst */
                particle_burst(e->x + ((s32)e->width << 7),
                               e->y + ((s32)e->height << 7),
                               3, PART_STAR, 120, 14);
            }
            continue; /* No AI during spawn-in */
        }

        /* AI per type — track state before for aggro detection */
        int prev_state = ai->state;
        switch (e->subtype) {
        case ENEMY_SENTRY: ai_sentry(e, ai, player_x, player_y); break;
        case ENEMY_PATROL: ai_patrol(e, ai, player_x, player_y); break;
        case ENEMY_FLYER:  ai_flyer(e, ai, player_x, player_y); break;
        case ENEMY_SHIELD: ai_shield(e, ai, player_x, player_y); break;
        case ENEMY_SPIKE:  ai_spike(e, ai, player_x, player_y); break;
        case ENEMY_HUNTER:    ai_hunter(e, ai, player_x, player_y); break;
        case ENEMY_DRONE:     ai_drone(e, ai, player_x, player_y); break;
        case ENEMY_TURRET:    ai_turret_enemy(e, ai, player_x, player_y); break;
        case ENEMY_MIMIC:     ai_mimic(e, ai, player_x, player_y); break;
        case ENEMY_CORRUPTOR: ai_corruptor(e, ai, player_x, player_y); break;
        case ENEMY_GHOST:     ai_ghost_enemy(e, ai, player_x, player_y); break;
        case ENEMY_BOMBER:    ai_bomber(e, ai, player_x, player_y); break;
        }

        /* Smoke Bomb: enemies lose tracking — revert new chase transitions */
        if (ability_is_smoke_active() && ai->state == ESTATE_CHASE && prev_state != ESTATE_CHASE) {
            ai->state = (u8)prev_state;
        }

        /* Grace timer: prevent chase after area load */
        if (spawn_grace_timer > 0 && ai->state == ESTATE_CHASE && prev_state != ESTATE_CHASE) {
            ai->state = (u8)prev_state;
        }

        /* Aggro indicator: "!" particle when enemy spots player */
        if (prev_state != ESTATE_CHASE && ai->state == ESTATE_CHASE) {
            particle_spawn(e->x + ((s32)e->width << 7),
                           e->y - FP8(8), 0, -48, PART_STAR, 16);
            chase_transitions++;
        }

        /* Time Warp: enemies move at half speed */
        if (ability_is_time_warp_active()) {
            e->vx >>= 1;
            e->vy >>= 1;
        }

        /* Apply velocity with tile collision resolution */
        e->on_ground = 0;
        e->x += e->vx;
        if (e->subtype != ENEMY_GHOST) physics_resolve_x(e);
        e->y += e->vy;
        if (e->subtype != ENEMY_GHOST) physics_resolve_y(e);

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
    int any_dying = 0;
    for (int i = 0; i < hw; i++) {
        Entity* e = entity_get(i);
        if (!e || e->type != ENT_ENEMY) continue;
        if (e->oam_index == OAM_NONE) continue;

        int slot = entity_to_slot(e);
        if (slot < 0) continue;

        int sx = (int)((e->x - cam_x) >> 8);
        int sy = (int)((e->y - cam_y) >> 8);

        /* Off-screen culling */
        if (sx < -16 || sx > SCREEN_W + 16 || sy < -16 || sy > SCREEN_H + 16) {
            OBJ_ATTR* oam = sprite_get(e->oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            continue;
        }

        int tile_id = ENEMY_TILE_BASE + e->subtype * 16 + e->anim_frame * 4;
        int pal_bank = enemy_pal_bank[e->subtype];
        u16 attr1_flip = e->facing ? ATTR1_HFLIP : 0;  /* sprite faces RIGHT natively */

        /* Death effect: mosaic ramp + flicker */
        EnemyAI* ai = &enemy_ai[slot];
        if (ai->state == ESTATE_DEAD) {
            any_dying = 1;
            /* Ramp mosaic: 0→15 over 30 frames */
            int mosaic_size = ai->state_timer / 2;
            if (mosaic_size > 15) mosaic_size = 15;
            video_mosaic_obj(mosaic_size);
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

        /* Spawn-in materialization flicker (visible 2 frames, hidden 1) */
        if (ai->state == ESTATE_SPAWN && (ai->state_timer % 3 == 2)) {
            OBJ_ATTR* oam = sprite_get(e->oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            continue;
        }

        /* Attack telegraph: particles first frame of ESTATE_ATTACK */
        if (ai->state == ESTATE_ATTACK && ai->state_timer < 6) {
            /* Type-specific telegraph particles */
            if (ai->state_timer == 0) {
                switch (e->subtype) {
                case ENEMY_HUNTER:
                    /* Charge-up sparks before lunge */
                    particle_burst(e->x, e->y, 2, PART_SPARK, 120, 8);
                    break;
                case ENEMY_TURRET:
                    /* Charge glow at muzzle */
                    particle_spawn(e->x + (e->facing ? FP8(-8) : FP8(8)),
                                   e->y + FP8(2), 0, 0, PART_STAR, 12);
                    break;
                case ENEMY_BOMBER:
                    /* Downward spark trail on bomb drop */
                    particle_spawn(e->x, e->y + FP8(8), 0, 64, PART_SPARK, 10);
                    break;
                case ENEMY_CORRUPTOR:
                    /* Purple muzzle flash */
                    particle_spawn(e->x + (e->facing ? FP8(-6) : FP8(6)),
                                   e->y, 0, 0, PART_SPARK, 8);
                    break;
                default:
                    break;
                }
            }
        }

        OBJ_ATTR* oam = sprite_get(e->oam_index);
        if (!oam) continue;
        u16 mosaic_bit = (ai->state == ESTATE_DEAD) ? ATTR0_MOSAIC : 0;
        oam->attr0 = (u16)(ATTR0_SQUARE | mosaic_bit | ((u16)sy & 0xFF));
        oam->attr1 = (u16)(ATTR1_SIZE_16 | attr1_flip | ((u16)sx & 0x1FF));
        oam->attr2 = (u16)(ATTR2_ID(tile_id) | ATTR2_PALBANK(pal_bank));
    }

    /* Clear mosaic if no enemies are dying (tracked during main loop above) */
    if (!any_dying) video_mosaic_obj(0);
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
                    int slot = entity_to_slot(e);
                    if (slot < 0) continue;
                    u32 bit = (u32)(1 << slot);
                    if (p->hit_mask & bit) continue;
                    p->hit_mask |= bit;
                }

                /* Shield: block frontal attacks + push player back */
                if (e->subtype == ENEMY_SHIELD) {
                    int from_front = (e->facing && px > ex) || (!e->facing && px < ex);
                    if (from_front) {
                        /* Blocked — deflect projectile with visual feedback */
                        audio_play_sfx(SFX_MENU_BACK);
                        projectile_deactivate(p);
                        /* Block sparks + flash + shake */
                        particle_burst(p->x, p->y, 3, PART_SPARK, 180, 10);
                        video_hit_flash_start(enemy_pal_bank[e->subtype], 2);
                        video_shake(2, 1);
                        /* Push player away from shield */
                        if (player) {
                            s16 kb = (player->x > e->x) ? 96 : -96;
                            player->vx = kb;
                        }
                        continue;
                    }
                }

                /* Critical hit check: base 5% + LCK/2% + skill tree crit bonus */
                {
                    int crit_chance = 5 + player_state.lck / 2;
                    /* Skill tree: offense branch index 0 = crit chance (+3/6/9%) */
                    crit_chance += player_state.skill_tree[0] * 3;
                    int is_crit = ((int)rand_range(100) < crit_chance);
                    int hit_dmg = p->damage;
                    if (is_crit) hit_dmg = hit_dmg * 2; /* Double damage on crit */

                    /* Upload (Technomancer ability): all enemies take 2x damage */
                    if (ability_is_upload_active()) hit_dmg *= 2;

                    /* Backstab (Infiltrator ability): 3x damage from behind */
                    if (ability_is_backstab_active()) {
                        int from_behind = (p->vx > 0 && e->facing == 0) ||
                                          (p->vx < 0 && e->facing == 1);
                        if (from_behind) hit_dmg *= 3;
                    }

                    /* Hit! Apply damage + weapon-type knockback */
                    enemy_damage(e, hit_dmg);
                    {
                        int kb;
                        switch (p->type) {
                        case SUBTYPE_PROJ_CHARGE: kb = 192; break; /* Heavy */
                        case SUBTYPE_PROJ_NOVA:   kb = 160; break;
                        case SUBTYPE_PROJ_RAPID:  kb = 64;  break; /* Light */
                        case SUBTYPE_PROJ_SPREAD: kb = 80;  break;
                        default:                  kb = 128; break;
                        }
                        e->vx = (s16)((p->vx > 0) ? kb : -kb);
                    }
                    total_dmg += hit_dmg;
                    /* Floating damage number */
                    hud_floattext_spawn(e->x, e->y - FP8(4), hit_dmg, is_crit);

                    if (is_crit) {
                        /* Crit visual feedback: particles + shake */
                        video_shake(3, 1);
                        particle_burst(p->x, p->y, 3, PART_STAR, 200, 14);
                        hud_notify("CRIT!", 20);
                        audio_play_sfx(SFX_BOSS_ROAR);
                    }
                    /* Impact spark at collision point */
                    particle_spawn(p->x, p->y, (s16)(-(p->vx >> 2)), -64,
                                   PART_SPARK, 12);
                }

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
    /* Track damage dealt stat */
    if (game_stats.damage_dealt < 65535 - dmg) game_stats.damage_dealt += (u16)dmg;
    else game_stats.damage_dealt = 65535;
    /* Type-specific hit SFX: mechanical/digital/organic */
    if (e->subtype == ENEMY_TURRET || e->subtype == ENEMY_DRONE)
        audio_play_sfx(SFX_ENEMY_HIT_MECH);
    else if (e->subtype == ENEMY_GHOST || e->subtype == ENEMY_MIMIC)
        audio_play_sfx(SFX_ENEMY_HIT_DIGI);
    else
        audio_play_sfx(SFX_ENEMY_HIT);

    /* Hit flash: briefly white-out the enemy's palette bank */
    int pal_bank = enemy_pal_bank[e->subtype];
    int flash_dur = (dmg >= 10) ? 4 : 2; /* Extended flash on heavy hits */
    video_hit_flash_start(pal_bank, flash_dur);

    /* Knockback */
    int slot = entity_to_slot(e);
    if (slot >= 0) {
        enemy_ai[slot].state = ESTATE_HIT;
        /* Heavy hit hitstop: use 254 as sentinel for 2 extra freeze frames */
        enemy_ai[slot].state_timer = (u8)((dmg >= 10) ? 254 : 0);
    }
    /* Push away from damage source — stationary enemies resist knockback */
    int kb;
    if (e->subtype == ENEMY_TURRET) {
        kb = 32; /* Turrets barely move */
    } else {
        kb = (dmg >= 10) ? 192 : 128;
    }
    e->vx = (s16)(e->facing ? kb : -kb);
    e->vy = (e->subtype == ENEMY_TURRET) ? 0 : -64;

    if (e->hp <= 0) {
        if (e->subtype == ENEMY_TURRET || e->subtype == ENEMY_DRONE)
            audio_play_sfx(SFX_ENEMY_DIE_MECH);
        else if (e->subtype == ENEMY_GHOST || e->subtype == ENEMY_MIMIC)
            audio_play_sfx(SFX_ENEMY_DIE_DIGI);
        else
            audio_play_sfx(SFX_ENEMY_DIE);
        /* Death burst particles — scales with enemy tier */
        {
            s32 cx = e->x + ((s32)e->width << 7);
            s32 cy = e->y + ((s32)e->height << 7);
            int tier = (slot >= 0) ? enemy_ai[slot].tier : 1;
            int pcount = 2 + (tier > 3 ? 2 : tier / 2); /* 2-4 particles */
            int pspeed = 120 + tier * 20;
            particle_burst(cx, cy, pcount, PART_BURST, pspeed, 14 + tier);
            /* Per-type death variety (use ENEMY_* enums, not SUBTYPE_*) */
            switch (e->subtype) {
            case ENEMY_DRONE:
                particle_burst(cx, cy, 2, PART_SPARK, 180, 6);
                break;
            case ENEMY_TURRET:
                particle_burst(cx, cy, 3, PART_BURST, 200, 16);
                video_shake(2, 1);
                break;
            case ENEMY_MIMIC:
                particle_burst(cx, cy, 3, PART_SPARK, 220, 8);
                break;
            case ENEMY_CORRUPTOR:
                particle_burst(cx, cy, 2, PART_STAR, 80, 20);
                break;
            case ENEMY_GHOST:
                particle_spawn(cx, cy, 0, -48, PART_STAR, 20);
                break;
            case ENEMY_BOMBER:
                particle_burst(cx, cy, 3, PART_BURST, 250, 16);
                video_shake(2, 1);
                break;
            default:
                break;
            }
        }
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
                    2, 3, 2, 4, 1, 5,  /* Sentry, Patrol, Flyer, Shield, Spike, Hunter */
                    1, 3, 2, 4, 2, 3,  /* Drone, Turret, Mimic, Corruptor, Ghost, Bomber */
                };
                int cr = base_credits[e->subtype] + enemy_ai[slot].tier;
                /* Skill tree: utility branch index 3 = credits bonus (+5/10/15%) */
                int cr_bonus = player_state.skill_tree[11] * 5;
                if (cr_bonus > 0) cr = cr * (100 + cr_bonus) / 100;
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

int enemy_get_chase_count(void) {
    return chase_transitions;
}

void enemy_reset_chase_count(void) {
    chase_transitions = 0;
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

void enemy_reset_grace_timer(void) {
    spawn_grace_timer = 60; /* 1 second grace period */
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
