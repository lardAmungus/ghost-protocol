/*
 * Ghost Protocol — Loot System
 *
 * Procedural weapon generation with rarity tiers.
 */
#include "game/loot.h"
#include "engine/rng.h"
#include <string.h>

const char* const loot_prefixes[PREFIX_COUNT] = {
    "Burning", "Cryo", "Quantum", "Void",
    "Plasma", "Neural", "Holo", "Surge",
    "Flux", "Omega", "Zero", "Dark",
};

const char* const loot_suffixes[SUFFIX_COUNT] = {
    "of Fury", "Prime", "Protocol", "MkII",
    "Ultra", "EX", "Sigma", "Apex",
    "Byte", "Core", "Edge", "Nova",
};

const char* const weapon_type_names[WEAPON_TYPE_COUNT] = {
    "Buster", "Rapid", "Spread", "Charger", "Beam",
};

static const char* const rarity_names[5] = {
    "Common", "Uncommon", "Rare", "Epic", "Legendary",
};

/* Rarity roll weights (cumulative out of 100) */
/* Base: Common 50, Uncommon 30, Rare 14, Epic 5, Legendary 1 */
static int roll_rarity(int player_lck) {
    int roll = (int)rand_range(100);
    /* LCK shifts odds: diminishing returns, capped at 10 point shift.
     * At LCK 20 → shift 10, LCK 30+ → still 10 (cap). */
    int shift = player_lck / 2;
    if (shift > 10) shift = 10;
    roll -= shift;
    if (roll < 0) roll = 0;

    if (roll < 1) return RARITY_LEGENDARY;
    if (roll < 6) return RARITY_EPIC;
    if (roll < 20) return RARITY_RARE;
    if (roll < 50) return RARITY_UNCOMMON;
    return RARITY_COMMON;
}

void loot_generate(LootItem* out, int tier, int rarity_floor, int player_lck) {
    memset(out, 0, sizeof(LootItem));

    /* Roll rarity (with floor) — reroll up to 3 times if below floor */
    int rarity = roll_rarity(player_lck);
    for (int reroll = 0; reroll < 3 && rarity < rarity_floor; reroll++) {
        rarity = roll_rarity(player_lck);
    }
    if (rarity < rarity_floor) rarity = rarity_floor;

    /* Random weapon type */
    out->type = (u8)rand_range(WEAPON_TYPE_COUNT);
    out->rarity = (u8)rarity;

    /* Base damage scales with tier and rarity */
    int base_dmg = 3 + tier * 2;
    int rarity_bonus = rarity * 2;
    out->stat1 = (u8)(base_dmg + rarity_bonus + (int)rand_range(4));

    /* Fire rate — lower is faster. Varies by weapon type */
    int base_rate;
    switch (out->type) {
    case WEAPON_RAPID:   base_rate = 4; break;
    case WEAPON_SPREAD:  base_rate = 12; break;
    case WEAPON_CHARGER: base_rate = 20; break;
    case WEAPON_BEAM:    base_rate = 2; break;
    default:             base_rate = 8; break; /* Buster */
    }
    {
        int rate = base_rate + (int)rand_range(4) - rarity;
        if (rate < 1) rate = 1;
        out->stat2 = (u8)rate;
    }

    /* Procedural name */
    out->prefix_id = (u8)rand_range(PREFIX_COUNT);
    out->suffix_id = (u8)rand_range(SUFFIX_COUNT);

    /* Level requirement */
    out->level = (u8)(tier > 0 ? tier * 2 - 1 : 1);
    out->flags = 0;
}

static char name_buf[48];

const char* loot_get_name(const LootItem* item) {
    /* Build name: "[Prefix] [Type] [Suffix]" — truncated to fit */
    const char* prefix = loot_prefixes[item->prefix_id % PREFIX_COUNT];
    const char* type = weapon_type_names[item->type % WEAPON_TYPE_COUNT];
    const char* suffix = loot_suffixes[item->suffix_id % SUFFIX_COUNT];

    /* Simple concatenation */
    int pos = 0;
    for (int i = 0; prefix[i] && pos < 12; i++) name_buf[pos++] = prefix[i];
    name_buf[pos++] = ' ';
    for (int i = 0; type[i] && pos < 22; i++) name_buf[pos++] = type[i];
    name_buf[pos++] = ' ';
    for (int i = 0; suffix[i] && pos < 30; i++) name_buf[pos++] = suffix[i];
    name_buf[pos] = '\0';
    return name_buf;
}

const char* loot_get_rarity_name(int rarity) {
    if (rarity < 0 || rarity > 4) return "";
    return rarity_names[rarity];
}

int loot_sell_value(const LootItem* item) {
    /* Sell value: base from damage + rarity multiplier */
    static const u8 rarity_mult[5] = { 1, 2, 4, 8, 16 };
    int r = item->rarity;
    if (r > 4) r = 4;
    return (int)item->stat1 * rarity_mult[r] / 2 + 1;
}

int loot_compare(const LootItem* a, const LootItem* b) {
    /* Simple DPS comparison: damage / fire_rate */
    int dps_a = a->stat1 * 100 / (a->stat2 > 0 ? a->stat2 : 1);
    int dps_b = b->stat1 * 100 / (b->stat2 > 0 ? b->stat2 : 1);
    return dps_a - dps_b;
}

/* ---- Inventory ---- */
static LootItem inv_items[INVENTORY_SIZE];
static u8 inv_used[INVENTORY_SIZE]; /* 1 if slot occupied */

void inventory_init(void) {
    memset(inv_items, 0, sizeof(inv_items));
    memset(inv_used, 0, sizeof(inv_used));
}

int inventory_add(const LootItem* item) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (!inv_used[i]) {
            inv_items[i] = *item;
            inv_used[i] = 1;
            return 1;
        }
    }
    return 0; /* Full */
}

void inventory_remove(int idx) {
    if (idx >= 0 && idx < INVENTORY_SIZE) {
        inv_used[idx] = 0;
        memset(&inv_items[idx], 0, sizeof(LootItem));
    }
}

LootItem* inventory_get(int idx) {
    if (idx >= 0 && idx < INVENTORY_SIZE && inv_used[idx]) {
        return &inv_items[idx];
    }
    return NULL;
}

LootItem* inventory_get_equipped(void) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i] && (inv_items[i].flags & LOOT_FLAG_EQUIPPED)) {
            return &inv_items[i];
        }
    }
    return NULL;
}

void inventory_equip(int idx) {
    if (idx < 0 || idx >= INVENTORY_SIZE || !inv_used[idx]) return;

    /* Unequip previous */
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i]) {
            inv_items[i].flags &= (u8)~LOOT_FLAG_EQUIPPED;
        }
    }
    inv_items[idx].flags |= LOOT_FLAG_EQUIPPED;
}

int inventory_count(void) {
    int count = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i]) count++;
    }
    return count;
}
