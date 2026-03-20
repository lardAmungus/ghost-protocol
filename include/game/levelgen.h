#ifndef GAME_LEVELGEN_H
#define GAME_LEVELGEN_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Procedural Level Generator
 *
 * Generates side-scrolling Net levels from seed + tier.
 * Variable width 12-16 sections (max 256x32 tiles).
 */

#define NUM_SECTIONS     16   /* Maximum sections (array size / external compat) */
#define MIN_SECTIONS     12
#define MAX_SECTIONS     16

/* Section types (lower 4 bits of sections[] entry) */
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
    /* Mega-section types */
    SECT_MEGA_ARENA,     /* 32-tile wide, 3 floor levels, walled */
    SECT_MEGA_TRANSIT,   /* 32-tile wide, alternating platforms + reward */
    SECT_MEGA_CONT,      /* Continuation of mega-section (skip generation) */
    SECT_TYPE_COUNT
};

/* Section modifiers (stored in upper 4 bits of sections[]) */
enum {
    MOD_NONE     = 0,
    MOD_FLOODED  = 1,   /* Bottom 4 rows hazard, platforms raised */
    MOD_DARK     = 2,   /* 60% decoration replaced with dark fill */
    MOD_OVERGROWN = 3,  /* Corruption spread from wall seeds */
    MOD_UNSTABLE = 4,   /* 30% platforms become breakable */
    MOD_COUNT
};

#define SECT_TYPE_MASK  0x0F
#define SECT_MOD_SHIFT  4
#define SECT_MOD_MASK   0xF0

/* Extract section type and modifier */
#define SECT_GET_TYPE(s)  ((s) & SECT_TYPE_MASK)
#define SECT_GET_MOD(s)   (((s) & SECT_MOD_MASK) >> SECT_MOD_SHIFT)

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
    /* Act-themed decorations (56-73) */
    NTILE_DECO_FLICKER,       /* 56: flickering light */
    NTILE_DECO_TERMINAL,      /* 57: inactive terminal */
    NTILE_DECO_DUCT,          /* 58: ventilation duct */
    NTILE_DECO_GRIME,         /* 59: grimy surface */
    NTILE_DECO_MOSS,          /* 60: digital moss/corruption */
    NTILE_DECO_RUST,          /* 61: rusted panel */
    NTILE_DECO_SLUDGE,        /* 62: toxic sludge drip */
    NTILE_DECO_WARNING,       /* 63: warning stripe */
    NTILE_DECO_BLAST,         /* 64: blast mark */
    NTILE_DECO_GRID,          /* 65: holographic grid */
    NTILE_DECO_VOID,          /* 66: void pixel */
    NTILE_DECO_ARC,           /* 67: electric arc */
    NTILE_DECO_PULSE,         /* 68: power pulse */
    NTILE_DECO_CORE,          /* 69: core fragment */
    NTILE_DECO_DIVIDER,       /* 70: path divider wall */
    NTILE_DECO_MEGA_WALL,     /* 71: mega-section reinforced wall */
    NTILE_DECO_REWARD,        /* 72: reward marker */
    NTILE_DECO_DARK,          /* 73: dark fill tile */
    NTILE_COUNT               /* 74 total tiles */
};

/* Level data (generated, not stored in save) */
typedef struct {
    u8 tiles[NET_MAP_W * NET_MAP_H];   /* Visual tile map */
    u8 collision[NET_MAP_W * NET_MAP_H]; /* Collision map (TILE_* values) */
    u16 seed;
    u8 tier;
    u8 is_boss_level;
    u8 spawn_x, spawn_y; /* Player start position (tiles) */
    u8 exit_x, exit_y;   /* Exit gate position (tiles) */
    u8 boss_tile_x, boss_tile_y; /* Boss spawn position (tiles) */
    u8 num_spawns;       /* Number of enemy spawn points */
    u8 spawn_points[48][2]; /* Enemy spawn positions (tile x,y) */
} LevelData;

extern LevelData level_data;

/* Per-act BG palette themes (16 colors each, palette bank 1) */
extern const u16 act_bg_palettes[6][16];

/* Per-act parallax tile data (up to 8 tiles = 64 bytes each) */
extern const u32 act_parallax_tiles[6][64]; /* 8 tiles x 8 words per tile */
extern const int act_parallax_tile_count[6]; /* Number of unique tiles per act */

/* Generate a level from seed and tier. */
void levelgen_generate(u16 seed, int tier, int is_boss);

/* Get visual tile at position. */
int levelgen_tile_at(int tx, int ty);

/* Get collision type at position. */
int levelgen_col_at(int tx, int ty);

/* Set collision type at position (for breakable walls, etc.). */
void levelgen_set_collision(int tx, int ty, int col_type);

#endif /* GAME_LEVELGEN_H */
