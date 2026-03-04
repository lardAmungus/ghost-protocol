#ifndef GAME_LOOT_H
#define GAME_LOOT_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Loot System
 *
 * Borderlands-style randomized weapons with rarity tiers.
 * 8-byte LootItem struct for compact storage.
 */

/* Weapon subtypes */
enum {
    WEAPON_BUSTER = 0,   /* Balanced */
    WEAPON_RAPID,        /* Fast fire */
    WEAPON_SPREAD,       /* Multi-shot */
    WEAPON_CHARGER,      /* Charge-up */
    WEAPON_BEAM,         /* Continuous */
    WEAPON_TYPE_COUNT
};

/* 8-byte item struct */
typedef struct {
    u8  type;       /* WEAPON_* subtype */
    u8  rarity;     /* RARITY_* */
    u8  stat1;      /* Damage (1-99) */
    u8  stat2;      /* Fire rate (1-99, lower = faster) */
    u8  prefix_id;  /* Procedural name prefix index */
    u8  suffix_id;  /* Procedural name suffix index */
    u8  level;      /* Item level requirement */
    u8  flags;      /* Bit flags (equipped, etc) */
} LootItem;

#define LOOT_FLAG_EQUIPPED (1 << 0)

#define INVENTORY_SIZE 20
#define PREFIX_COUNT   12
#define SUFFIX_COUNT   12

extern const char* const loot_prefixes[PREFIX_COUNT];
extern const char* const loot_suffixes[SUFFIX_COUNT];
extern const char* const weapon_type_names[WEAPON_TYPE_COUNT];

/* Generate a random loot item at given tier/rarity floor. */
void loot_generate(LootItem* out, int tier, int rarity_floor, int player_lck);

/* Get the display name of a loot item (writes to static buffer). */
const char* loot_get_name(const LootItem* item);

/* Get rarity name. */
const char* loot_get_rarity_name(int rarity);

/* Compare two items: returns positive if a is better, negative if b is better. */
int loot_compare(const LootItem* a, const LootItem* b);

/* Get sell value of an item in credits. */
int loot_sell_value(const LootItem* item);

/* ---- Inventory ---- */

/* Initialize inventory. */
void inventory_init(void);

/* Add item to inventory. Returns 1 on success, 0 if full. */
int inventory_add(const LootItem* item);

/* Remove item at index. */
void inventory_remove(int idx);

/* Get item at index (NULL if empty). */
LootItem* inventory_get(int idx);

/* Get equipped weapon (NULL if none). */
LootItem* inventory_get_equipped(void);

/* Equip item at index (unequips previous). */
void inventory_equip(int idx);

/* Get inventory count. */
int inventory_count(void);

#endif /* GAME_LOOT_H */
