/*
 * Ghost Protocol — Boss System
 *
 * 6 story bosses with phase-based AI patterns.
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
#include "engine/rng.h"
#include "game/particle.h"

BossState boss_state;

static Entity* boss_entity;

/* Boss names */
static const char* const boss_names[BOSS_TYPE_COUNT] = {
    "MICROSLOP", "GOGOL", "AMAZOMB", "CRAPPLE", "FACEPLANT", "DAEMON"
};

/* Boss base stats per type */
static const struct {
    s16 hp;
    s16 atk;
    u8  width, height;
} boss_stats[BOSS_TYPE_COUNT] = {
    { 60, 4, 24, 24 },   /* Firewall */
    { 80, 5, 24, 24 },   /* Blackout */
    { 110, 6, 28, 28 },  /* Worm */
    { 150, 7, 28, 28 },  /* Nexus Core */
    { 200, 8, 28, 28 },  /* Root Access */
    { 280, 9, 28, 28 },  /* Daemon */
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

/* Daemon: AXIOM's failsafe backup — corrupted humanoid silhouette */
static const u32 spr_boss_daemon[2][16][8] = {
  { /* frame 0 */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00D11D00, 0x0D3443D0, 0xD3444443, 0x14444441 },
    { 0x00000000, 0x00011100, 0x001567D0, 0x01567710, 0x15677710, 0x56777651, 0x67777765, 0x56777765 },
    { 0x00000000, 0x00111000, 0x0D765100, 0x01776510, 0x01777651, 0x15677765, 0x56777776, 0x56777765 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00D11D00, 0x0D3443D0, 0x34444430, 0x14444410 },
    { 0x13434310, 0x01343410, 0x00134310, 0x00013100, 0x00D11D00, 0x0D1111D0, 0xD1222221, 0x12333321 },
    { 0x15677651, 0x01567710, 0x001567D0, 0x00011100, 0x00D11D00, 0x0D3443D0, 0xD3444443, 0x34434443 },
    { 0x15677651, 0x01776510, 0x0D765100, 0x00111000, 0x00D11D00, 0x0D3443D0, 0x34444430, 0x34434430 },
    { 0x01343100, 0x00143100, 0x00013100, 0x00001100, 0x00D11D00, 0x0D1111D0, 0x12222210, 0x12333210 },
    { 0x12343321, 0x12434321, 0x01343410, 0x01434310, 0x01343410, 0x01343410, 0x00134310, 0x00134310 },
    { 0x34344443, 0x43434443, 0x43434443, 0x34344443, 0x13434431, 0x01343410, 0x0D134310, 0x00D1D1D0 },
    { 0x34443430, 0x34443430, 0x34443434, 0x34443430, 0x13443431, 0x01343410, 0x0D134310, 0x00D1D1D0 },
    { 0x12343210, 0x12343210, 0x01243410, 0x01343410, 0x01343410, 0x01343410, 0x00134310, 0x00134310 },
    { 0x0D134310, 0x0D1343D0, 0x00D131D0, 0x00D131D0, 0x00D131D0, 0x0D11D1D0, 0xD111D000, 0xDDD00000 },
    { 0x00D1D000, 0x0D131D00, 0x0D1431D0, 0x0D143100, 0x00D13100, 0x00D1D100, 0x000D1D00, 0x0000DD00 },
    { 0x00D1D000, 0x00D131D0, 0x0D1341D0, 0x001341D0, 0x001310D0, 0x001D1D00, 0x00D1D000, 0x00DD0000 },
    { 0x00134310, 0x0D1343D0, 0x00D131D0, 0x00D131D0, 0x00D131D0, 0x0D1D11D0, 0x000D111D, 0x00000DDD },
  },
  { /* frame 1 — attack pose with extended arms */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00D11D00, 0x0D3F43D0, 0xD34F4443, 0x1444F441 },
    { 0x00000000, 0x00011100, 0x001F67D0, 0x015F7710, 0x1F677710, 0x5F777651, 0x6F7F7765, 0x5F7F7765 },
    { 0x00000000, 0x00111000, 0x0D76F100, 0x01776F10, 0x01777F51, 0x156777F5, 0x567F7776, 0x567F7765 },
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00D11D00, 0x0D34F3D0, 0x3444F430, 0x14444F10 },
    { 0x13434310, 0x01343410, 0x00134310, 0x00D13100, 0x0D1111D0, 0xD1222221, 0x12333321, 0x12343321 },
    { 0x1F677651, 0x01F67710, 0x001F67D0, 0x00D11100, 0x0D3443D0, 0xD3F44443, 0x3443F443, 0x34344F43 },
    { 0x1F677651, 0x01776F10, 0x0D76F100, 0x00111D00, 0x0D3443D0, 0x3444F430, 0x3443F430, 0x344F4430 },
    { 0x01343100, 0x00143100, 0x001D3100, 0x00D11D00, 0x0D1111D0, 0x12222210, 0x12333210, 0x12343210 },
    { 0x12434321, 0x01343410, 0x01434310, 0x01343410, 0x01343410, 0x00134310, 0x0D134310, 0x0D1343D0 },
    { 0x43434443, 0x43434443, 0x34344443, 0x13434431, 0x01343410, 0x0D134310, 0x0D131D00, 0x0D1431D0 },
    { 0x34443430, 0x34443434, 0x34443430, 0x13443431, 0x01343410, 0x0D134310, 0x00D131D0, 0x0D1341D0 },
    { 0x12343210, 0x01243410, 0x01343410, 0x01343410, 0x01343410, 0x00134310, 0x00134310, 0x0D1343D0 },
    { 0x00D131D0, 0x00D131D0, 0x00D131D0, 0x0D11D1D0, 0xD11D0000, 0xDD000000, 0x00000000, 0x00000000 },
    { 0x0D143100, 0x00D13100, 0x00D1D100, 0x000D1D00, 0x0000DD00, 0x00000000, 0x00000000, 0x00000000 },
    { 0x001341D0, 0x001310D0, 0x001D1D00, 0x00D1D000, 0x00DD0000, 0x00000000, 0x00000000, 0x00000000 },
    { 0x00D131D0, 0x00D131D0, 0x0D1D11D0, 0x0D1D000D, 0x00000DDD, 0x00000000, 0x00000000, 0x00000000 },
  },
};

/* Boss palettes: 0=trans, 1-4=body ramp (dark→light), 5-7=energy ramp,
 * 8-9=inner detail, A=unused, B-C=accent pair, D=AA edge, E=unused, F=white
 * Each boss has a unique color identity: */
static const u16 pal_boss[BOSS_TYPE_COUNT][16] = {
    /* Firewall (Microslop): cyan/teal firewall with amber energy core */
    { 0x0000,
      RGB15_C(1,4,6),    /* 1: deep teal shadow */
      RGB15_C(4,12,16),  /* 2: dark teal */
      RGB15_C(10,20,24), /* 3: mid teal */
      RGB15_C(18,28,30), /* 4: bright teal surface */
      RGB15_C(22,16,2),  /* 5: amber glow dark */
      RGB15_C(28,22,6),  /* 6: amber glow mid */
      RGB15_C(31,28,12), /* 7: amber glow bright */
      RGB15_C(8,18,22),  /* 8: inner teal dark */
      RGB15_C(14,24,28), /* 9: inner teal light */
      RGB15_C(6,14,18),  /* A: teal accent */
      RGB15_C(3,8,12),   /* B: accent dark */
      RGB15_C(8,18,24),  /* C: accent bright */
      RGB15_C(2,6,8),    /* D: AA edge */
      RGB15_C(24,30,31), /* E: highlight */
      RGB15_C(31,31,31)  /* F: white */
    },
    /* Blackout (Gogol): deep purple/violet with electric magenta energy */
    { 0x0000,
      RGB15_C(3,1,5),    /* 1: deep violet shadow */
      RGB15_C(8,3,12),   /* 2: dark violet */
      RGB15_C(14,6,18),  /* 3: mid violet */
      RGB15_C(22,12,26), /* 4: bright violet surface */
      RGB15_C(20,4,24),  /* 5: magenta glow dark */
      RGB15_C(26,8,28),  /* 6: magenta glow mid */
      RGB15_C(31,18,31), /* 7: magenta glow bright */
      RGB15_C(10,4,14),  /* 8: inner violet dark */
      RGB15_C(16,8,20),  /* 9: inner violet light */
      RGB15_C(12,5,16),  /* A: violet accent */
      RGB15_C(6,2,10),   /* B: accent dark */
      RGB15_C(14,6,20),  /* C: accent bright */
      RGB15_C(4,1,7),    /* D: AA edge */
      RGB15_C(28,22,31), /* E: highlight */
      RGB15_C(31,31,31)  /* F: white */
    },
    /* Worm (Amazomb): toxic green with sickly amber/gold energy */
    { 0x0000,
      RGB15_C(1,3,1),    /* 1: deep green shadow */
      RGB15_C(3,10,3),   /* 2: dark green */
      RGB15_C(6,16,6),   /* 3: mid green */
      RGB15_C(12,24,12), /* 4: bright green surface */
      RGB15_C(20,16,2),  /* 5: amber glow dark */
      RGB15_C(26,22,6),  /* 6: amber glow mid */
      RGB15_C(31,28,12), /* 7: amber glow bright */
      RGB15_C(4,12,4),   /* 8: inner green dark */
      RGB15_C(8,18,8),   /* 9: inner green light */
      RGB15_C(6,14,6),   /* A: green accent */
      RGB15_C(2,6,2),    /* B: accent dark */
      RGB15_C(5,12,5),   /* C: accent bright */
      RGB15_C(2,5,2),    /* D: AA edge */
      RGB15_C(20,28,20), /* E: highlight */
      RGB15_C(31,31,31)  /* F: white */
    },
    /* Nexus (Crapple): cold blue/steel with electric blue energy */
    { 0x0000,
      RGB15_C(1,3,7),    /* 1: deep blue shadow */
      RGB15_C(3,8,18),   /* 2: dark blue */
      RGB15_C(6,14,24),  /* 3: mid blue */
      RGB15_C(12,22,30), /* 4: bright blue surface */
      RGB15_C(8,16,28),  /* 5: electric blue dark */
      RGB15_C(14,22,31), /* 6: electric blue mid */
      RGB15_C(22,28,31), /* 7: electric blue bright */
      RGB15_C(4,10,20),  /* 8: inner blue dark */
      RGB15_C(8,16,26),  /* 9: inner blue light */
      RGB15_C(6,12,22),  /* A: blue accent */
      RGB15_C(2,5,12),   /* B: accent dark */
      RGB15_C(5,10,20),  /* C: accent bright */
      RGB15_C(2,4,8),    /* D: AA edge */
      RGB15_C(20,28,31), /* E: highlight */
      RGB15_C(31,31,31)  /* F: white */
    },
    /* Root (Faceplant): crimson/red with hot orange energy */
    { 0x0000,
      RGB15_C(5,1,1),    /* 1: deep red shadow */
      RGB15_C(14,3,3),   /* 2: dark red */
      RGB15_C(20,6,6),   /* 3: mid red */
      RGB15_C(28,12,10), /* 4: bright red surface */
      RGB15_C(24,16,2),  /* 5: orange glow dark */
      RGB15_C(28,22,6),  /* 6: orange glow mid */
      RGB15_C(31,28,12), /* 7: orange glow bright */
      RGB15_C(16,4,4),   /* 8: inner red dark */
      RGB15_C(22,8,8),   /* 9: inner red light */
      RGB15_C(18,6,6),   /* A: red accent */
      RGB15_C(10,2,2),   /* B: accent dark */
      RGB15_C(18,5,5),   /* C: accent bright */
      RGB15_C(7,2,2),    /* D: AA edge */
      RGB15_C(31,20,18), /* E: highlight */
      RGB15_C(31,31,31)  /* F: white */
    },
    /* Daemon: dark purple/crimson corruption with unstable red/violet energy */
    { 0x0000,
      RGB15_C(5,1,7),    /* 1: corruption shadow */
      RGB15_C(12,3,14),  /* 2: dark corruption */
      RGB15_C(18,5,20),  /* 3: mid corruption */
      RGB15_C(24,8,26),  /* 4: bright corruption surface */
      RGB15_C(28,3,6),   /* 5: crimson glow dark */
      RGB15_C(31,8,12),  /* 6: crimson glow mid */
      RGB15_C(31,16,20), /* 7: crimson glow bright */
      RGB15_C(14,3,16),  /* 8: inner corruption dark */
      RGB15_C(20,5,22),  /* 9: inner corruption light */
      RGB15_C(16,4,18),  /* A: corruption accent */
      RGB15_C(8,1,10),   /* B: accent dark */
      RGB15_C(16,3,18),  /* C: accent bright */
      RGB15_C(6,1,8),    /* D: AA edge */
      RGB15_C(28,18,31), /* E: highlight */
      RGB15_C(31,31,31)  /* F: white */
    },
};


/* Boss OBJ tile 144 (after player 48 + enemies 6*16=96 → 48+96=144)
 * Each boss: 2 frames × 16 tiles = 32 tiles in VRAM (144-175) */
#define BOSS_TILE_BASE 240  /* After 12 enemy types: 48 + 12*16 = 240 */
#define BOSS_PAL_BANK  7

/* Lookup table: boss type → sprite data pointer */
static const u32 (*const boss_sprite_data[BOSS_TYPE_COUNT])[16][8] = {
    spr_boss_firewall,
    spr_boss_blackout,
    spr_boss_worm,
    spr_boss_nexus,
    spr_boss_root,
    spr_boss_daemon,
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
    boss_state.stage = 1;
    boss_state.stage_announced = 0;
    boss_state.enrage_announced = 0;
    boss_entity = NULL;
}

void boss_spawn(int boss_type, int tier) {
    if (boss_type < 0 || boss_type >= BOSS_TYPE_COUNT) return;

    boss_init();
    boss_state.type = (u8)boss_type;
    {
        int base_hp = boss_stats[boss_type].hp + tier * 5;
        int base_atk = boss_stats[boss_type].atk + tier;
        /* NG+ scaling: boss HP ×2, ATK +30% */
        if (game_stats.ng_plus > 0) {
            base_hp = base_hp * 2;
            base_atk = base_atk * 13 / 10;
        }
        /* Threat level scaling: +10% per level */
        if (game_stats.bb_threat_level > 0) {
            base_hp = base_hp * (10 + game_stats.bb_threat_level) / 10;
            base_atk = base_atk * (10 + game_stats.bb_threat_level) / 10;
        }
        boss_state.hp = (s16)base_hp;
        boss_state.max_hp = boss_state.hp;
        boss_state.atk = (s16)base_atk;
    }
    boss_state.active = 1;
    boss_state.stage = 1;
    boss_state.stage_announced = 0;
    /* Codex: unlock boss entry on first encounter */
    codex_unlock(CODEX_BOSS_BASE + boss_type);
    boss_state.enrage_announced = 0;

    /* Spawn entity */
    Entity* e = entity_spawn(ENT_BOSS);
    if (!e) { boss_state.active = 0; return; }

    boss_entity = e;
    e->width = boss_stats[boss_type].width;
    e->height = boss_stats[boss_type].height;
    e->hp = boss_state.hp;
    e->facing = 1;

    /* Place boss in boss arena center.
     * Boss arena is always the last section, floor at y=28.
     * Use exit-relative positioning when valid, fall back to known arena coords. */
    {
        int bx_tile = (int)level_data.exit_x - 4; /* 4 left of exit = arena center */
        int by_tile = (int)level_data.exit_y - 2; /* 2 above exit = standing height */
        /* Validate: boss arena starts at tile (NUM_SECTIONS-1)*16 */
        int arena_start = (NUM_SECTIONS - 1) * 16;
        int arena_end = arena_start + 11;
        if (bx_tile < arena_start || bx_tile > arena_end) bx_tile = arena_start + 8;
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
    int stage = boss_state.stage;

    /* Pattern alternates: wall of projectiles vs vertical sweep */
    if ((boss_state.pattern_idx & 1) == 0) {
        /* WALL: P1=5, P2=7, P3=9 projectiles forming a wall */
        if (boss_state.phase == BPHASE_ATTACK1 && boss_state.phase_timer == 25) {
            int count = 5 + (stage - 1) * 2;
            int half = count / 2;
            for (int i = 0; i < count; i++) {
                s32 sy = e->y + (i - half) * 6 * 256;
                projectile_spawn(e->x, sy, dir, 0,
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            /* Muzzle flash particles */
            particle_burst(e->x, e->y, 3, PART_SPARK, 150, 10);
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
        /* P3: fire a second delayed wall */
        if (stage >= 3 && boss_state.phase == BPHASE_ATTACK1 && boss_state.phase_timer == 40) {
            for (int i = -2; i <= 2; i++) {
                s32 sy = e->y + i * 8 * 256;
                projectile_spawn(e->x, sy, (s16)(dir * 3 / 4), 0,
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
        }
    } else {
        /* SWEEP: 3 staggered shots (high, mid, low). P2+ adds extras */
        if (boss_state.phase == BPHASE_ATTACK1) {
            if (boss_state.phase_timer == 15 || boss_state.phase_timer == 25 ||
                boss_state.phase_timer == 35) {
                int shot = (boss_state.phase_timer - 15) / 10;
                s32 sy = e->y + (shot - 1) * 10 * 256;
                projectile_spawn(e->x, sy, dir, 0,
                    (s16)(boss_state.atk + 1), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                /* P2+: double shot */
                if (stage >= 2) {
                    projectile_spawn(e->x, sy, dir, (s16)(shot == 1 ? -96 : 96),
                        boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                }
                audio_play_sfx(SFX_SHOOT);
            }
        }
    }
    /* ATTACK2: Slow advance. P2+ faster, P3 adds barrier spin */
    if (boss_state.phase == BPHASE_ATTACK2) {
        int spd = 96 + (stage - 1) * 48;
        e->vx = e->facing ? (s16)-spd : (s16)spd;
    }
    (void)player_x; (void)player_y; (void)enraged;
}

static void attack_blackout(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));
    int stage = boss_state.stage;

    /* ATTACK1: Teleport to random position + aimed shot */
    if (boss_state.phase == BPHASE_ATTACK1) {
        if (boss_state.phase_timer == 10) {
            /* Teleport: disappear particles at old position */
            particle_burst(e->x + (s32)(e->width << 7), e->y + (s32)(e->height << 7),
                           4, PART_STAR, 200, 15);
            /* Snap to player's X region but offset */
            int ofs_px = 40 + (stage - 1) * 10;
            s32 offset = ((boss_state.pattern_idx & 1) ? ofs_px : -ofs_px) * 256;
            e->x = player_x + offset;
            if (e->x < FP8(16)) e->x = FP8(16);
            int max_x = (NET_MAP_W * 8 - e->width) * 256;
            if (e->x > max_x) e->x = max_x;
            e->vy = 0;
            /* Reappear particles at new position */
            particle_burst(e->x + (s32)(e->width << 7), e->y + (s32)(e->height << 7),
                           4, PART_STAR, 200, 15);
            audio_play_sfx(SFX_DASH);
        }
        /* P3: second teleport mid-attack */
        if (stage >= 3 && boss_state.phase_timer == 20) {
            int ofs = (boss_state.pattern_idx & 2) ? 50 : -50;
            e->x = player_x + ofs * 256;
            if (e->x < FP8(16)) e->x = FP8(16);
            int max_x = (NET_MAP_W * 8 - e->width) * 256;
            if (e->x > max_x) e->x = max_x;
            audio_play_sfx(SFX_DASH);
        }
        if (boss_state.phase_timer == 25) {
            /* Aimed shot toward player */
            int dx = (int)((player_x - e->x) >> 8);
            int dy = (int)((player_y - e->y) >> 8);
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (dist < 1) dist = 1;
            s16 pvx = (s16)(dx * 384 / dist);
            s16 pvy = (s16)(dy * 384 / dist);
            projectile_spawn(e->x, e->y, pvx, pvy,
                (s16)(boss_state.atk + 2), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            /* P2+: spread shot. P3: triple spread */
            if (stage >= 2) {
                projectile_spawn(e->x, e->y, pvx, (s16)(pvy + 160),
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            if (stage >= 3) {
                projectile_spawn(e->x, e->y, pvx, (s16)(pvy - 160),
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
    }
    /* ATTACK2: Quick dash past player. Faster per stage */
    if (boss_state.phase == BPHASE_ATTACK2) {
        int spd = 320 + (stage - 1) * 64;
        e->vx = e->facing ? (s16)-spd : (s16)spd;
    }
    (void)enraged;
}

static void attack_worm(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));
    s16 dir = e->facing ? -384 : 384;
    int stage = boss_state.stage;

    /* ATTACK1: Ground-level wave of projectiles */
    if (boss_state.phase == BPHASE_ATTACK1) {
        if (boss_state.phase_timer == 20) {
            /* P1=3, P2=5, P3=7 low projectiles along the ground */
            int count = 3 + (stage - 1) * 2;
            for (int i = 0; i < count; i++) {
                s32 sx = e->x + (e->facing ? -(i * 8 * 256) : (i * 8 * 256));
                projectile_spawn(sx, e->y + FP8(8), dir, 64,
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
        /* P2+: second wave at frame 35 */
        if (stage >= 2 && boss_state.phase_timer == 35) {
            s16 rdir = e->facing ? 384 : -384; /* Reverse direction */
            int count = 2 + stage;
            for (int i = 0; i < count; i++) {
                s32 sx = e->x + (e->facing ? (i * 8 * 256) : -(i * 8 * 256));
                projectile_spawn(sx, e->y + FP8(4), rdir, 32,
                    boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            }
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
    /* ATTACK2: Burrow charge (fast horizontal + slight bob). Faster per stage */
    if (boss_state.phase == BPHASE_ATTACK2) {
        int spd = 256 + (stage - 1) * 64;
        e->vx = e->facing ? (s16)-spd : (s16)spd;
        /* Bobbing vertical movement */
        int bob_phase = boss_state.phase_timer & 15;
        e->vy = (bob_phase < 8) ? -48 : 48;
    }
    (void)enraged;
}

static void attack_nexus(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));
    int stage = boss_state.stage;

    /* ATTACK1: Spiral projectile burst */
    if (boss_state.phase == BPHASE_ATTACK1) {
        /* Fire projectiles in expanding pattern over multiple frames.
         * P1: every 6 frames, P2: every 4, P3: every 3 */
        int fire_rate = (stage >= 3) ? 3 : (stage >= 2) ? 4 : 6;
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
    /* P3: homing shots added every 8 frames */
    if (stage >= 3 && boss_state.phase == BPHASE_ATTACK1 &&
        boss_state.phase_timer >= 15 && (boss_state.phase_timer & 7) == 0) {
        int dx = (int)((player_x - e->x) >> 8);
        int dy = (int)((player_y - e->y) >> 8);
        int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
        if (dist < 1) dist = 1;
        projectile_spawn(e->x, e->y, (s16)(dx * 256 / dist), (s16)(dy * 256 / dist),
            boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
    }

    /* ATTACK2: Float toward player (Nexus Core hovers). Faster per stage */
    if (boss_state.phase == BPHASE_ATTACK2) {
        int dx = (int)((player_x - e->x) >> 8);
        int dy = (int)((player_y - e->y) >> 8);
        int spd_h = 128 + (stage - 1) * 48;
        int spd_v = 96 + (stage - 1) * 32;
        e->vx = (dx > 0) ? (s16)spd_h : (s16)-spd_h;
        e->vy = (dy > 0) ? (s16)spd_v : (s16)-spd_v;
    }
    (void)enraged;
}

static void attack_root(Entity* e, s32 player_x, s32 player_y, int enraged) {
    int ent_idx = (int)(e - entity_get(0));
    int stage = boss_state.stage;

    /* Root Access: aggressive multi-phase final boss */
    if (boss_state.phase == BPHASE_ATTACK1) {
        /* Phase varies by pattern_idx for unpredictability */
        int variant = boss_state.pattern_idx % 3;
        if (variant == 0) {
            /* Rapid burst: P1=3, P2=5, P3=7 shots */
            int fires_at_12 = (boss_state.phase_timer == 12);
            int fires_at_20 = (boss_state.phase_timer == 20);
            int fires_at_28 = (boss_state.phase_timer == 28);
            int fires_at_16 = (stage >= 2 && boss_state.phase_timer == 16);
            int fires_at_24 = (stage >= 2 && boss_state.phase_timer == 24);
            int fires_at_8  = (stage >= 3 && boss_state.phase_timer == 8);
            int fires_at_32 = (stage >= 3 && boss_state.phase_timer == 32);
            if (fires_at_8 || fires_at_12 || fires_at_16 || fires_at_20 ||
                fires_at_24 || fires_at_28 || fires_at_32) {
                s16 dir = e->facing ? -448 : 448;
                projectile_spawn(e->x, e->y, dir, 0,
                    (s16)(boss_state.atk + 2), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                audio_play_sfx(SFX_SHOOT_RAPID);
            }
        } else if (variant == 1) {
            /* Aimed shot + spread. P3: 5-way spread */
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
                /* P3: extra wide spread */
                if (stage >= 3) {
                    projectile_spawn(e->x, e->y, pvx, (s16)(pvy - 256),
                        boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                    projectile_spawn(e->x, e->y, pvx, (s16)(pvy + 256),
                        boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                }
                audio_play_sfx(SFX_SHOOT_CHARGE);
            }
        } else {
            /* Wide fan: P1=5, P2=7, P3=9 projectiles */
            if (boss_state.phase_timer == 25) {
                s16 base_dir = e->facing ? -320 : 320;
                int half = 2 + (stage - 1);
                for (int i = -half; i <= half; i++) {
                    projectile_spawn(e->x, e->y, base_dir, (s16)(i * 96),
                        boss_state.atk, SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
                }
                audio_play_sfx(SFX_SHOOT_CHARGE);
            }
        }
    }
    /* ATTACK2: Fast charge + jump. P3: double jump */
    if (boss_state.phase == BPHASE_ATTACK2) {
        int spd = 320 + (stage - 1) * 48;
        e->vx = e->facing ? (s16)-spd : (s16)spd;
        if (boss_state.phase_timer == 20) {
            e->vy = -384;
        }
        /* P3: second jump at frame 28 */
        if (stage >= 3 && boss_state.phase_timer == 28) {
            e->vy = -320;
        }
    }
    (void)enraged;
}

static void attack_daemon(Entity* e, s32 player_x, s32 player_y, int enraged) {
    /* DAEMON copies attack patterns from previous bosses, escalating per stage:
     * Stage 1: Firewall wall + Blackout teleport (familiar patterns)
     * Stage 2: Adds Worm ground wave + Nexus spiral (harder combos)
     * Stage 3: All previous + Root rapid burst, unique corruption zones */
    int stage = boss_state.stage;
    int variant = boss_state.pattern_idx % (2 + stage); /* More variants per stage */

    if (boss_state.phase == BPHASE_ATTACK1) {
        if (variant == 0) {
            attack_firewall(e, player_x, player_y, enraged);
        } else if (variant == 1) {
            attack_blackout(e, player_x, player_y, enraged);
        } else if (variant == 2 && stage >= 2) {
            attack_worm(e, player_x, player_y, enraged);
        } else if (variant == 3 && stage >= 2) {
            attack_nexus(e, player_x, player_y, enraged);
        } else {
            /* Fallback: Root's fan attack */
            attack_root(e, player_x, player_y, enraged);
        }
        /* Stage 2+: extra aimed shot */
        if (stage >= 2 && boss_state.phase_timer == 30) {
            int ent_idx = (int)(e - entity_get(0));
            int dx = (int)((player_x - e->x) >> 8);
            int dy = (int)((player_y - e->y) >> 8);
            int dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
            if (dist < 1) dist = 1;
            s16 pvx = (s16)(dx * 384 / dist);
            s16 pvy = (s16)(dy * 384 / dist);
            projectile_spawn(e->x, e->y, pvx, pvy,
                (s16)(boss_state.atk + 3), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
            audio_play_sfx(SFX_SHOOT_CHARGE);
        }
        /* Stage 3: additional rapid fire */
        if (stage >= 3 && (boss_state.phase_timer & 7) == 4 &&
            boss_state.phase_timer >= 10 && boss_state.phase_timer <= 35) {
            int ent_idx = (int)(e - entity_get(0));
            s16 dir = e->facing ? -448 : 448;
            projectile_spawn(e->x, e->y, dir, 0,
                (s16)(boss_state.atk + 1), SUBTYPE_PROJ_ENEMY, PROJ_ENEMY, (u8)ent_idx);
        }
    }
    /* ATTACK2: Teleport + charge combo. Faster per stage */
    if (boss_state.phase == BPHASE_ATTACK2) {
        if (boss_state.phase_timer == 5) {
            int ofs = (boss_state.pattern_idx & 1) ? 48 : -48;
            e->x = player_x + ofs * 256;
            if (e->x < FP8(16)) e->x = FP8(16);
            int max_x = (NET_MAP_W * 8 - e->width) * 256;
            if (e->x > max_x) e->x = max_x;
            audio_play_sfx(SFX_DASH);
        }
        /* P3: second teleport */
        if (stage >= 3 && boss_state.phase_timer == 18) {
            int ofs = (boss_state.pattern_idx & 2) ? -40 : 40;
            e->x = player_x + ofs * 256;
            if (e->x < FP8(16)) e->x = FP8(16);
            int max_x = (NET_MAP_W * 8 - e->width) * 256;
            if (e->x > max_x) e->x = max_x;
            audio_play_sfx(SFX_DASH);
        }
        int spd = 288 + (stage - 1) * 48;
        e->vx = e->facing ? (s16)-spd : (s16)spd;
    }
    (void)enraged;
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
    { 25, 45, 35, 22 },  /* Daemon: aggressive, moderate window */
};

/* Check if boss is enraged (<10% HP) */
static int boss_is_enraged(void) {
    return (boss_state.hp > 0 && boss_state.max_hp > 0 &&
            boss_state.hp * 10 <= boss_state.max_hp);
}

/* Stage-based timing multiplier: P1=100%, P2=85%, P3=70%, enrage=50% */
static int stage_scale_dur(int base_dur, int stage) {
    int dur;
    if (stage >= 3) dur = base_dur * 7 / 10;
    else if (stage >= 2) dur = base_dur * 85 / 100;
    else dur = base_dur;
    /* Enrage: halve all durations for frantic pacing */
    if (boss_is_enraged()) dur = dur / 2;
    if (dur < 8) dur = 8; /* minimum duration */
    return dur;
}

/* Stage-based vulnerability window: P1=45f, P2=30f, P3=20f, enrage=15f */
static int stage_vuln_dur(int base_dur, int stage) {
    int dur;
    if (stage >= 3) dur = base_dur * 45 / 100;
    else if (stage >= 2) dur = base_dur * 65 / 100;
    else dur = base_dur;
    /* Enrage: tighter vulnerability windows */
    if (boss_is_enraged()) dur = dur * 2 / 3;
    if (dur < 10) dur = 10;
    return dur;
}

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
    int stage = boss_state.stage;

    /* Stage is used as the "enraged" parameter — stage 2+ = enraged behavior */
    int enraged = (stage >= 2);

    /* Check for stage transitions based on HP thresholds */
    if (boss_state.hp > 0 && boss_state.phase != BPHASE_DEAD &&
        boss_state.phase != BPHASE_TRANSITION) {
        int new_stage = 1;
        /* P2 at 60% HP, P3 at 30% HP */
        if (boss_state.hp * 10 <= boss_state.max_hp * 3) {
            new_stage = 3;
        } else if (boss_state.hp * 10 <= boss_state.max_hp * 6) {
            new_stage = 2;
        }
        if (new_stage > boss_state.stage) {
            boss_state.stage = (u8)new_stage;
            boss_state.stage_announced = 0;
            boss_state.phase = BPHASE_TRANSITION;
            boss_state.phase_timer = 0;
            e->vx = 0;
            e->vy = 0;
            audio_play_sfx(SFX_BOSS_PHASE);
            video_shake(8, 2);
            video_flash_start(2, 6);
            if (new_stage == 2) {
                hud_notify("PHASE 2!", 90);
            } else {
                hud_notify("FINAL PHASE!", 120);
            }
            /* Phase transition particle explosion */
            {
                s32 bcx = e->x + (s32)(e->width << 7);
                s32 bcy = e->y + (s32)(e->height << 7);
                particle_burst(bcx, bcy, 6, PART_BURST, 250, 18);
                particle_burst(bcx, bcy, 4, PART_STAR, 180, 16);
            }
        }
    }

    /* Check for enrage at <10% HP */
    if (boss_is_enraged() && !boss_state.enrage_announced &&
        boss_state.phase != BPHASE_DEAD && boss_state.phase != BPHASE_TRANSITION) {
        boss_state.enrage_announced = 1;
        hud_notify("ENRAGED!", 60);
        video_shake(4, 2);
        audio_play_sfx(SFX_BOSS_ROAR);
        {
            s32 bcx = e->x + (s32)(e->width << 7);
            s32 bcy = e->y + (s32)(e->height << 7);
            particle_burst(bcx, bcy, 8, PART_BURST, 300, 20);
            particle_burst(bcx, bcy, 6, PART_SPARK, 200, 14);
            particle_burst(bcx, bcy, 4, PART_STAR, 150, 18);
        }
    }

    switch (boss_state.phase) {
    case BPHASE_TRANSITION:
        /* 30-frame stagger: palette flash, then resume attack cycle */
        if (boss_state.phase_timer >= 30) {
            boss_state.phase = BPHASE_IDLE;
            boss_state.phase_timer = 0;
            boss_state.stage_announced = 1;
        }
        break;

    case BPHASE_IDLE:
    {
        /* Wait, then start attacking */
        int idle_dur = stage_scale_dur(boss_timing[bt].idle_dur, stage);
        if (boss_state.phase_timer >= (u16)idle_dur) {
            boss_state.phase = BPHASE_ATTACK1;
            boss_state.phase_timer = 0;
            audio_play_sfx(SFX_SHOOT_CHARGE); /* Attack telegraph */
            hud_notify("INCOMING!", 20);
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
        case BOSS_DAEMON:      attack_daemon(e, player_x, player_y, enraged); break;
        }
        int atk1_dur = stage_scale_dur(boss_timing[bt].attack1_dur, stage);
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
        case BOSS_DAEMON:      attack_daemon(e, player_x, player_y, enraged); break;
        }
        int atk2_dur = stage_scale_dur(boss_timing[bt].attack2_dur, stage);
        if (boss_state.phase_timer >= (u16)atk2_dur) {
            e->vx = 0;
            boss_state.phase = BPHASE_VULNERABLE;
            boss_state.phase_timer = 0;
            boss_state.hit_count = 0;
            /* Vulnerability sparkle burst — signals "attack now!" */
            hud_notify("STRIKE NOW!", 25);
            particle_burst(e->x + (s32)(e->width << 7), e->y + (s32)(e->height << 7),
                           6, PART_STAR, 180, 20);
            audio_play_sfx(SFX_ABILITY); /* Vulnerability open cue */
            video_shake(2, 1);           /* Small shake to draw attention */
        }
        break;
    }
    case BPHASE_VULNERABLE:
    {
        /* Open to damage — shorter windows in later stages */
        int vuln_dur = stage_vuln_dur(boss_timing[bt].vuln_dur, stage);

        /* Continuous vulnerability sparkles — orbiting stars every 4 frames */
        if ((boss_state.phase_timer & 3) == 0) {
            s32 bcx = e->x + ((s32)e->width << 7);
            s32 bcy = e->y + ((s32)e->height << 7);
            /* Radiate outward from boss center with slight randomness */
            s16 vx = (s16)((int)rand_range(201) - 100);
            s16 vy = (s16)((int)rand_range(201) - 100);
            particle_spawn(bcx, bcy, vx, vy, PART_STAR, 12);
        }

        if (boss_state.phase_timer >= (u16)vuln_dur) {
            boss_state.phase = BPHASE_IDLE;
            boss_state.phase_timer = 0;
            boss_state.pattern_idx++;
        }
        break;
    }

    case BPHASE_DEAD:
    {
        /* Sequential explosion death animation over 60 frames */
        s32 bcx = e->x + ((s32)e->width << 7);
        s32 bcy = e->y + ((s32)e->height << 7);

        /* Sub-explosions at intervals */
        if (boss_state.phase_timer == 5 || boss_state.phase_timer == 15 ||
            boss_state.phase_timer == 25 || boss_state.phase_timer == 40) {
            /* Random offset within boss hitbox */
            s32 ox = (s32)((int)rand_range((u32)e->width) - (int)(e->width / 2)) * 256;
            s32 oy = (s32)((int)rand_range((u32)e->height) - (int)(e->height / 2)) * 256;
            particle_burst(bcx + ox, bcy + oy, 2, PART_BURST, 180, 12);
            video_shake(2, 1);
        }

        /* Final big explosion */
        if (boss_state.phase_timer == 55) {
            particle_burst(bcx, bcy, 4, PART_BURST, 250, 20);
            particle_burst(bcx, bcy, 3, PART_STAR, 200, 16);
            video_shake(6, 2);
            video_flash_start(2, 6);
            audio_play_sfx(SFX_BOSS_EXPLODE);
        }

        if (boss_state.phase_timer >= 60) {
            boss_state.defeated = 1;
            boss_state.active = 0;
            entity_despawn(e);
            boss_entity = NULL;
            hud_set_boss(NULL, 0, 0);
            return;
        }
        break;
    }

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

    /* Off-screen culling — prevent ghost sprites from Y/X wrapping */
    if (sx < -32 || sx > SCREEN_W + 32 || sy < -32 || sy > SCREEN_H + 32) {
        OBJ_ATTR* oam = sprite_get(e->oam_index);
        if (oam) oam->attr0 = ATTR0_HIDE;
        return;
    }

    /* ---- Boss palette effects (layered: base → damage → phase effects) ---- */
    {
        const u16* base = pal_boss[boss_state.type];
        int hp_pct = (boss_state.max_hp > 0) ? (boss_state.hp * 100 / boss_state.max_hp) : 100;

        /* Base palette with damage tint: shift toward red as HP drops */
        if (hp_pct < 60 && boss_state.phase != BPHASE_TRANSITION) {
            /* Intensity ramps up: 60-30% mild, 30-0% heavy */
            int shift = (hp_pct < 30) ? 6 : 3;
            for (int i = 1; i < 16; i++) {
                u16 c = base[i];
                int r = (c & 0x1F);
                int g = (c >> 5) & 0x1F;
                int b = (c >> 10) & 0x1F;
                r = r + shift; if (r > 31) r = 31;
                g = g - shift / 2; if (g < 0) g = 0;
                b = b - shift / 2; if (b < 0) b = 0;
                pal_obj_mem[BOSS_PAL_BANK * 16 + i] = (u16)(r | (g << 5) | (b << 10));
            }
        } else if (boss_state.phase != BPHASE_TRANSITION) {
            /* Restore base palette */
            memcpy16(&pal_obj_mem[BOSS_PAL_BANK * 16], base, 16);
        }

        /* Per-stage palette darkening: stage 2 slightly darker, stage 3 more intense */
        if (boss_state.stage >= 2 && boss_state.phase != BPHASE_TRANSITION) {
            int dark = (boss_state.stage == 3) ? 2 : 1;
            /* Darken body colors (1-4), intensify energy colors (5-7) */
            for (int i = 1; i <= 4; i++) {
                u16 c = pal_obj_mem[BOSS_PAL_BANK * 16 + i];
                int r = (c & 0x1F) - dark; if (r < 0) r = 0;
                int g = ((c >> 5) & 0x1F) - dark; if (g < 0) g = 0;
                int b = ((c >> 10) & 0x1F) - dark; if (b < 0) b = 0;
                pal_obj_mem[BOSS_PAL_BANK * 16 + i] = (u16)(r | (g << 5) | (b << 10));
            }
            for (int i = 5; i <= 7; i++) {
                u16 c = pal_obj_mem[BOSS_PAL_BANK * 16 + i];
                int r = (c & 0x1F) + dark; if (r > 31) r = 31;
                int g = ((c >> 5) & 0x1F) + dark; if (g > 31) g = 31;
                int b = ((c >> 10) & 0x1F) + dark; if (b > 31) b = 31;
                pal_obj_mem[BOSS_PAL_BANK * 16 + i] = (u16)(r | (g << 5) | (b << 10));
            }
        }

        /* Phase transition flash: white pulse */
        if (boss_state.phase == BPHASE_TRANSITION) {
            if (boss_state.phase_timer < 15 && (boss_state.phase_timer & 1)) {
                for (int i = 1; i < 16; i++)
                    pal_obj_mem[BOSS_PAL_BANK * 16 + i] = RGB15(31, 31, 31);
            } else {
                memcpy16(&pal_obj_mem[BOSS_PAL_BANK * 16], base, 16);
            }
        }

        /* Attack telegraph: brighten toward white during first 8 frames of attacks */
        if ((boss_state.phase == BPHASE_ATTACK1 || boss_state.phase == BPHASE_ATTACK2) &&
            boss_state.phase_timer < 8 && (boss_state.phase_timer & 1)) {
            for (int i = 5; i <= 7; i++)
                pal_obj_mem[BOSS_PAL_BANK * 16 + i] = RGB15(31, 31, 31);
        }

        /* Vulnerability: golden glow across energy colors */
        if (boss_state.phase == BPHASE_VULNERABLE && (boss_state.phase_timer & 2)) {
            pal_obj_mem[BOSS_PAL_BANK * 16 + 4] = RGB15(28, 24, 4);
            pal_obj_mem[BOSS_PAL_BANK * 16 + 5] = RGB15(31, 28, 8);
            pal_obj_mem[BOSS_PAL_BANK * 16 + 6] = RGB15(28, 22, 4);
            pal_obj_mem[BOSS_PAL_BANK * 16 + 7] = RGB15(31, 30, 12);
        }

        /* Enrage: pulsing red glow on entire palette + periodic shake */
        if (boss_is_enraged() && boss_state.phase != BPHASE_DEAD &&
            boss_state.phase != BPHASE_TRANSITION) {
            /* Red pulse on 8-frame cycle */
            int pulse = (boss_state.phase_timer & 7);
            int intensity = (pulse < 4) ? pulse : (7 - pulse); /* 0-3-0 wave */
            for (int i = 1; i < 16; i++) {
                u16 c = pal_obj_mem[BOSS_PAL_BANK * 16 + i];
                int r = (c & 0x1F) + intensity * 2; if (r > 31) r = 31;
                int g = ((c >> 5) & 0x1F);
                int b = ((c >> 10) & 0x1F);
                pal_obj_mem[BOSS_PAL_BANK * 16 + i] = (u16)(r | (g << 5) | (b << 10));
            }
            /* Periodic screen shake every 60 frames */
            if ((boss_state.phase_timer % 60) == 0) {
                video_shake(4, 1);
            }
        }
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
    boss_state.hp -= (s16)actual;
    boss_state.hit_count++;
    audio_play_sfx(SFX_ENEMY_HIT);
    video_shake(2, 1);
    video_hit_flash_start(7, 1); /* Boss palette bank = 7 */
    /* Floating damage number at boss position */
    {
        Entity* be = boss_get_entity();
        if (be && actual > 0) {
            hud_floattext_spawn(be->x, be->y - FP8(8), actual, 0);
        }
    }

    if (boss_state.hp <= 0) {
        boss_state.hp = 0;
        boss_state.phase = BPHASE_DEAD;
        boss_state.phase_timer = 0;
        audio_play_sfx(SFX_ENEMY_DIE);
        video_shake(8, 3);
        /* Massive death explosion: 8 burst fragments + 4 sparkles */
        {
            Entity* be_tmp = boss_get_entity();
            if (be_tmp) {
                s32 bcx = be_tmp->x + ((s32)be_tmp->width << 7);
                s32 bcy = be_tmp->y + ((s32)be_tmp->height << 7);
                particle_burst(bcx, bcy, 8, PART_BURST, 300, 30);
                particle_burst(bcx, bcy, 4, PART_STAR, 150, 24);
            }
        }

        hud_notify("BOSS DEFEATED!", 120);

        /* Award XP: 100 base + 50 per boss type (higher bosses = more XP) */
        player_add_xp(100 + boss_state.type * 50);

        /* Award credits: 50 base + 30 per boss type */
        {
            int creds = 50 + boss_state.type * 30;
            /* Skill tree: utility branch index 3 = credits bonus (+5/10/15%) */
            int cr_bonus = player_state.skill_tree[11] * 5;
            if (cr_bonus > 0) creds = creds * (100 + cr_bonus) / 100;
            player_state.credits += (u16)creds;
        }

        /* Drop loot at boss position */
        Entity* be = boss_get_entity();
        if (be) {
            int tier = boss_state.type + 2; /* Boss tier = type + 2 for better drops */
            /* Endgame boss contracts: mythic drops */
            int rarity_floor = RARITY_RARE;
            if (game_stats.endgame_unlocked && game_stats.bb_endgame_contracts > 0 &&
                (game_stats.bb_endgame_contracts % 25) == 0) {
                rarity_floor = RARITY_MYTHIC;
            }
            itemdrop_roll(be->x, be->y, tier, rarity_floor, player_state.lck);
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
