/*
 * Ghost Protocol — Net World Tile Management
 *
 * Handles BG1 tileset and column streaming for scrolling levels.
 * BG1 is 64x32 tiles (two screenblocks: SBB 28+29).
 * Level is 256 tiles wide — stream columns as camera scrolls via ring buffer.
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
    [NTILE_EMPTY] = {
        /* Subtle void texture: very sparse dim dots suggest digital space */
        0x00000000, 0x00000000, 0x00000100, 0x00000000,
        0x00100000, 0x00000000, 0x00000000, 0x01000000,
    },
    /* --- Structural --- */
    [NTILE_FLOOR] = {
        /* Metal plating: beveled edges, rivet detail, subtle shading */
        0x77666677, 0x76556567, 0x65565556, 0x65585556,
        0x65565556, 0x65556556, 0x76556567, 0x57666675,
    },
    [NTILE_FLOOR_CIRCUIT] = {
        /* Floor with glowing circuit traces running through metal */
        0x77666677, 0x76556567, 0x65589556, 0x65898956,
        0x65589556, 0x65569856, 0x76558967, 0x57666675,
    },
    [NTILE_FLOOR_CRACKED] = {
        /* Damaged metal floor: cracks revealing void beneath */
        0x77606677, 0x76050567, 0x60565056, 0x65500556,
        0x05565550, 0x65050556, 0x76505067, 0x57660675,
    },
    [NTILE_FLOOR_GRATE] = {
        /* Metal grating: cross-hatch pattern over dark void */
        0x11515151, 0x51151515, 0x15510515, 0x51051051,
        0x15105105, 0x51501510, 0x15151151, 0x51515511,
    },
    [NTILE_WALL] = {
        /* Brick wall: staggered blocks, mortar lines, depth + texture */
        0x11111111, 0x13322133, 0x14432143, 0x13432143,
        0x11111111, 0x21332133, 0x21443214, 0x21332133,
    },
    [NTILE_WALL_PANEL] = {
        /* Smooth tech panel: inset rectangle with highlight edge */
        0x41111114, 0x12333321, 0x13233231, 0x13233231,
        0x13288231, 0x13233231, 0x12333321, 0x41111114,
    },
    [NTILE_WALL_CIRCUIT] = {
        /* Wall with glowing circuit network: nodes and traces */
        0x11981119, 0x19891988, 0x18F91898, 0x11991189,
        0x89111198, 0x98919891, 0x19F18891, 0x11181199,
    },
    [NTILE_WALL_CAP_TOP] = {
        /* Top cap: rim accent, beveled transition to wall */
        0x44444444, 0x43434343, 0x43333334, 0x13222231,
        0x12221222, 0x11211121, 0x12321232, 0x11211121,
    },
    [NTILE_WALL_CAP_BOT] = {
        /* Bottom cap: wall to rim with accent lip */
        0x11211121, 0x12321232, 0x11211121, 0x12221222,
        0x13222231, 0x43333334, 0x43434343, 0x44444444,
    },
    [NTILE_PLATFORM] = {
        /* Floating platform: gold trim, hover glow underside */
        0xCDCDCDCD, 0xD9898989, 0x67766776, 0x65566556,
        0x65586558, 0x56565656, 0x19515191, 0x01111110,
    },
    [NTILE_PLATFORM_ENERGY] = {
        /* Energy platform: pulsing bright core, gold edges */
        0xDFDFDFDF, 0xD9F99F9D, 0x699FF996, 0x569FF965,
        0x569FF965, 0x69899896, 0x19515191, 0x01111110,
    },
    /* --- Wall detail --- */
    [NTILE_PIPE_H] = {
        /* Horizontal pipe: cylindrical shading with rivet bands */
        0x00000000, 0x11111111, 0x12323231, 0x13434341,
        0x13434341, 0x12323231, 0x11111111, 0x00000000,
    },
    [NTILE_PIPE_V] = {
        /* Vertical pipe: cylindrical shading, consistent highlights */
        0x00123100, 0x00134100, 0x00123100, 0x00134100,
        0x00111100, 0x00134100, 0x00123100, 0x00134100,
    },
    [NTILE_VENT] = {
        /* Ventilation grate: angled slats with dark interior */
        0x44111144, 0x11302031, 0x12030201, 0x13020301,
        0x12030201, 0x13020301, 0x11302031, 0x44111144,
    },
    [NTILE_SCREEN] = {
        /* Wall-mounted display: data readout with scan lines */
        0x41111114, 0x18E89891, 0x1989E891, 0x1898F891,
        0x19E98E91, 0x1889E891, 0x41111114, 0x00191000,
    },
    [NTILE_WINDOW] = {
        /* Dark window: reflective edge, void interior with distant glow */
        0x41111114, 0x13000031, 0x12000021, 0x120E0021,
        0x12000E21, 0x120000E1, 0x13000031, 0x41111114,
    },
    [NTILE_JUNCTION_BOX] = {
        /* Junction box: wired panel with glowing internals */
        0x41111114, 0x12898921, 0x1298F821, 0x129F9821,
        0x1289F921, 0x12898921, 0x13222231, 0x41111114,
    },
    /* --- Hazards --- */
    [NTILE_HAZARD_SPIKE] = {
        /* Spike trap: sharp crystalline spikes, bright tips */
        0x000F0000, 0x00AFA000, 0x00BFB000, 0x0ABFBA00,
        0x0BFFFB00, 0xABFFFBA0, 0xBFFBFFBF, 0xABBAABBA,
    },
    [NTILE_HAZARD_BEAM] = {
        /* Energy beam: bright core, hazard glow falloff */
        0x00AFFA00, 0x0ABFFBA0, 0xABFFFFBA, 0xBFFFFFFF,
        0xBFFFFFFF, 0xABFFFFBA, 0x0ABFFBA0, 0x00AFFA00,
    },
    [NTILE_HAZARD_TESLA] = {
        /* Tesla coil: forked lightning from base node */
        0x000FF000, 0x00F99F00, 0x0FB99BF0, 0xFB0FF0BF,
        0x0F0BF0F0, 0xF00B00F0, 0x0F000B0F, 0x11111111,
    },
    [NTILE_HAZARD_BEAM_OFF] = {
        /* Inactive beam emitter: dim outline, no glow */
        0x000AA000, 0x00A11A00, 0x0A1551A0, 0xA155551A,
        0x0A1551A0, 0x00A11A00, 0x000AA000, 0x00011000,
    },
    /* --- Edge/transition --- */
    [NTILE_FLOOR_EDGE_L] = {
        /* Floor left edge: wall-to-floor transition, shadow */
        0x17766677, 0x16556567, 0x15565556, 0x15585556,
        0x15565556, 0x15556556, 0x16556567, 0x11111111,
    },
    [NTILE_FLOOR_EDGE_R] = {
        /* Floor right edge: floor-to-wall transition, shadow */
        0x77666671, 0x76556561, 0x65565551, 0x65585551,
        0x65565551, 0x65556551, 0x76556561, 0x11111111,
    },
    [NTILE_CORNER_TL] = {
        /* Top-left corner: rim wraps, floor meets wall */
        0x44444444, 0x43434343, 0x14333321, 0x13222121,
        0x16566556, 0x15585558, 0x15565556, 0x11111111,
    },
    [NTILE_CORNER_TR] = {
        /* Top-right corner: rim wraps, wall meets floor */
        0x44444444, 0x43434343, 0x12333341, 0x12122231,
        0x65665651, 0x85558551, 0x65565551, 0x11111111,
    },
    [NTILE_CORNER_BL] = {
        /* Bottom-left corner: floor wraps down to wall */
        0x11111111, 0x13222121, 0x14333321, 0x43434343,
        0x44444444, 0x00000000, 0x00000000, 0x00000000,
    },
    [NTILE_CORNER_BR] = {
        /* Bottom-right corner: floor wraps down to wall */
        0x11111111, 0x12122231, 0x12333341, 0x43434343,
        0x44444444, 0x00000000, 0x00000000, 0x00000000,
    },
    [NTILE_PLAT_EDGE_L] = {
        /* Platform left edge: gold cap curves down */
        0x1DCDCDCD, 0x19898989, 0x16776677, 0x15566556,
        0x15586558, 0x15656565, 0x11515191, 0x10111110,
    },
    [NTILE_PLAT_EDGE_R] = {
        /* Platform right edge: gold cap curves down */
        0xCDCDCDC1, 0x98989891, 0x77667761, 0x65566551,
        0x85586551, 0x56565651, 0x19151511, 0x01111101,
    },
    /* --- Decorative --- */
    [NTILE_CABLE_H] = {
        /* Horizontal data cable: shielded with bright pulses */
        0x00000000, 0x00000000, 0x11111111, 0x189F9891,
        0x198F8981, 0x11111111, 0x00000000, 0x00000000,
    },
    [NTILE_CABLE_V] = {
        /* Vertical data cable: shielded, pulse nodes */
        0x00110000, 0x00890000, 0x00910000, 0x00F10000,
        0x00910000, 0x00890000, 0x00110000, 0x00F10000,
    },
    [NTILE_CABLE_CORNER] = {
        /* Cable corner: smooth 90-degree bend */
        0x00110000, 0x00891000, 0x00191111, 0x000989F9,
        0x00019898, 0x00001111, 0x00000000, 0x00000000,
    },
    [NTILE_DATA_STREAM] = {
        /* Cascading data particles: bright and dim interleaved */
        0x00E00F00, 0x0F00E00E, 0xE00F00E0, 0x00E0F00F,
        0x0F00E00E, 0xE0F000E0, 0x00E00F0E, 0xF00E00F0,
    },
    [NTILE_SERVER_TOP] = {
        /* Server rack top: LED row, data panels */
        0x44111144, 0x1F9F9F91, 0x18989891, 0x11111111,
        0x12898921, 0x12989821, 0x12898921, 0x13333331,
    },
    [NTILE_SERVER_BOT] = {
        /* Server rack bottom: panels and base vent */
        0x13333331, 0x12898921, 0x12989821, 0x12898921,
        0x11111111, 0x1F989F91, 0x18989891, 0x44111144,
    },
    [NTILE_CONSOLE] = {
        /* Terminal console: screen display with keyboard base */
        0x41111114, 0x19E8F891, 0x1E89E891, 0x198F8E91,
        0x41111114, 0x0CDCCDC0, 0x0DCCDDCD, 0x01111110,
    },
    [NTILE_CIRCUIT_NODE] = {
        /* Circuit junction: bright core, radiating traces */
        0x00181000, 0x01898100, 0x189F9810, 0x89FFF980,
        0x189F9810, 0x01898100, 0x00181000, 0x00010000,
    },
    [NTILE_MEMORY_BANK] = {
        /* Memory bank: dual chip rows with pin headers */
        0x41111114, 0x13288231, 0x12398321, 0x13288231,
        0x11111111, 0x13288231, 0x12398321, 0x13288231,
    },
    [NTILE_CONDUIT] = {
        /* Power conduit: thick glowing pipe, bright center */
        0x00191000, 0x01999100, 0x19F9F910, 0x09FFF900,
        0x19F9F910, 0x01999100, 0x00191000, 0x00010000,
    },
    [NTILE_BROKEN_PANEL] = {
        /* Damaged panel: missing chunks, exposed wiring */
        0x41101114, 0x12220821, 0x10030321, 0x12302091,
        0x10020321, 0x12090821, 0x13200231, 0x41110114,
    },
    [NTILE_GLITCH] = {
        /* Glitch artifact: scattered bright pixels, chaotic */
        0x009F0000, 0xF800E900, 0x009800F9, 0x9F000008,
        0x0000F900, 0x0F890000, 0x000009F9, 0xE9000F00,
    },
    /* --- Section atmosphere --- */
    [NTILE_CORRIDOR_GREEBLE] = {
        /* Corridor wall detail: tech panel strips with circuit accents */
        0x11111111, 0x12131213, 0x13148314, 0x12139213,
        0x11111111, 0x31218121, 0x41319131, 0x31213121,
    },
    [NTILE_SHAFT_RIVET] = {
        /* Climb shaft rivet: hexagonal bolt head, beveled */
        0x00000000, 0x00133100, 0x01344310, 0x01344310,
        0x01344310, 0x01233210, 0x00122100, 0x00000000,
    },
    [NTILE_ARENA_BORDER] = {
        /* Arena border: gold-trimmed wall frame */
        0xCDCDCDCD, 0xDC111111, 0xCD12221C, 0xDC12321D,
        0xCD12321C, 0xDC12221D, 0xCD111111, 0xCDCDCDCD,
    },
    [NTILE_BOSS_FLOOR] = {
        /* Boss arena floor: gold-accented dark metal */
        0xCDC66CDC, 0xD6556556, 0xC5589855, 0xD5598955,
        0xC5589855, 0xD6556556, 0xC5555555, 0x11C11C11,
    },
    [NTILE_DESCENT_RAIL] = {
        /* Descent guide rail: vertical track with notches */
        0x13000000, 0x24100000, 0x13100000, 0x24100000,
        0x13100000, 0x24000000, 0x13100000, 0x24100000,
    },
    [NTILE_MAZE_JUNCTION] = {
        /* Maze path junction: circuit-marked floor crossing */
        0x89666689, 0x96556559, 0x65589856, 0x65898956,
        0x65898956, 0x65589856, 0x96556559, 0x89111189,
    },
    [NTILE_DATA_WATERFALL] = {
        /* Cascading data waterfall: dense vertical stream */
        0xFE00EF00, 0x0EF00FE0, 0xE00FE00E, 0x0FE000EF,
        0xE00FE00F, 0x0EF0FE00, 0xF00EE00F, 0x00FE0FE0,
    },
    [NTILE_NEON_SIGN] = {
        /* Neon sign: gold frame with bright interior glow */
        0x41111114, 0x1CDCCDC1, 0x1D9FF9D1, 0x1C9FF9C1,
        0x1D9FF9D1, 0x1CDCCDC1, 0x41111114, 0x00000000,
    },
    /* --- Functional --- */
    [NTILE_LADDER] = {
        /* Climbable ladder: metal rungs, rail shading */
        0x91000019, 0x82888828, 0x91000019, 0x91000019,
        0x82888828, 0x91000019, 0x91000019, 0x82888828,
    },
    [NTILE_BREAKABLE] = {
        /* Breakable block: brick pattern, stressed joints */
        0x23332333, 0x34213421, 0x23132313, 0x22222222,
        0x32233223, 0x21342134, 0x31233123, 0x22222222,
    },
    [NTILE_BREAKABLE_CRACKED] = {
        /* Weakened breakable: fracture lines, loose pieces */
        0x23302333, 0x34010421, 0x20130013, 0x22002222,
        0x32030223, 0x01342030, 0x31200123, 0x22220022,
    },
    [NTILE_EXIT_FRAME] = {
        /* Exit gate frame: gold ornate border, deep opening */
        0xCDCDCDCD, 0xDC1111CD, 0xCD0000DC, 0xDC0000CD,
        0xCD0000DC, 0xDC0000CD, 0xCD1111DC, 0xCDCDCDCD,
    },
    [NTILE_EXIT_GATE] = {
        /* Exit portal: swirling bright energy within gold frame */
        0xCDCDCDCD, 0xD9F9D9DC, 0xC9DFFD9C, 0xD9FFFFD9,
        0xC9FFFFC9, 0xD9DFFD9D, 0xC9F9D9DC, 0xCDCDCDCD,
    },
    [NTILE_SPAWN] = {
        /* Spawn marker: faint holographic ring (visible in level, not gameplay) */
        0x00088000, 0x00800800, 0x08000080, 0x80000008,
        0x08000080, 0x00800800, 0x00088000, 0x00000000,
    },
};

/* Act-themed BG palettes — 6 variations (0=freelance, 1-5=acts)
 * 0=void, 1-4=wall ramp, 5-7=floor ramp, 8-9=circuit glow,
 * A-B=hazard, C-D=gold accent, E=data dim, F=warm white */
static const u16 act_palettes[6][16] = {
    /* Act 0: Freelance — Deep purple/cyan cyberspace
     * Walls=indigo, Floors=blue-grey, Glow=cyan, Hazard=red, Accent=gold */
    {
        RGB15_C(1, 0, 4),     /* 0: void — deep indigo */
        RGB15_C(3, 2, 10),    /* 1: wall deep shadow */
        RGB15_C(6, 6, 16),    /* 2: wall shadow */
        RGB15_C(10, 12, 22),  /* 3: wall mid */
        RGB15_C(16, 20, 28),  /* 4: wall highlight */
        RGB15_C(4, 5, 12),    /* 5: floor dark */
        RGB15_C(8, 10, 18),   /* 6: floor mid */
        RGB15_C(14, 18, 26),  /* 7: floor bright */
        RGB15_C(0, 16, 24),   /* 8: circuit glow dim */
        RGB15_C(4, 28, 31),   /* 9: circuit glow bright */
        RGB15_C(24, 4, 2),    /* A: hazard dim red */
        RGB15_C(31, 10, 6),   /* B: hazard bright red */
        RGB15_C(22, 18, 4),   /* C: accent gold dark */
        RGB15_C(31, 28, 10),  /* D: accent gold bright */
        RGB15_C(6, 10, 18),   /* E: data stream dim */
        RGB15_C(31, 31, 28),  /* F: warm white */
    },
    /* Act 1: The Glitch — Toxic green corruption with RGB artifacts
     * Walls=dark green, Floors=teal, Glow=neon green, Hazard=magenta, Accent=lime */
    {
        RGB15_C(0, 3, 1),     /* 0: void — deep green */
        RGB15_C(1, 6, 3),     /* 1: wall deep shadow */
        RGB15_C(3, 12, 6),    /* 2: wall shadow */
        RGB15_C(6, 18, 10),   /* 3: wall mid */
        RGB15_C(10, 26, 16),  /* 4: wall highlight */
        RGB15_C(2, 8, 5),     /* 5: floor dark */
        RGB15_C(4, 14, 8),    /* 6: floor mid */
        RGB15_C(8, 22, 14),   /* 7: floor bright */
        RGB15_C(4, 24, 12),   /* 8: circuit glow dim */
        RGB15_C(8, 31, 20),   /* 9: circuit glow bright — neon green */
        RGB15_C(28, 4, 20),   /* A: hazard dim magenta (glitch!) */
        RGB15_C(31, 12, 28),  /* B: hazard bright magenta */
        RGB15_C(20, 28, 4),   /* C: accent lime dark */
        RGB15_C(28, 31, 10),  /* D: accent lime bright */
        RGB15_C(2, 14, 8),    /* E: data stream dim */
        RGB15_C(26, 31, 26),  /* F: glitch white-green */
    },
    /* Act 2: Traceback — Cold blue server room with clean lines
     * Walls=dark navy, Floors=steel blue, Glow=electric blue, Hazard=orange, Accent=white-blue */
    {
        RGB15_C(0, 1, 5),     /* 0: void — deep navy */
        RGB15_C(2, 3, 12),    /* 1: wall deep shadow */
        RGB15_C(4, 6, 18),    /* 2: wall shadow */
        RGB15_C(8, 12, 24),   /* 3: wall mid */
        RGB15_C(14, 20, 30),  /* 4: wall highlight */
        RGB15_C(3, 4, 12),    /* 5: floor dark — steel */
        RGB15_C(6, 8, 20),    /* 6: floor mid */
        RGB15_C(12, 16, 28),  /* 7: floor bright */
        RGB15_C(4, 14, 28),   /* 8: circuit glow dim */
        RGB15_C(10, 24, 31),  /* 9: circuit glow bright — electric blue */
        RGB15_C(28, 14, 2),   /* A: hazard dim orange (alarm) */
        RGB15_C(31, 22, 6),   /* B: hazard bright orange */
        RGB15_C(16, 22, 31),  /* C: accent blue-white dark */
        RGB15_C(24, 28, 31),  /* D: accent blue-white bright */
        RGB15_C(4, 8, 20),    /* E: data stream dim */
        RGB15_C(28, 30, 31),  /* F: ice white */
    },
    /* Act 3: Deep Packet — Amber/orange organic-digital
     * Walls=brown, Floors=dark amber, Glow=hot amber, Hazard=toxic purple, Accent=gold-orange */
    {
        RGB15_C(4, 2, 0),     /* 0: void — deep brown */
        RGB15_C(8, 4, 1),     /* 1: wall deep shadow */
        RGB15_C(14, 8, 3),    /* 2: wall shadow */
        RGB15_C(20, 12, 5),   /* 3: wall mid */
        RGB15_C(28, 20, 10),  /* 4: wall highlight */
        RGB15_C(6, 4, 2),     /* 5: floor dark */
        RGB15_C(12, 8, 4),    /* 6: floor mid */
        RGB15_C(20, 14, 6),   /* 7: floor bright */
        RGB15_C(24, 16, 2),   /* 8: circuit glow dim amber */
        RGB15_C(31, 24, 6),   /* 9: circuit glow bright amber */
        RGB15_C(18, 4, 22),   /* A: hazard dim toxic purple */
        RGB15_C(28, 8, 31),   /* B: hazard bright toxic purple */
        RGB15_C(26, 20, 4),   /* C: accent gold-orange dark */
        RGB15_C(31, 28, 10),  /* D: accent gold-orange bright */
        RGB15_C(14, 8, 2),    /* E: data stream dim */
        RGB15_C(31, 30, 22),  /* F: warm amber white */
    },
    /* Act 4: Zero Day — Red destruction, exposed infrastructure
     * Walls=dark crimson, Floors=rusty brown, Glow=blood red, Hazard=hot orange, Accent=burnt yellow */
    {
        RGB15_C(4, 0, 0),     /* 0: void — deep crimson */
        RGB15_C(10, 2, 2),    /* 1: wall deep shadow */
        RGB15_C(16, 4, 4),    /* 2: wall shadow */
        RGB15_C(24, 8, 6),    /* 3: wall mid */
        RGB15_C(30, 14, 10),  /* 4: wall highlight */
        RGB15_C(8, 3, 2),     /* 5: floor dark rusty */
        RGB15_C(14, 6, 4),    /* 6: floor mid */
        RGB15_C(22, 10, 6),   /* 7: floor bright */
        RGB15_C(26, 4, 4),    /* 8: circuit glow dim blood */
        RGB15_C(31, 12, 8),   /* 9: circuit glow bright blood */
        RGB15_C(31, 18, 2),   /* A: hazard dim hot orange */
        RGB15_C(31, 28, 6),   /* B: hazard bright hot orange-yellow */
        RGB15_C(24, 14, 4),   /* C: accent burnt yellow dark */
        RGB15_C(31, 22, 8),   /* D: accent burnt yellow bright */
        RGB15_C(16, 4, 3),    /* E: data stream dim */
        RGB15_C(31, 28, 24),  /* F: heated white */
    },
    /* Act 5: Ghost Protocol — Ethereal purple/silver, minimal, ghostly
     * Walls=deep purple, Floors=slate violet, Glow=electric violet, Hazard=cyan, Accent=silver */
    {
        RGB15_C(2, 0, 5),     /* 0: void — deep purple */
        RGB15_C(5, 2, 12),    /* 1: wall deep shadow */
        RGB15_C(8, 5, 20),    /* 2: wall shadow */
        RGB15_C(14, 10, 26),  /* 3: wall mid */
        RGB15_C(22, 18, 31),  /* 4: wall highlight */
        RGB15_C(4, 3, 12),    /* 5: floor dark slate */
        RGB15_C(8, 6, 20),    /* 6: floor mid */
        RGB15_C(16, 14, 28),  /* 7: floor bright */
        RGB15_C(14, 6, 26),   /* 8: circuit glow dim violet */
        RGB15_C(24, 14, 31),  /* 9: circuit glow bright violet */
        RGB15_C(4, 20, 26),   /* A: hazard dim cyan (ethereal danger) */
        RGB15_C(10, 28, 31),  /* B: hazard bright cyan */
        RGB15_C(20, 18, 28),  /* C: accent silver dark */
        RGB15_C(28, 26, 31),  /* D: accent silver bright */
        RGB15_C(8, 4, 18),    /* E: data stream dim */
        RGB15_C(31, 30, 31),  /* F: spectral white */
    },
};

/* ---- Per-act parallax tile sets (8 tiles each) ---- */
#define PARA_TILE_COUNT 8

/* Act 0 (Freelance): Cyberpunk cityscape — buildings + data flow */
static const u32 para_tiles_act0[PARA_TILE_COUNT][8] = {
    /* 0: empty sky with stars */
    { 0x00000000, 0x00020000, 0x00000000, 0x00000020,
      0x02000000, 0x00000000, 0x00000200, 0x00000000, },
    /* 1: tall building silhouette */
    { 0x00011000, 0x00311300, 0x00311300, 0x00311300,
      0x00311300, 0x00311300, 0x00311300, 0x00311300, },
    /* 2: short building silhouette */
    { 0x00000000, 0x00000000, 0x00000000, 0x01111110,
      0x01311310, 0x01311310, 0x01311310, 0x01311310, },
    /* 3: medium building with antenna */
    { 0x00010000, 0x00010000, 0x01111100, 0x01311310,
      0x01311310, 0x01311310, 0x01311310, 0x01311310, },
    /* 4: building base with window glow */
    { 0x01311310, 0x01344310, 0x01311310, 0x01344310,
      0x01311310, 0x01344310, 0x01311310, 0x11111111, },
    /* 5: data flow at base */
    { 0x11111111, 0x15000051, 0x00500500, 0x05000050,
      0x50050005, 0x00500500, 0x05000050, 0x11111111, },
    /* 6: skyline gap (street) */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x11111111, },
    /* 7: building top with light */
    { 0x00040000, 0x00141000, 0x01111100, 0x01311310,
      0x01311310, 0x01311310, 0x01311310, 0x01311310, },
};

/* Act 1 (The Glitch): Corrupted static — noise, pixel artifacts, RGB shifts */
static const u32 para_tiles_act1[PARA_TILE_COUNT][8] = {
    /* 0: empty void with rare pixel noise */
    { 0x00000000, 0x00000300, 0x00000000, 0x05000000,
      0x00000000, 0x00030000, 0x00000000, 0x00000000, },
    /* 1: static noise block — dense random pixels */
    { 0x30504020, 0x02030405, 0x50203040, 0x04050302,
      0x20503010, 0x03020504, 0x40305020, 0x05040203, },
    /* 2: RGB shift artifact — horizontal color bands */
    { 0x44444444, 0x00000000, 0x55555555, 0x00000000,
      0x33333333, 0x00000000, 0x66666666, 0x00000000, },
    /* 3: corrupted scanline — broken horizontal pattern */
    { 0x00000000, 0x11111111, 0x30050300, 0x11111111,
      0x00000000, 0x11111111, 0x00503005, 0x11111111, },
    /* 4: pixel breakup — scattered bright fragments */
    { 0x00400060, 0x60000004, 0x00060040, 0x04006000,
      0x06000400, 0x00600004, 0x40000600, 0x00040060, },
    /* 5: glitch tear — horizontal displacement */
    { 0x22222200, 0x00222222, 0x22220022, 0x00002222,
      0x22222200, 0x22000022, 0x02222220, 0x22200022, },
    /* 6: corruption wave — diagonal artifact */
    { 0x50000000, 0x05000000, 0x00500000, 0x00050000,
      0x00005000, 0x00000500, 0x00000050, 0x00000005, },
    /* 7: dead pixels — sparse bright dots on dark */
    { 0x00060000, 0x00000400, 0x04000006, 0x00000000,
      0x00040000, 0x06000000, 0x00000004, 0x00600000, },
};

/* Act 2 (Traceback): Server room — trace lines, LED columns, rack outlines */
static const u32 para_tiles_act2[PARA_TILE_COUNT][8] = {
    /* 0: dark background with faint grid dots */
    { 0x00000000, 0x00000000, 0x00010000, 0x00000000,
      0x00000000, 0x00000000, 0x00000001, 0x00000000, },
    /* 1: horizontal trace line — clean */
    { 0x00000000, 0x00000000, 0x00000000, 0x22222222,
      0x34343434, 0x22222222, 0x00000000, 0x00000000, },
    /* 2: vertical LED column — blinking indicators */
    { 0x00110000, 0x00440000, 0x00110000, 0x00550000,
      0x00110000, 0x00440000, 0x00110000, 0x00550000, },
    /* 3: server rack outline — top */
    { 0x11111111, 0x10000001, 0x10233201, 0x10233201,
      0x10000001, 0x10233201, 0x10233201, 0x10000001, },
    /* 4: server rack outline — bottom */
    { 0x10000001, 0x10233201, 0x10233201, 0x10000001,
      0x10233201, 0x10233201, 0x10000001, 0x11111111, },
    /* 5: network cable bundle — horizontal */
    { 0x00000000, 0x33333333, 0x25252525, 0x33333333,
      0x00000000, 0x22222222, 0x34343434, 0x22222222, },
    /* 6: status LED row */
    { 0x00000000, 0x00000000, 0x04050405, 0x00000000,
      0x00000000, 0x00000000, 0x05040504, 0x00000000, },
    /* 7: data port — square connector */
    { 0x00000000, 0x00111100, 0x01255210, 0x01233210,
      0x01233210, 0x01255210, 0x00111100, 0x00000000, },
};

/* Act 3 (Deep Packet): Organic-digital — circuit veins, bio tendrils, node dots */
static const u32 para_tiles_act3[PARA_TILE_COUNT][8] = {
    /* 0: dark with faint bio-speckles */
    { 0x00000000, 0x00010000, 0x00000000, 0x00000010,
      0x00000000, 0x01000000, 0x00000000, 0x00000100, },
    /* 1: circuit vein — branching tendril vertical */
    { 0x00020000, 0x00230000, 0x00023000, 0x00230200,
      0x02300020, 0x02002300, 0x00020230, 0x00230020, },
    /* 2: circuit vein — horizontal branch */
    { 0x00000000, 0x00000000, 0x02223000, 0x23334520,
      0x02234520, 0x00223000, 0x00000000, 0x00000000, },
    /* 3: pulsing node dot — bright center radiating */
    { 0x00000000, 0x00022000, 0x00234200, 0x02345320,
      0x02345320, 0x00234200, 0x00022000, 0x00000000, },
    /* 4: bio-growth tendril — curving organic shape */
    { 0x00000020, 0x00000200, 0x00002300, 0x00023200,
      0x00232000, 0x02320000, 0x02300000, 0x23000000, },
    /* 5: dense vein mesh — overlapping patterns */
    { 0x20023002, 0x02302320, 0x00230230, 0x23002300,
      0x30230023, 0x02300230, 0x23023002, 0x30020230, },
    /* 6: node cluster — multiple small dots */
    { 0x00300000, 0x00000030, 0x03000000, 0x00000300,
      0x00030000, 0x30000003, 0x00000030, 0x00300000, },
    /* 7: bio-circuit junction — vein intersection */
    { 0x00020000, 0x00232000, 0x02345200, 0x23455320,
      0x02345200, 0x00232000, 0x00020000, 0x00000000, },
};

/* Act 4 (Zero Day): Decay — cracks, exposed wiring, crumbling fragments */
static const u32 para_tiles_act4[PARA_TILE_COUNT][8] = {
    /* 0: dark with dust particles */
    { 0x00000000, 0x00010000, 0x00000000, 0x10000000,
      0x00000001, 0x00000000, 0x00100000, 0x00000000, },
    /* 1: surface crack — diagonal fracture */
    { 0x20000000, 0x02000000, 0x00200000, 0x00320000,
      0x00032000, 0x00003200, 0x00000320, 0x00000030, },
    /* 2: exposed wiring — dangling cables */
    { 0x00300000, 0x00300000, 0x00350000, 0x00300500,
      0x05300050, 0x00530000, 0x00053000, 0x00005300, },
    /* 3: crumbling fragment — broken surface chunk */
    { 0x00000000, 0x00221000, 0x02332100, 0x02342310,
      0x01232100, 0x00122000, 0x00010000, 0x00000000, },
    /* 4: fractured grid — damaged floor pattern */
    { 0x11101111, 0x10001001, 0x10101010, 0x00010100,
      0x10100010, 0x10010101, 0x01001000, 0x11110111, },
    /* 5: debris field — scattered small fragments */
    { 0x00200010, 0x10000002, 0x00010020, 0x02000100,
      0x00020001, 0x01000200, 0x20000010, 0x00100020, },
    /* 6: crack web — multi-directional fractures */
    { 0x00002000, 0x00020200, 0x00200020, 0x02000002,
      0x20000020, 0x02000200, 0x00200020, 0x00020000, },
    /* 7: rusted panel — corroded surface with holes */
    { 0x11111111, 0x12321001, 0x12321010, 0x10001231,
      0x12310010, 0x10012321, 0x10100012, 0x11111111, },
};

/* Act 5 (Ghost Protocol): Minimal wireframe — sparse dots, faint grid, geometry */
static const u32 para_tiles_act5[PARA_TILE_COUNT][8] = {
    /* 0: near-empty void — extremely sparse */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, },
    /* 1: faint grid intersection — single dot */
    { 0x00000000, 0x00000000, 0x00000000, 0x00010000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, },
    /* 2: wireframe horizontal line — dashed */
    { 0x00000000, 0x00000000, 0x00000000, 0x10100101,
      0x00000000, 0x00000000, 0x00000000, 0x00000000, },
    /* 3: wireframe vertical line — dashed */
    { 0x00010000, 0x00000000, 0x00010000, 0x00000000,
      0x00010000, 0x00000000, 0x00010000, 0x00000000, },
    /* 4: geometric outline — diamond */
    { 0x00010000, 0x00101000, 0x01000100, 0x10000010,
      0x01000100, 0x00101000, 0x00010000, 0x00000000, },
    /* 5: sparse dot field — minimal constellation */
    { 0x00000002, 0x00000000, 0x02000000, 0x00000000,
      0x00000000, 0x00020000, 0x00000000, 0x00000002, },
    /* 6: wireframe box outline */
    { 0x11111111, 0x10000001, 0x10000001, 0x10000001,
      0x10000001, 0x10000001, 0x10000001, 0x11111111, },
    /* 7: geometric triangle outline */
    { 0x00010000, 0x00101000, 0x00100100, 0x01000010,
      0x01000010, 0x10000001, 0x10000001, 0x11111111, },
};

/* Act 6 (Trace Route): all acts reuse act0 tiles — map builder mixes patterns */

/* Pointer table for per-act tile data */
static const u32 (*const para_tile_sets[6])[8] = {
    para_tiles_act0, para_tiles_act1, para_tiles_act2,
    para_tiles_act3, para_tiles_act4, para_tiles_act5,
};

/* Per-act parallax palettes — fully populated 16 colors for richer art
 * 0=void/bg, 1=dark shade, 2=mid shade, 3=light shade,
 * 4=highlight, 5=accent bright, 6=accent dim, 7=secondary */
static const u16 parallax_palettes[6][16] = {
    /* Act 0: Freelance — cool purple/cyan cityscape */
    { RGB15_C(1,0,3),   RGB15_C(3,3,8),   RGB15_C(5,6,14),  RGB15_C(5,5,12),
      RGB15_C(8,14,22), RGB15_C(2,12,16), RGB15_C(4,8,18),  RGB15_C(6,10,20),
      0,0,0,0,0,0,0,0 },
    /* Act 1: The Glitch — toxic green with RGB interference */
    { RGB15_C(0,1,0),   RGB15_C(2,5,2),   RGB15_C(4,10,3),  RGB15_C(6,16,4),
      RGB15_C(10,24,6), RGB15_C(8,31,8),  RGB15_C(28,4,4),  RGB15_C(4,4,28),
      0,0,0,0,0,0,0,0 },
    /* Act 2: Traceback — cold blue server room */
    { RGB15_C(0,0,2),   RGB15_C(2,2,8),   RGB15_C(3,4,14),  RGB15_C(5,8,20),
      RGB15_C(6,14,28), RGB15_C(8,22,31), RGB15_C(2,6,12),  RGB15_C(4,10,18),
      0,0,0,0,0,0,0,0 },
    /* Act 3: Deep Packet — amber/orange bio-circuit */
    { RGB15_C(2,1,0),   RGB15_C(5,3,1),   RGB15_C(10,6,2),  RGB15_C(16,10,3),
      RGB15_C(24,16,4), RGB15_C(31,22,6), RGB15_C(8,4,1),   RGB15_C(12,8,2),
      0,0,0,0,0,0,0,0 },
    /* Act 4: Zero Day — deep red decay */
    { RGB15_C(2,0,0),   RGB15_C(6,2,1),   RGB15_C(12,4,2),  RGB15_C(18,6,3),
      RGB15_C(24,8,4),  RGB15_C(31,12,6), RGB15_C(8,3,1),   RGB15_C(14,5,2),
      0,0,0,0,0,0,0,0 },
    /* Act 5: Ghost Protocol — ethereal purple/white wireframe */
    { RGB15_C(1,0,2),   RGB15_C(6,3,12),  RGB15_C(10,6,20), RGB15_C(16,12,26),
      RGB15_C(22,18,31),RGB15_C(28,26,31),RGB15_C(4,2,8),   RGB15_C(8,4,14),
      0,0,0,0,0,0,0,0 },
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

    memcpy16(&tile_mem[2][1], para_tile_sets[act], PARA_TILE_COUNT * 8 * sizeof(u32) / 2);
    memcpy16(&pal_bg_mem[6 * 16], parallax_palettes[act], 16);

    u16* map = (u16*)se_mem[30];
    u16 pal6 = (6 << 12);
    u32 h = (u32)seed * 2654435761u;

    /* Helper: seeded hash per cell */
    #define PHASH(r,c) (((u32)((r)*37+(c)*13)+h)^(((u32)((r)*37+(c)*13)+h)>>7))

    switch (act) {
    default:
    case 0: { /* Freelance: Cyberpunk cityscape with buildings + data */
        /* Sky with sparse stars */
        for (int r = 0; r < 24; r++)
            for (int c = 0; c < 32; c++) {
                int t = ((PHASH(r,c) & 31) < 2 && r >= 6) ? 1 : 0;
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        /* Seeded skyline */
        u8 sky[16];
        { static const u8 tops[] = {1,2,3,7};
          u32 sh = h;
          for (int i = 0; i < 16; i++) {
              sh = sh * 1103515245u + 12345;
              int r5 = (int)((sh >> 16) & 7);
              sky[i] = (r5 < 3) ? tops[(sh >> 8) & 3] : (r5 < 5) ? 4 : 6;
          }
          sky[(h>>4)&15] = 6; sky[((h>>8)+7)&15] = 6;
        }
        for (int r = 24; r < 28; r++)
            for (int c = 0; c < 32; c++) {
                int t = sky[c&15];
                if (r > 24 && t != 6) t = 4;
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        for (int r = 28; r < 31; r++)
            for (int c = 0; c < 32; c++)
                map[r*32+c] = (u16)((u16)(sky[c&15]==6 ? 6 : 4) | pal6);
        for (int c = 0; c < 32; c++)
            map[31*32+c] = (u16)(5 | pal6);
        break;
    }
    case 1: { /* The Glitch: Chaotic static with corruption bands */
        for (int r = 0; r < 32; r++)
            for (int c = 0; c < 32; c++) {
                int t = 0;
                u32 ph = PHASH(r, c);
                if (r >= 8 && r <= 11) {
                    /* Corruption band: dense static */
                    t = (ph & 3) == 0 ? 1 : (ph & 7) == 1 ? 2 : (ph & 15) < 3 ? 4 : 0;
                } else if (r >= 20 && r <= 24) {
                    /* Second corruption band */
                    t = (ph & 3) == 0 ? 3 : (ph & 7) == 1 ? 5 : (ph & 15) < 2 ? 6 : 0;
                } else {
                    /* Sparse noise and artifacts */
                    t = (ph & 31) < 2 ? 7 : (ph & 63) < 2 ? 4 : 0;
                }
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        break;
    }
    case 2: { /* Traceback: Server room grid with LED columns */
        for (int r = 0; r < 32; r++)
            for (int c = 0; c < 32; c++) {
                int t = 0;
                if ((c & 7) == 0) {
                    /* Vertical LED columns every 8 tiles */
                    t = 2;
                } else if ((r & 7) == 3) {
                    /* Horizontal cable runs */
                    t = (c & 3) == 0 ? 7 : 1;
                } else if (r >= 20 && (c & 3) == 2) {
                    /* Server racks in lower portion */
                    t = (r & 1) == 0 ? 3 : 4;
                } else if ((PHASH(r,c) & 15) == 0) {
                    /* Sparse status LEDs */
                    t = 6;
                } else {
                    t = 0;
                }
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        break;
    }
    case 3: { /* Deep Packet: Organic vein network growing from nodes */
        /* Fill with dark, then place organic structures */
        for (int r = 0; r < 32; r++)
            for (int c = 0; c < 32; c++) {
                int t = 0;
                u32 ph = PHASH(r, c);
                /* Place node clusters at seeded positions */
                int nr = (int)(((h >> ((u32)c & 15u)) + (u32)r) & 7);
                if ((c & 7) == 3 && (r & 7) == 4) {
                    t = 3; /* Pulsing node */
                } else if (((c + 1) & 7) == 3 && (r & 7) == 4) {
                    t = 7; /* Junction near node */
                } else if ((c & 7) == 3 && ((r & 7) >= 1 && (r & 7) <= 6)) {
                    /* Vertical veins from nodes */
                    t = 1;
                } else if ((r & 7) == 4 && (c & 7) >= 1 && (c & 7) <= 5) {
                    /* Horizontal veins */
                    t = 2;
                } else if ((ph & 7) == 0 && nr < 3) {
                    /* Random organic growth */
                    t = (ph & 1) ? 4 : 6;
                } else if ((ph & 31) < 2) {
                    t = 0; /* Dark speckle */
                }
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        break;
    }
    case 4: { /* Zero Day: Decayed structures with cracks spreading */
        for (int r = 0; r < 32; r++)
            for (int c = 0; c < 32; c++) {
                int t = 0;
                u32 ph = PHASH(r, c);
                /* Damaged infrastructure base pattern */
                if (r >= 22) {
                    /* Ground debris and damaged panels */
                    t = (ph & 3) == 0 ? 7 : (ph & 7) < 2 ? 5 : (ph & 15) < 4 ? 3 : 0;
                } else if ((ph & 7) == 0) {
                    /* Cracks spreading through structure */
                    t = (r + c) & 1 ? 1 : 6;
                } else if ((ph & 15) == 1) {
                    /* Exposed wiring */
                    t = 2;
                } else if ((ph & 31) < 2) {
                    /* Dust particles */
                    t = 0;
                } else if (r >= 14 && r <= 18 && (ph & 7) < 3) {
                    /* Dense damage band */
                    t = (ph & 3) == 0 ? 4 : (ph & 1) ? 3 : 5;
                }
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        break;
    }
    case 5: { /* Trace Route: Chaotic fragments — mix all visual motifs */
        for (int r = 0; r < 32; r++)
            for (int c = 0; c < 32; c++) {
                int t = 0;
                u32 ph = PHASH(r, c);
                /* Mostly void with scattered elements from different "acts" */
                if ((ph & 7) == 0) {
                    /* Building fragment (from act 0 tiles) */
                    t = ((ph >> 3) & 7) + 1;
                    if (t > 7) t = 7;
                } else if ((ph & 15) == 1) {
                    t = 5; /* Data flow fragment */
                } else if ((ph & 31) == 2) {
                    t = 0; /* Void */
                }
                map[r*32+c] = (u16)((u16)t | pal6);
            }
        break;
    }
    }

    #undef PHASH
}
