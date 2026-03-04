/*
 * Ghost Protocol — Net World Tile Management
 *
 * Handles BG1 tileset and column streaming for scrolling levels.
 * BG1 is 64x32 tiles (two screenblocks: SBB 28+29).
 * Level is 128 tiles wide — stream columns as camera scrolls.
 */
#include "game/networld.h"
#include "game/levelgen.h"
#include "game/common.h"
#include <string.h>

/* Net tileset (NTILE_COUNT unique 8x8 tiles) loaded into CBB1
 * Palette: 0=void, 1=wall shadow, 2=wall base, 3=wall highlight,
 *   4=wall rim, 5=floor shadow, 6=floor base, 7=floor highlight,
 *   8=circuit dark, 9=circuit bright, A=hazard dark, B=hazard bright,
 *   C=gold dark, D=gold bright, E=data dim, F=warm white */
static const u32 net_tiles[NTILE_COUNT][8] = {
    [NTILE_EMPTY] = { 0, 0, 0, 0, 0, 0, 0, 0 },
    /* --- Structural --- */
    [NTILE_FLOOR] = {
        0x99999999, 0x56565656, 0x55555555, 0x56755675,
        0x55555555, 0x65556555, 0x55555555, 0x11111111,
    },
    [NTILE_FLOOR_CIRCUIT] = {
        0x98989898, 0x56565656, 0x55585555, 0x55589555,
        0x55558955, 0x55558555, 0x55555555, 0x11111111,
    },
    [NTILE_FLOOR_CRACKED] = {
        0x99999999, 0x56505656, 0x55055555, 0x50575675,
        0x55550555, 0x65556505, 0x55555055, 0x11111111,
    },
    [NTILE_FLOOR_GRATE] = {
        0x11111111, 0x50505050, 0x05050505, 0x50505050,
        0x05050505, 0x50505050, 0x05050505, 0x11111111,
    },
    [NTILE_WALL] = {
        0x11111111, 0x12332133, 0x12332133, 0x11121112,
        0x11111111, 0x21331233, 0x21331233, 0x11211121,
    },
    [NTILE_WALL_PANEL] = {
        0x11111111, 0x12222221, 0x12333321, 0x12322321,
        0x12322321, 0x12333321, 0x12222221, 0x11111111,
    },
    [NTILE_WALL_CIRCUIT] = {
        0x11881188, 0x18881888, 0x18991888, 0x11891188,
        0x88111188, 0x88818881, 0x11118881, 0x11111188,
    },
    [NTILE_WALL_CAP_TOP] = {
        0x44444444, 0x43333334, 0x12222221, 0x12221222,
        0x11211121, 0x11111111, 0x22122212, 0x11211121,
    },
    [NTILE_WALL_CAP_BOT] = {
        0x11211121, 0x22122212, 0x11111111, 0x12221222,
        0x12222221, 0x43333334, 0x44444444, 0x00000000,
    },
    [NTILE_PLATFORM] = {
        0xCCCCCCCC, 0x89898989, 0x56775677, 0x56565656,
        0x55855585, 0x56565656, 0x55155515, 0x11111111,
    },
    [NTILE_PLATFORM_ENERGY] = {
        0xFFFFFFFF, 0x89998999, 0x59995999, 0x56965696,
        0x55985598, 0x56565656, 0x55155515, 0x11111111,
    },
    /* --- Wall detail --- */
    [NTILE_PIPE_H] = {
        0x00000000, 0x00000000, 0x11111111, 0x23232323,
        0x32323232, 0x11111111, 0x00000000, 0x00000000,
    },
    [NTILE_PIPE_V] = {
        0x00120000, 0x00230000, 0x00120000, 0x00230000,
        0x00120000, 0x00230000, 0x00120000, 0x00230000,
    },
    [NTILE_VENT] = {
        0x11111111, 0x10201020, 0x12021202, 0x10201020,
        0x12021202, 0x10201020, 0x12021202, 0x11111111,
    },
    [NTILE_SCREEN] = {
        0x11111111, 0x18989891, 0x19899891, 0x18989891,
        0x19899891, 0x18989891, 0x11111111, 0x00010000,
    },
    [NTILE_WINDOW] = {
        0x11111111, 0x10000001, 0x10000001, 0x10000001,
        0x10000001, 0x10000001, 0x10000001, 0x11111111,
    },
    [NTILE_JUNCTION_BOX] = {
        0x11111111, 0x12898921, 0x12989821, 0x12899821,
        0x12988921, 0x12898921, 0x12222221, 0x11111111,
    },
    /* --- Hazards --- */
    [NTILE_HAZARD_SPIKE] = {
        0x0000F000, 0x000AFA00, 0x000BFB00, 0x00ABFBA0,
        0x00BFFFB0, 0x0ABFFFBA, 0x0BFFFFFB, 0xABFFFBBA,
    },
    [NTILE_HAZARD_BEAM] = {
        0x000FF000, 0x00AFFA00, 0x0ABFFBA0, 0xABFFFFBA,
        0xABFFFFBA, 0x0ABFFBA0, 0x00AFFA00, 0x000FF000,
    },
    [NTILE_HAZARD_TESLA] = {
        0x000FF000, 0x00FBBF00, 0x0FB99BF0, 0x0F0990F0,
        0x00F00F00, 0x0F0000F0, 0xF000000F, 0x11111111,
    },
    [NTILE_HAZARD_BEAM_OFF] = {
        0x000AA000, 0x00A11A00, 0x0A1111A0, 0xA111111A,
        0x0A1111A0, 0x00A11A00, 0x000AA000, 0x00001000,
    },
    /* --- Edge/transition --- */
    [NTILE_FLOOR_EDGE_L] = {
        0x19999999, 0x15656565, 0x15555555, 0x15675567,
        0x15555555, 0x16555655, 0x15555555, 0x11111111,
    },
    [NTILE_FLOOR_EDGE_R] = {
        0x99999991, 0x56565651, 0x55555551, 0x56755671,
        0x55555551, 0x65556551, 0x55555551, 0x11111111,
    },
    [NTILE_CORNER_TL] = {
        0x44444444, 0x43333334, 0x13222221, 0x12221222,
        0x15555555, 0x15655655, 0x15555555, 0x11111111,
    },
    [NTILE_CORNER_TR] = {
        0x44444444, 0x43333334, 0x12222231, 0x22212221,
        0x55555551, 0x55655651, 0x55555551, 0x11111111,
    },
    [NTILE_CORNER_BL] = {
        0x11111111, 0x12221222, 0x13222221, 0x43333334,
        0x44444444, 0x00000000, 0x00000000, 0x00000000,
    },
    [NTILE_CORNER_BR] = {
        0x11111111, 0x22212221, 0x12222231, 0x43333334,
        0x44444444, 0x00000000, 0x00000000, 0x00000000,
    },
    [NTILE_PLAT_EDGE_L] = {
        0x1CCCCCCC, 0x18989898, 0x15677567, 0x15656565,
        0x15585558, 0x15656565, 0x15155515, 0x11111111,
    },
    [NTILE_PLAT_EDGE_R] = {
        0xCCCCCCC1, 0x89898981, 0x56775671, 0x56565651,
        0x55855581, 0x56565651, 0x55155151, 0x11111111,
    },
    /* --- Decorative --- */
    [NTILE_CABLE_H] = {
        0x00000000, 0x00000000, 0x11111111, 0x18989891,
        0x89F989F9, 0x11111111, 0x00000000, 0x00000000,
    },
    [NTILE_CABLE_V] = {
        0x00110000, 0x00810000, 0x00910000, 0x00F10000,
        0x00910000, 0x00810000, 0x00110000, 0x00910000,
    },
    [NTILE_CABLE_CORNER] = {
        0x00010000, 0x00810000, 0x00011111, 0x00098989,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
    },
    [NTILE_DATA_STREAM] = {
        0x000E0000, 0x0E00F0E0, 0x00E00E00, 0xE0FE00FE,
        0x00E00E00, 0x0F0000E0, 0x000E0F00, 0x0000E000,
    },
    [NTILE_SERVER_TOP] = {
        0x11111111, 0x12898921, 0x12989821, 0x11111111,
        0x12898921, 0x12989821, 0x11111111, 0x12333321,
    },
    [NTILE_SERVER_BOT] = {
        0x12333321, 0x11111111, 0x12898921, 0x12989821,
        0x11111111, 0x12898921, 0x12989821, 0x11111111,
    },
    [NTILE_CONSOLE] = {
        0x11111111, 0x18989891, 0x19899891, 0x18989891,
        0x1CD11DC1, 0x01CCCC10, 0x01CDDC10, 0x01111110,
    },
    [NTILE_CIRCUIT_NODE] = {
        0x00080000, 0x00898000, 0x089F9800, 0x89F9F980,
        0x089F9800, 0x00898000, 0x00080000, 0x00000000,
    },
    [NTILE_MEMORY_BANK] = {
        0x11111111, 0x12322321, 0x12388321, 0x12322321,
        0x11111111, 0x12322321, 0x12388321, 0x12322321,
    },
    [NTILE_CONDUIT] = {
        0x00190000, 0x00991000, 0x09999100, 0x09999100,
        0x00991000, 0x00190000, 0x00010000, 0x00010000,
    },
    [NTILE_BROKEN_PANEL] = {
        0x11101111, 0x12220221, 0x12030321, 0x10322021,
        0x12022321, 0x12300321, 0x12220221, 0x11111011,
    },
    [NTILE_GLITCH] = {
        0x00890000, 0x98009800, 0x00980089, 0x89000000,
        0x00009800, 0x09890000, 0x00000989, 0x98000000,
    },
    /* --- Section atmosphere --- */
    [NTILE_CORRIDOR_GREEBLE] = {
        0x11111111, 0x12121212, 0x11131113, 0x12121212,
        0x11111111, 0x21212121, 0x31113111, 0x21212121,
    },
    [NTILE_SHAFT_RIVET] = {
        0x00000000, 0x00000000, 0x00122100, 0x01233210,
        0x01233210, 0x00122100, 0x00000000, 0x00000000,
    },
    [NTILE_ARENA_BORDER] = {
        0xCDCDCDCD, 0xD1111111, 0xC1222221, 0xD1222221,
        0xC1222221, 0xD1222221, 0xC1111111, 0xCDCDCDCD,
    },
    [NTILE_BOSS_FLOOR] = {
        0xCDCDCDCD, 0x56565656, 0x55855585, 0x58955895,
        0x55855585, 0x56565656, 0x55555555, 0x11111111,
    },
    [NTILE_DESCENT_RAIL] = {
        0x10000000, 0x21000000, 0x10000000, 0x21000000,
        0x10000000, 0x21000000, 0x10000000, 0x21000000,
    },
    [NTILE_MAZE_JUNCTION] = {
        0x89999989, 0x56565656, 0x55585855, 0x58955985,
        0x55855855, 0x56565656, 0x55555555, 0x11111111,
    },
    [NTILE_DATA_WATERFALL] = {
        0x0E000F00, 0x0FE000E0, 0xE000E000, 0x000F000E,
        0x0E000E00, 0x00E0F0E0, 0xF000E000, 0x000E000F,
    },
    [NTILE_NEON_SIGN] = {
        0x11111111, 0x1CCDDCC1, 0x1C9999C1, 0x1D9FF9D1,
        0x1C9999C1, 0x1CCDDCC1, 0x11111111, 0x00000000,
    },
    /* --- Functional --- */
    [NTILE_LADDER] = {
        0x81000018, 0x81888818, 0x81000018, 0x81000018,
        0x81888818, 0x81000018, 0x81000018, 0x81888818,
    },
    [NTILE_BREAKABLE] = {
        0x22222222, 0x23212321, 0x32123212, 0x22222222,
        0x21232123, 0x12321232, 0x22222222, 0x11111111,
    },
    [NTILE_BREAKABLE_CRACKED] = {
        0x22202222, 0x23012321, 0x30123012, 0x22022222,
        0x21230123, 0x12301232, 0x22222022, 0x11111111,
    },
    [NTILE_EXIT_FRAME] = {
        0xCDCDCDCD, 0xC1111111, 0xD1000001, 0xC1000001,
        0xD1000001, 0xC1000001, 0xC1111111, 0xCDCDCDCD,
    },
    [NTILE_EXIT_GATE] = {
        0xCDCDCDCD, 0xC9D9D9C1, 0xD9CDCDC1, 0xC9D9FFDC,
        0xD9D9FFDC, 0xC9CDCDC1, 0xC9D9D9C1, 0xCDCDCDCD,
    },
    [NTILE_SPAWN] = { 0, 0, 0, 0, 0, 0, 0, 0 },
};

/* Act-themed BG palettes — 6 variations (0=freelance, 1-5=acts)
 * 0=void, 1-4=wall ramp, 5-7=floor ramp, 8-9=circuit glow,
 * A-B=hazard, C-D=gold accent, E=data dim, F=warm white */
static const u16 act_palettes[6][16] = {
    /* Act 0: Freelance — Deep purple/cyan (default) */
    {
        RGB15_C(2, 1, 5),     RGB15_C(4, 4, 10),    RGB15_C(6, 8, 16),
        RGB15_C(10, 14, 22),  RGB15_C(16, 20, 26),  RGB15_C(5, 6, 12),
        RGB15_C(8, 10, 18),   RGB15_C(12, 16, 24),  RGB15_C(0, 18, 22),
        RGB15_C(0, 28, 31),   RGB15_C(22, 4, 2),    RGB15_C(31, 8, 4),
        RGB15_C(22, 18, 4),   RGB15_C(31, 26, 8),   RGB15_C(4, 10, 16),
        RGB15_C(31, 31, 28),
    },
    /* Act 1: The Glitch — Green/teal corruption */
    {
        RGB15_C(1, 4, 2),     RGB15_C(2, 8, 4),     RGB15_C(4, 12, 6),
        RGB15_C(6, 18, 10),   RGB15_C(10, 24, 16),  RGB15_C(3, 8, 5),
        RGB15_C(5, 12, 8),    RGB15_C(8, 18, 12),   RGB15_C(0, 20, 14),
        RGB15_C(4, 31, 20),   RGB15_C(22, 4, 2),    RGB15_C(31, 8, 4),
        RGB15_C(18, 22, 4),   RGB15_C(26, 31, 8),   RGB15_C(2, 14, 8),
        RGB15_C(28, 31, 28),
    },
    /* Act 2: Traceback — Blue trace paths */
    {
        RGB15_C(1, 2, 6),     RGB15_C(3, 4, 12),    RGB15_C(5, 8, 18),
        RGB15_C(8, 12, 24),   RGB15_C(12, 18, 28),  RGB15_C(4, 5, 14),
        RGB15_C(6, 8, 20),    RGB15_C(10, 14, 26),  RGB15_C(4, 12, 28),
        RGB15_C(8, 20, 31),   RGB15_C(22, 4, 2),    RGB15_C(31, 8, 4),
        RGB15_C(16, 20, 28),  RGB15_C(22, 28, 31),  RGB15_C(4, 8, 20),
        RGB15_C(28, 30, 31),
    },
    /* Act 3: Deep Packet — Amber/orange data streams */
    {
        RGB15_C(4, 2, 1),     RGB15_C(8, 4, 2),     RGB15_C(14, 8, 4),
        RGB15_C(20, 12, 6),   RGB15_C(26, 18, 10),  RGB15_C(8, 5, 3),
        RGB15_C(12, 8, 4),    RGB15_C(18, 12, 6),   RGB15_C(22, 14, 0),
        RGB15_C(31, 22, 4),   RGB15_C(24, 4, 2),    RGB15_C(31, 10, 4),
        RGB15_C(24, 18, 4),   RGB15_C(31, 26, 8),   RGB15_C(14, 8, 2),
        RGB15_C(31, 28, 20),
    },
    /* Act 4: Zero Day — Red corruption bleeding */
    {
        RGB15_C(5, 1, 1),     RGB15_C(10, 3, 3),    RGB15_C(16, 5, 5),
        RGB15_C(22, 8, 8),    RGB15_C(28, 12, 12),  RGB15_C(10, 4, 4),
        RGB15_C(14, 6, 6),    RGB15_C(20, 10, 10),  RGB15_C(24, 4, 4),
        RGB15_C(31, 10, 8),   RGB15_C(28, 4, 2),    RGB15_C(31, 12, 4),
        RGB15_C(24, 14, 4),   RGB15_C(31, 20, 8),   RGB15_C(16, 4, 4),
        RGB15_C(31, 28, 26),
    },
    /* Act 5: Ghost Protocol — Purple/white transcendence */
    {
        RGB15_C(3, 1, 6),     RGB15_C(6, 3, 12),    RGB15_C(10, 6, 20),
        RGB15_C(16, 10, 26),  RGB15_C(22, 16, 30),  RGB15_C(6, 4, 14),
        RGB15_C(10, 7, 20),   RGB15_C(16, 12, 26),  RGB15_C(14, 4, 24),
        RGB15_C(22, 12, 31),  RGB15_C(24, 4, 4),    RGB15_C(31, 10, 8),
        RGB15_C(20, 16, 28),  RGB15_C(28, 24, 31),  RGB15_C(8, 4, 18),
        RGB15_C(31, 30, 31),
    },
};

/* Parallax BG tiles — distant cyberpunk cityscape (8 tiles in CBB2) */
#define PARA_TILE_COUNT 8
static const u32 parallax_tiles[PARA_TILE_COUNT][8] = {
    /* Tile 0: empty sky with stars */
    { 0x00000000, 0x00020000, 0x00000000, 0x00000020,
      0x02000000, 0x00000000, 0x00000200, 0x00000000, },
    /* Tile 1: tall building silhouette */
    { 0x00011000, 0x00311300, 0x00311300, 0x00311300,
      0x00311300, 0x00311300, 0x00311300, 0x00311300, },
    /* Tile 2: short building silhouette */
    { 0x00000000, 0x00000000, 0x00000000, 0x01111110,
      0x01311310, 0x01311310, 0x01311310, 0x01311310, },
    /* Tile 3: medium building with antenna */
    { 0x00010000, 0x00010000, 0x01111100, 0x01311310,
      0x01311310, 0x01311310, 0x01311310, 0x01311310, },
    /* Tile 4: building base with window glow */
    { 0x01311310, 0x01344310, 0x01311310, 0x01344310,
      0x01311310, 0x01344310, 0x01311310, 0x11111111, },
    /* Tile 5: data flow at base */
    { 0x11111111, 0x15000051, 0x00500500, 0x05000050,
      0x50050005, 0x00500500, 0x05000050, 0x11111111, },
    /* Tile 6: skyline gap (street) */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x11111111, },
    /* Tile 7: building top with light */
    { 0x00040000, 0x00141000, 0x01111100, 0x01311310,
      0x01311310, 0x01311310, 0x01311310, 0x01311310, },
};
/* Act-themed parallax palettes (6 variations) */
static const u16 parallax_palettes[6][16] = {
    /* Act 0: Freelance — cool purple/cyan */
    { RGB15_C(1,0,3), RGB15_C(3,3,8), RGB15_C(5,6,14), RGB15_C(5,5,12),
      RGB15_C(8,14,22), RGB15_C(2,12,16), 0,0,0,0,0,0,0,0,0,0 },
    /* Act 1: The Glitch — green/teal */
    { RGB15_C(0,2,1), RGB15_C(2,6,3), RGB15_C(3,10,6), RGB15_C(3,8,4),
      RGB15_C(4,18,10), RGB15_C(2,14,8), 0,0,0,0,0,0,0,0,0,0 },
    /* Act 2: Traceback — blue */
    { RGB15_C(0,1,4), RGB15_C(2,3,10), RGB15_C(4,6,16), RGB15_C(3,4,12),
      RGB15_C(6,12,24), RGB15_C(2,8,18), 0,0,0,0,0,0,0,0,0,0 },
    /* Act 3: Deep Packet — amber */
    { RGB15_C(3,1,0), RGB15_C(6,3,1), RGB15_C(10,6,2), RGB15_C(8,5,2),
      RGB15_C(18,12,4), RGB15_C(12,8,2), 0,0,0,0,0,0,0,0,0,0 },
    /* Act 4: Zero Day — red */
    { RGB15_C(3,0,0), RGB15_C(8,2,2), RGB15_C(12,4,4), RGB15_C(10,3,3),
      RGB15_C(20,6,4), RGB15_C(14,4,2), 0,0,0,0,0,0,0,0,0,0 },
    /* Act 5: Ghost Protocol — purple/white */
    { RGB15_C(2,0,4), RGB15_C(5,2,10), RGB15_C(8,4,16), RGB15_C(6,3,12),
      RGB15_C(14,8,24), RGB15_C(8,4,18), 0,0,0,0,0,0,0,0,0,0 },
};

/* Column streaming state */
static int last_streamed_left = -1;
static int last_streamed_right = -1;

static void write_column_to_vram(int level_col) {
    if (level_col < 0 || level_col >= NET_MAP_W) return;

    /* Map level column to BG1 screenblock column (wrapping at 64) */
    int sbb_col = level_col & 63;
    /* Which screenblock? SBB28 for cols 0-31, SBB29 for cols 32-63 */
    int sbb = (sbb_col < 32) ? 28 : 29;
    int local_col = sbb_col & 31;

    u16* map = (u16*)se_mem[sbb];

    for (int row = 0; row < NET_MAP_H; row++) {
        int tile_id = levelgen_tile_at(level_col, row);
        if (tile_id < 0 || tile_id >= NTILE_COUNT) tile_id = NTILE_EMPTY;
        /* Palette bank 1 for CBB1 tiles */
        map[row * 32 + local_col] = (u16)((u16)tile_id | (1 << 12));
    }
}

void networld_load_tileset(int act) {
    if (act < 0 || act > 5) act = 0;

    /* Load Net tiles into CBB1 */
    memcpy16(&tile_mem[1][0], net_tiles, sizeof(net_tiles) / 2);

    /* Load act-themed palette into BG palette bank 1 */
    memcpy16(&pal_bg_mem[1 * 16], act_palettes[act], 16);

    last_streamed_left = -1;
    last_streamed_right = -1;
}

void networld_load_visible(int cam_px) {
    int start_col = cam_px / 8;
    if (start_col < 0) start_col = 0;
    int end_col = start_col + 31; /* 30 columns visible + 1 buffer */
    if (end_col >= NET_MAP_W) end_col = NET_MAP_W - 1;

    for (int col = start_col; col <= end_col; col++) {
        write_column_to_vram(col);
    }

    last_streamed_left = start_col;
    last_streamed_right = end_col;
}

void networld_stream_columns(int cam_px) {
    int left_col = cam_px / 8;
    if (left_col < 0) left_col = 0;
    int right_col = left_col + 32; /* stream one screen ahead */
    if (right_col >= NET_MAP_W) right_col = NET_MAP_W - 1;

    /* Stream new right columns.
     * If adding a column would exceed the 64-slot VRAM ring, advance the left
     * marker so we never have two world-columns aliased to the same VRAM slot. */
    while (last_streamed_right < right_col) {
        last_streamed_right++;
        write_column_to_vram(last_streamed_right);
        if (last_streamed_right - last_streamed_left >= 64)
            last_streamed_left++;
    }

    /* Stream new left columns (camera scrolling left).
     * Mirror the ring invariant: bump the right marker if the ring would overflow.
     * This also invalidates stale right-side data that was overwritten by left writes. */
    while (last_streamed_left > left_col) {
        last_streamed_left--;
        write_column_to_vram(last_streamed_left);
        if (last_streamed_right - last_streamed_left >= 64)
            last_streamed_right--;
    }

    /* Camera moved right: advance left marker (those columns are off-screen). */
    if (last_streamed_left < left_col)
        last_streamed_left = left_col;
}

void networld_load_parallax(u16 seed, int act) {
    if (act < 0 || act > 5) act = 0;

    /* Load 8 parallax tiles into CBB2 tiles 1-8 */
    memcpy16(&tile_mem[2][1], parallax_tiles, sizeof(parallax_tiles) / 2);
    /* Load act-themed parallax palette into BG palette bank 6 */
    memcpy16(&pal_bg_mem[6 * 16], parallax_palettes[act], 16);

    /* Build cityscape pattern in SBB30 (32x32)
     * Seed varies the skyline silhouette for per-run variety.
     * Top rows (0-23): sky with stars (tile 0)
     * Row 24-27: building tops (tiles 1,3,7,2 seeded)
     * Row 28-30: building bodies (tile 4 repeating)
     * Row 31: base with data flow (tile 5) and streets (tile 6) */
    u16* map = (u16*)se_mem[30];
    u16 pal6 = (6 << 12);

    /* Simple hash for deterministic pseudo-random from seed */
    u32 h = (u32)seed * 2654435761u;

    /* Sky rows — sparse stars, pattern seeded */
    for (int r = 0; r < 24; r++) {
        for (int c = 0; c < 32; c++) {
            int t = 0;
            u32 hash = (u32)(r * 37 + c * 13) + h;
            hash ^= hash >> 7;
            /* Sparse stars in upper sky, denser near skyline */
            int threshold = (r < 12) ? 24 : 8;
            if ((hash & 31) < 2 && (int)(hash & 15) < threshold) t = 0;
            else if ((((u32)(r + c) + (h & 7)) & 7) == 0 && r >= 8) t = 1;
            map[r * 32 + c] = (u16)((u16)t | pal6);
        }
    }

    /* Generate seeded skyline — 16 entries, varies per contract */
    u8 skyline[16];
    {
        static const u8 top_tiles[] = { 1, 2, 3, 7 }; /* Building tops */
        u32 sh = h;
        for (int i = 0; i < 16; i++) {
            sh = sh * 1103515245u + 12345;
            int r5 = (int)((sh >> 16) & 7);
            if (r5 < 3) {
                skyline[i] = top_tiles[(sh >> 8) & 3];
            } else if (r5 < 5) {
                skyline[i] = 4; /* building body */
            } else {
                skyline[i] = 6; /* street gap */
            }
        }
        /* Ensure at least 2 street gaps for visual rhythm */
        skyline[(h >> 4) & 15] = 6;
        skyline[((h >> 8) + 7) & 15] = 6;
    }

    /* Building top rows — seeded cityscape silhouette */
    for (int r = 24; r < 28; r++) {
        for (int c = 0; c < 32; c++) {
            int t = skyline[c & 15];
            if (r >= 26 && (t == 6)) t = 6;
            else if (r > 24 && t != 6) t = 4;
            map[r * 32 + c] = (u16)((u16)t | pal6);
        }
    }

    /* Building body rows */
    for (int r = 28; r < 31; r++) {
        for (int c = 0; c < 32; c++) {
            int t = skyline[c & 15];
            map[r * 32 + c] = (u16)((u16)(t == 6 ? 6 : 4) | pal6);
        }
    }

    /* Base row with data flow */
    for (int c = 0; c < 32; c++) {
        map[31 * 32 + c] = (u16)(5 | pal6);
    }
}
