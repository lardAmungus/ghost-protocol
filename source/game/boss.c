/*
 * Ghost Protocol — Boss System
 *
 * 5 story bosses with phase-based AI patterns.
 * Only one boss active at a time. Uses 32x32 OBJ sprite.
 */
#include "game/boss.h"
#include "game/common.h"
#include "game/projectile.h"
#include "game/levelgen.h"
#include "game/physics.h"
#include "game/hud.h"
#include "game/player.h"
#include "game/itemdrop.h"
#include "engine/sprite.h"
#include "engine/entity.h"
#include "engine/collision.h"
#include "engine/audio.h"
#include "engine/video.h"

BossState boss_state;

static Entity* boss_entity;

/* Boss names */
static const char* const boss_names[BOSS_TYPE_COUNT] = {
    "MICROSLOP", "GOGOL", "AMAZOMB", "CRAPPLE", "FACEPLANT"
};

/* Boss base stats per type */
static const struct {
    s16 hp;
    s16 atk;
    u8  width, height;
} boss_stats[BOSS_TYPE_COUNT] = {
    { 40, 4, 24, 24 },  /* Firewall */
    { 50, 5, 24, 24 },  /* Blackout */
    { 60, 6, 28, 28 },  /* Worm */
    { 80, 7, 28, 28 },  /* Nexus Core */
    { 100, 8, 28, 28 }, /* Root Access */
};

/* Boss sprites: 2 frames × 16 tiles = 32 tiles per boss in ROM
 * Palette indices: 0=transparent, 1-4=body material ramp,
 *   5-7=energy/glow ramp, 8-A=inner detail ramp,
 *   B-C=accent pair, D-E=AA edges, F=white highlight
 * Only one boss loaded at a time (32 tiles in VRAM). */

/* Boss sprite data redesigned with clear silhouettes */
static const u32 spr_boss_firewall[2][16][8] = {
  { /* frame 0 */
    { 0x11111100, 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110, 0x34344310 },
    { 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0x31343431 },
    { 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0x34313434 },
    { 0x00111111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434 },
    { 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110 },
    { 0x43134343, 0x11111111, 0x11111111, 0x00000001, 0x5665D001, 0x67776501, 0x77777650, 0x77777650 },
    { 0x43431343, 0x11111111, 0x34311111, 0x43431000, 0x1111100D, 0x34343105, 0x34343106, 0x11111106 },
    { 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01434, 0x00D01111 },
    { 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310 },
    { 0x77777650, 0x76776750, 0x5665D001, 0x00000001, 0x11111111, 0x11111111, 0x31343431, 0x43134343 },
    { 0x34343106, 0x34343105, 0x1111100D, 0x43431000, 0x43431111, 0x11111111, 0x34313434, 0x43431343 },
    { 0x00D13434, 0x00D01434, 0x00D01111, 0x00D01343, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343 },
    { 0x11111110, 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x111111D0, 0xDDDDDD00 },
    { 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0xDDDDDDDD },
    { 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0xDDDDDDDD },
    { 0x00D01111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343, 0x00D11111, 0x0000DDDD },
  },
  { /* frame 1 */
    { 0x11111100, 0x34344310, 0x13434310, 0x11111110, 0x343C4310, 0x13434310, 0x11111110, 0x34344310 },
    { 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0x31343431 },
    { 0x11111111, 0x343134C4, 0x43431343, 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0x34313434 },
    { 0x00111111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434 },
    { 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110 },
    { 0x43134343, 0x11111111, 0x11111111, 0x00000001, 0x56F65D01, 0x7F7F7651, 0x7FFF7F65, 0x7FFF7F65 },
    { 0xC3431343, 0x11111111, 0x34311111, 0x43431000, 0x1111100D, 0x34343156, 0x3434316F, 0x1111116F },
    { 0x00D01343, 0x00D01111, 0x00D13434, 0x00D013C3, 0x0D001111, 0x0D013434, 0x0D001434, 0x0D001111 },
    { 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310, 0x11111110, 0x34344310, 0x13434310 },
    { 0x7F7F7651, 0xF7FF7F51, 0x56F65D01, 0x00000001, 0x11111111, 0x11111111, 0x31343431, 0x43134343 },
    { 0x34343156, 0x34C43115, 0x1111100D, 0x43431000, 0x43431111, 0x11111111, 0xC4313434, 0x43431343 },
    { 0x0D013434, 0x0D001434, 0x0D001111, 0x00D01343, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343 },
    { 0x11111110, 0x3434C310, 0x13434310, 0x11111110, 0x34344310, 0x13C34310, 0x111111D0, 0xDDDDDD00 },
    { 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0x31343431, 0x43134343, 0x11111111, 0xDDDDDDDD },
    { 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0x34313434, 0x43431343, 0x11111111, 0xDDDDDDDD },
    { 0x00D01111, 0x00D13434, 0x00D01343, 0x00D01111, 0x00D13434, 0x00D01343, 0x00D11111, 0x0000DDDD },
  },
};

static const u32 spr_boss_blackout[2][16][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xD0000000, 0x1D000000, 0x31D00000 },
    { 0x00000000, 0x1111D000, 0x34431D00, 0x444431D0, 0x4444431D, 0x44444431, 0x44444443, 0x11111444 },
    { 0x00000000, 0x0000000D, 0x000000D1, 0x00000D13, 0x0000D134, 0x000D1344, 0x00D13444, 0x0D134441 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x431D0000, 0x4431D000, 0x14431D00, 0x014431D0, 0x0014431D, 0x0014431D, 0x014431D0, 0x14431D00 },
    { 0x00000014, 0x5665D001, 0x67776500, 0x67777650, 0x67777765, 0x67777765, 0x67777650, 0x67776500 },
    { 0xD1344100, 0xD134100D, 0x13441005, 0x13441005, 0x13441005, 0x13441005, 0x13441005, 0x13441005 },
    { 0x00000000, 0x00000000, 0x0000000D, 0x000000D0, 0x00000D00, 0x00000D00, 0x000000D0, 0x0000000D },
    { 0x4431D000, 0x431D0000, 0x31D00000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x5665D001, 0x00000014, 0x34343444, 0x43434443, 0x44344431, 0x4344431D, 0x444431D0, 0x34431D00 },
    { 0xD134100D, 0xD1344100, 0x0D134443, 0x00D13444, 0x000D1344, 0x0000D134, 0x00000D13, 0x000000D1 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x131D0000, 0x1D000000, 0x10D00000, 0xD00D0000, 0x0000D000, 0x00000D00, 0x00000D00, 0x00000000 },
    { 0x0000000D, 0x0000000D, 0x000000D0, 0x000000D0, 0x00000D00, 0x00000D00, 0x0000D000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xD0000000, 0x1D000000, 0x31D00000 },
    { 0x00000000, 0x1111D000, 0x34431D00, 0x440431D0, 0x4440431D, 0x04440431, 0x44440443, 0x11111044 },
    { 0x00000000, 0x0000000D, 0x000000D1, 0x00000D13, 0x0000D134, 0x000D1344, 0x00D01340, 0x0D134401 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x431D0000, 0x0431D000, 0x10431D00, 0x010431D0, 0x0010431D, 0x0010431D, 0x010431D0, 0x10431D00 },
    { 0x00000010, 0x5F65D001, 0x6F7F6500, 0x67F77650, 0x67F7F765, 0x67F7F765, 0x67F77650, 0x6F7F6500 },
    { 0xD1304000, 0xD134100D, 0x13401005, 0x13041005, 0x13041005, 0x13441005, 0x13041005, 0x13041005 },
    { 0x00000000, 0x00000000, 0x0000000D, 0x000000D0, 0x00000D00, 0x00000D00, 0x000000D0, 0x0000000D },
    { 0x0431D000, 0x431D0000, 0x31D00000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x5F65D001, 0x00000010, 0x34043404, 0x43434043, 0x04344031, 0x4344031D, 0x044031D0, 0x34031D00 },
    { 0xD134100D, 0xD1304100, 0x0D130443, 0x00D13040, 0x000D1304, 0x0000D130, 0x00000D13, 0x000000D1 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x131D0000, 0x0D000000, 0x000D0000, 0x0000D000, 0x00000000, 0x00000D00, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x0000000D, 0x000000D0, 0x00000000, 0x0000D000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_boss_worm[2][16][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x11111D00, 0x344431D0, 0x5677651D, 0x1344431D, 0x111111D0, 0xD3431D00, 0xD131D000 },
    { 0x00000000, 0x0000000D, 0x000000D1, 0x00000D01, 0x00000D00, 0x000000D0, 0x00000000, 0x1D000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xD0000000, 0x1D000000, 0x31D00000, 0x431D0000 },
    { 0x0D1D0000, 0xD131D000, 0x13431D00, 0x134431D0, 0x01111111, 0xD1434343, 0x01343434, 0x00134343 },
    { 0x1311D000, 0x344431D0, 0x4344431D, 0x43434310, 0x3434431D, 0x43434310, 0x1111111D, 0x4343431D },
    { 0x000000D1, 0x000000D1, 0x00000D13, 0x00000D13, 0x0000D014, 0x0000D013, 0x000D0111, 0x00001343 },
    { 0x3431D000, 0x43431D00, 0x343431D0, 0x4343431D, 0x4343431D, 0x1111111D, 0x13434310, 0x01434310 },
    { 0x00D13434, 0x00D01343, 0x00D00134, 0x00000D13, 0x00000D01, 0x00000D01, 0x000000D0, 0x000000D0 },
    { 0x343431D0, 0x11111D00, 0x43431000, 0x43431000, 0x11111D00, 0xD13431D0, 0xD01431D0, 0xD111111D },
    { 0x00001434, 0x00001111, 0x00000D13, 0x00000D01, 0x000000D1, 0x00000000, 0x00000000, 0x00000000 },
    { 0x011111D0, 0xD1431D00, 0xD131D000, 0x0D1D0000, 0x00D00000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0000000D, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0xD013431D, 0x0D1431D0, 0x0D131D00, 0x00D1D000, 0x0D00D000, 0xD0000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x000000D0, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x111D0000, 0x4431D000, 0x77651D00, 0x44431D00, 0x1111D000, 0x431D0000, 0x31D00000 },
    { 0x00000000, 0x00000D11, 0x0000D134, 0x000D0156, 0x000D0013, 0x0000D011, 0x000000D3, 0x000000D1 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0xD0000000, 0x1D000000, 0x31D00000, 0x431D0000, 0x34311D00, 0x343431D0 },
    { 0xD1D111D0, 0x0D13431D, 0xD0134431, 0xD0143443, 0x00134344, 0x0D134343, 0x00D13434, 0x0000D134 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D, 0x0000000D, 0x00000000, 0x00000000 },
    { 0x00000000, 0xD0000000, 0x1D000000, 0x31D00000, 0x431D0000, 0x1111D000, 0x431D0000, 0x31D00000 },
    { 0x4343431D, 0x34343431, 0x13434343, 0xD1343434, 0xD0134343, 0x0D011111, 0x0D001343, 0x00D01434 },
    { 0x0000D013, 0x0000D001, 0x000000D0, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x1D000000, 0xD0000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x000D1111, 0x0000D131, 0x00000D1D, 0x000000D0, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_boss_nexus[2][16][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0D131D00, 0xD13431D0, 0x00D1D000, 0x00000000, 0x111D0000, 0x4431D000, 0x55431D00, 0x765431D0 },
    { 0x00D131D0, 0x0D13431D, 0x000D1D00, 0x00000000, 0x0000D111, 0x000D1344, 0x00D13455, 0x0D134567 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0xD0000000, 0x1D000000, 0x1D000000, 0x31D00000, 0x31D00000, 0x31D00000, 0x31D00000 },
    { 0x7765431D, 0xF7765431, 0xF7F76543, 0xFF7F6543, 0xFF7F7654, 0xFFF7F654, 0xFF7F7654, 0xFFF7F654 },
    { 0xD1345677, 0xD345677F, 0x1345677F, 0x13456F7F, 0x34567F7F, 0x3456F7FF, 0x34567F7F, 0x134567F7 },
    { 0x00000000, 0x00000000, 0x0000000D, 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1, 0x0000000D },
    { 0x1D000000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0xF7F76543, 0xFF776543, 0x77765431, 0x7765431D, 0x555431D0, 0x44431D00, 0x1111D000, 0x431D0000 },
    { 0x1345677F, 0x0D345677, 0x0D134567, 0x00D13456, 0x000D1345, 0x0000D134, 0x0000D111, 0x00000D13 },
    { 0x0000000D, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x31D00000, 0xD31D0000, 0x00D1D000, 0x000D1D00, 0x0000D1D0, 0x00000D00, 0x00000000, 0x00000000 },
    { 0x000000D1, 0x00000D13, 0x0000D1D0, 0x000D1D00, 0x00D1D000, 0x000D0000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x0D131D00, 0xD13431D0, 0x00D1D000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x0D131D00, 0xD13431D0, 0x000D1D00, 0x00000000, 0x111D0000, 0x4431D000, 0x5F431D00, 0x76F431D0 },
    { 0x00D131D0, 0x0D13431D, 0x0000D1D0, 0x00000000, 0x0000D111, 0x000D134F, 0x00D1345F, 0x0D13456F },
    { 0x00D131D0, 0x0D13431D, 0x00000D1D, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0xD0000000, 0x1D000000, 0x1D000000, 0x31D00000, 0x31D00000, 0x31D00000, 0x31D00000 },
    { 0x77F5431D, 0xF776F431, 0xF7F7F543, 0xFF7F6F43, 0xFF7F7F54, 0xFFF7F6F4, 0xFF7F7F54, 0xFFF7F6F4 },
    { 0xD13456F7, 0xD3456F7F, 0x1345F77F, 0x134F6F7F, 0x345F7F7F, 0x34F6F7FF, 0x345F7F7F, 0x1345F6F7 },
    { 0x00000000, 0x00000000, 0x0000000D, 0x0000000D, 0x000000D1, 0x000000D1, 0x000000D1, 0x0000000D },
    { 0x1D000000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0xF7F7F543, 0xFF776F43, 0x777F5431, 0xF76F431D, 0xF5F431D0, 0xF4431D00, 0x1111D000, 0x431D0000 },
    { 0x1345F77F, 0x0D3456F7, 0x0D13456F, 0x00D13456, 0x000D1345, 0x0000D134, 0x0000D111, 0x00000D13 },
    { 0x0000000D, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0xD0000000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000 },
    { 0x31D00D00, 0xD3D0D1D0, 0x0D1D0D1D, 0x000D00D1, 0x000D000D, 0x00000000, 0x00000000, 0x00000000 },
    { 0x000D00D1, 0x0D1D0D13, 0x0D1D0D1D, 0xD1D0D0D0, 0x1D00D000, 0xD0D00000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u32 spr_boss_root[2][16][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x07170000, 0x15751D00, 0x5C7C5100, 0x01575100, 0x07170000, 0x00D00000, 0x111D0000, 0x4431D000 },
    { 0x00717000, 0x015751D0, 0xC7C51001, 0x15751000, 0x07170000, 0x000D0000, 0x0000D111, 0x000D1344 },
    { 0x00000000, 0x0000000D, 0x00000015, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xD0000000, 0x1D000000 },
    { 0x77651D00, 0x4431D000, 0x111D0000, 0x34321D00, 0x434321D0, 0x3434321D, 0x43434321, 0x34343432 },
    { 0x000D1566, 0x000D1344, 0x0000D111, 0x000D1234, 0x000D1343, 0x000D1234, 0x000D1343, 0x000D1234 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x21D00000, 0x321D0000, 0x321D0000, 0x21D00000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000 },
    { 0x43434343, 0x34343434, 0x34343434, 0x43434343, 0x34343432, 0x43434321, 0x3434321D, 0x434321D0 },
    { 0x000D1343, 0x000D1234, 0x000D1234, 0x000D1343, 0x000D1234, 0x000D1343, 0x000D1234, 0x000D1343 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x34321D00, 0x4321D000, 0x111D0000, 0xD131D000, 0xD131D000, 0xD1121D00, 0xD1111D00, 0x00000000 },
    { 0x000D1234, 0x0000D123, 0x00000D11, 0x0000D131, 0x0000D131, 0x000D1211, 0x000D1111, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
  { /* frame 1 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x07F70000, 0x157F51D0, 0x5C7FC510, 0x01575100, 0x07170000, 0x0D1D000F, 0x111D0000, 0x4431D000 },
    { 0x00717000, 0x0157F510, 0xC7C51001, 0x15751000, 0x07170000, 0x00D1D000, 0x0000D111, 0x000D1344 },
    { 0x0000000F, 0x0000000D, 0x00000015, 0x00000000, 0x00000000, 0x000000F0, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xD0000000, 0x1D000000, 0x31D00000 },
    { 0x7F651D00, 0x4431D000, 0x111D0000, 0x3432D1D0, 0x3432131D, 0x34321131, 0x3432D113, 0x3432D011 },
    { 0x000D1566, 0x000D1344, 0x0000D111, 0x0D1D1234, 0xD1D13434, 0x0D123434, 0xD1343434, 0x12343434 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0000000D },
    { 0x21D00000, 0x321D0000, 0x321D0000, 0x21D00000, 0x1D000000, 0xD0000000, 0x00000000, 0x00000000 },
    { 0x43434343, 0x34343434, 0x34343434, 0x43434343, 0x34343432, 0x43434321, 0x3434321D, 0x434321D0 },
    { 0x0D134343, 0xD0123434, 0xD0123434, 0x000D1343, 0x000D1234, 0x000D1343, 0x000D1234, 0x000D1343 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x34321D00, 0x4321D000, 0x111D0000, 0xD131D000, 0xD131D000, 0xD1121D00, 0xD1111D00, 0x00000000 },
    { 0x000D1234, 0x0000D123, 0x00000D11, 0x0000D131, 0x0000D131, 0x000D1211, 0x000D1111, 0x00000000 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
  },
};

static const u16 pal_boss[BOSS_TYPE_COUNT][16] = {
    { 0x0000, RGB15_C(2,6,8), RGB15_C(6,16,20), RGB15_C(12,22,26), RGB15_C(18,28,30), RGB15_C(24,18,4), RGB15_C(28,24,8), RGB15_C(31,30,14), RGB15_C(10,20,24), RGB15_C(14,24,28), 0x0000, RGB15_C(4,12,16), RGB15_C(8,20,24), RGB15_C(3,8,10), 0x0000, RGB15_C(31,31,31) },
    { 0x0000, RGB15_C(4,2,6), RGB15_C(10,4,14), RGB15_C(16,8,20), RGB15_C(22,14,26), RGB15_C(20,4,24), RGB15_C(28,10,30), RGB15_C(31,20,31), RGB15_C(12,6,16), RGB15_C(18,10,22), 0x0000, RGB15_C(8,3,12), RGB15_C(14,6,18), RGB15_C(5,3,8), 0x0000, RGB15_C(31,31,31) },
    { 0x0000, RGB15_C(2,4,2), RGB15_C(4,12,4), RGB15_C(8,18,8), RGB15_C(14,24,14), RGB15_C(22,18,4), RGB15_C(28,24,8), RGB15_C(31,28,14), RGB15_C(6,14,6), RGB15_C(10,20,10), 0x0000, RGB15_C(3,8,3), RGB15_C(6,14,6), RGB15_C(3,6,3), 0x0000, RGB15_C(31,31,31) },
    { 0x0000, RGB15_C(2,4,8), RGB15_C(4,10,20), RGB15_C(8,16,26), RGB15_C(14,22,30), RGB15_C(10,18,28), RGB15_C(16,24,31), RGB15_C(24,30,31), RGB15_C(6,12,22), RGB15_C(10,18,26), 0x0000, RGB15_C(3,6,14), RGB15_C(6,12,22), RGB15_C(3,5,10), 0x0000, RGB15_C(31,31,31) },
    { 0x0000, RGB15_C(6,2,2), RGB15_C(16,4,4), RGB15_C(22,8,8), RGB15_C(28,14,12), RGB15_C(24,20,4), RGB15_C(28,26,8), RGB15_C(31,30,14), RGB15_C(18,6,6), RGB15_C(24,10,10), 0x0000, RGB15_C(12,3,3), RGB15_C(20,6,6), RGB15_C(8,3,3), 0x0000, RGB15_C(31,31,31) },
};


/* Boss OBJ tile 144 (after player 48 + enemies 6*16=96 → 48+96=144)
 * Each boss: 2 frames × 16 tiles = 32 tiles in VRAM (144-175) */
#define BOSS_TILE_BASE 144
#define BOSS_PAL_BANK  7

/* Lookup table: boss type → sprite data pointer */
static const u32 (*const boss_sprite_data[BOSS_TYPE_COUNT])[16][8] = {
    spr_boss_firewall,
    spr_boss_blackout,
    spr_boss_worm,
    spr_boss_nexus,
    spr_boss_root,
};

void boss_init(void) {
    boss_state.type = 0;
    boss_state.phase = BPHASE_IDLE;
    boss_state.hp = 0;
    boss_state.max_hp = 0;
    boss_state.atk = 0;
    boss_state.phase_timer = 0;
    boss_state.pattern_idx = 0;
    boss_state.hit_count = 0;
    boss_state.defeated = 0;
    boss_state.active = 0;
    boss_entity = NULL;
}

void boss_spawn(int boss_type, int tier) {
    if (boss_type < 0 || boss_type >= BOSS_TYPE_COUNT) return;

    boss_init();
    boss_state.type = (u8)boss_type;
    boss_state.hp = (s16)(boss_stats[boss_type].hp + tier * 5);
    boss_state.max_hp = boss_state.hp;
    boss_state.atk = (s16)(boss_stats[boss_type].atk + tier);
    boss_state.active = 1;

    /* Spawn entity */
    Entity* e = entity_spawn(ENT_BOSS);
    if (!e) { boss_state.active = 0; return; }

    boss_entity = e;
    e->width = boss_stats[boss_type].width;
    e->height = boss_stats[boss_type].height;
    e->hp = boss_state.hp;
    e->facing = 1;

    /* Place boss in boss arena center.
     * Boss arena is always the last section (tiles 112-127), floor at y=28.
     * Use exit-relative positioning when valid, fall back to known arena coords. */
    {
        int bx_tile = (int)level_data.exit_x - 4; /* 4 left of exit = arena center */
        int by_tile = (int)level_data.exit_y - 2; /* 2 above exit = standing height */
        /* Validate: boss arena starts at tile 112, floor at 28, ceiling at 2 */
        if (bx_tile < 112 || bx_tile > 123) bx_tile = 120;
        if (by_tile < 4  || by_tile > 26)   by_tile = 25;
        e->x = (s32)bx_tile * 8 * 256;
        e->y = (s32)by_tile * 8 * 256;
    }

    /* Allocate OAM — if pool exhausted, despawn and bail */
    int oam = sprite_alloc();
    if (oam < 0) {
        entity_despawn(e);
        boss_entity = NULL;
        boss_state.active = 0;
        return;
    }
    e->oam_index = (u8)oam;

    /* Load boss tiles — 2 frames × 16 tiles = 32 tiles */
    memcpy16(&tile_mem[4][BOSS_TILE_BASE], boss_sprite_data[boss_type],
             2 * 16 * 8 * sizeof(u32) / 2);

    /* Load boss palette */
    memcpy16(&pal_obj_mem[BOSS_PAL_BANK * 16], pal_boss[boss_type], 16);

    /* Update HUD */
    hud_set_boss(boss_names[boss_type], boss_state.hp, boss_state.max_hp);

    audio_play_sfx(SFX_BOSS_ROAR);
}

/* ---- Per-boss attack patterns ---- */

static void attack_firewall(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));
    s16 dir = e->facing ? -384 : 384;

    /* Pattern alternates: wall of projectiles vs vertical sweep */
    if ((boss_state.pattern_idx & 1) == 0) {
        /* WALL: 5 projectiles (7 when enraged) forming a wall */
        if (boss_state.phase == BPHASE_ATTACK1 && boss_state.phase_timer == 25) {
            int count = enraged ? 7 : 5;
            int half = count / 2;
            for (int i = 0; i < count; i++) {
                s32 sy = e->y + (i - half) * 6 * 256;
                projectile_spawn(e->x, sy, dir, 0,
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
    } else {
        /* SWEEP: 3 staggered shots (high, mid, low) */
        if (boss_state.phase == BPHASE_ATTACK1) {
            if (boss_state.phase_timer == 15 || boss_state.phase_timer == 25 ||
                boss_state.phase_timer == 35) {
                int shot = (boss_state.phase_timer - 15) / 10;
                s32 sy = e->y + (shot - 1) * 10 * 256;
                projectile_spawn(e->x, sy, dir, 0,
                    (s16)(boss_state.atk + 1), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                audio_play_sfx(SFX_SHOOT);
            }
        }
    }
    /* ATTACK2: Slow advance (firewall is a barrier, not a charger) */
    if (boss_state.phase == BPHASE_ATTACK2) {
        e->vx = e->facing ? -96 : 96; /* Slow approach */
    }
    (void)player_x; (void)player_y; (void)enraged;
}

static void attack_blackout(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));

    /* ATTACK1: Teleport to random position + aimed shot */
    if (boss_state.phase == BPHASE_ATTACK1) {
        if (boss_state.phase_timer == 10) {
            /* Teleport: snap to player's X region but offset
             * Enraged: wider offset (±60 vs ±40) for harder dodging */
            int ofs_px = enraged ? 60 : 40;
            s32 offset = ((boss_state.pattern_idx & 1) ? ofs_px : -ofs_px) * 256;
            e->x = player_x + offset;
            /* Clamp to map */
            if (e->x < FP8(16)) e->x = FP8(16);
            int max_x = (NET_MAP_W * 8 - e->width) * 256;
            if (e->x > max_x) e->x = max_x;
            e->vy = 0;
            audio_play_sfx(SFX_DASH);
        }
        if (boss_state.phase_timer == 25) {
            /* Aimed shot toward player */
            int dx = (int)((player_x - e->x) >> 8);
            int dy = (int)((player_y - e->y) >> 8);
            /* Normalize to ~384 speed */
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (dist < 1) dist = 1;
            s16 pvx = (s16)(dx * 384 / dist);
            s16 pvy = (s16)(dy * 384 / dist);
            projectile_spawn(e->x, e->y, pvx, pvy,
                (s16)(boss_state.atk + 2), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            /* Enraged: fire a second spread shot */
            if (enraged) {
                projectile_spawn(e->x, e->y, pvx, (s16)(pvy + 160),
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
    }
    /* ATTACK2: Quick dash past player */
    if (boss_state.phase == BPHASE_ATTACK2) {
        e->vx = e->facing ? -320 : 320; /* Fast dash */
    }
}

static void attack_worm(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));
    s16 dir = e->facing ? -384 : 384;

    /* ATTACK1: Ground-level wave of projectiles */
    if (boss_state.phase == BPHASE_ATTACK1) {
        if (boss_state.phase_timer == 20) {
            /* 3 low projectiles (5 when enraged) along the ground */
            int count = enraged ? 5 : 3;
            for (int i = 0; i < count; i++) {
                s32 sx = e->x + (e->facing ? -(i * 8 * 256) : (i * 8 * 256));
                projectile_spawn(sx, e->y + FP8(8), dir, 64,
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
        if (boss_state.phase_timer == 40) {
            /* Lob a projectile in an arc toward player */
            int dx = (int)((player_x - e->x) >> 8);
            int dy = (int)((player_y - e->y) >> 8);
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (dist < 1) dist = 1;
            s16 pvx = (s16)(dx * 256 / dist);
            s16 pvy = (s16)(dy * 192 / dist - 256);
            projectile_spawn(e->x, e->y - FP8(8), pvx, pvy,
                (s16)(boss_state.atk + 1), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT);
        }
    }
    /* ATTACK2: Burrow charge (fast horizontal + slight bob) */
    if (boss_state.phase == BPHASE_ATTACK2) {
        e->vx = e->facing ? -256 : 256;
        /* Bobbing vertical movement */
        int phase = boss_state.phase_timer & 15;
        e->vy = (phase < 8) ? -48 : 48;
    }
}

static void attack_nexus(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));

    /* ATTACK1: Spiral projectile burst */
    if (boss_state.phase == BPHASE_ATTACK1) {
        /* Fire projectiles in expanding pattern over multiple frames.
         * 8-way directions for more chaotic coverage.
         * Enraged: fire every 3 frames instead of 6. */
        int fire_rate = enraged ? 3 : 6;
        if (boss_state.phase_timer >= 10 && boss_state.phase_timer <= 40 &&
            (boss_state.phase_timer % fire_rate) == 0) {
            int angle_step = boss_state.phase_timer * 3 + boss_state.pattern_idx * 7;
            /* 8-direction spiral: cardinal + diagonal */
            s16 speeds[8][2] = {
                { 320, 0 }, { 224, 224 }, { 0, 320 }, { -224, 224 },
                { -320, 0 }, { -224, -224 }, { 0, -320 }, { 224, -224 }
            };
            int idx = (angle_step >> 2) & 7;
            projectile_spawn(e->x, e->y, speeds[idx][0], speeds[idx][1],
                boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT);
        }
    }
    /* ATTACK2: Float toward player (Nexus Core hovers) */
    if (boss_state.phase == BPHASE_ATTACK2) {
        int dx = (int)((player_x - e->x) >> 8);
        int dy = (int)((player_y - e->y) >> 8);
        e->vx = (dx > 0) ? 128 : -128;
        e->vy = (dy > 0) ? 96 : -96;
    }
}

static void attack_root(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));

    /* Root Access: aggressive multi-phase final boss */
    if (boss_state.phase == BPHASE_ATTACK1) {
        /* Phase varies by pattern_idx for unpredictability */
        int variant = boss_state.pattern_idx % 3;
        if (variant == 0) {
            /* Rapid burst: 3 shots (5 when enraged) */
            int fires_at_12 = (boss_state.phase_timer == 12);
            int fires_at_20 = (boss_state.phase_timer == 20);
            int fires_at_28 = (boss_state.phase_timer == 28);
            int fires_at_16 = (enraged && boss_state.phase_timer == 16);
            int fires_at_24 = (enraged && boss_state.phase_timer == 24);
            if (fires_at_12 || fires_at_20 || fires_at_28 || fires_at_16 || fires_at_24) {
                s16 dir = e->facing ? -448 : 448;
                projectile_spawn(e->x, e->y, dir, 0,
                    (s16)(boss_state.atk + 2), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                audio_play_sfx(SFX_SHOOT_RAPID);
            }
        } else if (variant == 1) {
            /* Aimed shot + spread */
            if (boss_state.phase_timer == 20) {
                int dx = (int)((player_x - e->x) >> 8);
                int dy = (int)((player_y - e->y) >> 8);
                int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
                if (dist < 1) dist = 1;
                s16 pvx = (s16)(dx * 384 / dist);
                s16 pvy = (s16)(dy * 384 / dist);
                projectile_spawn(e->x, e->y, pvx, pvy,
                    (s16)(boss_state.atk + 3), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                /* Spread pair */
                projectile_spawn(e->x, e->y, pvx, (s16)(pvy - 128),
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                projectile_spawn(e->x, e->y, pvx, (s16)(pvy + 128),
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                audio_play_sfx(SFX_SHOOT_CHARGE);
            }
        } else {
            /* Wide fan: 5 projectiles in an arc (7 when enraged) */
            if (boss_state.phase_timer == 25) {
                s16 base_dir = e->facing ? -320 : 320;
                int half = enraged ? 3 : 2;
                for (int i = -half; i <= half; i++) {
                    projectile_spawn(e->x, e->y, base_dir, (s16)(i * 96),
                        boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                }
                audio_play_sfx(SFX_SHOOT_CHARGE);
            }
        }
    }
    /* ATTACK2: Fast charge + jump */
    if (boss_state.phase == BPHASE_ATTACK2) {
        e->vx = e->facing ? -320 : 320; /* Very fast */
        /* Jump at midpoint for unpredictability */
        if (boss_state.phase_timer == 20) {
            e->vy = -384;
        }
    }
}

/* ---- Phase timing per boss type (frames) ---- */
static const struct {
    u16 idle_dur;
    u16 attack1_dur;
    u16 attack2_dur;
    u16 vuln_dur;
} boss_timing[BOSS_TYPE_COUNT] = {
    { 50, 50, 40, 45 },  /* Firewall: slow, generous vulnerability window */
    { 25, 35, 25, 20 },  /* Blackout: fast cycle, tight window */
    { 45, 50, 35, 30 },  /* Worm: medium paced */
    { 35, 45, 40, 25 },  /* Nexus Core: measured, moderate window */
    { 20, 35, 30, 20 },  /* Root Access: relentless, tight but fair window */
};

IWRAM_CODE void boss_update(s32 player_x, s32 player_y) {
    if (!boss_state.active || boss_state.defeated) return;
    if (!boss_entity || boss_entity->type != ENT_BOSS) return;

    Entity* e = boss_entity;
    int dx = (int)((player_x - e->x) >> 8);
    /* Hysteresis to prevent jitter when player is near center */
    if (dx < -16) e->facing = 1;
    else if (dx > 16) e->facing = 0;

    boss_state.phase_timer++;
    int bt = boss_state.type;

    /* Enrage at <50% HP: 20% shorter phase durations (faster cycle) */
    int enraged = (boss_state.hp * 2 < boss_state.max_hp);

    switch (boss_state.phase) {
    case BPHASE_IDLE:
    {
        /* Wait, then start attacking */
        int idle_dur = boss_timing[bt].idle_dur;
        if (enraged) idle_dur = idle_dur * 4 / 5;
        if (boss_state.phase_timer >= (u16)idle_dur) {
            boss_state.phase = BPHASE_ATTACK1;
            boss_state.phase_timer = 0;
            audio_play_sfx(SFX_SHOOT_CHARGE); /* Attack telegraph */
        }
        break;
    }
    case BPHASE_ATTACK1:
    {
        /* Per-boss attack pattern */
        switch (bt) {
        case BOSS_FIREWALL:    attack_firewall(e, player_x, player_y, enraged); break;
        case BOSS_BLACKOUT:    attack_blackout(e, player_x, player_y, enraged); break;
        case BOSS_WORM:        attack_worm(e, player_x, player_y, enraged); break;
        case BOSS_NEXUS_CORE:  attack_nexus(e, player_x, player_y, enraged); break;
        case BOSS_ROOT_ACCESS: attack_root(e, player_x, player_y, enraged); break;
        }
        int atk1_dur = boss_timing[bt].attack1_dur;
        if (enraged) atk1_dur = atk1_dur * 4 / 5;
        if (boss_state.phase_timer >= (u16)atk1_dur) {
            boss_state.phase = BPHASE_ATTACK2;
            boss_state.phase_timer = 0;
        }
        break;
    }
    case BPHASE_ATTACK2:
    {
        /* Per-boss movement pattern (called in attack functions above) */
        switch (bt) {
        case BOSS_FIREWALL:    attack_firewall(e, player_x, player_y, enraged); break;
        case BOSS_BLACKOUT:    attack_blackout(e, player_x, player_y, enraged); break;
        case BOSS_WORM:        attack_worm(e, player_x, player_y, enraged); break;
        case BOSS_NEXUS_CORE:  attack_nexus(e, player_x, player_y, enraged); break;
        case BOSS_ROOT_ACCESS: attack_root(e, player_x, player_y, enraged); break;
        }
        int atk2_dur = boss_timing[bt].attack2_dur;
        if (enraged) atk2_dur = atk2_dur * 4 / 5;
        if (boss_state.phase_timer >= (u16)atk2_dur) {
            e->vx = 0;
            boss_state.phase = BPHASE_VULNERABLE;
            boss_state.phase_timer = 0;
            boss_state.hit_count = 0;
            audio_play_sfx(SFX_ABILITY); /* Vulnerability open cue */
        }
        break;
    }
    case BPHASE_VULNERABLE:
        /* Open to damage */
        if (boss_state.phase_timer >= boss_timing[bt].vuln_dur) {
            boss_state.phase = BPHASE_IDLE;
            boss_state.phase_timer = 0;
            boss_state.pattern_idx++;
        }
        break;

    case BPHASE_DEAD:
        /* Death animation */
        if (boss_state.phase_timer >= 60) {
            boss_state.defeated = 1;
            boss_state.active = 0;
            /* entity_despawn handles sprite_free + oam_index clearing */
            entity_despawn(e);
            boss_entity = NULL;
            hud_set_boss(NULL, 0, 0);
            return; /* Entity despawned — do NOT access 'e' below */
        }
        break;

    default:
        break;
    }

    /* Apply velocity with proper collision (skip on dead boss — pure death anim) */
    if (e->type == ENT_BOSS && boss_state.phase != BPHASE_DEAD) {
        /* Gravity: apply to all bosses except NEXUS_CORE (floater) and WORM in
         * ATTACK2 (bobbing overrides gravity with manual vy). */
        int needs_gravity = (bt != BOSS_NEXUS_CORE);
        if (bt == BOSS_WORM && boss_state.phase == BPHASE_ATTACK2) needs_gravity = 0;
        if (needs_gravity) {
            e->vy += 32;
            if (e->vy > 512) e->vy = 512;
        }

        /* Axis-separated resolve: snaps position on collision, handles
         * one-way platforms, and prevents sinking through the floor. */
        e->on_ground = 0;
        e->on_wall = 0;

        e->x += e->vx;
        physics_resolve_x(e);

        e->y += e->vy;
        physics_resolve_y(e);

        /* Ground detection fallback (1px below bottom) */
        if (!e->on_ground) {
            int px = e->x >> 8;
            int py = e->y >> 8;
            e->on_ground = (u8)collision_check_ground(px, py + e->height - 1, e->width);
        }

        /* World bounds */
        if (e->x < 0) e->x = 0;
        if (e->y < 0) e->y = 0;
        int max_x = (NET_MAP_W * 8 - e->width) * 256;
        int max_y = (NET_MAP_H * 8 - e->height) * 256;
        if (e->x > max_x) e->x = max_x;
        if (e->y > max_y) e->y = max_y;
    }

    /* Update HUD boss HP */
    hud_set_boss(boss_names[boss_state.type], boss_state.hp, boss_state.max_hp);
}

void boss_draw(s32 cam_x, s32 cam_y) {
    if (!boss_state.active || !boss_entity) return;
    if (boss_entity->oam_index == OAM_NONE) return;

    Entity* e = boss_entity;
    int sx = (int)((e->x - cam_x) >> 8);
    int sy = (int)((e->y - cam_y) >> 8);

    /* Vulnerability flash */
    if (boss_state.phase == BPHASE_VULNERABLE && (boss_state.phase_timer & 2)) {
        /* Gold tint during vulnerability */
        u16 gold = RGB15(28, 24, 4);
        pal_obj_mem[BOSS_PAL_BANK * 16 + 4] = gold;
    } else if (boss_state.phase != BPHASE_VULNERABLE) {
        /* Restore palette */
        pal_obj_mem[BOSS_PAL_BANK * 16 + 4] = pal_boss[boss_state.type][4];
    }

    /* Death flicker */
    if (boss_state.phase == BPHASE_DEAD) {
        if (boss_state.phase_timer & 2) {
            OBJ_ATTR* oam = sprite_get(e->oam_index);
            if (oam) oam->attr0 = ATTR0_HIDE;
            return;
        }
    }

    u16 attr1_flip = e->facing ? ATTR1_HFLIP : 0;

    /* Select animation frame: attack phases use frame 1, others frame 0 */
    int boss_frame = 0;
    if (boss_state.phase == BPHASE_ATTACK1 || boss_state.phase == BPHASE_ATTACK2 ||
        boss_state.phase == BPHASE_DEAD) {
        boss_frame = 1;
    }
    int tile_id = BOSS_TILE_BASE + boss_frame * 16;

    OBJ_ATTR* oam = sprite_get(e->oam_index);
    if (!oam) return;
    oam->attr0 = (u16)(ATTR0_SQUARE | ((u16)sy & 0xFF));
    oam->attr1 = (u16)(ATTR1_SIZE_32 | attr1_flip | ((u16)sx & 0x1FF));
    oam->attr2 = (u16)(ATTR2_ID(tile_id) | ATTR2_PALBANK(BOSS_PAL_BANK));
}

IWRAM_CODE int boss_check_player_attack(Entity* player) {
    if (!boss_state.active || !boss_entity || boss_entity->hp <= 0) return 0;
    if (!player) return 0;

    Projectile* pool = projectile_get_pool();
    int total_dmg = 0;

    for (int j = 0; j < MAX_PROJECTILES; j++) {
        Projectile* p = &pool[j];
        if (!(p->flags & PROJ_ACTIVE)) continue;
        if (p->flags & PROJ_ENEMY) continue;

        Entity* e = boss_entity;
        int px = (int)(p->x >> 8);
        int py = (int)(p->y >> 8);
        int ex = (int)(e->x >> 8);
        int ey = (int)(e->y >> 8);

        if (px >= ex && px <= ex + (int)e->width &&
            py >= ey && py <= ey + (int)e->height) {

            /* Piercing dedup: only hit boss once per projectile */
            if (p->flags & PROJ_PIERCE) {
                u16 boss_bit = (u16)(1 << 15);
                if (p->hit_mask & boss_bit) continue;
                p->hit_mask |= boss_bit;
            }

            int dmg = p->damage;

            /* 2x damage during vulnerability */
            if (boss_state.phase == BPHASE_VULNERABLE) {
                dmg *= 2;
            }

            boss_damage(dmg);
            total_dmg += dmg;

            if (!(p->flags & PROJ_PIERCE)) {
                projectile_deactivate(p);
            }
        }
    }
    return total_dmg;
}

void boss_damage(int dmg) {
    /* Damage resistance outside vulnerability window:
     * IDLE/ATTACK phases take 50% damage to reward hitting during vulnerability */
    int actual = dmg;
    if (boss_state.phase == BPHASE_VULNERABLE) {
        /* Diminishing returns: each hit in this window reduces dmg by 15%
         * hit 0 = 100%, hit 1 = 85%, hit 2 = 72%, hit 3 = 61%, ... */
        int scale = 256;
        for (int i = 0; i < boss_state.hit_count && i < 6; i++)
            scale = scale * 217 >> 8; /* 217/256 ≈ 0.85 */
        actual = dmg * scale >> 8;
        if (actual < 1) actual = 1;
    } else if (boss_state.phase != BPHASE_DEAD) {
        if (dmg > 0) {
            actual = (dmg + 1) >> 1;
            if (actual < 1) actual = 1;
        } else {
            actual = 0;
        }
    }
    int was_above_half = (boss_state.hp * 2 >= boss_state.max_hp);
    boss_state.hp -= (s16)actual;
    boss_state.hit_count++;
    audio_play_sfx(SFX_ENEMY_HIT);
    video_shake(4, 1);

    /* Enrage notification on first drop below 50% */
    if (was_above_half && boss_state.hp > 0 && boss_state.hp * 2 < boss_state.max_hp) {
        hud_notify("BOSS ENRAGED!", 60);
        video_shake(10, 2);
    }

    if (boss_state.hp <= 0) {
        boss_state.hp = 0;
        boss_state.phase = BPHASE_DEAD;
        boss_state.phase_timer = 0;
        audio_play_sfx(SFX_ENEMY_DIE);
        video_shake(15, 3);

        /* Award XP: 50 base + 30 per boss type (higher bosses = more XP) */
        player_add_xp(50 + boss_state.type * 30);

        /* Drop loot at boss position */
        Entity* be = boss_get_entity();
        if (be) {
            int tier = boss_state.type + 2; /* Boss tier = type + 2 for better drops */
            itemdrop_roll(be->x, be->y, tier, RARITY_RARE, player_state.lck);
        }
    }
}

int boss_is_active(void) {
    return boss_state.active && !boss_state.defeated;
}

Entity* boss_get_entity(void) {
    return boss_entity;
}

const char* boss_get_name(int boss_type) {
    if (boss_type < 0 || boss_type >= BOSS_TYPE_COUNT) return "";
    return boss_names[boss_type];
}
