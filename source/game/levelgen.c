/*
 * Ghost Protocol — Open-World Level Generator
 *
 * Generates 256x32 tile explorable levels from seed + tier.
 * Scatters terrain features across an open playfield.
 * Uses seeded RNG for deterministic generation.
 */
#include "game/levelgen.h"
#include "game/networld.h"
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

/* ---- Tile helpers ---- */

static void set_tile(int tx, int ty, int visual, int col) {
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return;
    int idx = ty * NET_MAP_W + tx;
    level_data.tiles[idx] = (u8)visual;
    level_data.collision[idx] = (u8)col;
}

/* Visual-only tile — changes appearance without touching collision. */
static void set_visual_tile(int tx, int ty, int visual) {
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return;
    level_data.tiles[ty * NET_MAP_W + tx] = (u8)visual;
}

static void fill_solid(int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h && y < NET_MAP_H; y++) {
        for (int x = x0; x < x0 + w && x < NET_MAP_W; x++) {
            set_tile(x, y, NTILE_WALL, TILE_SOLID);
        }
    }
}

static void add_platform(int x, int y, int w) {
    for (int i = 0; i < w && x + i < NET_MAP_W; i++) {
        set_tile(x + i, y, NTILE_PLATFORM, TILE_PLATFORM);
    }
}

static void add_spawn_point(int tx, int ty) {
    if (level_data.num_spawns >= 48) return;
    if (tx < 0 || tx >= NET_MAP_W || ty < 0 || ty >= NET_MAP_H) return;
    level_data.spawn_points[level_data.num_spawns][0] = (u8)tx;
    level_data.spawn_points[level_data.num_spawns][1] = (u8)ty;
    level_data.num_spawns++;
}

/* ---- Open-world terrain feature generators ---- */

/* Feature type enum (internal) */
enum {
    FEAT_PILLAR = 0,
    FEAT_PLATFORMS,
    FEAT_PIT,
    FEAT_OVERHANG,
    FEAT_TOWER,
    FEAT_HAZARD_ZONE,
    FEAT_BREAKABLE_WALL,
    FEAT_COUNT
};

static void gen_feat_pillar(int x, int tier) {
    int w = 2 + lrng_range(2);          /* 2-3 wide */
    int h = 8 + lrng_range(8) + tier;   /* 8-16+ tall */
    if (h > 26) h = 26;
    int top_y = 30 - h;
    if (top_y < 3) top_y = 3;
    fill_solid(x, top_y, w, 30 - top_y);
    /* Cap top */
    for (int cx = x; cx < x + w && cx < NET_MAP_W - 1; cx++)
        set_tile(cx, top_y, NTILE_WALL_CAP_TOP, TILE_SOLID);
    /* Optional platform on top */
    if (lrng_range(3) == 0)
        add_platform(x - 1, top_y - 1, w + 2);
}

static void gen_feat_platforms(int x, int tier) {
    int num_plats = 2 + lrng_range(2) + (tier > 2 ? 1 : 0);
    for (int i = 0; i < num_plats; i++) {
        int pw = 3 + lrng_range(3);
        int px = x + lrng_range(6) - 2;
        int py = 8 + i * 5 + lrng_range(3);
        if (py > 27) py = 27;
        if (px < 1) px = 1;
        if (px + pw >= NET_MAP_W - 1) pw = NET_MAP_W - 2 - px;
        add_platform(px, py, pw);
        if (px >= 1)
            set_tile(px, py, NTILE_PLAT_EDGE_L, TILE_PLATFORM);
        if (px + pw - 1 < NET_MAP_W - 1)
            set_tile(px + pw - 1, py, NTILE_PLAT_EDGE_R, TILE_PLATFORM);
    }
}

static void gen_feat_pit(int x, int tier) {
    int w = 4 + lrng_range(4);
    if (x + w >= NET_MAP_W - 2) w = NET_MAP_W - 3 - x;
    if (w < 2) return;
    for (int cx = x; cx < x + w && cx < NET_MAP_W - 1; cx++)
        set_tile(cx, 30, NTILE_EMPTY, TILE_EMPTY);
    int hazard_type = (tier >= 3 && lrng_range(2)) ? TILE_TESLA : TILE_HAZARD;
    int hazard_ntile = (hazard_type == TILE_TESLA) ? NTILE_HAZARD_TESLA : NTILE_HAZARD_SPIKE;
    for (int cx = x; cx < x + w && cx < NET_MAP_W - 1; cx++)
        set_tile(cx, 31, hazard_ntile, hazard_type);
    if (lrng_range(3) > 0)
        add_platform(x + 1, 30, GP_MIN(w - 2, 4));
    if (x > 1)
        set_tile(x - 1, 30, NTILE_FLOOR_EDGE_R, TILE_SOLID);
    if (x + w < NET_MAP_W - 1)
        set_tile(x + w, 30, NTILE_FLOOR_EDGE_L, TILE_SOLID);
}

static void gen_feat_overhang(int x, int tier) {
    int w = 6 + lrng_range(4);
    int drop = 5 + lrng_range(4) + tier;
    if (drop > 20) drop = 20;
    if (x + w >= NET_MAP_W - 1) w = NET_MAP_W - 2 - x;
    if (w < 3) return;
    for (int cy = 2; cy < 2 + drop; cy++) {
        for (int cx = x; cx < x + w && cx < NET_MAP_W - 1; cx++)
            set_tile(cx, cy, NTILE_WALL_PANEL, TILE_SOLID);
    }
    for (int cx = x; cx < x + w && cx < NET_MAP_W - 1; cx++)
        set_tile(cx, 2 + drop, NTILE_WALL_CAP_BOT, TILE_SOLID);
}

static void gen_feat_tower(int x, int tier) {
    int w = 4;
    int h = 12 + lrng_range(6) + tier * 2;
    if (h > 26) h = 26;
    int top_y = 30 - h;
    if (top_y < 3) top_y = 3;
    /* Outer walls */
    for (int cy = top_y; cy < 30; cy++) {
        set_tile(x, cy, NTILE_WALL, TILE_SOLID);
        if (x + w - 1 < NET_MAP_W)
            set_tile(x + w - 1, cy, NTILE_WALL, TILE_SOLID);
    }
    for (int cx = x; cx < x + w && cx < NET_MAP_W; cx++)
        set_tile(cx, top_y, NTILE_WALL_CAP_TOP, TILE_SOLID);
    /* Internal platforms alternating sides */
    int side = 0;
    for (int py = top_y + 3; py < 29; py += 4) {
        if (side == 0)
            add_platform(x + 1, py, 2);
        else
            add_platform(x + 1, py, 2);
        side = !side;
    }
    /* Entry opening at base */
    set_tile(x, 28, NTILE_EMPTY, TILE_EMPTY);
    set_tile(x, 29, NTILE_EMPTY, TILE_EMPTY);
    /* Top opening */
    if (x + w - 1 < NET_MAP_W) {
        set_tile(x + w - 1, top_y + 1, NTILE_EMPTY, TILE_EMPTY);
        set_tile(x + w - 1, top_y + 2, NTILE_EMPTY, TILE_EMPTY);
    }
    add_spawn_point(x + 1, top_y + 2);
}

static void gen_feat_hazard_zone(int x, int tier) {
    int w = 5 + lrng_range(4);
    if (x + w >= NET_MAP_W - 1) w = NET_MAP_W - 2 - x;
    if (w < 3) return;
    int haz = (tier >= 2 && lrng_range(3) == 0) ? TILE_CORRUPT : TILE_HAZARD;
    int hntile = (haz == TILE_CORRUPT) ? NTILE_DECO_SLUDGE : NTILE_HAZARD_SPIKE;
    for (int cx = x; cx < x + w && cx < NET_MAP_W - 1; cx++)
        set_tile(cx, 30, hntile, haz);
    add_platform(x + 1, 27, GP_MIN(w - 2, 5));
    if (w > 5)
        add_platform(x + 3, 24, 3);
}

static void gen_feat_breakable_wall(int x, int tier) {
    (void)tier;
    int h = 5 + lrng_range(4);
    int top_y = 30 - h;
    if (top_y < 3) top_y = 3;
    for (int cy = top_y; cy < 30; cy++) {
        set_tile(x, cy, NTILE_BREAKABLE, TILE_BREAKABLE);
        set_tile(x + 1, cy, NTILE_BREAKABLE_CRACKED, TILE_BREAKABLE);
    }
}

/* ---- Main generation entry point ---- */

void levelgen_generate(u16 seed, int tier, int is_boss) {
    memset(&level_data, 0, sizeof(level_data));
    level_data.seed = seed;
    level_data.tier = (u8)tier;
    level_data.is_boss_level = (u8)is_boss;

    /* Seed local RNG */
    lrng_state = (u32)seed * 2654435761u;

    /* --- PHASE 1: Borders + ground floor --- */
    /* Ceiling (y=0) and sub-ceiling (y=1) */
    for (int x = 0; x < NET_MAP_W; x++) {
        set_tile(x, 0, NTILE_WALL, TILE_SOLID);
        set_tile(x, 1, NTILE_WALL_CAP_BOT, TILE_SOLID);
    }
    /* Ground floor (y=30) and sub-floor (y=31) */
    for (int x = 0; x < NET_MAP_W; x++) {
        set_tile(x, 30, NTILE_FLOOR, TILE_SOLID);
        set_tile(x, 31, NTILE_WALL, TILE_SOLID);
    }
    /* Left wall (x=0) and right wall (x=255) */
    for (int y = 2; y < 30; y++) {
        set_tile(0, y, NTILE_WALL, TILE_SOLID);
        set_tile(NET_MAP_W - 1, y, NTILE_WALL, TILE_SOLID);
    }

    /* --- PHASE 2: Scatter terrain features --- */
    {
        int num_features = 12 + tier * 2 + lrng_range(5);
        int cursor_x = 8; /* Start after spawn zone buffer */

        for (int f = 0; f < num_features && cursor_x < NET_MAP_W - 20; f++) {
            /* Spacing: tighter further right (denser far zone) */
            int max_gap = (cursor_x < 80) ? 20 : (cursor_x < 180) ? 14 : 10;
            int min_gap = (cursor_x < 80) ? 12 : (cursor_x < 180) ? 8 : 5;
            int gap = min_gap + lrng_range(max_gap - min_gap + 1);
            cursor_x += gap;
            if (cursor_x >= NET_MAP_W - 16) break;

            /* Pick feature type — weight by tier and position */
            int feat_type;
            int r = lrng_range(100);
            if (r < 20)       feat_type = FEAT_PILLAR;
            else if (r < 38)  feat_type = FEAT_PLATFORMS;
            else if (r < 50)  feat_type = FEAT_PIT;
            else if (r < 62)  feat_type = FEAT_OVERHANG;
            else if (r < 75)  feat_type = FEAT_TOWER;
            else if (r < 88)  feat_type = FEAT_HAZARD_ZONE;
            else               feat_type = FEAT_BREAKABLE_WALL;

            /* Higher tiers bias toward hazards and towers */
            if (tier >= 3 && feat_type == FEAT_PILLAR && lrng_range(2))
                feat_type = FEAT_HAZARD_ZONE;

            switch (feat_type) {
            case FEAT_PILLAR:         gen_feat_pillar(cursor_x, tier); break;
            case FEAT_PLATFORMS:      gen_feat_platforms(cursor_x, tier); break;
            case FEAT_PIT:            gen_feat_pit(cursor_x, tier); break;
            case FEAT_OVERHANG:       gen_feat_overhang(cursor_x, tier); break;
            case FEAT_TOWER:          gen_feat_tower(cursor_x, tier); break;
            case FEAT_HAZARD_ZONE:    gen_feat_hazard_zone(cursor_x, tier); break;
            case FEAT_BREAKABLE_WALL: gen_feat_breakable_wall(cursor_x, tier); break;
            }

            /* Add enemy spawn near this feature */
            add_spawn_point(cursor_x + 2, 29);
        }
    }

    /* --- PHASE 3: Place spawn, boss, exit --- */

    /* Player spawn: left side, on ground */
    level_data.spawn_x = 4;
    level_data.spawn_y = 29;

    /* Exit: far right zone */
    {
        int exit_x = 200 + lrng_range(40);
        if (exit_x >= NET_MAP_W - 2) exit_x = NET_MAP_W - 4;
        /* Find floor at this x */
        int exit_y = 29;
        for (int y = 5; y < 30; y++) {
            if (level_data.collision[y * NET_MAP_W + exit_x] == TILE_SOLID &&
                level_data.collision[(y - 1) * NET_MAP_W + exit_x] != TILE_SOLID) {
                exit_y = y - 1;
                break;
            }
        }
        level_data.exit_x = (u8)exit_x;
        level_data.exit_y = (u8)exit_y;
        /* Clear space for gate */
        for (int gy = exit_y - 1; gy <= exit_y; gy++) {
            if (gy >= 0 && level_data.collision[gy * NET_MAP_W + exit_x] == TILE_SOLID)
                set_tile(exit_x, gy, NTILE_EMPTY, TILE_EMPTY);
        }
        set_tile(exit_x, exit_y, NTILE_EXIT_GATE, TILE_EMPTY);
        set_tile(exit_x, exit_y - 1, NTILE_EXIT_GATE, TILE_EMPTY);
        set_tile(exit_x - 1, exit_y, NTILE_EXIT_FRAME, TILE_EMPTY);
        set_tile(exit_x - 1, exit_y - 1, NTILE_EXIT_FRAME, TILE_EMPTY);
        set_tile(exit_x + 1, exit_y, NTILE_EXIT_FRAME, TILE_EMPTY);
        set_tile(exit_x + 1, exit_y - 1, NTILE_EXIT_FRAME, TILE_EMPTY);
    }

    /* Boss position: mid-to-far zone */
    if (is_boss) {
        int boss_x = 130 + lrng_range(80);
        /* Ensure distance from exit (at least 30 tiles) */
        while (boss_x > (int)level_data.exit_x - 30 &&
               boss_x < (int)level_data.exit_x + 30 &&
               boss_x > 130) {
            boss_x -= 10;
        }
        if (boss_x < 130) boss_x = 130;
        /* Find ground at boss position */
        int boss_y = 29;
        for (int y = 5; y < 30; y++) {
            if (level_data.collision[y * NET_MAP_W + boss_x] == TILE_SOLID &&
                level_data.collision[(y - 1) * NET_MAP_W + boss_x] != TILE_SOLID) {
                boss_y = y - 1;
                break;
            }
        }
        /* Clear a small area for the boss (8 tiles wide, 4 tall) */
        for (int cy = boss_y - 3; cy <= boss_y; cy++) {
            for (int cx = boss_x - 3; cx <= boss_x + 4; cx++) {
                if (cx > 0 && cx < NET_MAP_W - 1 && cy >= 2 && cy < 30) {
                    if (level_data.collision[cy * NET_MAP_W + cx] == TILE_SOLID)
                        set_tile(cx, cy, NTILE_EMPTY, TILE_EMPTY);
                }
            }
        }
        /* Ensure floor under boss */
        for (int cx = boss_x - 3; cx <= boss_x + 4; cx++) {
            if (cx > 0 && cx < NET_MAP_W - 1) {
                int floor_y = boss_y + 1;
                if (floor_y < 30 && level_data.collision[floor_y * NET_MAP_W + cx] != TILE_SOLID)
                    set_tile(cx, floor_y, NTILE_FLOOR, TILE_SOLID);
            }
        }
        level_data.boss_tile_x = (u8)boss_x;
        level_data.boss_tile_y = (u8)boss_y;
    } else {
        level_data.boss_tile_x = 0;
        level_data.boss_tile_y = 0;
    }

    /* --- PHASE 4: Decoration --- */
    for (int x = 2; x < NET_MAP_W - 2; x++) {
        for (int y = 3; y < 30; y++) {
            if (level_data.collision[y * NET_MAP_W + x] != TILE_EMPTY) continue;
            int adj_solid = 0;
            if (x > 0 && level_data.collision[y * NET_MAP_W + (x-1)] == TILE_SOLID) adj_solid++;
            if (x < NET_MAP_W-1 && level_data.collision[y * NET_MAP_W + (x+1)] == TILE_SOLID) adj_solid++;
            if (y > 0 && level_data.collision[(y-1) * NET_MAP_W + x] == TILE_SOLID) adj_solid++;
            if (y < NET_MAP_H-1 && level_data.collision[(y+1) * NET_MAP_W + x] == TILE_SOLID) adj_solid++;

            if (adj_solid > 0 && lrng_range(10) < 2) {
                int r = lrng_range(8);
                int deco;
                switch (r) {
                case 0: deco = NTILE_PIPE_H; break;
                case 1: deco = NTILE_PIPE_V; break;
                case 2: deco = NTILE_CABLE_H; break;
                case 3: deco = NTILE_CABLE_V; break;
                case 4: deco = NTILE_VENT; break;
                case 5: deco = NTILE_BROKEN_PANEL; break;
                case 6: deco = NTILE_CIRCUIT_NODE; break;
                default: deco = NTILE_DECO_GRIME; break;
                }
                set_visual_tile(x, y, deco);
            }
        }
    }

    /* Per-act environmental storytelling */
    {
        int act = (int)quest_state.current_act;
        /* Scatter act-themed vignettes across the level (every ~16 tiles) */
        for (int sx = 16; sx < NET_MAP_W - 16; sx += 16) {
            switch (act) {
            case 0: /* Act 1 "The Glitch" — system instability */
                {
                    int gx = sx + 3 + lrng_range(10);
                    for (int gy = 4; gy < 20; gy += 2 + lrng_range(3)) {
                        if (level_data.collision[gy * NET_MAP_W + gx] == TILE_EMPTY)
                            set_visual_tile(gx, gy, NTILE_GLITCH);
                    }
                }
                for (int v = 0; v < 2; v++) {
                    int vx = sx + 1 + lrng_range(14);
                    int vy = 3 + lrng_range(6);
                    if (level_data.collision[vy * NET_MAP_W + vx] == TILE_EMPTY)
                        set_visual_tile(vx, vy, NTILE_SCREEN);
                }
                break;
            case 1: /* Act 2 "Traceback" — surveillance */
                {
                    int cx = sx + 2 + lrng_range(12);
                    int cy = 3;
                    if (level_data.collision[cy * NET_MAP_W + cx] == TILE_EMPTY)
                        set_visual_tile(cx, cy, NTILE_JUNCTION_BOX);
                }
                for (int hx = sx; hx < sx + 16 && hx < NET_MAP_W; hx++) {
                    if (level_data.collision[3 * NET_MAP_W + hx] == TILE_EMPTY && lrng_range(3) == 0)
                        set_visual_tile(hx, 3, NTILE_CABLE_H);
                }
                break;
            case 2: /* Act 3 "Deep Packet" — organic corruption */
                for (int t = 0; t < 3; t++) {
                    int tx = sx + 1 + lrng_range(14);
                    int start_y = 4 + lrng_range(8);
                    for (int ty = start_y; ty < start_y + 4 + lrng_range(6); ty++) {
                        if (ty >= NET_MAP_H) break;
                        if (level_data.collision[ty * NET_MAP_W + tx] == TILE_EMPTY)
                            set_visual_tile(tx, ty, (lrng_range(3) == 0) ? NTILE_DATA_STREAM : NTILE_DATA_WATERFALL);
                        if (lrng_range(3) == 0) tx += (lrng_range(2) == 0) ? 1 : -1;
                        if (tx < sx || tx >= sx + 16) break;
                    }
                }
                break;
            case 3: /* Act 4 "Zero Day" — destruction */
                for (int b = 0; b < 4; b++) {
                    int bx = sx + 1 + lrng_range(14);
                    int by = 4 + lrng_range(16);
                    if (by < NET_MAP_H && level_data.collision[by * NET_MAP_W + bx] == TILE_EMPTY)
                        set_visual_tile(bx, by, NTILE_BROKEN_PANEL);
                }
                break;
            case 4: /* Act 5 "Ghost Protocol" — abandoned */
                if (lrng_range(3) == 0) {
                    int mx = sx + 3 + lrng_range(10);
                    int my = 4 + lrng_range(8);
                    if (level_data.collision[my * NET_MAP_W + mx] == TILE_EMPTY)
                        set_visual_tile(mx, my, NTILE_SCREEN);
                }
                break;
            case 5: /* Act 6 "Trace Route" — chaotic */
                for (int c = 0; c < 5; c++) {
                    int cx = sx + lrng_range(16);
                    int cy = 3 + lrng_range(20);
                    if (cy < NET_MAP_H && level_data.collision[cy * NET_MAP_W + cx] == TILE_EMPTY) {
                        int ar = lrng_range(6);
                        int tile = (ar == 0) ? NTILE_GLITCH : (ar == 1) ? NTILE_BROKEN_PANEL :
                                   (ar == 2) ? NTILE_DATA_STREAM : (ar == 3) ? NTILE_DATA_WATERFALL :
                                   (ar == 4) ? NTILE_CIRCUIT_NODE : NTILE_CONDUIT;
                        set_visual_tile(cx, cy, tile);
                    }
                }
                break;
            }
            /* Universal: occasional neon signs */
            if (lrng_range(4) == 0) {
                int ny = 4 + lrng_range(6);
                if (level_data.collision[ny * NET_MAP_W + sx] == TILE_EMPTY)
                    set_visual_tile(sx, ny, NTILE_NEON_SIGN);
            }
        }
    }

    /* Wall surface detail — replace ~10% of plain NTILE_WALL with variants */
    for (int x = 0; x < NET_MAP_W; x++) {
        for (int y = 2; y < NET_MAP_H; y++) {
            if (level_data.tiles[y * NET_MAP_W + x] != NTILE_WALL) continue;
            if (level_data.collision[y * NET_MAP_W + x] != TILE_SOLID) continue;
            if (y > 0 && level_data.collision[(y-1) * NET_MAP_W + x] == TILE_SOLID &&
                level_data.tiles[(y-1) * NET_MAP_W + x] == NTILE_WALL) {
                if (lrng_range(12) == 0)
                    level_data.tiles[y * NET_MAP_W + x] = NTILE_WALL_PANEL;
            } else {
                int r = lrng_range(10);
                if (r == 0) level_data.tiles[y * NET_MAP_W + x] = NTILE_WALL_PANEL;
                else if (r == 1) level_data.tiles[y * NET_MAP_W + x] = NTILE_WALL_CIRCUIT;
                else if (r == 2) level_data.tiles[y * NET_MAP_W + x] = NTILE_WINDOW;
            }
        }
    }

    /* Validate spawn points */
    for (int i = 0; i < (int)level_data.num_spawns; i++) {
        int tx = level_data.spawn_points[i][0];
        int ty = level_data.spawn_points[i][1];

        /* Reject spawns inside solid tiles */
        if (tx >= 0 && tx < NET_MAP_W && ty >= 0 && ty < NET_MAP_H &&
            level_data.collision[ty * NET_MAP_W + tx] == TILE_SOLID) {
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
    if (col_type == TILE_EMPTY) {
        level_data.tiles[ty * NET_MAP_W + tx] = NTILE_EMPTY;
        networld_refresh_column(tx);
    }
}
