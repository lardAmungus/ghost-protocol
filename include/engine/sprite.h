#ifndef ENGINE_SPRITE_H
#define ENGINE_SPRITE_H

#include <tonc.h>

#define SPRITE_MAX 128

/* Initialize the shadow OAM buffer — hides all sprites. */
void sprite_init(void);

/* Allocate the next free OAM slot. Returns index, or -1 if full. */
int sprite_alloc(void);

/* Free an OAM slot (hides the sprite). */
void sprite_free(int index);

/* Get pointer to a shadow OAM entry for direct manipulation. */
OBJ_ATTR* sprite_get(int index);

/* DMA-copy shadow OAM to hardware. Called from VBlank ISR. */
void sprite_oam_update(void);

#endif /* ENGINE_SPRITE_H */
