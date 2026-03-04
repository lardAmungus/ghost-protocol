#include "engine/entity.h"
#include "engine/sprite.h"
#include <string.h>

static Entity entity_pool[MAX_ENTITIES];
static int entity_high_water = 0; /* Highest active index + 1 */
static int entity_free_hint = 0;  /* O(1) spawn in common case */

void entity_init(void) {
    /* Properly despawn all entities to free their OAM sprites */
    for (int i = 0; i < MAX_ENTITIES; i++) {
        if (entity_pool[i].type != ENT_NONE) {
            entity_despawn(&entity_pool[i]);
        }
    }
    memset(entity_pool, 0, sizeof(entity_pool));
    entity_high_water = 0;
    entity_free_hint = 0;
}

Entity* entity_spawn(int type) {
    for (int i = entity_free_hint; i < MAX_ENTITIES; i++) {
        if (entity_pool[i].type == ENT_NONE) {
            memset(&entity_pool[i], 0, sizeof(Entity));
            entity_pool[i].type = (u8)type;
            entity_pool[i].oam_index = OAM_NONE;
            if (i + 1 > entity_high_water) entity_high_water = i + 1;
            entity_free_hint = i + 1;
            return &entity_pool[i];
        }
    }
    return NULL;
}

void entity_despawn(Entity* e) {
    if (!e) return;
    if (e->oam_index != OAM_NONE) {
        sprite_free(e->oam_index);
        e->oam_index = OAM_NONE;
    }
    e->type = ENT_NONE;

    /* Update free hint so next spawn can find this slot quickly */
    int idx = (int)(e - entity_pool);
    if (idx < entity_free_hint) entity_free_hint = idx;

    /* Shrink high-water mark if this was the last active entity */
    if (idx + 1 == entity_high_water) {
        while (entity_high_water > 0 &&
               entity_pool[entity_high_water - 1].type == ENT_NONE) {
            entity_high_water--;
        }
    }
}

Entity* entity_get(int index) {
    if (index < 0 || index >= MAX_ENTITIES) return NULL;
    return &entity_pool[index];
}

int entity_get_high_water(void) {
    return entity_high_water;
}

IWRAM_CODE void entity_update_all(void) {
    for (int i = 0; i < entity_high_water; i++) {
        if (entity_pool[i].type == ENT_NONE) continue;
        Entity* e = &entity_pool[i];
        /* Apply velocity */
        e->x += e->vx;
        e->y += e->vy;
    }
}
