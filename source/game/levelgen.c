/*
 * Ghost Protocol — Procedural Level Generator
 *
 * Generates 256x32 tile side-scrolling levels from seed + tier.
 * Uses seeded RNG for deterministic generation.
 */
#include "game/levelgen.h"
#include "game/quest.h"
#include "engine/rng.h"
#include "engine/collision.h"
#include <string.h>

EWRAM_BSS LevelData level_data;

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
    if (level_data.num_spawns >= 48) return;
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

    /* Rich decoration below and around platforms */
    /* Data streams flowing between platforms */
    set_tile(sect_x + 4 + lrng_range(4), 28, NTILE_DATA_STREAM, TILE_EMPTY);
    set_tile(sect_x + 9 + lrng_range(4), 28, NTILE_DATA_STREAM, TILE_EMPTY);

    /* Vertical cables hanging from ceiling */
    {
        int c1 = sect_x + 3 + lrng_range(4);
        int c2 = sect_x + 9 + lrng_range(4);
        for (int cy = 3; cy < 10 + lrng_range(6); cy++)
            set_tile(c1, cy, NTILE_CABLE_V, TILE_EMPTY);
        for (int cy = 3; cy < 8 + lrng_range(6); cy++)
            set_tile(c2, cy, NTILE_CABLE_V, TILE_EMPTY);
    }

    /* Ceiling pipe runs */
    for (int cpx = sect_x + 2; cpx < sect_x + 14; cpx += 4 + lrng_range(2))
        set_tile(cpx, 3, NTILE_PIPE_H, TILE_EMPTY);

    /* Neon sign */
    if (lrng_range(2) == 0)
        set_tile(sect_x + 6 + lrng_range(4), 4, NTILE_NEON_SIGN, TILE_EMPTY);

    /* Floor grate */
    set_tile(sect_x + 7, 30, NTILE_FLOOR_GRATE, TILE_SOLID);

    /* Broken panel at ground level */
    if (lrng_range(3) == 0)
        set_tile(sect_x + 2 + lrng_range(12), 29, NTILE_BROKEN_PANEL, TILE_EMPTY);

    /* Glitch artifact */
    if (lrng_range(3) == 0)
        set_tile(sect_x + 3 + lrng_range(10), 20 + lrng_range(6), NTILE_GLITCH, TILE_EMPTY);

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

    /* Wall-mounted screens at intervals */
    set_tile(sect_x + 1, 8, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 14, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 20, NTILE_SCREEN, TILE_EMPTY);

    /* Junction boxes at pipe joints */
    set_tile(sect_x + 2, 10, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 13, 18, NTILE_JUNCTION_BOX, TILE_EMPTY);

    /* Memory banks recessed in walls */
    set_tile(sect_x + 1, 12, NTILE_MEMORY_BANK, TILE_EMPTY);
    set_tile(sect_x + 14, 22, NTILE_MEMORY_BANK, TILE_EMPTY);

    /* Data streams in open shaft space */
    if (lrng_range(2) == 0) {
        int ds_x = sect_x + 6 + lrng_range(4);
        for (int dy = 5; dy < 16; dy += 3)
            set_tile(ds_x, dy, NTILE_DATA_STREAM, TILE_EMPTY);
    }

    /* Broken panels near base */
    set_tile(sect_x + 3, 28, NTILE_BROKEN_PANEL, TILE_EMPTY);
    set_tile(sect_x + 12, 28, NTILE_BROKEN_PANEL, TILE_EMPTY);

    /* Floor details */
    set_tile(sect_x + 7, 30, NTILE_FLOOR_GRATE, TILE_SOLID);
    set_tile(sect_x + 8, 30, NTILE_FLOOR_GRATE, TILE_SOLID);

    /* Glitch artifact in shaft */
    if (lrng_range(3) == 0)
        set_tile(sect_x + 5 + lrng_range(6), 8 + lrng_range(10), NTILE_GLITCH, TILE_EMPTY);

    if (tier > 2) {
        add_hazard(sect_x + 7, 29);
    }
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
    int py1 = 24 - jy1;      /* First platform Y (outer scope for spawn refs) */
    int py2 = py1 - 3 - jy2; /* Second platform Y (default for layout 0) */
    if (layout == 0) {
        /* Staircase layout — floor=28, chain up 4-5 tiles at a time */
        add_platform(sect_x + 3, py1, 4);
        set_tile(sect_x + 3, py1, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 6, py1, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 7, py2, 4);
        set_tile(sect_x + 7, py2, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 10, py2, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 3, py2 - 3, 4);
    } else if (layout == 1) {
        /* Symmetric layout */
        py2 = py1 - 4;           /* 4 tiles above py1 */
        add_platform(sect_x + 2, py1, 3);
        add_platform(sect_x + 11, py1, 3);
        add_platform(sect_x + 6, py2, 4);
        set_tile(sect_x + 6, py2, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 9, py2, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
    } else {
        /* Classic offset layout */
        py2 = py1 - 4;           /* 4 tiles above py1 */
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

    /* Decorative: wall detail (walls are already TILE_SOLID, decorations
     * replace visual only — keep TILE_SOLID since they're on wall columns) */
    {
        int deco = lrng_range(3);
        if (deco == 0) {
            set_tile(sect_x + 1, 10, NTILE_SCREEN, TILE_EMPTY);
            set_tile(sect_x + 14, 16, NTILE_SCREEN, TILE_EMPTY);
            set_tile(sect_x + 1, 24, NTILE_JUNCTION_BOX, TILE_EMPTY);
            set_tile(sect_x + 14, 24, NTILE_JUNCTION_BOX, TILE_EMPTY);
        } else if (deco == 1) {
            set_tile(sect_x + 1, 8, NTILE_SERVER_TOP, TILE_EMPTY);
            set_tile(sect_x + 1, 9, NTILE_SERVER_BOT, TILE_EMPTY);
            set_tile(sect_x + 14, 14, NTILE_SERVER_TOP, TILE_EMPTY);
            set_tile(sect_x + 14, 15, NTILE_SERVER_BOT, TILE_EMPTY);
            set_tile(sect_x + 1, 20, NTILE_DATA_STREAM, TILE_EMPTY);
            set_tile(sect_x + 14, 20, NTILE_DATA_STREAM, TILE_EMPTY);
        } else {
            set_tile(sect_x + 1, 10, NTILE_BROKEN_PANEL, TILE_EMPTY);
            set_tile(sect_x + 14, 12, NTILE_BROKEN_PANEL, TILE_EMPTY);
            set_tile(sect_x + 1, 16, NTILE_CONDUIT, TILE_EMPTY);
            set_tile(sect_x + 14, 18, NTILE_CONDUIT, TILE_EMPTY);
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

    /* Multiple enemy spawns — arena is the combat-heavy section.
     * Use actual platform Y values (py1, py2) computed above. */
    int num_enemies = 2 + tier / 2;
    if (num_enemies > 5) num_enemies = 5;
    for (int i = 0; i < num_enemies; i++) {
        /* Spread spawns across floor and platforms */
        if (i < 2) {
            add_spawn_point(sect_x + 2 + lrng_range(12), 27);
        } else if (i == 2) {
            /* First platform — use actual py1 */
            add_spawn_point(sect_x + 4, py1 - 1);
        } else {
            /* Second platform — use actual py2 */
            add_spawn_point(sect_x + 10, py2 - 1);
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
                set_tile(hx, 27, NTILE_HAZARD_TESLA, TILE_TESLA);
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
    /* Extra spawn on upper platforms at tier 3+ — use Y=16 (lower half, near platforms) */
    if (tier >= 3) {
        int sx_extra = side ? sect_x + 2 : sect_x + 8;
        add_spawn_point(sx_extra + 2, y > 10 ? y - 4 : 16);
    }

    /* Cable runs between platforms */
    for (int r = 10; r < 26; r += 4) {
        set_tile(sect_x + 7, r, NTILE_CABLE_V, TILE_EMPTY);
    }

    /* Wall-mounted screens at descent checkpoints */
    set_tile(sect_x + 1, 8, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 16, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 24, NTILE_SCREEN, TILE_EMPTY);

    /* Junction boxes on pipe runs */
    set_tile(sect_x + 1, 12, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 14, 20, NTILE_JUNCTION_BOX, TILE_EMPTY);

    /* Console at bottom */
    set_tile(sect_x + 4, 29, NTILE_CONSOLE, TILE_EMPTY);

    /* Broken panels showing wear */
    set_tile(sect_x + 1, 18, NTILE_BROKEN_PANEL, TILE_EMPTY);
    set_tile(sect_x + 14, 10, NTILE_BROKEN_PANEL, TILE_EMPTY);

    /* Floor circuit traces at bottom */
    set_tile(sect_x + 5, 30, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 10, 30, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Floor grate */
    set_tile(sect_x + 7, 30, NTILE_FLOOR_GRATE, TILE_SOLID);

    /* Glitch near data waterfalls */
    if (lrng_range(3) == 0)
        set_tile(sect_x + 6, 14 + lrng_range(8), NTILE_GLITCH, TILE_EMPTY);
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

    /* Dense decoration: circuit nodes, server racks, cables */
    /* Circuit nodes at all path junctions */
    set_tile(sect_x + 3, 6, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 12, 6, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 3, 14, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 12, 14, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 7, 21, NTILE_CIRCUIT_NODE, TILE_EMPTY);

    /* Server racks in open spaces between paths */
    set_tile(sect_x + 1, 4, NTILE_SERVER_TOP, TILE_EMPTY);
    set_tile(sect_x + 1, 5, NTILE_SERVER_BOT, TILE_EMPTY);
    set_tile(sect_x + 14, 12, NTILE_SERVER_TOP, TILE_EMPTY);
    set_tile(sect_x + 14, 13, NTILE_SERVER_BOT, TILE_EMPTY);
    set_tile(sect_x + 1, 20, NTILE_SERVER_TOP, TILE_EMPTY);
    set_tile(sect_x + 1, 21, NTILE_SERVER_BOT, TILE_EMPTY);

    /* Vertical cables between path layers */
    for (int cy = paths[0] + 1; cy < paths[1]; cy++)
        set_tile(sect_x + 4, cy, NTILE_CABLE_V, TILE_EMPTY);
    for (int cy = paths[1] + 1; cy < paths[2]; cy++)
        set_tile(sect_x + 11, cy, NTILE_CABLE_V, TILE_EMPTY);

    /* Horizontal pipes along path ceilings */
    for (int px = sect_x + 3; px < sect_x + 13; px += 4)
        set_tile(px, paths[0] - 2, NTILE_PIPE_H, TILE_EMPTY);

    /* Screens and consoles */
    set_tile(sect_x + 14, 6, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 14, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 6, 27, NTILE_CONSOLE, TILE_EMPTY);

    /* Data streams in corridors */
    set_tile(sect_x + 6, paths[0] - 1, NTILE_DATA_STREAM, TILE_EMPTY);
    set_tile(sect_x + 10, paths[1] - 1, NTILE_DATA_STREAM, TILE_EMPTY);

    /* Floor details */
    set_tile(sect_x + 4, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 11, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Neon sign in top open space */
    set_tile(sect_x + 7, 4, NTILE_NEON_SIGN, TILE_EMPTY);

    /* Memory banks */
    set_tile(sect_x + 14, 20, NTILE_MEMORY_BANK, TILE_EMPTY);

    add_spawn_point(sect_x + 4, paths[0] - 1);
    add_spawn_point(sect_x + 10, paths[1] - 1);
    if (tier >= 2)
        add_spawn_point(sect_x + 7, paths[2] - 1);

    /* Breakable walls in maze paths (block shortcuts) */
    if (tier > 1) {
        int bpath = lrng_range(3);
        set_tile(sect_x + 7, paths[bpath], NTILE_BREAKABLE, TILE_SOLID);
    }
    /* Glitch artifacts in open space */
    if (lrng_range(2) == 0)
        set_tile(sect_x + 5 + lrng_range(6), 6 + lrng_range(3), NTILE_GLITCH, TILE_EMPTY);

    /* Maze hazards */
    if (tier > 2) {
        int hpath = lrng_range(3);
        add_hazard(sect_x + 4 + lrng_range(8), paths[hpath] - 1);
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

    /* Rich boss arena decoration */
    /* Wall-mounted screens — surveillance feel */
    set_tile(sect_x + 1, 8, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 8, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 16, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 16, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 24, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 24, NTILE_SCREEN, TILE_EMPTY);

    /* Central conduit array on ceiling */
    for (int cx = sect_x + 6; cx <= sect_x + 9; cx++)
        set_tile(cx, 3, NTILE_CONDUIT, TILE_EMPTY);

    /* Cable network from conduit to walls */
    set_tile(sect_x + 3, 3, NTILE_CABLE_CORNER, TILE_EMPTY);
    set_tile(sect_x + 12, 3, NTILE_CABLE_CORNER, TILE_EMPTY);
    set_tile(sect_x + 4, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 5, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 10, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 11, 3, NTILE_CABLE_H, TILE_EMPTY);

    /* Vertical conduits down walls */
    for (int r = 4; r < 10; r++) {
        set_tile(sect_x + 1, r, NTILE_CONDUIT, TILE_EMPTY);
        set_tile(sect_x + 14, r, NTILE_CONDUIT, TILE_EMPTY);
    }

    /* Data streams on walls mid-section */
    set_tile(sect_x + 1, 12, NTILE_DATA_STREAM, TILE_EMPTY);
    set_tile(sect_x + 14, 12, NTILE_DATA_STREAM, TILE_EMPTY);
    set_tile(sect_x + 1, 20, NTILE_DATA_STREAM, TILE_EMPTY);
    set_tile(sect_x + 14, 20, NTILE_DATA_STREAM, TILE_EMPTY);

    /* Junction boxes at wall intersections */
    set_tile(sect_x + 1, 10, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 14, 10, NTILE_JUNCTION_BOX, TILE_EMPTY);

    /* Console at player entry side */
    set_tile(sect_x + 4, 27, NTILE_CONSOLE, TILE_EMPTY);

    /* Floor circuit traces in boss arena */
    set_tile(sect_x + 3, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 7, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 8, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 12, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Memory banks near entry */
    set_tile(sect_x + 2, 26, NTILE_MEMORY_BANK, TILE_EMPTY);

    /* Neon signs marking the arena */
    set_tile(sect_x + 5, 4, NTILE_NEON_SIGN, TILE_EMPTY);
    set_tile(sect_x + 10, 4, NTILE_NEON_SIGN, TILE_EMPTY);

    /* Entry opening in left wall (3 tiles high, at floor level) */
    for (int oy = 25; oy <= 27; oy++) {
        set_tile(sect_x, oy, NTILE_EMPTY, TILE_EMPTY);
    }

    /* Boss spawn at center */
    add_spawn_point(sect_x + 8, 27);

    /* Tier-based arena modifications */
    if (tier >= 2) {
        set_tile(sect_x + 4, 28, NTILE_EMPTY, TILE_EMPTY);
        set_tile(sect_x + 11, 28, NTILE_EMPTY, TILE_EMPTY);
    }
    if (tier >= 3) {
        set_tile(sect_x + 4, 29, NTILE_HAZARD_SPIKE, TILE_HAZARD);
        set_tile(sect_x + 11, 29, NTILE_HAZARD_SPIKE, TILE_HAZARD);
    }
    if (tier >= 4) {
        add_platform(sect_x + 6, 14, 4);
        set_tile(sect_x + 6, 14, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 9, 14, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
    }
}

static void gen_waterfall(int sect_x, int tier) {
    /* 2 sub-variants: 0=twin falls, 1=cascade grotto */
    int variant = lrng_range(2);

    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++)
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    fill_floor(sect_x, 30, 16);
    set_tile(sect_x, 30, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 30, NTILE_FLOOR_EDGE_R, TILE_SOLID);

    if (variant == 0) {
        /* Twin falls — two data streams with platforms between */
        for (int y = 3; y < 28; y++) {
            set_tile(sect_x + 5, y, NTILE_DATA_WATERFALL, TILE_EMPTY);
            set_tile(sect_x + 10, y, NTILE_DATA_WATERFALL, TILE_EMPTY);
        }
        /* Platforms between the falls — spawn on every other to avoid overcrowding */
        int y = 8;
        int plat_idx = 0;
        while (y < 27) {
            add_platform(sect_x + 6, y, 4);
            set_tile(sect_x + 6, y, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
            set_tile(sect_x + 9, y, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
            if ((plat_idx & 1) == 0)
                add_spawn_point(sect_x + 7, y - 1);
            plat_idx++;
            y += 4 + lrng_range(2);
        }
        /* Side ledges for entry/exit */
        add_platform(sect_x + 1, 14, 3);
        add_platform(sect_x + 12, 20, 3);
        /* Mist effect — data stream particles near falls */
        set_tile(sect_x + 4, 12, NTILE_DATA_STREAM, TILE_EMPTY);
        set_tile(sect_x + 11, 16, NTILE_DATA_STREAM, TILE_EMPTY);
        set_tile(sect_x + 4, 22, NTILE_DATA_STREAM, TILE_EMPTY);
        set_tile(sect_x + 11, 8, NTILE_DATA_STREAM, TILE_EMPTY);
    } else {
        /* Cascade grotto — wide central fall with offset platforms */
        for (int y = 3; y < 28; y++) {
            set_tile(sect_x + 7, y, NTILE_DATA_WATERFALL, TILE_EMPTY);
            set_tile(sect_x + 8, y, NTILE_DATA_WATERFALL, TILE_EMPTY);
        }
        int y = 8;
        int side = 0;
        int plat_idx2 = 0;
        while (y < 27) {
            int px = side ? sect_x + 10 : sect_x + 2;
            int pw = 3 + lrng_range(2);
            add_platform(px, y, pw);
            set_tile(px, y, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
            if (px + pw - 1 < NET_MAP_W)
                set_tile(px + pw - 1, y, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
            if ((plat_idx2 & 1) == 0)
                add_spawn_point(px + 1, y - 1);
            plat_idx2++;
            y += 4 + lrng_range(2);
            side = 1 - side;
        }
    }

    /* Pipe runs on sides with junction boxes */
    for (int r = 4; r < 28; r++) {
        set_tile(sect_x + 1, r, NTILE_PIPE_V, TILE_EMPTY);
        set_tile(sect_x + 14, r, NTILE_PIPE_V, TILE_EMPTY);
    }
    set_tile(sect_x + 1, 10, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 14, 18, NTILE_JUNCTION_BOX, TILE_EMPTY);

    /* Ceiling cables connecting to falls */
    set_tile(sect_x + 3, 3, NTILE_CABLE_CORNER, TILE_EMPTY);
    set_tile(sect_x + 12, 3, NTILE_CABLE_CORNER, TILE_EMPTY);
    set_tile(sect_x + 4, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 5, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 10, 3, NTILE_CABLE_H, TILE_EMPTY);
    set_tile(sect_x + 11, 3, NTILE_CABLE_H, TILE_EMPTY);

    /* Wall decorations */
    set_tile(sect_x + 1, 6, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 24, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 22, NTILE_CONDUIT, TILE_EMPTY);
    set_tile(sect_x + 14, 8, NTILE_CONDUIT, TILE_EMPTY);

    /* Floor decoration */
    set_tile(sect_x + 3, 30, NTILE_FLOOR_GRATE, TILE_SOLID);
    set_tile(sect_x + 12, 30, NTILE_FLOOR_GRATE, TILE_SOLID);
    set_tile(sect_x + 7, 30, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 8, 30, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Glitch artifacts near data streams */
    if (lrng_range(3) == 0) {
        set_tile(sect_x + 6, 10 + lrng_range(8), NTILE_GLITCH, TILE_EMPTY);
    }
    /* Neon sign */
    if (lrng_range(2) == 0) {
        set_tile(sect_x + 2 + lrng_range(3), 4, NTILE_NEON_SIGN, TILE_EMPTY);
    }

    if (tier > 2) {
        add_hazard(sect_x + 7, 29);
        if (tier > 4) add_hazard(sect_x + 4, 29);
    }
    /* Corruption zones at base of waterfalls (Act 3+) */
    if (tier >= 3) {
        set_tile(sect_x + 5, 29, NTILE_HAZARD_BEAM, TILE_CORRUPT);
        set_tile(sect_x + 10, 29, NTILE_HAZARD_BEAM, TILE_CORRUPT);
    }
}

static void gen_transit(int sect_x, int tier) {
    /* 2 sub-variants: 0=platform relay, 1=data highway */
    int variant = lrng_range(2);

    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++)
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    fill_floor(sect_x, 30, 16);
    set_tile(sect_x, 30, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 30, NTILE_FLOOR_EDGE_R, TILE_SOLID);

    /* Alternating platforms at 2 heights */
    int high_y = 18 - lrng_range(3);
    int low_y = 24 - lrng_range(2);
    for (int i = 0; i < 4; i++) {
        int px = sect_x + 1 + i * 4;
        int py = (i & 1) ? high_y : low_y;
        add_platform(px, py, 3);
        set_tile(px, py, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(px + 2, py, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        if (i == 1 || i == 3)
            add_spawn_point(px + 1, py - 1);
    }

    /* Cables connecting platforms */
    for (int r = high_y + 1; r < low_y; r++)
        set_tile(sect_x + 7, r, NTILE_CABLE_V, TILE_EMPTY);

    if (variant == 0) {
        /* Platform relay — junction nodes at platforms, server bank below */
        set_tile(sect_x + 2, high_y - 1, NTILE_CIRCUIT_NODE, TILE_EMPTY);
        set_tile(sect_x + 10, high_y - 1, NTILE_CIRCUIT_NODE, TILE_EMPTY);
        /* Server bank under low platforms */
        set_tile(sect_x + 2, 26, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 2, 27, NTILE_SERVER_BOT, TILE_EMPTY);
        set_tile(sect_x + 13, 26, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 13, 27, NTILE_SERVER_BOT, TILE_EMPTY);
        /* Pipe network along ceiling */
        for (int px = sect_x + 2; px < sect_x + 14; px += 3)
            set_tile(px, 3, NTILE_PIPE_H, TILE_EMPTY);
        /* Console at ground level */
        set_tile(sect_x + 7, 29, NTILE_CONSOLE, TILE_EMPTY);
    } else {
        /* Data highway — horizontal data streams that push player */
        for (int x = sect_x + 3; x < sect_x + 13; x++) {
            set_tile(x, high_y + 2, NTILE_DATA_STREAM, TILE_STREAM);
        }
        /* Memory banks on walls */
        set_tile(sect_x + 1, 10, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 14, 14, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 1, 20, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 14, 24, NTILE_MEMORY_BANK, TILE_EMPTY);
        /* Conduit run on floor */
        set_tile(sect_x + 4, 30, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
        set_tile(sect_x + 11, 30, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    }

    /* Common decoration: neon signs, screens, wall detail */
    set_tile(sect_x + 3, 3, NTILE_NEON_SIGN, TILE_EMPTY);
    set_tile(sect_x + 12, 3, NTILE_NEON_SIGN, TILE_EMPTY);
    set_tile(sect_x + 1, 16, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 10, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 27, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 14, 27, NTILE_JUNCTION_BOX, TILE_EMPTY);

    /* Floor grates */
    set_tile(sect_x + 6, 30, NTILE_FLOOR_GRATE, TILE_SOLID);
    set_tile(sect_x + 9, 30, NTILE_FLOOR_GRATE, TILE_SOLID);

    /* Glitch near cables */
    if (lrng_range(3) == 0)
        set_tile(sect_x + 8, high_y + 3, NTILE_GLITCH, TILE_EMPTY);

    if (tier > 3) {
        add_hazard(sect_x + 5, 29);
        add_hazard(sect_x + 10, 29);
    }
}

static void gen_security(int sect_x, int tier) {
    /* 2 sub-variants: 0=beam grid, 1=tesla maze */
    int variant = lrng_range(2);

    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++)
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    fill_floor(sect_x, 28, 16);
    set_tile(sect_x, 28, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 28, NTILE_FLOOR_EDGE_R, TILE_SOLID);

    if (variant == 0) {
        /* Beam grid — horizontal beam rows with safe gaps */
        int beam_rows[] = { 10, 16, 22 };
        int num_rows = (tier < 3) ? 2 : 3;
        for (int r = 0; r < num_rows; r++) {
            int by = beam_rows[r];
            int gap_start = sect_x + 2 + lrng_range(8);
            for (int x = sect_x + 1; x < sect_x + 15; x++) {
                if (x >= gap_start && x < gap_start + 3)
                    continue;
                set_tile(x, by, NTILE_HAZARD_BEAM, TILE_HAZARD);
            }
            add_platform(gap_start, by + 1, 3);
        }
        /* Tesla emitters on walls at beam endpoints */
        set_tile(sect_x + 1, 10, NTILE_HAZARD_TESLA, TILE_TESLA);
        set_tile(sect_x + 14, 10, NTILE_HAZARD_TESLA, TILE_TESLA);
        if (num_rows > 1) {
            set_tile(sect_x + 1, 16, NTILE_HAZARD_TESLA, TILE_TESLA);
            set_tile(sect_x + 14, 16, NTILE_HAZARD_TESLA, TILE_TESLA);
        }
    } else {
        /* Tesla maze — vertical beam columns with gaps */
        int beam_cols[] = { 4, 8, 12 };
        int num_cols = (tier < 3) ? 2 : 3;
        for (int c = 0; c < num_cols; c++) {
            int bx = sect_x + beam_cols[c];
            int gap_start = 8 + lrng_range(10);
            for (int y = 4; y < 26; y++) {
                if (y >= gap_start && y < gap_start + 4)
                    continue;
                set_tile(bx, y, NTILE_HAZARD_BEAM, TILE_HAZARD);
            }
            /* Platform at each gap */
            add_platform(bx - 1, gap_start + 4, 3);
        }
        /* Tesla emitters at column tops */
        for (int c = 0; c < num_cols; c++)
            set_tile(sect_x + beam_cols[c], 3, NTILE_HAZARD_TESLA, TILE_TESLA);
    }

    add_spawn_point(sect_x + 4, 27);
    add_spawn_point(sect_x + 12, 27);

    /* Security screens monitoring the grid */
    set_tile(sect_x + 7, 3, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 8, 3, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 1, 6, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 20, NTILE_SCREEN, TILE_EMPTY);

    /* Side wall conduit runs */
    for (int r = 5; r < 26; r += 4) {
        set_tile(sect_x + 1, r, NTILE_CONDUIT, TILE_EMPTY);
        set_tile(sect_x + 14, r + 2, NTILE_CONDUIT, TILE_EMPTY);
    }

    /* Floor circuit traces */
    set_tile(sect_x + 3, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 7, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 11, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Pipe network above */
    set_tile(sect_x + 4, 3, NTILE_PIPE_H, TILE_EMPTY);
    set_tile(sect_x + 11, 3, NTILE_PIPE_H, TILE_EMPTY);

    /* Junction boxes */
    set_tile(sect_x + 1, 26, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 14, 26, NTILE_JUNCTION_BOX, TILE_EMPTY);
}

static void gen_cache(int sect_x, int tier) {
    /* 2 sub-variants: 0=hidden vault, 1=data archive */
    int variant = lrng_range(2);

    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++)
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);
    fill_floor(sect_x, 28, 16);
    set_tile(sect_x, 28, NTILE_FLOOR_EDGE_L, TILE_SOLID);
    set_tile(sect_x + 15, 28, NTILE_FLOOR_EDGE_R, TILE_SOLID);

    /* Outer corridor always present */
    add_spawn_point(sect_x + 4, 27);
    add_platform(sect_x + 2, 22, 5);
    set_tile(sect_x + 2, 22, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
    set_tile(sect_x + 6, 22, NTILE_PLAT_EDGE_R, TILE_PLATFORM);

    /* Breakable wall enclosure (inner loot room) */
    int room_x = sect_x + 8;
    int room_y = 18;
    for (int bx = room_x; bx < room_x + 6 && bx < NET_MAP_W; bx++) {
        set_tile(bx, room_y, NTILE_BREAKABLE, TILE_SOLID);
        set_tile(bx, room_y + 4, NTILE_BREAKABLE, TILE_SOLID);
    }
    for (int by = room_y + 1; by < room_y + 4; by++) {
        set_tile(room_x, by, NTILE_BREAKABLE, TILE_SOLID);
        if (room_x + 5 < NET_MAP_W)
            set_tile(room_x + 5, by, NTILE_BREAKABLE, TILE_SOLID);
    }
    /* Circuit floor inside room */
    for (int bx = room_x + 1; bx < room_x + 5 && bx < NET_MAP_W; bx++)
        set_tile(bx, room_y + 4, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Item spawn OUTSIDE room entrance (not inside sealed breakable walls) */
    add_spawn_point(room_x - 2, room_y + 3);

    if (variant == 0) {
        /* Hidden vault — server banks, conduits, sealed feel */
        set_tile(sect_x + 2, 4, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 2, 5, NTILE_SERVER_BOT, TILE_EMPTY);
        set_tile(sect_x + 4, 4, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 4, 5, NTILE_SERVER_BOT, TILE_EMPTY);
        /* Conduit run connecting servers to vault */
        for (int cx = sect_x + 5; cx < room_x; cx++)
            set_tile(cx, 5, NTILE_CABLE_H, TILE_EMPTY);
        set_tile(room_x, 5, NTILE_CABLE_CORNER, TILE_EMPTY);
        for (int cy = 6; cy < room_y; cy++)
            set_tile(room_x, cy, NTILE_CABLE_V, TILE_EMPTY);
        /* Screens showing vault status */
        set_tile(sect_x + 1, 12, NTILE_SCREEN, TILE_EMPTY);
        set_tile(sect_x + 1, 16, NTILE_SCREEN, TILE_EMPTY);
    } else {
        /* Data archive — memory banks, data streams, scholarly */
        set_tile(sect_x + 1, 6, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 1, 10, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 1, 14, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 1, 18, NTILE_MEMORY_BANK, TILE_EMPTY);
        /* Data streams flowing down from ceiling */
        set_tile(sect_x + 3, 4, NTILE_DATA_WATERFALL, TILE_EMPTY);
        set_tile(sect_x + 3, 8, NTILE_DATA_WATERFALL, TILE_EMPTY);
        set_tile(sect_x + 3, 12, NTILE_DATA_WATERFALL, TILE_EMPTY);
        /* Circuit nodes connecting archive to vault */
        set_tile(sect_x + 6, 16, NTILE_CIRCUIT_NODE, TILE_EMPTY);
        set_tile(sect_x + 6, 10, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    }

    /* Console near vault entrance */
    set_tile(sect_x + 7, 27, NTILE_CONSOLE, TILE_EMPTY);
    /* Broken panels hint something was forced open */
    set_tile(sect_x + 14, 10, NTILE_BROKEN_PANEL, TILE_EMPTY);
    set_tile(sect_x + 14, 16, NTILE_BROKEN_PANEL, TILE_EMPTY);
    /* Neon sign above vault */
    set_tile(sect_x + 10, room_y - 2, NTILE_NEON_SIGN, TILE_EMPTY);
    /* Pipe along ceiling */
    for (int px = sect_x + 6; px < sect_x + 14; px += 3)
        set_tile(px, 3, NTILE_PIPE_H, TILE_EMPTY);
    /* Floor grate near entrance */
    set_tile(sect_x + 5, 28, NTILE_FLOOR_GRATE, TILE_SOLID);

    /* Additional breakable at higher tiers */
    if (tier > 2) {
        set_tile(sect_x + 3, 24, NTILE_BREAKABLE, TILE_SOLID);
        set_tile(sect_x + 3, 25, NTILE_BREAKABLE, TILE_SOLID);
    }
}

static void gen_network(int sect_x, int tier) {
    /* 2 sub-variants: 0=layered network, 1=hub-spoke */
    int variant = lrng_range(2);

    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++)
        set_tile(x, 2, NTILE_WALL_CAP_BOT, TILE_SOLID);

    /* 4 floor layers */
    int layers[] = { 10, 16, 22, 28 };
    for (int l = 0; l < 4; l++) {
        fill_floor(sect_x, layers[l], 16);
        set_tile(sect_x, layers[l], NTILE_FLOOR_EDGE_L, TILE_SOLID);
        set_tile(sect_x + 15, layers[l], NTILE_FLOOR_EDGE_R, TILE_SOLID);

        /* Gaps for vertical movement — stagger position per layer */
        int gap = sect_x + 3 + l * 3 + lrng_range(3);
        set_tile(gap, layers[l], NTILE_EMPTY, TILE_EMPTY);
        set_tile(gap + 1, layers[l], NTILE_EMPTY, TILE_EMPTY);

        /* Spawn on each layer — avoid gap tiles */
        int sp_x = sect_x + 2 + lrng_range(4);
        if (sp_x == gap || sp_x == gap + 1) sp_x = sect_x + 13;
        add_spawn_point(sp_x, layers[l] - 1);
    }

    if (variant == 0) {
        /* Layered network — cables between layers, server racks per floor */
        /* Vertical cables through gaps */
        for (int l = 0; l < 3; l++) {
            int cable_x = sect_x + 7 + l * 2;
            for (int cy = layers[l] + 1; cy < layers[l + 1]; cy++)
                set_tile(cable_x, cy, NTILE_CABLE_V, TILE_EMPTY);
        }
        /* Server racks on alternate layers */
        set_tile(sect_x + 2, layers[0] - 2, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 2, layers[0] - 1, NTILE_SERVER_BOT, TILE_EMPTY);
        set_tile(sect_x + 13, layers[1] - 2, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 13, layers[1] - 1, NTILE_SERVER_BOT, TILE_EMPTY);
        set_tile(sect_x + 2, layers[2] - 2, NTILE_SERVER_TOP, TILE_EMPTY);
        set_tile(sect_x + 2, layers[2] - 1, NTILE_SERVER_BOT, TILE_EMPTY);
        /* Data streams on bottom layer */
        set_tile(sect_x + 5, layers[3] - 1, NTILE_DATA_STREAM, TILE_EMPTY);
        set_tile(sect_x + 10, layers[3] - 1, NTILE_DATA_STREAM, TILE_EMPTY);
    } else {
        /* Hub-spoke — central conduit column with radial connections */
        for (int cy = 3; cy < 28; cy++)
            set_tile(sect_x + 8, cy, NTILE_CONDUIT, TILE_EMPTY);
        /* Horizontal cables from conduit to walls */
        for (int l = 0; l < 4; l++) {
            int hy = layers[l] - 2;
            for (int hx = sect_x + 2; hx < sect_x + 8; hx++)
                set_tile(hx, hy, NTILE_CABLE_H, TILE_EMPTY);
            for (int hx = sect_x + 9; hx < sect_x + 14; hx++)
                set_tile(hx, hy, NTILE_CABLE_H, TILE_EMPTY);
        }
        /* Memory banks at cable endpoints */
        set_tile(sect_x + 1, layers[0] - 2, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 14, layers[1] - 2, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 1, layers[2] - 2, NTILE_MEMORY_BANK, TILE_EMPTY);
        set_tile(sect_x + 14, layers[3] - 2, NTILE_MEMORY_BANK, TILE_EMPTY);
    }

    /* Common decoration */
    /* Circuit nodes at layer intersections */
    set_tile(sect_x + 4, 7, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 11, 13, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 4, 19, NTILE_CIRCUIT_NODE, TILE_EMPTY);
    set_tile(sect_x + 11, 25, NTILE_CIRCUIT_NODE, TILE_EMPTY);

    /* Screens and consoles */
    set_tile(sect_x + 1, 5, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 5, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 6, layers[3] - 1, NTILE_CONSOLE, TILE_EMPTY);

    /* Floor circuit traces on bottom */
    set_tile(sect_x + 4, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 8, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 12, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);

    /* Pipe on ceiling */
    for (int px = sect_x + 3; px < sect_x + 13; px += 4)
        set_tile(px, 3, NTILE_PIPE_H, TILE_EMPTY);

    /* Glitch artifacts */
    if (lrng_range(3) == 0)
        set_tile(sect_x + 5 + lrng_range(6), 14, NTILE_GLITCH, TILE_EMPTY);

    if (tier > 2)
        add_hazard(sect_x + 7, layers[1] - 1);
    if (tier > 4)
        add_hazard(sect_x + 10, layers[2] - 1);
    /* Corruption zones on bottom layer (Act 3+) */
    if (tier >= 3) {
        set_tile(sect_x + 3, layers[3] - 1, NTILE_HAZARD_BEAM, TILE_CORRUPT);
        set_tile(sect_x + 12, layers[3] - 1, NTILE_HAZARD_BEAM, TILE_CORRUPT);
    }
}

static void gen_gauntlet(int sect_x, int tier) {
    /* 2 sub-variants: 0=combat pit, 1=killbox corridor */
    int variant = lrng_range(2);

    fill_floor(sect_x, 28, 16);
    set_tile(sect_x, 28, NTILE_CORNER_BL, TILE_SOLID);
    set_tile(sect_x + 15, 28, NTILE_CORNER_BR, TILE_SOLID);
    fill_solid(sect_x, 0, 16, 2);
    for (int x = sect_x; x < sect_x + 16 && x < NET_MAP_W; x++)
        set_tile(x, 2, NTILE_ARENA_BORDER, TILE_SOLID);

    /* Side walls */
    fill_solid(sect_x, 3, 1, 25);
    fill_solid(sect_x + 15, 3, 1, 25);
    for (int oy = 25; oy <= 27; oy++) {
        set_tile(sect_x, oy, NTILE_EMPTY, TILE_EMPTY);
        set_tile(sect_x + 15, oy, NTILE_EMPTY, TILE_EMPTY);
    }

    if (variant == 0) {
        /* Combat pit — open arena with cover platforms */
        add_platform(sect_x + 3, 22, 4);
        set_tile(sect_x + 3, 22, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 6, 22, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        add_platform(sect_x + 9, 18, 4);
        set_tile(sect_x + 9, 18, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 12, 18, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        /* Extra high platform for sniping */
        add_platform(sect_x + 5, 12, 6);
        set_tile(sect_x + 5, 12, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        set_tile(sect_x + 10, 12, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
        /* Wall-mounted screens tracking combat */
        set_tile(sect_x + 1, 8, NTILE_SCREEN, TILE_EMPTY);
        set_tile(sect_x + 14, 8, NTILE_SCREEN, TILE_EMPTY);
        set_tile(sect_x + 1, 16, NTILE_SCREEN, TILE_EMPTY);
        set_tile(sect_x + 14, 16, NTILE_SCREEN, TILE_EMPTY);
    } else {
        /* Killbox corridor — narrow with obstacles */
        /* Low ceiling in center */
        fill_solid(sect_x + 4, 3, 8, 4);
        for (int x = sect_x + 4; x < sect_x + 12; x++)
            set_tile(x, 7, NTILE_WALL_CAP_BOT, TILE_SOLID);
        /* Platforms at different heights */
        add_platform(sect_x + 2, 20, 3);
        add_platform(sect_x + 7, 16, 3);
        add_platform(sect_x + 11, 22, 3);
        /* Breakable cover blocks */
        set_tile(sect_x + 5, 27, NTILE_BREAKABLE, TILE_SOLID);
        set_tile(sect_x + 10, 27, NTILE_BREAKABLE, TILE_SOLID);
        /* Wall greeble */
        set_tile(sect_x + 1, 10, NTILE_CORRIDOR_GREEBLE, TILE_EMPTY);
        set_tile(sect_x + 14, 14, NTILE_CORRIDOR_GREEBLE, TILE_EMPTY);
    }

    /* Dense enemy spawns — up to 5 */
    int num = 3 + tier / 2;
    if (num > 5) num = 5;
    for (int i = 0; i < num; i++) {
        int sx = sect_x + 2 + (i * 3) % 12;
        int sy = (i < 3) ? 27 : 17;
        add_spawn_point(sx, sy);
    }

    /* Hazards scale with tier */
    if (tier > 2) {
        add_hazard(sect_x + 5, 27);
        add_hazard(sect_x + 11, 27);
    }
    if (tier > 4) {
        set_tile(sect_x + 3, 27, NTILE_HAZARD_TESLA, TILE_HAZARD);
    }

    /* Common decoration */
    set_tile(sect_x + 1, 12, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 14, 12, NTILE_SCREEN, TILE_EMPTY);
    set_tile(sect_x + 7, 3, NTILE_NEON_SIGN, TILE_EMPTY);
    set_tile(sect_x + 8, 3, NTILE_NEON_SIGN, TILE_EMPTY);

    /* Wall conduits */
    set_tile(sect_x + 1, 20, NTILE_CONDUIT, TILE_EMPTY);
    set_tile(sect_x + 14, 20, NTILE_CONDUIT, TILE_EMPTY);
    set_tile(sect_x + 1, 24, NTILE_JUNCTION_BOX, TILE_EMPTY);
    set_tile(sect_x + 14, 24, NTILE_JUNCTION_BOX, TILE_EMPTY);

    /* Floor details */
    set_tile(sect_x + 4, 28, NTILE_FLOOR_GRATE, TILE_SOLID);
    set_tile(sect_x + 8, 28, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
    set_tile(sect_x + 12, 28, NTILE_FLOOR_GRATE, TILE_SOLID);

    /* Data streams on walls */
    set_tile(sect_x + 1, 6, NTILE_DATA_STREAM, TILE_EMPTY);
    set_tile(sect_x + 14, 6, NTILE_DATA_STREAM, TILE_EMPTY);
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
    for (int s = 0; s < NUM_SECTIONS; s++) {
        if (s == NUM_SECTIONS - 1 && is_boss) {
            level_data.sections[s] = SECT_BOSS;
        } else if (s == 0) {
            level_data.sections[s] = SECT_FLAT; /* Start easy */
        } else {
            /* Weighted random section type, reroll on repeat */
            int chosen;
            int attempts = 0;
            do {
                int r = lrng_range(140);
                if (r < 12) {
                    chosen = SECT_FLAT;
                } else if (r < 24) {
                    chosen = SECT_PLATFORMS;
                } else if (r < 34) {
                    chosen = SECT_VERTICAL;
                } else if (r < 48) {
                    chosen = SECT_ARENA;
                } else if (r < 58) {
                    chosen = SECT_CORRIDOR;
                } else if (r < 70) {
                    chosen = SECT_DESCENT;
                } else if (r < 82) {
                    chosen = SECT_MAZE;
                } else if (r < 92) {
                    chosen = SECT_WATERFALL;
                } else if (r < 102) {
                    chosen = SECT_TRANSIT;
                } else if (r < 112) {
                    chosen = SECT_SECURITY;
                } else if (r < 120) {
                    chosen = SECT_CACHE;
                } else if (r < 130) {
                    chosen = SECT_NETWORK;
                } else {
                    chosen = SECT_GAUNTLET;
                }
                attempts++;
            } while (chosen == prev_sect && attempts < 3);
            level_data.sections[s] = (u8)chosen;
        }
        prev_sect = level_data.sections[s];
    }

    /* Generate each section */
    for (int s = 0; s < NUM_SECTIONS; s++) {
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
        case SECT_WATERFALL: gen_waterfall(sect_x, tier); break;
        case SECT_TRANSIT:   gen_transit(sect_x, tier); break;
        case SECT_SECURITY:  gen_security(sect_x, tier); break;
        case SECT_CACHE:     gen_cache(sect_x, tier); break;
        case SECT_NETWORK:   gen_network(sect_x, tier); break;
        case SECT_GAUNTLET:  gen_gauntlet(sect_x, tier); break;
        }
    }

    /* PASS 1: Fill solid below each column's lowest floor to prevent void.
     * Must run BEFORE stitch pass so that stitch can detect floor surfaces
     * correctly and carve through the filled area. */
    for (int x = 0; x < NET_MAP_W; x++) {
        /* Scan top-down for the LOWEST floor surface (solid with empty above) */
        int lowest_floor = -1;
        for (int y = 3; y < NET_MAP_H; y++) {
            int col = level_data.collision[y * NET_MAP_W + x];
            if (col == TILE_SOLID || col == TILE_PLATFORM) {
                int above = (y > 0) ? level_data.collision[(y - 1) * NET_MAP_W + x] : TILE_SOLID;
                if (above != TILE_SOLID && above != TILE_PLATFORM) {
                    lowest_floor = y; /* Keep scanning — we want the LOWEST */
                }
            }
        }
        if (lowest_floor >= 0) {
            for (int fy = lowest_floor + 1; fy < NET_MAP_H; fy++) {
                if (level_data.collision[fy * NET_MAP_W + x] == TILE_EMPTY) {
                    set_tile(x, fy, NTILE_WALL, TILE_SOLID);
                }
            }
        }
    }

    /* PASS 2: Smooth floor seams at section boundaries (cosmetic) */
    for (int s = 1; s < NUM_SECTIONS; s++) {
        int bx = s * 16;
        for (int by = 0; by < NET_MAP_H; by++) {
            int left_tile = level_data.tiles[by * NET_MAP_W + (bx - 1)];
            int right_tile = level_data.tiles[by * NET_MAP_W + bx];
            int left_col = level_data.collision[by * NET_MAP_W + (bx - 1)];
            int right_col = level_data.collision[by * NET_MAP_W + bx];
            if (left_col == TILE_SOLID && right_col == TILE_SOLID) {
                if (left_tile == NTILE_FLOOR_EDGE_R)
                    set_tile(bx - 1, by, NTILE_FLOOR, TILE_SOLID);
                if (right_tile == NTILE_FLOOR_EDGE_L)
                    set_tile(bx, by, NTILE_FLOOR, TILE_SOLID);
            }
        }
    }

    /* PASS 3: Section boundary stitch — ensure connectivity between sections.
     * Finds floor surfaces on each side of the boundary. The floor surface is
     * the topmost solid tile that has non-solid above it (walking surface).
     * After fill-below ran, columns below floors are all solid, so we scan
     * top-down to find the actual walking surface. */
    for (int s = 1; s < NUM_SECTIONS; s++) {
        int bx = s * 16;

        /* Find floor surface: topmost solid tile with empty above (walking level) */
        int left_floor = -1, right_floor = -1;
        for (int by = 3; by < NET_MAP_H - 1; by++) {
            if (left_floor < 0 &&
                level_data.collision[by * NET_MAP_W + (bx - 1)] == TILE_SOLID &&
                level_data.collision[(by - 1) * NET_MAP_W + (bx - 1)] != TILE_SOLID) {
                /* Skip ceiling tiles (solid with solid above AND below for >2 rows) */
                int below = (by + 1 < NET_MAP_H) ? level_data.collision[(by + 1) * NET_MAP_W + (bx - 1)] : TILE_SOLID;
                if (below == TILE_SOLID) {
                    /* This could be ceiling-bottom or actual floor; check if there's
                     * significant empty space above (at least 3 tiles for player) */
                    int space = 0;
                    for (int cy = by - 1; cy >= 0 && level_data.collision[cy * NET_MAP_W + (bx - 1)] != TILE_SOLID; cy--)
                        space++;
                    if (space >= 3) left_floor = by;
                }
            }
            if (right_floor < 0 &&
                level_data.collision[by * NET_MAP_W + bx] == TILE_SOLID &&
                level_data.collision[(by - 1) * NET_MAP_W + bx] != TILE_SOLID) {
                int below = (by + 1 < NET_MAP_H) ? level_data.collision[(by + 1) * NET_MAP_W + bx] : TILE_SOLID;
                if (below == TILE_SOLID) {
                    int space = 0;
                    for (int cy = by - 1; cy >= 0 && level_data.collision[cy * NET_MAP_W + bx] != TILE_SOLID; cy--)
                        space++;
                    if (space >= 3) right_floor = by;
                }
            }
        }
        /* Fallback: scan from bottom for any solid (unfilled sections) */
        if (left_floor < 0) {
            for (int by = NET_MAP_H - 2; by >= 3; by--) {
                if (level_data.collision[by * NET_MAP_W + (bx - 1)] == TILE_SOLID &&
                    (by == 0 || level_data.collision[(by - 1) * NET_MAP_W + (bx - 1)] != TILE_SOLID)) {
                    left_floor = by; break;
                }
            }
        }
        if (right_floor < 0) {
            for (int by = NET_MAP_H - 2; by >= 3; by--) {
                if (level_data.collision[by * NET_MAP_W + bx] == TILE_SOLID &&
                    (by == 0 || level_data.collision[(by - 1) * NET_MAP_W + bx] != TILE_SOLID)) {
                    right_floor = by; break;
                }
            }
        }
        if (left_floor < 0) left_floor = 28;
        if (right_floor < 0) right_floor = 28;

        /* Use the lower floor (higher y) as connecting level */
        int floor_y = GP_MAX(left_floor, right_floor);
        int high_floor = GP_MIN(left_floor, right_floor);

        /* Carve passage: 4 tiles above higher floor down to connecting floor.
         * Wide enough (6 cols) and tall enough for player (14px = ~2 tiles). */
        int carve_top = high_floor - 4;
        if (carve_top < 1) carve_top = 1;
        for (int cy = carve_top; cy < floor_y; cy++) {
            for (int fx = bx - 3; fx <= bx + 2; fx++) {
                if (fx >= 0 && fx < NET_MAP_W) {
                    int col = level_data.collision[cy * NET_MAP_W + fx];
                    if (col == TILE_SOLID) {
                        set_tile(fx, cy, NTILE_EMPTY, TILE_EMPTY);
                    }
                }
            }
        }

        /* Ensure connecting floor exists (6 tiles wide) */
        if (floor_y < NET_MAP_H) {
            for (int fx = bx - 3; fx <= bx + 2; fx++) {
                if (fx >= 0 && fx < NET_MAP_W &&
                    level_data.collision[floor_y * NET_MAP_W + fx] != TILE_SOLID) {
                    set_tile(fx, floor_y, NTILE_FLOOR, TILE_SOLID);
                }
            }
        }

        /* Stepping platforms for large floor height differences */
        int diff = floor_y - high_floor;
        if (diff > 3) {
            int y = high_floor;
            while (y + 3 < floor_y) {
                y += 3;
                add_platform(bx - 3, y, 6);
            }
        }
    }

    /* PASS 4: Validate spawn has floor beneath */
    if (level_data.collision[27 * NET_MAP_W + 4] != TILE_SOLID &&
        level_data.collision[28 * NET_MAP_W + 4] != TILE_SOLID) {
        for (int fx = 2; fx <= 8; fx++) {
            set_tile(fx, 28, NTILE_FLOOR, TILE_SOLID);
        }
    }

    /* Set spawn position — find the actual floor in section 0 */
    {
        int spawn_floor = 28;
        for (int sy = 5; sy < NET_MAP_H - 1; sy++) {
            if (level_data.collision[sy * NET_MAP_W + 4] == TILE_SOLID &&
                level_data.collision[(sy - 1) * NET_MAP_W + 4] != TILE_SOLID) {
                spawn_floor = sy;
                break;
            }
        }
        level_data.spawn_x = 4;
        level_data.spawn_y = (u8)(spawn_floor - 1);
    }

    /* PASS 5: Exit at far right — find the floor in the last section */
    {
        int exit_floor = -1;
        int exit_col = NET_MAP_W - 4;
        for (int ey = 5; ey < NET_MAP_H - 1; ey++) {
            if (level_data.collision[ey * NET_MAP_W + exit_col] == TILE_SOLID &&
                level_data.collision[(ey - 1) * NET_MAP_W + exit_col] != TILE_SOLID) {
                exit_floor = ey;
                break;
            }
        }
        if (exit_floor < 0) exit_floor = 28;
        level_data.exit_x = (u8)exit_col;
        level_data.exit_y = (u8)(exit_floor - 1);
        /* Ensure floor exists under exit */
        for (int fx = exit_col - 1; fx <= exit_col + 1; fx++) {
            if (fx >= 0 && fx < NET_MAP_W &&
                level_data.collision[exit_floor * NET_MAP_W + fx] != TILE_SOLID) {
                set_tile(fx, exit_floor, NTILE_FLOOR, TILE_SOLID);
            }
        }
        /* Clear space for gate (3 tiles tall) */
        for (int gy = exit_floor - 3; gy < exit_floor; gy++) {
            if (gy >= 0 && level_data.collision[gy * NET_MAP_W + exit_col] == TILE_SOLID) {
                set_tile(exit_col, gy, NTILE_EMPTY, TILE_EMPTY);
            }
        }
    }
    set_tile(level_data.exit_x, level_data.exit_y, NTILE_EXIT_GATE, TILE_EMPTY);
    set_tile(level_data.exit_x, level_data.exit_y - 1, NTILE_EXIT_GATE, TILE_EMPTY);
    set_tile(level_data.exit_x - 1, level_data.exit_y, NTILE_EXIT_FRAME, TILE_EMPTY);
    set_tile(level_data.exit_x - 1, level_data.exit_y - 1, NTILE_EXIT_FRAME, TILE_EMPTY);
    set_tile(level_data.exit_x + 1, level_data.exit_y, NTILE_EXIT_FRAME, TILE_EMPTY);
    set_tile(level_data.exit_x + 1, level_data.exit_y - 1, NTILE_EXIT_FRAME, TILE_EMPTY);

    /* PASS 6: Per-act environmental storytelling — scatter act-themed narrative
     * vignettes across the level. Only modifies visual tiles in EMPTY collision
     * spaces, so no gameplay impact. Each act has a distinct visual personality. */
    {
        int act = (int)quest_state.current_act;
        /* Place 2-3 vignettes per section (skip section 0 = spawn area) */
        for (int s = 1; s < NUM_SECTIONS; s++) {
            int sx = s * 16;

            switch (act) {
            case 0: /* Freelance / Act 1 "The Glitch" — system instability */
                /* Corrupted data streams: vertical glitch columns */
                {
                    int gx = sx + 3 + lrng_range(10);
                    for (int gy = 4; gy < 20; gy += 2 + lrng_range(3)) {
                        if (level_data.collision[gy * NET_MAP_W + gx] == TILE_EMPTY)
                            set_tile(gx, gy, NTILE_GLITCH, TILE_EMPTY);
                    }
                }
                /* Error screens — flickering wall monitors */
                for (int v = 0; v < 2; v++) {
                    int vx = sx + 1 + lrng_range(14);
                    int vy = 3 + lrng_range(6);
                    if (level_data.collision[vy * NET_MAP_W + vx] == TILE_EMPTY) {
                        set_tile(vx, vy, NTILE_SCREEN, TILE_EMPTY);
                        /* Broken panel below screen = corrupted terminal */
                        if (vy + 1 < NET_MAP_H && level_data.collision[(vy+1) * NET_MAP_W + vx] == TILE_EMPTY)
                            set_tile(vx, vy + 1, NTILE_BROKEN_PANEL, TILE_EMPTY);
                    }
                }
                /* Scattered circuit debris on floors */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 10; fy < NET_MAP_H; fy++) {
                        if (level_data.tiles[fy * NET_MAP_W + fx] == NTILE_FLOOR && lrng_range(6) == 0)
                            set_tile(fx, fy, NTILE_FLOOR_CRACKED, TILE_SOLID);
                    }
                }
                break;

            case 1: /* Act 2 "Traceback" — corporate surveillance, methodical */
                /* Surveillance camera vignette: junction box + cable network */
                {
                    int cx = sx + 2 + lrng_range(12);
                    int cy = 3;
                    if (level_data.collision[cy * NET_MAP_W + cx] == TILE_EMPTY) {
                        set_tile(cx, cy, NTILE_JUNCTION_BOX, TILE_EMPTY);
                        /* Cables radiating from junction box */
                        if (cx > 0 && level_data.collision[cy * NET_MAP_W + cx - 1] == TILE_EMPTY)
                            set_tile(cx - 1, cy, NTILE_CABLE_H, TILE_EMPTY);
                        if (cx + 1 < NET_MAP_W && level_data.collision[cy * NET_MAP_W + cx + 1] == TILE_EMPTY)
                            set_tile(cx + 1, cy, NTILE_CABLE_H, TILE_EMPTY);
                        if (cy + 1 < NET_MAP_H && level_data.collision[(cy+1) * NET_MAP_W + cx] == TILE_EMPTY)
                            set_tile(cx, cy + 1, NTILE_CABLE_V, TILE_EMPTY);
                    }
                }
                /* Server rack workstations — paired server + console */
                {
                    int wx = sx + 1 + lrng_range(6);
                    int wy = 4 + lrng_range(4);
                    if (level_data.collision[wy * NET_MAP_W + wx] == TILE_EMPTY &&
                        level_data.collision[(wy+1) * NET_MAP_W + wx] == TILE_EMPTY) {
                        set_tile(wx, wy, NTILE_SERVER_TOP, TILE_EMPTY);
                        set_tile(wx, wy + 1, NTILE_SERVER_BOT, TILE_EMPTY);
                        /* Console next to rack = analyst station */
                        if (wx + 1 < NET_MAP_W && level_data.collision[(wy+1) * NET_MAP_W + wx + 1] == TILE_EMPTY)
                            set_tile(wx + 1, wy + 1, NTILE_CONSOLE, TILE_EMPTY);
                    }
                }
                /* Data highway: horizontal cable runs across ceiling */
                for (int hx = sx; hx < sx + 16; hx++) {
                    if (level_data.collision[3 * NET_MAP_W + hx] == TILE_EMPTY && lrng_range(3) == 0)
                        set_tile(hx, 3, NTILE_CABLE_H, TILE_EMPTY);
                }
                /* Clean circuit floors — organized infrastructure */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 10; fy < NET_MAP_H; fy++) {
                        if (level_data.tiles[fy * NET_MAP_W + fx] == NTILE_FLOOR && lrng_range(5) == 0)
                            set_tile(fx, fy, NTILE_FLOOR_CIRCUIT, TILE_SOLID);
                    }
                }
                break;

            case 2: /* Act 3 "Deep Packet" — organic-digital corruption */
                /* Bio-growth: data streams that look like organic tendrils */
                for (int t = 0; t < 3; t++) {
                    int tx = sx + 1 + lrng_range(14);
                    int start_y = 4 + lrng_range(8);
                    for (int ty = start_y; ty < start_y + 4 + lrng_range(6); ty++) {
                        if (ty >= NET_MAP_H) break;
                        if (level_data.collision[ty * NET_MAP_W + tx] == TILE_EMPTY) {
                            set_tile(tx, ty, (lrng_range(3) == 0) ? NTILE_DATA_STREAM : NTILE_DATA_WATERFALL, TILE_EMPTY);
                        }
                        /* Organic branching: sometimes shift sideways */
                        if (lrng_range(3) == 0) tx += (lrng_range(2) == 0) ? 1 : -1;
                        if (tx < sx || tx >= sx + 16) break;
                    }
                }
                /* Corruption nodes: circuit nodes surrounded by glitch */
                {
                    int nx = sx + 3 + lrng_range(10);
                    int ny = 6 + lrng_range(10);
                    if (level_data.collision[ny * NET_MAP_W + nx] == TILE_EMPTY) {
                        set_tile(nx, ny, NTILE_CIRCUIT_NODE, TILE_EMPTY);
                        /* Glitch artifacts radiating from node */
                        for (int d = 0; d < 4; d++) {
                            int dx2 = (d == 0) ? -1 : (d == 1) ? 1 : 0;
                            int dy2 = (d == 2) ? -1 : (d == 3) ? 1 : 0;
                            int gx = nx + dx2;
                            int gy = ny + dy2;
                            if (gx >= 0 && gx < NET_MAP_W && gy >= 0 && gy < NET_MAP_H &&
                                level_data.collision[gy * NET_MAP_W + gx] == TILE_EMPTY)
                                set_tile(gx, gy, NTILE_GLITCH, TILE_EMPTY);
                        }
                    }
                }
                /* Cracked floors: corruption seeping through */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 10; fy < NET_MAP_H; fy++) {
                        if (level_data.tiles[fy * NET_MAP_W + fx] == NTILE_FLOOR && lrng_range(4) == 0)
                            set_tile(fx, fy, NTILE_FLOOR_CRACKED, TILE_SOLID);
                    }
                }
                break;

            case 3: /* Act 4 "Zero Day" — destruction, battle damage */
                /* Breached infrastructure: broken panels everywhere */
                for (int b = 0; b < 4; b++) {
                    int bx2 = sx + 1 + lrng_range(14);
                    int by2 = 4 + lrng_range(16);
                    if (level_data.collision[by2 * NET_MAP_W + bx2] == TILE_EMPTY)
                        set_tile(bx2, by2, NTILE_BROKEN_PANEL, TILE_EMPTY);
                }
                /* Severed conduit: conduit with sparking cables */
                {
                    int cx = sx + 3 + lrng_range(10);
                    int cy = 5 + lrng_range(8);
                    if (level_data.collision[cy * NET_MAP_W + cx] == TILE_EMPTY) {
                        set_tile(cx, cy, NTILE_CONDUIT, TILE_EMPTY);
                        /* Severed cable dangling below */
                        for (int d = 1; d <= 2 + lrng_range(3); d++) {
                            if (cy + d < NET_MAP_H && level_data.collision[(cy+d) * NET_MAP_W + cx] == TILE_EMPTY)
                                set_tile(cx, cy + d, NTILE_CABLE_V, TILE_EMPTY);
                        }
                    }
                }
                /* Destroyed server racks — tops only (bottoms blown off) */
                {
                    int rx = sx + 2 + lrng_range(12);
                    int ry = 4 + lrng_range(6);
                    if (level_data.collision[ry * NET_MAP_W + rx] == TILE_EMPTY) {
                        set_tile(rx, ry, NTILE_SERVER_TOP, TILE_EMPTY);
                        if (ry + 1 < NET_MAP_H && level_data.collision[(ry+1) * NET_MAP_W + rx] == TILE_EMPTY)
                            set_tile(rx, ry + 1, NTILE_BROKEN_PANEL, TILE_EMPTY);
                    }
                }
                /* Heavy damage to floors */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 10; fy < NET_MAP_H; fy++) {
                        if (level_data.tiles[fy * NET_MAP_W + fx] == NTILE_FLOOR && lrng_range(3) == 0)
                            set_tile(fx, fy, NTILE_FLOOR_CRACKED, TILE_SOLID);
                    }
                }
                /* Exposed grates where panels were */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 10; fy < NET_MAP_H; fy++) {
                        if (level_data.tiles[fy * NET_MAP_W + fx] == NTILE_FLOOR && lrng_range(8) == 0)
                            set_tile(fx, fy, NTILE_FLOOR_GRATE, TILE_SOLID);
                    }
                }
                break;

            case 4: /* Act 5 "Ghost Protocol" — minimal, abandoned, eerie */
                /* Sparse lone screens — as if someone left in a hurry */
                if (lrng_range(3) == 0) {
                    int mx = sx + 3 + lrng_range(10);
                    int my = 4 + lrng_range(8);
                    if (level_data.collision[my * NET_MAP_W + mx] == TILE_EMPTY)
                        set_tile(mx, my, NTILE_SCREEN, TILE_EMPTY);
                }
                /* Fading data streams — single isolated wisps */
                if (lrng_range(2) == 0) {
                    int dx2 = sx + 2 + lrng_range(12);
                    int dy2 = 6 + lrng_range(10);
                    if (level_data.collision[dy2 * NET_MAP_W + dx2] == TILE_EMPTY)
                        set_tile(dx2, dy2, NTILE_DATA_STREAM, TILE_EMPTY);
                }
                /* Clean, empty corridors — remove some existing decoration for sparse feel */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 3; fy < NET_MAP_H; fy++) {
                        int tile = level_data.tiles[fy * NET_MAP_W + fx];
                        /* Thin out dense decoration for the abandoned feel */
                        if ((tile == NTILE_CORRIDOR_GREEBLE || tile == NTILE_MEMORY_BANK ||
                             tile == NTILE_JUNCTION_BOX) && lrng_range(3) == 0) {
                            if (level_data.collision[fy * NET_MAP_W + fx] == TILE_EMPTY)
                                set_tile(fx, fy, NTILE_EMPTY, TILE_EMPTY);
                        }
                    }
                }
                break;

            case 5: /* Act 6 "Trace Route" — chaotic hybrid, everything breaking */
                /* Chaotic mix: glitch artifacts, broken panels, data streams */
                for (int c = 0; c < 5; c++) {
                    int cx = sx + lrng_range(16);
                    int cy = 3 + lrng_range(20);
                    if (cy < NET_MAP_H && level_data.collision[cy * NET_MAP_W + cx] == TILE_EMPTY) {
                        int r = lrng_range(6);
                        int tile = (r == 0) ? NTILE_GLITCH : (r == 1) ? NTILE_BROKEN_PANEL :
                                   (r == 2) ? NTILE_DATA_STREAM : (r == 3) ? NTILE_DATA_WATERFALL :
                                   (r == 4) ? NTILE_CIRCUIT_NODE : NTILE_CONDUIT;
                        set_tile(cx, cy, tile, TILE_EMPTY);
                    }
                }
                /* Fragmented cables: short random runs */
                for (int c = 0; c < 3; c++) {
                    int cx = sx + 1 + lrng_range(14);
                    int cy = 3 + lrng_range(10);
                    int len = 1 + lrng_range(3);
                    int horiz = lrng_range(2);
                    for (int k = 0; k < len; k++) {
                        int kx = horiz ? cx + k : cx;
                        int ky = horiz ? cy : cy + k;
                        if (kx >= NET_MAP_W || ky >= NET_MAP_H) break;
                        if (level_data.collision[ky * NET_MAP_W + kx] == TILE_EMPTY)
                            set_tile(kx, ky, horiz ? NTILE_CABLE_H : NTILE_CABLE_V, TILE_EMPTY);
                    }
                }
                /* Heavy floor damage: cracked + grate mix */
                for (int fx = sx; fx < sx + 16; fx++) {
                    for (int fy = 10; fy < NET_MAP_H; fy++) {
                        if (level_data.tiles[fy * NET_MAP_W + fx] == NTILE_FLOOR) {
                            int r = lrng_range(6);
                            if (r == 0) set_tile(fx, fy, NTILE_FLOOR_CRACKED, TILE_SOLID);
                            else if (r == 1) set_tile(fx, fy, NTILE_FLOOR_GRATE, TILE_SOLID);
                        }
                    }
                }
                break;
            }

            /* Universal story element: neon signs near section entrances (all acts) */
            if (lrng_range(4) == 0) {
                int ny = 4 + lrng_range(6);
                if (level_data.collision[ny * NET_MAP_W + sx] == TILE_EMPTY)
                    set_tile(sx, ny, NTILE_NEON_SIGN, TILE_EMPTY);
            }
        }
    }

    /* PASS 7: Wall surface detail — replace ~10% of plain NTILE_WALL with
     * visual variants (panels, circuits) for less repetitive solid fills.
     * Only touches visual tiles, collision stays TILE_SOLID. */
    for (int x = 0; x < NET_MAP_W; x++) {
        for (int y = 2; y < NET_MAP_H; y++) {
            if (level_data.tiles[y * NET_MAP_W + x] != NTILE_WALL) continue;
            if (level_data.collision[y * NET_MAP_W + x] != TILE_SOLID) continue;
            /* Only decorate walls with empty space above (wall surfaces, not deep fill) */
            if (y > 0 && level_data.collision[(y-1) * NET_MAP_W + x] == TILE_SOLID &&
                level_data.tiles[(y-1) * NET_MAP_W + x] == NTILE_WALL) {
                /* Deep fill — sparse decoration */
                if (lrng_range(12) == 0) {
                    level_data.tiles[y * NET_MAP_W + x] = NTILE_WALL_PANEL;
                }
            } else {
                /* Surface wall — richer decoration */
                int r = lrng_range(10);
                if (r == 0) level_data.tiles[y * NET_MAP_W + x] = NTILE_WALL_PANEL;
                else if (r == 1) level_data.tiles[y * NET_MAP_W + x] = NTILE_WALL_CIRCUIT;
                else if (r == 2) level_data.tiles[y * NET_MAP_W + x] = NTILE_WINDOW;
            }
        }
    }

    /* Validate spawn points: ensure each has ground below and isn't inside a wall */
    for (int i = 0; i < (int)level_data.num_spawns; i++) {
        int tx = level_data.spawn_points[i][0];
        int ty = level_data.spawn_points[i][1];

        /* Reject spawns inside solid tiles (e.g., inside sealed rooms or walls) */
        if (tx >= 0 && tx < NET_MAP_W && ty >= 0 && ty < NET_MAP_H &&
            level_data.collision[ty * NET_MAP_W + tx] == TILE_SOLID) {
            /* Try shifting up until we find non-solid */
            int shifted = 0;
            for (int uy = ty - 1; uy >= 2; uy--) {
                if (level_data.collision[uy * NET_MAP_W + tx] != TILE_SOLID) {
                    level_data.spawn_points[i][1] = (u8)uy;
                    ty = uy;
                    shifted = 1;
                    break;
                }
            }
            if (!shifted) {
                /* Completely buried — remove */
                level_data.num_spawns--;
                level_data.spawn_points[i][0] = level_data.spawn_points[level_data.num_spawns][0];
                level_data.spawn_points[i][1] = level_data.spawn_points[level_data.num_spawns][1];
                i--;
                continue;
            }
        }

        /* Ensure ground within 3 tiles below */
        int found = 0;
        for (int dy = 1; dy <= 3; dy++) {
            int cy = ty + dy;
            if (cy >= NET_MAP_H) break;
            int col = level_data.collision[cy * NET_MAP_W + tx];
            if (col == TILE_SOLID || col == TILE_PLATFORM) {
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

void levelgen_set_collision(int tx, int ty, int col_type) {
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return;
    level_data.collision[ty * NET_MAP_W + tx] = (u8)col_type;
    /* Also update visual tile to empty for breakable walls */
    if (col_type == TILE_EMPTY) {
        level_data.tiles[ty * NET_MAP_W + tx] = NTILE_EMPTY;
    }
}
