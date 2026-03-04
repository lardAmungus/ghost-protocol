#ifndef GAME_ITEMDROP_H
#define GAME_ITEMDROP_H

#include <tonc.h>
#include "game/loot.h"

/*
 * Ghost Protocol — Item Drop System
 *
 * Spawns item pickup entities when enemies die.
 * Items float with bobbing animation, picked up on player contact.
 */

#define MAX_DROPS 8

/* Initialize item drop system. */
void itemdrop_init(void);

/* Spawn a loot drop at world position (8.8 fixed-point). */
void itemdrop_spawn(s32 x, s32 y, const LootItem* item);

/* Roll for a drop from an enemy kill. tier and rarity_floor determine quality. */
void itemdrop_roll(s32 x, s32 y, int tier, int rarity_floor, int player_lck);

/* Update all drops (bobbing, lifetime). */
void itemdrop_update_all(void);

/* Draw all drops. */
void itemdrop_draw_all(s32 cam_x, s32 cam_y);

/* Check player pickup (returns 1 if item picked up). */
int itemdrop_check_pickup(s32 player_x, s32 player_y);

/* Clear all drops. */
void itemdrop_clear_all(void);

#endif /* GAME_ITEMDROP_H */
