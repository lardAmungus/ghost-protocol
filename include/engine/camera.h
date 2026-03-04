#ifndef ENGINE_CAMERA_H
#define ENGINE_CAMERA_H

#include <tonc.h>

typedef struct {
    s32 x, y;               /* 8.8 fixed-point, top-left corner of view */
    s32 target_x, target_y;
    s32 look_ahead;          /* 8.8 horizontal look-ahead offset */
    int dead_zone_w, dead_zone_h;
} Camera;

/* Initialize camera with dead zone dimensions (pixels). */
void camera_init(Camera* cam, int dz_w, int dz_h);

/* Update camera to follow player position (8.8 fixed-point). */
IWRAM_CODE void camera_update(Camera* cam, s32 player_x, s32 player_y);

#endif /* ENGINE_CAMERA_H */
