#ifndef ENGINE_ENTITY_H
#define ENGINE_ENTITY_H

#include <tonc.h>

#define MAX_ENTITIES 64
#define OAM_NONE     0xFF  /* Sentinel: no OAM sprite allocated */

/* Entity types — 0 means inactive/free slot */
enum {
    ENT_NONE = 0,
    ENT_PLAYER,
    ENT_ENEMY,
    ENT_PROJECTILE,
    ENT_ITEM,
    ENT_BOSS,
    ENT_TYPE_COUNT
};

typedef struct {
    u8  type;           /* 0 = inactive/free slot */
    u8  oam_index;      /* Index into shadow OAM */
    u16 flags;
    s32 x, y;           /* 8.8 fixed-point world position */
    s16 vx, vy;         /* velocity */
    s16 hp;
    u8  anim_frame;
    u8  anim_timer;
    u8  width, height;  /* Bounding box in pixels */
    u16 last_hit_id;    /* Attack generation that last hit this entity */
    u8  on_ground;      /* 1 if standing on solid surface */
    u8  on_wall;        /* 1 if touching wall (left or right) */
    u8  facing;         /* 0 = right, 1 = left */
    u8  subtype;        /* Sub-classification within entity type */
} Entity;

/* Initialize the entity pool — marks all slots as inactive. */
void entity_init(void);

/* Spawn an entity of the given type. Returns pointer, or NULL if pool full. */
Entity* entity_spawn(int type);

/* Despawn an entity (marks slot inactive). */
void entity_despawn(Entity* e);

/* Get entity by pool index. */
Entity* entity_get(int index);

/* Get entity high-water mark (upper bound for active entity iteration). */
int entity_get_high_water(void);

/* Update all active entities. */
void entity_update_all(void);

#endif /* ENGINE_ENTITY_H */
