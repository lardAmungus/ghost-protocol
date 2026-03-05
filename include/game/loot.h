#ifndef GAME_LOOT_H
#define GAME_LOOT_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Loot System
 *
 * Borderlands-style randomized items with rarity tiers.
 * 8-byte LootItem struct for compact storage.
 * 3 equipment categories: Weapon, Armor, Accessory.
 */

/* Item categories (stored in type high 2 bits) */
#define LOOT_CAT_WEAPON    0
#define LOOT_CAT_ARMOR     1
#define LOOT_CAT_ACCESSORY 2

#define LOOT_CATEGORY(type) (((type) >> 6) & 3)
#define LOOT_SUBTYPE(type)  ((type) & 0x3F)
#define LOOT_MAKE_TYPE(cat, sub) ((u8)(((cat) << 6) | ((sub) & 0x3F)))

/* Weapon subtypes (8 types) */
enum {
    WEAPON_BUSTER = 0,   /* Balanced */
    WEAPON_RAPID,        /* Fast fire */
    WEAPON_SPREAD,       /* Multi-shot */
    WEAPON_CHARGER,      /* Charge-up */
    WEAPON_BEAM,         /* Continuous */
    WEAPON_LASER,        /* Hold for ramping stream */
    WEAPON_HOMING,       /* Slow tracking projectile */
    WEAPON_NOVA,         /* Short-range AoE burst */
    WEAPON_TYPE_COUNT
};

/* Armor subtypes (10 types) */
enum {
    ARMOR_DATA_VEST = 0,    /* +DEF */
    ARMOR_FIREWALL,         /* +DEF, hazard resist */
    ARMOR_NEURAL_PLATE,     /* +DEF high */
    ARMOR_STEALTH_SUIT,     /* +DEF, speed */
    ARMOR_CIRCUIT_WEAVE,    /* +DEF, regen */
    ARMOR_PLATING,          /* +DEF very high */
    ARMOR_NANO_MESH,        /* +DEF, resist */
    ARMOR_PHASE_SHELL,      /* +DEF, dodge */
    ARMOR_QUANTUM_GUARD,    /* +DEF, reflect */
    ARMOR_TITAN_FRAME,      /* +DEF max */
    ARMOR_TYPE_COUNT
};

/* Accessory subtypes (10 types) */
enum {
    ACC_SPRINT_MODULE = 0,  /* +SPD */
    ACC_LUCKY_CHIP,         /* +LCK */
    ACC_REGEN_CORE,         /* HP regen */
    ACC_SHIELD_CELL,        /* +max HP */
    ACC_CRIT_LENS,          /* +crit chance */
    ACC_XP_BOOSTER,         /* +XP gain */
    ACC_CREDIT_FINDER,      /* +credits */
    ACC_AMMO_BELT,          /* +fire rate */
    ACC_PHASE_RING,         /* +dash speed */
    ACC_POWER_CELL,         /* +ATK */
    ACC_TYPE_COUNT
};

/* 8-byte item struct — category encoded in type high bits */
typedef struct {
    u8  type;       /* high 2 bits: LOOT_CAT_*, low 6: subtype */
    u8  rarity;     /* RARITY_* */
    u8  stat1;      /* Weapon: damage. Armor: DEF. Accessory: bonus value */
    u8  stat2;      /* Weapon: fire rate. Armor/Accessory: secondary */
    u8  prefix_id;  /* Procedural name prefix index */
    u8  suffix_id;  /* Procedural name suffix index */
    u8  level;      /* Item level requirement */
    u8  flags;      /* Bit flags */
} LootItem;

/* LootItem flags */
#define LOOT_FLAG_EQUIPPED (1 << 0)
#define LOOT_FLAG_SET_MASK 0x0E  /* bits 1-3: set_id (0=none, 1-6=set) */
#define LOOT_SET_ID(flags) (((flags) >> 1) & 7)
#define LOOT_SET_FLAG(id)  ((u8)(((id) & 7) << 1))

#define INVENTORY_SIZE 20
#define PREFIX_COUNT   20
#define SUFFIX_COUNT   20

/* Set bonus IDs (matches boss order) */
enum {
    SET_NONE = 0,
    SET_MICROSLOP,   /* fire rate + bounce */
    SET_GOGOL,       /* detection + crit blind */
    SET_AMAZOMB,     /* drop rate + extra drop */
    SET_CRAPPLE,     /* max HP + regen */
    SET_FACEPLANT,   /* XP + heal on level */
    SET_TRACE,       /* credit + shop discount */
    SET_COUNT
};

extern const char* const loot_prefixes[PREFIX_COUNT];
extern const char* const loot_suffixes[SUFFIX_COUNT];
extern const char* const weapon_type_names[WEAPON_TYPE_COUNT];

/* Generate a random loot item at given tier/rarity floor. */
void loot_generate(LootItem* out, int tier, int rarity_floor, int player_lck);

/* Generate armor item. */
void loot_generate_armor(LootItem* out, int tier, int rarity_floor, int player_lck);

/* Generate accessory item. */
void loot_generate_accessory(LootItem* out, int tier, int rarity_floor, int player_lck);

/* Generate random item of any category. */
void loot_generate_any(LootItem* out, int tier, int rarity_floor, int player_lck);

/* Get the display name of a loot item (writes to static buffer). */
const char* loot_get_name(const LootItem* item);

/* Get rarity name. */
const char* loot_get_rarity_name(int rarity);

/* Compare two items: returns positive if a is better, negative if b is better. */
int loot_compare(const LootItem* a, const LootItem* b);

/* Get sell value of an item in credits. */
int loot_sell_value(const LootItem* item);

/* Get salvage shard value of an item. */
int loot_salvage_value(const LootItem* item);

/* Count how many pieces of a set are equipped. */
int loot_equipped_set_count(int set_id);

/* ---- Inventory ---- */

void inventory_init(void);
int inventory_add(const LootItem* item);
void inventory_remove(int idx);
LootItem* inventory_get(int idx);
LootItem* inventory_get_equipped(void);       /* Equipped weapon */
LootItem* inventory_get_equipped_armor(void);  /* Equipped armor */
LootItem* inventory_get_equipped_accessory(void); /* Equipped accessory */
void inventory_equip(int idx);
int inventory_count(void);

/* ---- Crafting ---- */

/* Fuse: 3 items of same rarity -> 1 item of next rarity. Returns 1 on success.
 * idx1/idx2/idx3 are inventory indices. Result replaces idx1. */
int craft_fuse(int idx1, int idx2, int idx3);

/* Salvage: break item -> shards. Returns shard count. */
int craft_salvage(int idx);

/* Forge: reroll item stats (keep rarity/type). Cost in shards. Returns 1 on success. */
int craft_forge(int idx, u16* shards);

#endif /* GAME_LOOT_H */
