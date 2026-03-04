/*
 * Ghost Protocol — Procedural Level Generator
 *
 * Generates 128x32 tile side-scrolling levels from seed + tier.
 * Uses seeded RNG for deterministic generation.
 */
#include "game/levelgen.h"
#include "engine/rng.h"
#include "engine/collision.h"
#include <string.h>

LevelData level_data;

/* Seeded local RNG (separate from global) */
static u32 lrng_state;

static u32 lrng_next(void) {
    lrng_state = lrng_state * 1103515245 + 12345;
    return (lrng_state >> 16) & 0x7FFF;
}

static int lrng_range(int max) {
    if (max <= 0) return 0;
    return (int)(lrng_next() % (u32)max);
}

/* ---- Section generators ---- */

static void set_tile(int tx, int ty, int visual, int col) {
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return;
    int idx = ty * NET_MAP_W + tx;
    level_data.tiles[idx] = (u8)visual;
    level_data.collision[idx] = (u8)col;
}

static void fill_solid(int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h && y < NET_MAP_H; y++) {
        for (int x = x0; x < x0 + w && x < NET_MAP_W; x++) {
            set_tile(x, y, NTILE_WALL, TILE_SOLID);
        }
    }
}

static void fill_floor(int x0, int y0, int w) {
    for (int x = x0; x < x0 + w && x < NET_MAP_W; x++) {
        set_tile(x, y0, NTILE_FLOOR, TILE_SOLID);
    }
}

static void add_platform(int x, int y, int w) {
    for (int i = 0; i < w && x + i < NET_MAP_W; i++) {
        set_tile(x + i, y, NTILE_PLATFORM, TILE_PLATFORM);
    }
}

static void add_hazard(int x, int y) {
    set_tile(x, y, NTILE_HAZARD_SPIKE, TILE_HAZARD);
}

static void add_spawn_point(int tx, int ty) {
    if (level_data.num_spawns >= 32) return;
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return;
    level_data.spawn_points[level_data.num_spawns][0] = (u8)tx;
    level_data.spawn_points[level_data.num_spawns][1] = (u8)ty;
    level_data.num_spawns++;
}

static void gen_flat(int sect_x, int tier) {
    /* 3 sub-variants: 0=data corridor, 1=server room, 2=maintenance tunnel */
    int variant = lrng_range(3);

    int floor_y = 27 + lrng_range(3); /* 27-29 */
    fill_floor(sect_x, floor_y, 16);
    /* Floor edges */
    set_tile(sect_x, floor_y, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, floor_y, NTILE_FLOOR_EDGE_R, TILE_SOLID);

    /* Ceiling with cap */
    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    }

    /* Add some platforms with edges */
    int num_plats = 1 + lrng_range(2);
    for (int i = 0; i < num_plats; i++) {
        int px = sect_x + 2 + lrng_range(10);
        int py = floor_y - 3 - lrng_range(3);
        int pw = 2 + lrng_range(3);
        add_platform(px, py, pw);
        set_tile(px, py, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        if (px + pw - 1 < NET_MAP_W) {
            set_tile(px + pw - 1, py, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        }
    }

    /* Enemy spawn */
    add_spawn_point(sect_x + 4 + lrng_range(8), floor_y - 1);

    /* Hazards scale with tier — guaranteed at tier 3+, extra at tier 5+ */
    if (tier > 2) {
        add_hazard(sect_x + 3 + lrng_range(10), floor_y - 1);
        if (tier > 4) {
            add_hazard(sect_x + 2 + lrng_range(12), floor_y - 1);
        }
    }

    /* Sub-variant specific decorations */
    if (variant == 0) {
        /* Data corridor: circuit floors, data streams, glitch artifacts */
        set_tile(sect_x + 4 + lrng_range(8), floor_y, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
        set_tile(sect_x + 2 + lrng_range(6), floor_y, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
        set_tile(sect_x + 3 + lrng_range(10), 8 + lrng_range(8), NTILE_DATA_STREAM, TILE_EMPTY);
        set_tile(sect_x + 1 + lrng_range(14), 6 + lrng_range(6), NTILE_DATA_STREAM, TILE_EMPTY);
        set_tile(sect_x + 5 + lrng_range(6), 4, NTILE_CABLE_H, TILE_EMPTY);
        if (lrng_range(2) == 0) {
            set_tile(sect_x + 3 + lrng_range(10), 5 + lrng_range(8), NTILE_GLITCH, TILE_EMPTY);
        }
        if (lrng_range(2) == 0) {
            set_tile(sect_x + 7 + lrng_range(4), 10 + lrng_range(6), NTILE_GLITCH, TILE_EMPTY);
        }
        set_tile(sect_x + 2 + lrng_range(12), 3, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    } else if (variant == 1) {
        /* Server room: server racks, screens, junction boxes */
        set_tile(sect_x + 2, 4, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 2, 5, NTILE_SERVER_BOT, TILE_EMPTY);
        set_tile(sect_x + 12, 4, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 12, 5, NTILE_SERVER_BOT, TILE_EMPTY);
        if (lrng_range(2) == 0) {
            set_tile(sect_x + 7, 4, NTILE_SERVER_TOP, TILE_EMPTY);
            set_tile(sect_x + 7, 5, NTILE_SERVER_BOT, TILE_EMPTY);
        }
        set_tile(sect_x + 4 + lrng_range(4), 3, NTILE_SCREEN, TILE_EMPTY);
        set_tile(sect_x + 9 + lrng_range(4), 3, NTILE_SCREEN, TILE_EMPTY);
        set_tile(sect_x + 1, 6, NTILE_JUNCTION_BOX, TILE_EMPTY);
        set_tile(sect_x + 14, 6, NTILE_JUNCTION_BOX, TILE_EMPTY);
        set_tile(sect_x + 5 + lrng_range(6), floor_y, NTILE_FLOOR_GRATE, TILE_SOLID);
    } else {
        /* Maintenance tunnel: pipes, vents, broken panels, conduits */
        set_tile(sect_x + 1 + lrng_range(6), 3, NTILE_PIPE_H, TILE_EMPTY);
        set_tile(sect_x + 8 + lrng_range(6), 3, NTILE_PIPE_H, TILE_EMPTY);
        set_tile(sect_x + 4 + lrng_range(8), 4, NTILE_VENT, TILE_EMPTY);
        for (int pv = 5; pv < floor_y - 4; pv += 3 + lrng_range(2)) {
            set_tile(sect_x + 1, pv, NTILE_PIPE_V, TILE_EMPTY);
            set_tile(sect_x + 14, pv, NTILE_PIPE_V, TILE_EMPTY);
        }
        set_tile(sect_x + 3 + lrng_range(10), 8 + lrng_range(6), NTILE_BROKEN_PANEL, TILE_EMPTY);
        set_tile(sect_x + 2 + lrng_range(12), 6 + lrng_range(4), NTILE_CONDUIT, TILE_EMPTY);
        set_tile(sect_x + 3 + lrng_range(10), floor_y, NTILE_FLOOR_CRACKED, TILE_SOLID);
        set_tile(sect_x + 7 + lrng_range(4), floor_y, NTILE_FLOOR_CRACKED, TILE_SOLID);
    }

    /* Common background decoration — guaranteed */
    set_tile(sect_x + 2 + lrng_range(12), 3, NTILE_CORRIDOR_GREEBLE, TILE_EMPTY);
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 2 + lrng_range(12), 10 + lrng_range(8), NTILE_MEMORY_BANK, TILE_EMPTY);
    }
}

static void gen_platforms(int sect_x, int tier) {
    /* Elevated platforms with gaps */
    int base_y = 26;
    /* Floor at bottom for catch */
    fill_floor(sect_x, 30, 16);
    set_tile(sect_x, 30, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 30, NTILE_FLOOR_EDGE_R, TILE_SOLID);
    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    }

    int num_plats = 3 + lrng_range(2);
    int px = sect_x + 1;
    int prev_py = base_y;
    for (int i = 0; i < num_plats; i++) {
        int py = base_y - lrng_range(3);
        /* Clamp vertical gap to max 3 tiles for traversability */
        if (py < prev_py - 3) py = prev_py - 3;
        if (py > prev_py + 3) py = prev_py + 3;
        int pw = 2 + lrng_range(3);
        /* Alternate between normal and energy platforms */
        if (lrng_range(4) == 0) {
            for (int j = 0; j < pw && px + j < NET_MAP_W; j++) {
                set_tile(px + j, py, NTILE_PLATFORM_ENERGY, TILE_PLATFORM);
            }
        } else {
            add_platform(px, py, pw);
        }
        set_tile(px, py, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        if (px + pw - 1 < NET_MAP_W) {
            set_tile(px + pw - 1, py, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        }
        if (i == num_plats / 2) {
            add_spawn_point(px + 1, py - 1);
        }
        /* Decoration below platform: server rack or circuit node */
        if (py + 2 < 30 && lrng_range(2) == 0) {
            set_tile(px + 1, py + 2, NTILE_SERVER_TOP, TILE_EMPTY);
            if (py + 3 < 30) {
                set_tile(px + 1, py + 3, NTILE_SERVER_BOT, TILE_EMPTY);
            }
        } else if (py + 2 < 30 && lrng_range(2) == 0) {
            set_tile(px + 1, py + 2, NTILE_CIRCUIT_NODE, TILE_EMPTY);
        }
        prev_py = py;
        px += pw + 1 + lrng_range(2);
        if (px >= sect_x + 15) break;
    }

    /* Decorative: data stream below a platform */
    set_tile(sect_x + 4 + lrng_range(8), 28, NTILE_DATA_STREAM, TILE_EMPTY);

    if (tier > 3) {
        add_hazard(sect_x + 3 + lrng_range(10), 29);
    }
}

static void gen_vertical(int sect_x, int tier) {
    /* Climb shaft — walls on sides, platforms zigzagging up */
    fill_solid(sect_x, 0, 2, NET_MAP_H);
    fill_solid(sect_x + 14, 0, 2, NET_MAP_H);

    /* Clear a 2-tile opening at y=28-29 in left wall for entry from previous section */
    set_tile(sect_x, 28, NTILE_EMPTY, TILE_EMPTY);
    set_tile(sect_x, 29, NTILE_EMPTY, TILE_EMPTY);
    set_tile(sect_x + 1, 28, NTILE_EMPTY, TILE_EMPTY);
    set_tile(sect_x + 1, 29, NTILE_EMPTY, TILE_EMPTY);

    /* Clear a 2-tile opening at y=28-29 in right wall for exit to next section */
    set_tile(sect_x + 14, 28, NTILE_EMPTY, TILE_EMPTY);
    set_tile(sect_x + 14, 29, NTILE_EMPTY, TILE_EMPTY);
    set_tile(sect_x + 15, 28, NTILE_EMPTY, TILE_EMPTY);
    set_tile(sect_x + 15, 29, NTILE_EMPTY, TILE_EMPTY);

    /* Shaft rivets on walls */
    for (int r = 4; r < 28; r += 4) {
        set_tile(sect_x + 1, r, NTILE_SHAFT_RIVET, TILE_SOLID);
        set_tile(sect_x + 14, r, NTILE_SHAFT_RIVET, TILE_SOLID);
    }
    /* Descent rails */
    set_tile(sect_x + 2, 4, NTILE_DESCENT_RAIL, TILE_EMPTY);
    set_tile(sect_x + 13, 4, NTILE_DESCENT_RAIL, TILE_EMPTY);

    /* Extend floor to full width (including under wall openings) */
    fill_floor(sect_x, 30, 16);

    int y = 26;
    int side = 0;
    while (y > 4) {
        int plx = side ? sect_x + 8 : sect_x + 3;
        add_platform(plx, y, 4);
        set_tile(plx, y, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(plx + 3, y, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        if (y == 14 || y == 22) {
            add_spawn_point(plx + 1, y - 1);
        }
        y -= 3 - lrng_range(2);
        side = 1 - side;
    }

    /* Pipe runs along both walls */
    for (int r = 3; r < 28; r++) {
        set_tile(sect_x + 2, r, NTILE_PIPE_V, TILE_EMPTY);
        set_tile(sect_x + 13, r, NTILE_PIPE_V, TILE_EMPTY);
    }

    if (tier > 2) {
        add_hazard(sect_x + 7, 29);
    }
    (void)tier;
}

static void gen_arena(int sect_x, int tier) {
    /* Open combat space */
    fill_floor(sect_x, 28, 16);
    set_tile(sect_x, 28, NTILE_CORNER_BL, TILE_SOLID);
    set_tile(sect_x + 15, 28, NTILE_CORNER_BR, TILE_SOLID);
    fill_solid(sect_x, 0, 16, 2);
    /* Arena border on ceiling */
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 2, NTILE_ARENA_BORDER, TILE_SOLID);
    }
    /* Walls on sides with wall caps */
    fill_solid(sect_x, 3, 1, 25);
    fill_solid(sect_x + 15, 3, 1, 25);
    set_tile(sect_x, 2, NTILE_CORNER_TL, TILE_SOLID);
    set_tile(sect_x + 15, 2, NTILE_CORNER_TR, TILE_SOLID);

    /* Platforms — layout varies with seed + per-platform Y jitter 0-1 */
    int layout = lrng_range(3);
    int jy1 = lrng_range(2); /* 0 to 1 */
    int jy2 = lrng_range(2); /* 0 to 1 */
    if (layout == 0) {
        /* Staircase layout — floor=28, chain up 4-5 tiles at a time */
        int py1 = 24 - jy1;          /* 23-24 (4-5 tiles from floor=28) */
        int py2 = py1 - 3 - jy2;     /* 3-4 tiles above py1 */
        add_platform(sect_x + 3, py1, 4);
        set_tile(sect_x + 3, py1, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 6, py1, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 7, py2, 4);
        set_tile(sect_x + 7, py2, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 10, py2, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 3, py2 - 3, 4);
    } else if (layout == 1) {
        /* Symmetric layout */
        int py1 = 24 - jy1;          /* 23-24 (4-5 tiles from floor=28) */
        int py2 = py1 - 4;           /* 4 tiles above py1 */
        add_platform(sect_x + 2, py1, 3);
        add_platform(sect_x + 11, py1, 3);
        add_platform(sect_x + 6, py2, 4);
        set_tile(sect_x + 6, py2, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 9, py2, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
    } else {
        /* Classic offset layout */
        int py1 = 24 - jy1;          /* 23-24 (4-5 tiles from floor=28) */
        int py2 = py1 - 4;           /* 4 tiles above py1 */
        add_platform(sect_x + 3, py1, 4);
        set_tile(sect_x + 3, py1, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 6, py1, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 9, py2, 4);
        set_tile(sect_x + 9, py2, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 12, py2, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 3, py2 - 3, 4);
    }

    /* Breakable blocks as cover (higher tiers) */
    if (tier > 2) {
        set_tile(sect_x + 7, 25, NTILE_BREAKABLE, TILE_SOLID);
        set_tile(sect_x + 8, 25, NTILE_BREAKABLE, TILE_SOLID);
    }

    /* Decorative: wall detail varies per instance */
    {
        int deco = lrng_range(3);
        if (deco == 0) {
            /* Tech arena: screens and junction boxes */
            set_tile(sect_x, 10, NTILE_SCREEN, TILE_SOLID);
            set_tile(sect_x + 15, 16, NTILE_SCREEN, TILE_SOLID);
            set_tile(sect_x, 24, NTILE_JUNCTION_BOX, TILE_SOLID);
            set_tile(sect_x + 15, 24, NTILE_JUNCTION_BOX, TILE_SOLID);
            set_tile(sect_x, 18, NTILE_WALL_PANEL, TILE_SOLID);
            set_tile(sect_x + 15, 8, NTILE_WALL_PANEL, TILE_SOLID);
        } else if (deco == 1) {
            /* Server arena: racks and data streams */
            set_tile(sect_x, 8, NTILE_SERVER_TOP, TILE_SOLID);
            set_tile(sect_x, 9, NTILE_SERVER_BOT, TILE_SOLID);
            set_tile(sect_x + 15, 14, NTILE_SERVER_TOP, TILE_SOLID);
            set_tile(sect_x + 15, 15, NTILE_SERVER_BOT, TILE_SOLID);
            set_tile(sect_x, 20, NTILE_DATA_STREAM, TILE_SOLID);
            set_tile(sect_x + 15, 20, NTILE_DATA_STREAM, TILE_SOLID);
            set_tile(sect_x, 24, NTILE_MEMORY_BANK, TILE_SOLID);
            set_tile(sect_x + 15, 24, NTILE_MEMORY_BANK, TILE_SOLID);
        } else {
            /* Ruined arena: broken panels and conduits */
            set_tile(sect_x, 10, NTILE_BROKEN_PANEL, TILE_SOLID);
            set_tile(sect_x + 15, 12, NTILE_BROKEN_PANEL, TILE_SOLID);
            set_tile(sect_x, 16, NTILE_CONDUIT, TILE_SOLID);
            set_tile(sect_x + 15, 18, NTILE_CONDUIT, TILE_SOLID);
            set_tile(sect_x, 24, NTILE_WALL_CIRCUIT, TILE_SOLID);
            set_tile(sect_x + 15, 24, NTILE_WALL_CIRCUIT, TILE_SOLID);
        }
    }
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 6 + lrng_range(4), 3, NTILE_NEON_SIGN, TILE_EMPTY);
    }

    /* Entry/exit openings in side walls (3 tiles high, at floor level) */
    for (int oy = 25; oy <= 27; oy++) {
        set_tile(sect_x, oy, NTILE_EMPTY, TILE_EMPTY);
        set_tile(sect_x + 15, oy, NTILE_EMPTY, TILE_EMPTY);
    }

    /* Multiple enemy spawns — arena is the combat-heavy section */
    int num_enemies = 2 + tier / 2;
    if (num_enemies > 5) num_enemies = 5;
    for (int i = 0; i < num_enemies; i++) {
        /* Spread spawns across floor and platforms */
        if (i < 2) {
            add_spawn_point(sect_x + 2 + lrng_range(12), 27);
        } else if (i == 2) {
            add_spawn_point(sect_x + 4, 22); /* Left platform */
        } else {
            add_spawn_point(sect_x + 10, 18); /* Right platform */
        }
    }
}

static void gen_corridor(int sect_x, int tier) {
    /* 2 sub-variants: 0=hazard gauntlet, 1=cable drip tunnel */
    int variant = lrng_range(2);

    /* Tight corridor with hazards */
    fill_floor(sect_x, 28, 16);
    set_tile(sect_x, 28, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 28, NTILE_FLOOR_EDGE_R, TILE_SOLID);
    fill_solid(sect_x, 0, 16, 7);  /* Low ceiling */
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 7, NTILE_WALL_CAP_BOT, TILE_SOLID);
    }

    /* Platforms to dodge over hazards */
    add_platform(sect_x + 4, 23, 6);
    set_tile(sect_x + 4, 23, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
    set_tile(sect_x + 9, 23, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
    /* Higher tiers add elevated secondary platform */
    if (tier >= 3) {
        int px2 = sect_x + 2 + lrng_range(4);
        add_platform(px2, 18, 3);
    }

    add_spawn_point(sect_x + 8, 27);
    /* Second spawn on platform if higher tier */
    if (tier > 2) {
        add_spawn_point(sect_x + 6, 22);
    }

    if (variant == 0) {
        /* Hazard gauntlet: many floor hazards, sparse decoration */
        set_tile(sect_x + 7, 28, NTILE_FLOOR_GRATE, TILE_SOLID);
        /* Hazard rows — mix spikes and tesla coils */
        int num_hazards = tier;
        if (num_hazards > 5) num_hazards = 5;
        for (int i = 0; i < num_hazards; i++) {
            int hx = sect_x + 2 + i * 3;
            if (i % 2 == 0) {
                add_hazard(hx, 27);
            } else {
                set_tile(hx, 27, NTILE_HAZARD_TESLA, TILE_HAZARD);
            }
        }
        /* Corridor greeble on ceiling walls */
        set_tile(sect_x + 3, 8, NTILE_CORRIDOR_GREEBLE, TILE_EMPTY);
        set_tile(sect_x + 10, 8, NTILE_CORRIDOR_GREEBLE, TILE_EMPTY);
        set_tile(sect_x + 1, 8, NTILE_PIPE_H, TILE_EMPTY);
        set_tile(sect_x + 14, 8, NTILE_PIPE_H, TILE_EMPTY);
        if (lrng_range(2) == 0) {
            set_tile(sect_x + 6 + lrng_range(4), 8, NTILE_VENT, TILE_EMPTY);
        }
    } else {
        /* Cable drip tunnel: hanging cables, data waterfalls, dense pipes */
        /* Vertical cables hanging from ceiling at irregular intervals */
        for (int cx = sect_x + 2; cx < sect_x + 14; cx += 2 + lrng_range(3)) {
            int cable_len = 4 + lrng_range(6);
            for (int cy = 8; cy < 8 + cable_len && cy < 22; cy++) {
                set_tile(cx, cy, NTILE_CABLE_V, TILE_EMPTY);
            }
        }
        /* Data waterfalls between cables */
        set_tile(sect_x + 5 + lrng_range(6), 10 + lrng_range(4), NTILE_DATA_WATERFALL, TILE_EMPTY);
        set_tile(sect_x + 3 + lrng_range(10), 14 + lrng_range(4), NTILE_DATA_WATERFALL, TILE_EMPTY);
        /* Pipe network on ceiling */
        for (int px = sect_x + 1; px < sect_x + 15; px += 3 + lrng_range(2)) {
            set_tile(px, 8, NTILE_PIPE_H, TILE_EMPTY);
        }
        /* Fewer floor hazards but cracked floor */
        if (tier > 2) {
            add_hazard(sect_x + 4 + lrng_range(8), 27);
        }
        set_tile(sect_x + 3 + lrng_range(5), 28, NTILE_FLOOR_CRACKED, TILE_SOLID);
        set_tile(sect_x + 9 + lrng_range(5), 28, NTILE_FLOOR_CRACKED, TILE_SOLID);
    }

    /* Breakable block obstacle */
    if (tier > 1 && lrng_range(3) == 0) {
        int bx = sect_x + 6 + lrng_range(4);
        set_tile(bx, 27, NTILE_BREAKABLE, TILE_SOLID);
    }
    /* Glitch artifact decoration */
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 3 + lrng_range(10), 12 + lrng_range(8), NTILE_GLITCH, TILE_EMPTY);
    }
}

static void gen_descent(int sect_x, int tier) {
    /* Downward path */
    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    }

    /* Descent rails on sides with pipe runs */
    for (int r = 3; r < 28; r++) {
        set_tile(sect_x, r, NTILE_DESCENT_RAIL, TILE_EMPTY);
        set_tile(sect_x + 15, r, NTILE_DESCENT_RAIL, TILE_EMPTY);
        if (r > 5 && r < 26) {
            set_tile(sect_x + 1, r, NTILE_PIPE_V, TILE_EMPTY);
            set_tile(sect_x + 14, r, NTILE_PIPE_V, TILE_EMPTY);
        }
    }

    int y = 8;
    int side = 0;
    while (y < 28) {
        int px = side ? sect_x + 8 : sect_x + 2;
        add_platform(px, y, 5);
        set_tile(px, y, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(px + 4, y, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        if (y > 16) {
            add_spawn_point(px + 2, y - 1);
        }
        /* Data waterfall between platforms */
        if (y > 10 && lrng_range(2) == 0) {
            set_tile(sect_x + 7, y + 1, NTILE_DATA_WATERFALL, TILE_EMPTY);
        }
        y += 3 + lrng_range(2);
        side = 1 - side;
    }
    fill_floor(sect_x, 30, 16);
    set_tile(sect_x, 30, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 30, NTILE_FLOOR_EDGE_R, TILE_SOLID);

    if (tier > 3) {
        add_hazard(sect_x + 6, 29);
        add_hazard(sect_x + 10, 29);
    }
    /* Extra spawn on upper platforms at tier 3+ */
    if (tier >= 3) {
        add_spawn_point(sect_x + 4 + lrng_range(6), 11);
    }

    /* Cable runs between platforms */
    for (int r = 10; r < 26; r += 6) {
        if (lrng_range(2) == 0) {
            set_tile(sect_x + 7, r, NTILE_CABLE_V, TILE_EMPTY);
        }
    }
    /* Console near bottom */
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 4, 29, NTILE_CONSOLE, TILE_EMPTY);
    }
}

static void gen_maze(int sect_x, int tier) {
    /* Multi-path section */
    fill_floor(sect_x, 28, 16);
    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    }

    /* Create 3 horizontal paths with maze junction tiles */
    int paths[3] = { 10, 18, 24 };
    for (int i = 0; i < 3; i++) {
        fill_floor(sect_x, paths[i], 16);
        set_tile(sect_x, paths[i], NTILE_FLOOR_EDGE_L, TILE_SOLID);
        set_tile(sect_x + 15, paths[i], NTILE_FLOOR_EDGE_R, TILE_SOLID);
        /* Make first 2 and last 2 columns passable (platform instead of solid)
         * so players can enter/exit the maze at section boundaries */
        set_tile(sect_x, paths[i], NTILE_PLATFORM, TILE_PLATFORM);
        set_tile(sect_x + 1, paths[i], NTILE_PLATFORM, TILE_PLATFORM);
        set_tile(sect_x + 14, paths[i], NTILE_PLATFORM, TILE_PLATFORM);
        set_tile(sect_x + 15, paths[i], NTILE_PLATFORM, TILE_PLATFORM);
        /* Maze junction at mid-point */
        set_tile(sect_x + 8, paths[i], NTILE_MAZE_JUNCTION, TILE_SOLID);
        /* Gaps in paths for vertical movement */
        int gap = sect_x + 5 + lrng_range(6);
        set_tile(gap, paths[i], NTILE_EMPTY, TILE_EMPTY);
        set_tile(gap + 1, paths[i], NTILE_EMPTY, TILE_EMPTY);
    }

    /* Decorative: circuit nodes at junctions, server racks */
    set_tile(sect_x + 3, 6, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 12, 14, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 1, 4, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 1, 5, NTILE_SERVER_BOT, TILE_EMPTY);
    }

    add_spawn_point(sect_x + 4, paths[0] - 1);
    add_spawn_point(sect_x + 10, paths[1] - 1);
    /* Third spawn on bottom path at higher tiers */
    if (tier >= 2) {
        add_spawn_point(sect_x + 7, paths[2] - 1);
    }

    /* Breakable walls in maze paths (block shortcuts) */
    if (tier > 1) {
        int bpath = lrng_range(3);
        set_tile(sect_x + 7, paths[bpath], NTILE_BREAKABLE, TILE_SOLID);
    }
    /* Glitch artifacts in open space */
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 5 + lrng_range(6), 6 + lrng_range(3), NTILE_GLITCH, TILE_EMPTY);
    }

    /* Maze hazards: spike traps on paths at higher tiers */
    if (tier > 2) {
        int hpath = lrng_range(3);
        int hx = sect_x + 4 + lrng_range(8);
        add_hazard(hx, paths[hpath] - 1);
        if (tier > 4) {
            int hpath2 = (hpath + 1) % 3;
            add_hazard(sect_x + 3 + lrng_range(10), paths[hpath2] - 1);
        }
    }
}

static void gen_boss(int sect_x, int tier) {
    /* Boss arena — special boss floor */
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 28, NTILE_BOSS_FLOOR, TILE_SOLID);
    }
    /* Ceiling with arena border */
    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++) {
        set_tile(x, 2, NTILE_ARENA_BORDER, TILE_SOLID);
    }
    /* Walls with corners */
    fill_solid(sect_x, 3, 1, 25);
    fill_solid(sect_x + 15, 3, 1, 25);
    set_tile(sect_x, 2, NTILE_CORNER_TL, TILE_SOLID);
    set_tile(sect_x + 15, 2, NTILE_CORNER_TR, TILE_SOLID);
    set_tile(sect_x, 28, NTILE_CORNER_BL, TILE_SOLID);
    set_tile(sect_x + 15, 28, NTILE_CORNER_BR, TILE_SOLID);

    /* Platform at center — 5 tiles above floor=28 (40px, within jump range) */
    add_platform(sect_x + 5, 23, 6);
    set_tile(sect_x + 5, 23, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
    set_tile(sect_x + 10, 23, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
    /* Upper platforms — 5 tiles above central (40px, reachable from central) */
    add_platform(sect_x + 2, 18, 4);
    add_platform(sect_x + 10, 18, 4);

    /* Decorative: wall screens, conduit, console */
    set_tile(sect_x, 12, NTILE_SCREEN, TILE_SOLID);
    set_tile(sect_x + 15, 12, NTILE_SCREEN, TILE_SOLID);
    set_tile(sect_x + 7, 3, NTILE_CONDUIT, TILE_EMPTY);
    set_tile(sect_x + 8, 3, NTILE_CONDUIT, TILE_EMPTY);
    set_tile(sect_x + 4, 27, NTILE_CONSOLE, TILE_EMPTY);

    /* Entry opening in left wall (3 tiles high, at floor level) */
    for (int oy = 25; oy <= 27; oy++) {
        set_tile(sect_x, oy, NTILE_EMPTY, TILE_EMPTY);
    }

    /* Boss spawn at center */
    add_spawn_point(sect_x + 8, 27);

    /* Data streams on side walls */
    set_tile(sect_x, 20, NTILE_DATA_STREAM, TILE_SOLID);
    set_tile(sect_x + 15, 20, NTILE_DATA_STREAM, TILE_SOLID);
    /* Cable corner decorations */
    set_tile(sect_x + 3, 3, NTILE_CABLE_CORNER, TILE_EMPTY);
    set_tile(sect_x + 12, 3, NTILE_CABLE_CORNER, TILE_EMPTY);
    /* Cables connecting to conduit */
    set_tile(sect_x + 5, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 10, 3, NTILE_CABLE_H, TILE_EMPTY);

    /* Tier-based arena modifications */
    if (tier >= 2) {
        /* Add floor hazard gaps — force jump during boss fight */
        set_tile(sect_x + 4, 28, NTILE_EMPTY, TILE_EMPTY);
        set_tile(sect_x + 11, 28, NTILE_EMPTY, TILE_EMPTY);
    }
    if (tier >= 3) {
        /* Add hazard spikes in floor gaps */
        set_tile(sect_x + 4, 29, NTILE_HAZARD_SPIKE, TILE_HAZARD);
        set_tile(sect_x + 11, 29, NTILE_HAZARD_SPIKE, TILE_HAZARD);
    }
    if (tier >= 4) {
        /* Add extra elevated platform — 4 tiles above upper platforms (reachable) */
        add_platform(sect_x + 6, 14, 4);
    }
}

/* ---- Main generator ---- */

void levelgen_generate(u16 seed, int tier, int is_boss) {
    /* Clear */
    memset(&level_data, 0, sizeof(level_data));
    level_data.seed = seed;
    level_data.tier = (u8)tier;
    level_data.is_boss_level = (u8)is_boss;

    /* Seed local RNG */
    lrng_state = (u32)seed * 2654435761u;

    /* Determine section types */
    /* Section type weights based on tier, no consecutive repeats */
    int prev_sect = -1;
    for (int s = 0; s < 8; s++) {
        if (s == 7 && is_boss) {
            level_data.sections[s] = SECT_BOSS;
        } else if (s == 0) {
            level_data.sections[s] = SECT_FLAT; /* Start easy */
        } else {
            /* Weighted random section type, reroll on repeat */
            int chosen;
            int attempts = 0;
            do {
                int r = lrng_range(100);
                if (r < 15) {
                    chosen = SECT_FLAT;
                } else if (r < 30) {
                    chosen = SECT_PLATFORMS;
                } else if (r < 42) {
                    chosen = SECT_VERTICAL;
                } else if (r < 58) {
                    chosen = SECT_ARENA;
                } else if (r < 70) {
                    chosen = SECT_CORRIDOR;
                } else if (r < 85) {
                    chosen = SECT_DESCENT;
                } else {
                    chosen = SECT_MAZE;
                }
                attempts++;
            } while (chosen == prev_sect && attempts < 3);
            level_data.sections[s] = (u8)chosen;
        }
        prev_sect = level_data.sections[s];
    }

    /* Generate each section */
    for (int s = 0; s < 8; s++) {
        int sect_x = s * 16;
        switch (level_data.sections[s]) {
        case SECT_FLAT:      gen_flat(sect_x, tier); break;
        case SECT_PLATFORMS: gen_platforms(sect_x, tier); break;
        case SECT_VERTICAL:  gen_vertical(sect_x, tier); break;
        case SECT_ARENA:     gen_arena(sect_x, tier); break;
        case SECT_CORRIDOR:  gen_corridor(sect_x, tier); break;
        case SECT_DESCENT:   gen_descent(sect_x, tier); break;
        case SECT_MAZE:      gen_maze(sect_x, tier); break;
        case SECT_BOSS:      gen_boss(sect_x, tier); break;
        }
    }

    /* Post-generation pass: smooth floor seams at section boundaries */
    for (int s = 1; s < 8; s++) {
        int bx = s * 16; /* Boundary column (first col of new section) */
        for (int by = 0; by < NET_MAP_H; by++) {
            int left_tile = level_data.tiles[by * NET_MAP_W + (bx - 1)];
            int right_tile = level_data.tiles[by * NET_MAP_W + bx];
            int left_col = level_data.collision[by * NET_MAP_W + (bx - 1)];
            int right_col = level_data.collision[by * NET_MAP_W + bx];
            /* Replace edge tiles with plain floor where both sides are solid floor */
            if (left_col == TILE_SOLID && right_col == TILE_SOLID) {
                if (left_tile == NTILE_FLOOR_EDGE_R) {
                    set_tile(bx - 1, by, NTILE_FLOOR, TILE_SOLID);
                }
                if (right_tile == NTILE_FLOOR_EDGE_L) {
                    set_tile(bx, by, NTILE_FLOOR, TILE_SOLID);
                }
            }
        }
    }

    /* Section boundary stitch pass: ensure connectivity between sections.
     * Uses the LOWER floor (max y) as the connecting level so no step-ups
     * are created. Carves from above the higher floor down to the lower
     * floor, clearing ALL solid tiles (not just NTILE_WALL). */
    for (int s = 1; s < 8; s++) {
        int bx = s * 16; /* First column of new section */

        /* Find lowest solid tile on each side (=floor surface) */
        int left_floor = -1, right_floor = -1;
        for (int by = NET_MAP_H - 2; by >= 3; by--) {
            if (left_floor < 0 && level_data.collision[by * NET_MAP_W + (bx - 1)] == TILE_SOLID) {
                left_floor = by;
            }
            if (right_floor < 0 && level_data.collision[by * NET_MAP_W + bx] == TILE_SOLID) {
                right_floor = by;
            }
            if (left_floor >= 0 && right_floor >= 0) break;
        }
        if (left_floor < 0) left_floor = 29;
        if (right_floor < 0) right_floor = 29;

        /* Use the LOWER floor (higher y = deeper in the level) as the connecting
         * floor level. This prevents step-up walls where a higher floor creates
         * an impassable 1-tile wall against a lower floor. */
        int floor_y = (left_floor > right_floor) ? left_floor : right_floor;
        int high_floor = (left_floor < right_floor) ? left_floor : right_floor;

        /* Carve from 3 tiles above the higher floor down to 1 above the lower
         * floor. This creates a passage tall enough for the player (14px = ~2 tiles)
         * and wide enough (4 cols = 32px > 12px hitbox). */
        int carve_top = high_floor - 3;
        if (carve_top < 1) carve_top = 1;
        for (int cy = carve_top; cy < floor_y; cy++) {
            for (int fx = bx - 2; fx <= bx + 1; fx++) {
                if (fx >= 0 && fx < NET_MAP_W) {
                    int idx = cy * NET_MAP_W + fx;
                    int col = level_data.collision[idx];
                    /* Only clear normal solid tiles; preserve hazards and breakables */
                    if (col == TILE_SOLID) {
                        set_tile(fx, cy, NTILE_EMPTY, TILE_EMPTY);
                    }
                }
            }
        }

        /* Ensure connecting floor exists at the lower level (4 tiles wide) */
        if (floor_y < NET_MAP_H) {
            for (int fx = bx - 2; fx <= bx + 1; fx++) {
                if (fx >= 0 && fx < NET_MAP_W &&
                    level_data.collision[floor_y * NET_MAP_W + fx] != TILE_SOLID) {
                    set_tile(fx, floor_y, NTILE_FLOOR, TILE_SOLID);
                }
            }
        }

        /* If floors differ by > 3 tiles, add stepping platforms between them */
        int diff = floor_y - high_floor;
        if (diff > 3) {
            int y = high_floor;
            while (y + 3 < floor_y) {
                y += 3;
                add_platform(bx - 2, y, 4);
            }
        }
    }

    /* Validate spawn has floor beneath */
    if (level_data.collision[27 * NET_MAP_W + 4] != TILE_SOLID) {
        /* Fill a small floor segment under spawn */
        for (int fx = 2; fx <= 6; fx++) {
            set_tile(fx, 27, NTILE_FLOOR, TILE_SOLID);
        }
    }

    /* Set spawn and exit */
    level_data.spawn_x = 4;
    level_data.spawn_y = 26;

    /* Exit at far right — find the floor in the last section */
    {
        int exit_floor = -1;
        int exit_col = 124;
        /* Scan down from y=20 to find walkable floor surface under exit.
         * Pick the first solid tile that has empty space above it (floor surface).
         * Skip platforms/ceilings that have solid above them. */
        for (int ey = 20; ey < NET_MAP_H - 1; ey++) {
            if (level_data.collision[ey * NET_MAP_W + exit_col] == TILE_SOLID &&
                level_data.collision[(ey - 1) * NET_MAP_W + exit_col] != TILE_SOLID) {
                exit_floor = ey;
                break;
            }
        }
        if (exit_floor < 0) exit_floor = 28;
        /* Place exit 1 tile above floor */
        level_data.exit_x = (u8)exit_col;
        level_data.exit_y = (u8)(exit_floor - 1);
        /* Ensure floor exists under exit */
        for (int fx = exit_col - 1; fx <= exit_col + 1; fx++) {
            if (fx >= 0 && fx < NET_MAP_W &&
                level_data.collision[exit_floor * NET_MAP_W + fx] != TILE_SOLID) {
                set_tile(fx, exit_floor, NTILE_FLOOR, TILE_SOLID);
            }
        }
        /* Clear space for gate (2 tiles tall) and passage above */
        for (int gy = exit_floor - 3; gy < exit_floor; gy++) {
            if (gy >= 0 && level_data.collision[gy * NET_MAP_W + exit_col] == TILE_SOLID) {
                set_tile(exit_col, gy, NTILE_EMPTY, TILE_EMPTY);
            }
        }
    }
    set_tile(level_data.exit_x, level_data.exit_y, NTILE_EXIT_GATE, TILE_EMPTY);
    set_tile(level_data.exit_x, level_data.exit_y - 1, NTILE_EXIT_GATE, TILE_EMPTY);
    /* Frame is decorative only — TILE_EMPTY so player can walk through to exit */
    set_tile(level_data.exit_x - 1, level_data.exit_y, NTILE_EXIT_FRAME, TILE_EMPTY);
    set_tile(level_data.exit_x - 1, level_data.exit_y - 1, NTILE_EXIT_FRAME, TILE_EMPTY);
    set_tile(level_data.exit_x + 1, level_data.exit_y, NTILE_EXIT_FRAME, TILE_EMPTY);
    set_tile(level_data.exit_x + 1, level_data.exit_y - 1, NTILE_EXIT_FRAME, TILE_EMPTY);

    /* Post-generation: fill solid below each column's floor to prevent void.
     * Finds the lowest floor surface (solid tile with non-solid above) in each
     * column and fills everything below it as solid. Prevents entities from
     * falling through empty space below floors. */
    for (int x = 0; x < NET_MAP_W; x++) {
        for (int y = NET_MAP_H - 1; y >= 3; y--) {
            int col = level_data.collision[y * NET_MAP_W + x];
            if (col == TILE_SOLID || col == TILE_PLATFORM) {
                int above = (y > 0) ? level_data.collision[(y - 1) * NET_MAP_W + x] : TILE_SOLID;
                if (above != TILE_SOLID && above != TILE_PLATFORM) {
                    /* Floor surface at y — fill solid below */
                    for (int fy = y + 1; fy < NET_MAP_H; fy++) {
                        if (level_data.collision[fy * NET_MAP_W + x] == TILE_EMPTY) {
                            set_tile(x, fy, NTILE_WALL, TILE_SOLID);
                        }
                    }
                }
                break;
            }
        }
    }

    /* Validate spawn points: ensure each has ground within 3 tiles below */
    for (int i = 0; i < (int)level_data.num_spawns; i++) {
        int tx = level_data.spawn_points[i][0];
        int ty = level_data.spawn_points[i][1];
        int found = 0;
        for (int dy = 1; dy <= 3; dy++) {
            int cy = ty + dy;
            if (cy >= NET_MAP_H) break;
            int col = level_data.collision[cy * NET_MAP_W + tx];
            if (col == TILE_SOLID || col == TILE_PLATFORM) {
                /* Ground found — adjust spawn to 1 tile above it */
                level_data.spawn_points[i][1] = (u8)(cy - 1);
                found = 1;
                break;
            }
        }
        if (!found) {
            /* Scan further down for any ground */
            for (int sy = ty + 4; sy < NET_MAP_H; sy++) {
                int col = level_data.collision[sy * NET_MAP_W + tx];
                if (col == TILE_SOLID || col == TILE_PLATFORM) {
                    level_data.spawn_points[i][1] = (u8)(sy - 1);
                    found = 1;
                    break;
                }
            }
        }
        if (!found) {
            /* No ground at all — remove spawn point */
            level_data.num_spawns--;
            level_data.spawn_points[i][0] = level_data.spawn_points[level_data.num_spawns][0];
            level_data.spawn_points[i][1] = level_data.spawn_points[level_data.num_spawns][1];
            i--;
        }
    }

    /* Set collision map for engine */
    collision_set_map(level_data.collision, NET_MAP_W, NET_MAP_H);
}

int levelgen_tile_at(int tx, int ty) {
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return NTILE_EMPTY;
    return level_data.tiles[ty * NET_MAP_W + tx];
}

int levelgen_col_at(int tx, int ty) {
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return TILE_EMPTY;
    return level_data.collision[ty * NET_MAP_W + tx];
}
