#include "engine/collision.h"

static const u8* col_map = NULL;
static int col_width  = 0;
static int col_height = 0;

void collision_set_map(const u8* map, int width_tiles, int height_tiles) {
    col_map = map;
    col_width = width_tiles;
    col_height = height_tiles;
}

IWRAM_CODE int collision_tile_at(int px, int py) {
    if (!col_map) return TILE_EMPTY;
    int tx = px >> 3;  /* Divide by 8 (tile size) */
    int ty = py >> 3;
    if (tx < 0 || tx >= col_width || ty < 0 || ty >= col_height)
        return TILE_EMPTY;
    return col_map[ty * col_width + tx];
}

IWRAM_CODE int collision_point_solid(int px, int py) {
    int tile = collision_tile_at(px, py);
    return tile == TILE_SOLID || tile == TILE_BREAKABLE;
}

IWRAM_CODE int collision_point_hazard(int px, int py) {
    int tile = collision_tile_at(px, py);
    return tile == TILE_HAZARD;
}

IWRAM_CODE int collision_rect_solid(int px, int py, int pw, int ph) {
    return collision_point_solid(px,          py) ||
           collision_point_solid(px + pw - 1, py) ||
           collision_point_solid(px,          py + ph - 1) ||
           collision_point_solid(px + pw - 1, py + ph - 1);
}

IWRAM_CODE int collision_aabb(const Entity* a, const Entity* b) {
    /* Convert 8.8 fixed-point to pixel positions */
    int ax = a->x >> 8;
    int ay = a->y >> 8;
    int bx = b->x >> 8;
    int by = b->y >> 8;

    return ax < bx + b->width  &&
           ax + a->width > bx  &&
           ay < by + b->height &&
           ay + a->height > by;
}

IWRAM_CODE int collision_point_platform(int px, int py) {
    int tile = collision_tile_at(px, py);
    return tile == TILE_PLATFORM;
}

IWRAM_CODE int collision_check_ground(int px, int py, int pw) {
    /* Check 1px below the bottom edge of an entity at two foot points */
    int foot_y = py + 1;
    int left_tile  = collision_tile_at(px + 2, foot_y);
    int right_tile = collision_tile_at(px + pw - 3, foot_y);
    return (left_tile == TILE_SOLID || left_tile == TILE_PLATFORM ||
            left_tile == TILE_BREAKABLE ||
            right_tile == TILE_SOLID || right_tile == TILE_PLATFORM ||
            right_tile == TILE_BREAKABLE);
}
