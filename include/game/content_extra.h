#ifndef GAME_CONTENT_EXTRA_H
#define GAME_CONTENT_EXTRA_H

#include <tonc.h>

#define ENV_ANIM_PATTERN_COUNT 6
#define ENV_ANIM_FRAMES_PER   60
#define ENV_ANIM_TOTAL_FRAMES 360
#define TILESET_THEME_COUNT   80
#define TILESET_TILES_PER     256

const void* env_anim_force_link(void);
const void* tileset_force_link(void);

static inline void content_extra_force_link_all(void) {
    volatile const void* p;
    p = env_anim_force_link();
    p = tileset_force_link();
    (void)p;
}

#endif /* GAME_CONTENT_EXTRA_H */
