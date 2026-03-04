/*
 * Ghost Protocol — Platformer Physics
 *
 * Gravity, axis-separated collision resolution, one-way platforms.
 * All positions/velocities are 8.8 fixed-point.
 */
#include "game/physics.h"
#include "engine/collision.h"

/* ---- Per-class physics tuning ---- */
const PhysicsParams physics_class[CLASS_COUNT] = {
    [CLASS_ASSAULT] = {
        .gravity         = 38,       /* ~0.15 px/f^2 */
        .max_fall        = 1024,     /* 4.0 px/f */
        .jump_vel        = -960,     /* -3.75 px/f (strong) */
        .jump_cut        = -256,     /* -1.0 px/f (on release) */
        .wall_jump_vx    = 640,      /* 2.5 px/f */
        .wall_jump_vy    = -832,     /* -3.25 px/f */
        .wall_slide_speed= 256,      /* 1.0 px/f */
        .move_speed      = 384,      /* 1.5 px/f */
        .accel           = 64,       /* 0.25 */
        .decel           = 96,       /* 0.375 */
        .air_accel       = 40,       /* 0.156 */
        .dash_speed      = 0,
        .dash_frames     = 0,
        .dash_cooldown   = 0,
        .max_jumps       = 1,
        .can_wall_jump   = 1,
        .can_dash        = 0,
    },
    [CLASS_INFILTRATOR] = {
        .gravity         = 34,       /* lighter */
        .max_fall        = 896,      /* 3.5 px/f */
        .jump_vel        = -896,     /* -3.5 px/f */
        .jump_cut        = -224,     /* -0.875 px/f */
        .wall_jump_vx    = 576,      /* 2.25 px/f */
        .wall_jump_vy    = -768,     /* -3.0 px/f */
        .wall_slide_speed= 192,      /* 0.75 px/f (slow slide) */
        .move_speed      = 448,      /* 1.75 px/f (fast) */
        .accel           = 72,       /* 0.28 */
        .decel           = 80,       /* 0.31 */
        .air_accel       = 52,       /* 0.20 */
        .dash_speed      = 768,      /* 3.0 px/f */
        .dash_frames     = 8,
        .dash_cooldown   = 30,
        .max_jumps       = 2,        /* double jump */
        .can_wall_jump   = 1,
        .can_dash        = 1,
    },
    [CLASS_TECHNOMANCER] = {
        .gravity         = 36,       /* middle */
        .max_fall        = 960,      /* 3.75 px/f */
        .jump_vel        = -928,     /* -3.625 px/f */
        .jump_cut        = -240,     /* -0.9375 */
        .wall_jump_vx    = 608,      /* 2.375 */
        .wall_jump_vy    = -800,     /* -3.125 */
        .wall_slide_speed= 224,      /* 0.875 */
        .move_speed      = 384,      /* 1.5 px/f */
        .accel           = 60,       /* 0.234 */
        .decel           = 88,       /* 0.34 */
        .air_accel       = 44,       /* 0.17 */
        .dash_speed      = 0,
        .dash_frames     = 0,
        .dash_cooldown   = 0,
        .max_jumps       = 1,
        .can_wall_jump   = 1,
        .can_dash        = 0,
    },
};

/* ---- Wall check ---- */

IWRAM_CODE int physics_check_wall(const Entity* e, int side) {
    int px = e->x >> 8;
    int py = e->y >> 8;
    int check_x;

    if (side < 0) {
        check_x = px - 1;
    } else {
        check_x = px + e->width;
    }

    /* Check two points on the side (1/3 and 2/3 height) */
    int h3 = e->height / 3;
    return collision_point_solid(check_x, py + h3) ||
           collision_point_solid(check_x, py + h3 * 2);
}

/* ---- X-axis collision resolution ---- */

IWRAM_CODE void physics_resolve_x(Entity* e) {
    int px = e->x >> 8;
    int py = e->y >> 8;
    int w = e->width;
    int h = e->height;

    /* Check top-left, mid-left, bottom-left (left edge) */
    /* and top-right, mid-right, bottom-right (right edge) */
    int inset = 2;  /* Vertical inset to avoid floor/ceiling corners */

    if (e->vx > 0) {
        /* Moving right — check right edge */
        int rx = px + w - 1;
        if (collision_point_solid(rx, py + inset) ||
            collision_point_solid(rx, py + h / 2) ||
            collision_point_solid(rx, py + h - 1 - inset)) {
            /* Push left to tile boundary */
            int tile_x = (rx >> 3) << 3;
            e->x = (s32)(tile_x - w) << 8;
            e->vx = 0;
            e->on_wall = 1;
        }
    } else if (e->vx < 0) {
        /* Moving left — check left edge */
        if (collision_point_solid(px, py + inset) ||
            collision_point_solid(px, py + h / 2) ||
            collision_point_solid(px, py + h - 1 - inset)) {
            /* Push right to tile boundary */
            int tile_x = ((px >> 3) + 1) << 3;
            e->x = (s32)tile_x << 8;
            e->vx = 0;
            e->on_wall = 1;
        }
    }
}

/* ---- Y-axis collision resolution ---- */

IWRAM_CODE void physics_resolve_y(Entity* e) {
    int px = e->x >> 8;
    int py = e->y >> 8;
    int w = e->width;
    int h = e->height;
    int inset = 2;  /* Horizontal inset to avoid wall corners */

    if (e->vy > 0) {
        /* Moving down — check bottom edge */
        int by = py + h - 1;
        int left_tile  = collision_tile_at(px + inset, by);
        int right_tile = collision_tile_at(px + w - 1 - inset, by);

        int solid = (left_tile == TILE_SOLID || left_tile == TILE_BREAKABLE ||
                     right_tile == TILE_SOLID || right_tile == TILE_BREAKABLE);

        /* One-way platforms: only solid when falling and feet at/above platform top */
        if (!solid) {
            int platform = (left_tile == TILE_PLATFORM || right_tile == TILE_PLATFORM);
            if (platform) {
                /* Check if bottom of entity is at or just past the top of the tile */
                int tile_top = (by >> 3) << 3;
                int prev_bottom = ((e->y - e->vy) >> 8) + h - 1;
                if (prev_bottom <= tile_top) {
                    solid = 1;
                }
            }
        }

        if (solid) {
            int tile_y = (by >> 3) << 3;
            e->y = (s32)(tile_y - h) << 8;
            e->vy = 0;
            e->on_ground = 1;
        }
    } else if (e->vy < 0) {
        /* Moving up — check top edge (only solid tiles, not platforms) */
        if (collision_point_solid(px + inset, py) ||
            collision_point_solid(px + w - 1 - inset, py)) {
            int tile_y = ((py >> 3) + 1) << 3;
            e->y = (s32)tile_y << 8;
            e->vy = 0;
        }
    }
}

/* ---- Combined physics update ---- */

IWRAM_CODE void physics_update(Entity* e, const PhysicsParams* params) {
    /* Reset per-frame flags */
    e->on_ground = 0;
    e->on_wall = 0;

    /* Apply gravity */
    e->vy += params->gravity;
    if (e->vy > params->max_fall) {
        e->vy = params->max_fall;
    }

    /* Apply velocity */
    e->x += e->vx;
    /* Resolve X collision */
    physics_resolve_x(e);

    e->y += e->vy;
    /* Resolve Y collision */
    physics_resolve_y(e);

    /* Ground detection: if not resolved by Y collision, check 1px below */
    if (!e->on_ground) {
        int px = e->x >> 8;
        int py = e->y >> 8;
        e->on_ground = (u8)collision_check_ground(px, py + e->height - 1, e->width);
    }

    /* Wall detection */
    if (!e->on_wall) {
        if (e->vx > 0) {
            e->on_wall = (u8)physics_check_wall(e, 1);
        } else if (e->vx < 0) {
            e->on_wall = (u8)physics_check_wall(e, -1);
        }
    }

    /* Wall slide: cap fall speed when sliding */
    if (e->on_wall && !e->on_ground && e->vy > params->wall_slide_speed) {
        e->vy = params->wall_slide_speed;
    }

    /* World bounds clamping */
    if (e->x < 0) {
        e->x = 0;
        e->vx = 0;
    }
    if (e->y < 0) {
        e->y = 0;
        e->vy = 0;
    }
    int max_x = (NET_MAP_PX - e->width) << 8;
    if (e->x > max_x) {
        e->x = max_x;
        e->vx = 0;
    }
    int max_y = (NET_MAP_PY - e->height) << 8;
    if (e->y > max_y) {
        e->y = max_y;
        e->vy = 0;
    }
}
