#ifndef GAME_LEVELGEN_H
#define GAME_LEVELGEN_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Procedural Level Generator
 *
 * Generates side-scrolling Net levels from seed + tier.
 * 256x32 tiles (2048x256 px), divided into 16 sections.
 */

#define NUM_SECTIONS 16

/* Section types */
enum {
    SECT_FLAT = 0,       /* Corridor */
    SECT_PLATFORMS,      /* Elevated with gaps */
    SECT_VERTICAL,       /* Climb shaft */
    SECT_ARENA,          /* Combat space */
    SECT_CORRIDOR,       /* Tight + hazards */
    SECT_DESCENT,        /* Downward path */
    SECT_MAZE,           /* Multi-path */
    SECT_BOSS,           /* Final arena */
    SECT_WATERFALL,      /* Data stream vertical features */
    SECT_TRANSIT,        /* Alternating height platforms */
    SECT_SECURITY,       /* Laser grid hazards */
    SECT_CACHE,          /* Hidden loot room */
    SECT_NETWORK,        /* Multi-layer connected paths */
    SECT_GAUNTLET,       /* Dense enemy combat */
    SECT_TYPE_COUNT
};

/* Net tile visual types (for BG1 rendering) */
enum {
    NTILE_EMPTY = 0,
    /* Structural (1-11) */
    NTILE_FLOOR,              /*  1: plain floor */
    NTILE_FLOOR_CIRCUIT,      /*  2: floor with circuit trace */
    NTILE_FLOOR_CRACKED,      /*  3: cracked/damaged floor */
    NTILE_FLOOR_GRATE,        /*  4: metal grate floor */
    NTILE_WALL,               /*  5: brick wall */
    NTILE_WALL_PANEL,         /*  6: smooth panel wall */
    NTILE_WALL_CIRCUIT,       /*  7: wall with circuit lines */
    NTILE_WALL_CAP_TOP,       /*  8: wall cap top edge */
    NTILE_WALL_CAP_BOT,       /*  9: wall cap bottom edge */
    NTILE_PLATFORM,           /* 10: floating platform */
    NTILE_PLATFORM_ENERGY,    /* 11: energy platform */
    /* Wall detail (12-17) */
    NTILE_PIPE_H,             /* 12: horizontal pipe */
    NTILE_PIPE_V,             /* 13: vertical pipe */
    NTILE_VENT,               /* 14: ventilation grate */
    NTILE_SCREEN,             /* 15: wall-mounted screen */
    NTILE_WINDOW,             /* 16: dark window */
    NTILE_JUNCTION_BOX,       /* 17: junction box */
    /* Hazards (18-21) */
    NTILE_HAZARD_SPIKE,       /* 18: spike trap */
    NTILE_HAZARD_BEAM,        /* 19: energy beam */
    NTILE_HAZARD_TESLA,       /* 20: tesla coil */
    NTILE_HAZARD_BEAM_OFF,    /* 21: inactive beam */
    /* Edge/transition (22-29) */
    NTILE_FLOOR_EDGE_L,       /* 22: floor left edge */
    NTILE_FLOOR_EDGE_R,       /* 23: floor right edge */
    NTILE_CORNER_TL,          /* 24: corner top-left */
    NTILE_CORNER_TR,          /* 25: corner top-right */
    NTILE_CORNER_BL,          /* 26: corner bottom-left */
    NTILE_CORNER_BR,          /* 27: corner bottom-right */
    NTILE_PLAT_EDGE_L,        /* 28: platform left edge */
    NTILE_PLAT_EDGE_R,        /* 29: platform right edge */
    /* Decorative (30-41) */
    NTILE_CABLE_H,            /* 30: horizontal cable */
    NTILE_CABLE_V,            /* 31: vertical cable */
    NTILE_CABLE_CORNER,       /* 32: cable corner */
    NTILE_DATA_STREAM,        /* 33: data stream particles */
    NTILE_SERVER_TOP,         /* 34: server rack top */
    NTILE_SERVER_BOT,         /* 35: server rack bottom */
    NTILE_CONSOLE,            /* 36: terminal console */
    NTILE_CIRCUIT_NODE,       /* 37: circuit junction node */
    NTILE_MEMORY_BANK,        /* 38: memory bank unit */
    NTILE_CONDUIT,            /* 39: power conduit */
    NTILE_BROKEN_PANEL,       /* 40: damaged wall panel */
    NTILE_GLITCH,             /* 41: glitch artifact */
    /* Section atmosphere (42-49) */
    NTILE_CORRIDOR_GREEBLE,   /* 42: corridor wall detail */
    NTILE_SHAFT_RIVET,        /* 43: climb shaft rivet */
    NTILE_ARENA_BORDER,       /* 44: arena border tile */
    NTILE_BOSS_FLOOR,         /* 45: boss arena floor */
    NTILE_DESCENT_RAIL,       /* 46: descent guide rail */
    NTILE_MAZE_JUNCTION,      /* 47: maze path junction */
    NTILE_DATA_WATERFALL,     /* 48: cascading data */
    NTILE_NEON_SIGN,          /* 49: neon sign tile */
    /* Functional (50-56) */
    NTILE_LADDER,             /* 50: climbable ladder */
    NTILE_BREAKABLE,          /* 51: breakable block */
    NTILE_BREAKABLE_CRACKED,  /* 52: weakened breakable */
    NTILE_EXIT_FRAME,         /* 53: exit gate frame */
    NTILE_EXIT_GATE,          /* 54: exit gate glow */
    NTILE_SPAWN,              /* 55: enemy spawn marker */
    NTILE_COUNT               /* 56 total tiles */
};

/* Level data (generated, not stored in save) */
typedef struct {
    u8 tiles[NET_MAP_W * NET_MAP_H];   /* Visual tile map */
    u8 collision[NET_MAP_W * NET_MAP_H]; /* Collision map (TILE_* values) */
    u8 sections[NUM_SECTIONS]; /* Section type per 16-column chunk */
    u16 seed;
    u8 tier;
    u8 is_boss_level;   /* 1 if last section is SECT_BOSS */
    u8 spawn_x, spawn_y; /* Player start position (tiles) */
    u8 exit_x, exit_y;   /* Exit gate position (tiles) */
    u8 num_spawns;       /* Number of enemy spawn points */
    u8 spawn_points[48][2]; /* Enemy spawn positions (tile x,y) */
} LevelData;

extern LevelData level_data;

/* Generate a level from seed and tier. */
void levelgen_generate(u16 seed, int tier, int is_boss);

/* Get visual tile at position. */
int levelgen_tile_at(int tx, int ty);

/* Get collision type at position. */
int levelgen_col_at(int tx, int ty);

/* Set collision type at position (for breakable walls, etc.). */
void levelgen_set_collision(int tx, int ty, int col_type);

#endif /* GAME_LEVELGEN_H */
