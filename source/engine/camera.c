#include "engine/camera.h"
#include "engine/input.h"
#include "game/common.h"

void camera_init(Camera* cam, int dz_w, int dz_h) {
    cam->x = 0;
    cam->y = 0;
    cam->target_x = 0;
    cam->target_y = 0;
    cam->look_ahead = 0;
    cam->dead_zone_w = dz_w;
    cam->dead_zone_h = dz_h;
}

IWRAM_CODE void camera_update(Camera* cam, s32 player_x, s32 player_y) {
    /* Apply look-ahead offset in facing direction */
    s32 look_x = player_x + cam->look_ahead;

    s32 screen_px = look_x - cam->x;
    s32 screen_py = player_y - cam->y;

    s32 left  = (120 - cam->dead_zone_w) << 8;
    s32 right = (120 + cam->dead_zone_w) << 8;
    s32 top   = (80 - cam->dead_zone_h) << 8;
    s32 bot   = (80 + cam->dead_zone_h) << 8;

    if (screen_px < left)  cam->target_x = look_x - left;
    if (screen_px > right) cam->target_x = look_x - right;
    if (screen_py < top)   cam->target_y = player_y - top;
    if (screen_py > bot)   cam->target_y = player_y - bot;

    /* Lerp smoothing: shift right by 2 gives 1/4 speed blend */
    /* Snap when close to avoid sub-pixel jitter (diff < 8 in 8.8 = 0.03 px) */
    s32 dx = cam->target_x - cam->x;
    s32 dy = cam->target_y - cam->y;
    if (dx > -8 && dx < 8) cam->x = cam->target_x;
    else cam->x += dx >> 2;
    if (dy > -8 && dy < 8) cam->y = cam->target_y;
    else cam->y += dy >> 2;

    /* Clamp to world boundaries (prevent showing off-map regions) */
    s32 max_x = (NET_MAP_W * 8 - 240) << 8;
    s32 max_y = (NET_MAP_H * 8 - 160) << 8;
    if (cam->x < 0) cam->x = 0;
    if (cam->x > max_x) cam->x = max_x;
    if (cam->y < 0) cam->y = 0;
    if (cam->y > max_y) cam->y = max_y;
}
