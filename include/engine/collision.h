#ifndef ENGINE_COLLISION_H
#define ENGINE_COLLISION_H

#include <tonc.h>
#include "engine/entity.h"

/* Tile collision types */
enum {
    TILE_EMPTY     = 0,
    TILE_SOLID     = 1,
    TILE_PLATFORM  = 2,   /* One-way: solid only from above */
    TILE_HAZARD    = 3,
    TILE_LADDER    = 4,
    TILE_BREAKABLE = 5,
    TILE_TESLA     = 6,   /* Toggles between hazard/empty on timer */
    TILE_CORRUPT   = 7,   /* Drains HP slowly */
    TILE_STREAM    = 8,   /* Pushes player horizontally */
};

/* Tesla grid toggle — set by state_net every 90 frames */
extern int collision_tesla_active;

/* Set the collision map (pointer to byte array, width in tiles). */
void collision_set_map(const u8* map, int width_tiles, int height_tiles);

/* Check tile type at a world pixel position. */
int collision_tile_at(int px, int py);

/* Check if a point hits a solid tile. */
int collision_point_solid(int px, int py);

/* Check if a point hits a hazard tile. */
int collision_point_hazard(int px, int py);

/* Check if a rectangle (4 corners) overlaps any solid tile. */
int collision_rect_solid(int px, int py, int pw, int ph);

/* AABB overlap test between two entities. */
int collision_aabb(const Entity* a, const Entity* b);

/* Check if a point is a one-way platform tile. */
int collision_point_platform(int px, int py);

/* Check ground directly below an entity's feet (1px below bottom edge). */
int collision_check_ground(int px, int py, int pw);

#endif /* ENGINE_COLLISION_H */
