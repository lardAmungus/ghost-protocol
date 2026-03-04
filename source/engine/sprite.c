#include "engine/sprite.h"
#include <string.h>

static OBJ_ATTR obj_buffer[SPRITE_MAX];
static u8 sprite_used[SPRITE_MAX];
static int next_free_hint = 0;  /* O(1) alloc in common case */

void sprite_init(void) {
    oam_init(obj_buffer, SPRITE_MAX);
    memset(sprite_used, 0, sizeof(sprite_used));
    next_free_hint = 0;
}

int sprite_alloc(void) {
    /* Start scan from hint for O(1) common case */
    for (int i = next_free_hint; i < SPRITE_MAX; i++) {
        if (!sprite_used[i]) {
            sprite_used[i] = 1;
            next_free_hint = i + 1;
            return i;
        }
    }
    /* Wrap around if hint was past free slots */
    for (int i = 0; i < next_free_hint; i++) {
        if (!sprite_used[i]) {
            sprite_used[i] = 1;
            next_free_hint = i + 1;
            return i;
        }
    }
    return -1;
}

void sprite_free(int index) {
    if (index < 0 || index >= SPRITE_MAX) return;
    if (!sprite_used[index]) return;  /* Already freed */
    sprite_used[index] = 0;
    obj_hide(&obj_buffer[index]);
    if (index < next_free_hint) next_free_hint = index;
}

OBJ_ATTR* sprite_get(int index) {
    if (index < 0 || index >= SPRITE_MAX) return NULL;
    return &obj_buffer[index];
}

void sprite_oam_update(void) {
    /* Use DMA3 for fast OAM copy during VBlank */
    dma3_cpy(oam_mem, obj_buffer, SPRITE_MAX * sizeof(OBJ_ATTR));
}
