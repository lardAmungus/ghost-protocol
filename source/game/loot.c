/*
 * Ghost Protocol — Loot System
 *
 * Procedural item generation with rarity tiers.
 * 3 equipment categories: Weapon (8 types), Armor (10), Accessory (10).
 */
#include "game/loot.h"
#include "game/hud.h"
#include "engine/rng.h"
#include "engine/audio.h"
#include <string.h>

const char* const loot_prefixes[PREFIX_COUNT] = {
    "Burning", "Cryo", "Quantum", "Void",
    "Plasma", "Neural", "Holo", "Surge",
    "Flux", "Omega", "Zero", "Dark",
    "Nano", "Stealth", "Siege", "Proto",
    "Phase", "Hyper", "Binary", "Arc",
};

const char* const loot_suffixes[SUFFIX_COUNT] = {
    "of Fury", "Prime", "Protocol", "MkII",
    "Ultra", "EX", "Sigma", "Apex",
    "Byte", "Core", "Edge", "Nova",
    "Breaker", "Render", "Sync", "Link",
    "Mod", "Compile", "Debug", "Trace",
};

const char* const weapon_type_names[WEAPON_TYPE_COUNT] = {
    "Buster", "Rapid", "Spread", "Charger",
    "Beam", "Laser", "Homing", "Nova",
};

static const char* const armor_type_names[ARMOR_TYPE_COUNT] = {
    "Data Vest", "Firewall", "Neural Plt",
    "Stealth", "Circ Weave", "Plating",
    "Nano Mesh", "Phase Shl", "Q-Guard", "Titan Frm",
};

static const char* const acc_type_names[ACC_TYPE_COUNT] = {
    "Sprint", "Lucky Chp", "Regen Core",
    "Shld Cell", "Crit Lens", "XP Boost",
    "Cred Find", "Ammo Belt", "Phase Rng", "Power Cell",
};

static const char* const rarity_names[RARITY_COUNT] = {
    "Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic",
};

/* Rarity roll weights (cumulative out of 100) */
static int roll_rarity(int player_lck) {
    int roll = (int)rand_range(100);
    int shift = player_lck / 2;
    if (shift > 10) shift = 10;
    roll -= shift;
    /* Allow negative rolls to reach Mythic tier */

    if (roll < -4) return RARITY_MYTHIC;
    if (roll < 1) return RARITY_LEGENDARY;
    if (roll < 6) return RARITY_EPIC;
    if (roll < 20) return RARITY_RARE;
    if (roll < 50) return RARITY_UNCOMMON;
    return RARITY_COMMON;
}

static int ensure_rarity(int rarity_floor, int player_lck) {
    int rarity = roll_rarity(player_lck);
    for (int reroll = 0; reroll < 3 && rarity < rarity_floor; reroll++) {
        rarity = roll_rarity(player_lck);
    }
    if (rarity < rarity_floor) rarity = rarity_floor;
    return rarity;
}

void loot_generate(LootItem* out, int tier, int rarity_floor, int player_lck) {
    memset(out, 0, sizeof(LootItem));

    int rarity = ensure_rarity(rarity_floor, player_lck);

    out->type = LOOT_MAKE_TYPE(LOOT_CAT_WEAPON, (u8)rand_range(WEAPON_TYPE_COUNT));
    out->rarity = (u8)rarity;

    int base_dmg = 3 + tier * 2;
    int rarity_bonus = rarity * 2;
    int dmg = base_dmg + rarity_bonus + (int)rand_range(4);
    if (dmg > 255) dmg = 255;
    out->stat1 = (u8)dmg;

    int sub = LOOT_SUBTYPE(out->type);
    int base_rate;
    switch (sub) {
    case WEAPON_RAPID:   base_rate = 4; break;
    case WEAPON_SPREAD:  base_rate = 12; break;
    case WEAPON_CHARGER: base_rate = 20; break;
    case WEAPON_BEAM:    base_rate = 2; break;
    case WEAPON_LASER:   base_rate = 3; break;
    case WEAPON_HOMING:  base_rate = 15; break;
    case WEAPON_NOVA:    base_rate = 18; break;
    default:             base_rate = 8; break;
    }
    {
        int rate = base_rate + (int)rand_range(4) - rarity;
        if (rate < 1) rate = 1;
        out->stat2 = (u8)rate;
    }

    out->prefix_id = (u8)rand_range(PREFIX_COUNT);
    out->suffix_id = (u8)rand_range(SUFFIX_COUNT);
    out->level = (u8)(tier > 0 ? tier * 2 - 1 : 1);
    out->flags = 0;
}

void loot_generate_armor(LootItem* out, int tier, int rarity_floor, int player_lck) {
    memset(out, 0, sizeof(LootItem));

    int rarity = ensure_rarity(rarity_floor, player_lck);

    out->type = LOOT_MAKE_TYPE(LOOT_CAT_ARMOR, (u8)rand_range(ARMOR_TYPE_COUNT));
    out->rarity = (u8)rarity;

    /* stat1 = DEF bonus, scales with tier and rarity */
    int def = 1 + tier + rarity * 2 + (int)rand_range(3);
    if (def > 255) def = 255;
    out->stat1 = (u8)def;

    /* stat2 = passive type (matches armor subtype for now) */
    out->stat2 = (u8)LOOT_SUBTYPE(out->type);

    out->prefix_id = (u8)rand_range(PREFIX_COUNT);
    out->suffix_id = (u8)rand_range(SUFFIX_COUNT);
    out->level = (u8)(tier > 0 ? tier * 2 - 1 : 1);
    out->flags = 0;
}

void loot_generate_accessory(LootItem* out, int tier, int rarity_floor, int player_lck) {
    memset(out, 0, sizeof(LootItem));

    int rarity = ensure_rarity(rarity_floor, player_lck);

    out->type = LOOT_MAKE_TYPE(LOOT_CAT_ACCESSORY, (u8)rand_range(ACC_TYPE_COUNT));
    out->rarity = (u8)rarity;

    /* stat1 = bonus value, scales with tier and rarity */
    int val = 1 + tier + rarity + (int)rand_range(3);
    if (val > 255) val = 255;
    out->stat1 = (u8)val;

    /* stat2 = bonus type (matches accessory subtype) */
    out->stat2 = (u8)LOOT_SUBTYPE(out->type);

    out->prefix_id = (u8)rand_range(PREFIX_COUNT);
    out->suffix_id = (u8)rand_range(SUFFIX_COUNT);
    out->level = (u8)(tier > 0 ? tier * 2 - 1 : 1);
    out->flags = 0;
}

void loot_generate_any(LootItem* out, int tier, int rarity_floor, int player_lck) {
    /* 60% weapon, 25% armor, 15% accessory */
    int roll = (int)rand_range(100);
    if (roll < 60) {
        loot_generate(out, tier, rarity_floor, player_lck);
    } else if (roll < 85) {
        loot_generate_armor(out, tier, rarity_floor, player_lck);
    } else {
        loot_generate_accessory(out, tier, rarity_floor, player_lck);
    }
}

static char name_buf[48];

const char* loot_get_name(const LootItem* item) {
    const char* prefix = loot_prefixes[item->prefix_id % PREFIX_COUNT];
    const char* type;
    int cat = LOOT_CATEGORY(item->type);
    int sub = LOOT_SUBTYPE(item->type);

    switch (cat) {
    case LOOT_CAT_ARMOR:
        type = armor_type_names[sub % ARMOR_TYPE_COUNT];
        break;
    case LOOT_CAT_ACCESSORY:
        type = acc_type_names[sub % ACC_TYPE_COUNT];
        break;
    default:
        type = weapon_type_names[sub % WEAPON_TYPE_COUNT];
        break;
    }

    const char* suffix = loot_suffixes[item->suffix_id % SUFFIX_COUNT];

    int pos = 0;
    for (int i = 0; prefix[i] && pos < 12; i++) name_buf[pos++] = prefix[i];
    if (pos < 47) name_buf[pos++] = ' ';
    for (int i = 0; type[i] && pos < 22; i++) name_buf[pos++] = type[i];
    if (pos < 47) name_buf[pos++] = ' ';
    for (int i = 0; suffix[i] && pos < 47; i++) name_buf[pos++] = suffix[i];
    name_buf[pos] = '\0';
    return name_buf;
}

const char* loot_get_rarity_name(int rarity) {
    if (rarity < 0 || rarity >= RARITY_COUNT) return "";
    return rarity_names[rarity];
}

int loot_sell_value(const LootItem* item) {
    static const u8 rarity_mult[RARITY_COUNT] = { 1, 2, 4, 8, 16, 32 };
    int r = item->rarity;
    if (r >= RARITY_COUNT) r = RARITY_COUNT - 1;
    return (int)item->stat1 * rarity_mult[r] / 2 + 1;
}

int loot_salvage_value(const LootItem* item) {
    /* Common=1, Uncommon=3, Rare=8, Epic=20, Legendary=40, Mythic=80 */
    static const u8 shard_values[RARITY_COUNT] = { 1, 3, 8, 20, 40, 80 };
    int r = item->rarity;
    if (r >= RARITY_COUNT) r = RARITY_COUNT - 1;
    return shard_values[r];
}

int loot_compare(const LootItem* a, const LootItem* b) {
    int cat_a = LOOT_CATEGORY(a->type);
    int cat_b = LOOT_CATEGORY(b->type);
    /* Can only compare same category */
    if (cat_a != cat_b) return 0;

    if (cat_a == LOOT_CAT_WEAPON) {
        int dps_a = a->stat1 * 100 / (a->stat2 > 0 ? a->stat2 : 1);
        int dps_b = b->stat1 * 100 / (b->stat2 > 0 ? b->stat2 : 1);
        return dps_a - dps_b;
    }
    /* Armor/Accessory: compare stat1 */
    return (int)a->stat1 - (int)b->stat1;
}

int loot_equipped_set_count(int set_id) {
    if (set_id <= SET_NONE || set_id >= SET_COUNT) return 0;
    int count = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        LootItem* item = inventory_get(i);
        if (item && (item->flags & LOOT_FLAG_EQUIPPED)) {
            if (LOOT_SET_ID(item->flags) == set_id) {
                count++;
            }
        }
    }
    return count;
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
        if (inv_used[i] && (inv_items[i].flags & LOOT_FLAG_EQUIPPED)
            && LOOT_CATEGORY(inv_items[i].type) == LOOT_CAT_WEAPON) {
            return &inv_items[i];
        }
    }
    return NULL;
}

LootItem* inventory_get_equipped_armor(void) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i] && (inv_items[i].flags & LOOT_FLAG_EQUIPPED)
            && LOOT_CATEGORY(inv_items[i].type) == LOOT_CAT_ARMOR) {
            return &inv_items[i];
        }
    }
    return NULL;
}

LootItem* inventory_get_equipped_accessory(void) {
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i] && (inv_items[i].flags & LOOT_FLAG_EQUIPPED)
            && LOOT_CATEGORY(inv_items[i].type) == LOOT_CAT_ACCESSORY) {
            return &inv_items[i];
        }
    }
    return NULL;
}

void inventory_equip(int idx) {
    if (idx < 0 || idx >= INVENTORY_SIZE || !inv_used[idx]) return;

    /* Unequip previously equipped item in same category (weapon/armor/accessory) */
    int cat = LOOT_CATEGORY(inv_items[idx].type);
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i] && (inv_items[i].flags & LOOT_FLAG_EQUIPPED) &&
            LOOT_CATEGORY(inv_items[i].type) == cat) {
            inv_items[i].flags &= (u8)~LOOT_FLAG_EQUIPPED;
        }
    }
    inv_items[idx].flags |= LOOT_FLAG_EQUIPPED;
    audio_play_sfx(SFX_PICKUP);
    hud_notify("EQUIPPED!", 30);

    /* Check ACH_FULL_SET: any set has 3 equipped pieces */
    {
        int set_id = LOOT_SET_ID(inv_items[idx].flags);
        if (set_id > SET_NONE && set_id < SET_COUNT) {
            if (loot_equipped_set_count(set_id) >= 3) {
                ach_unlock_celebrate(ACH_FULL_SET);
            }
        }
    }
}

int inventory_count(void) {
    int count = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inv_used[i]) count++;
    }
    return count;
}

/* ---- Crafting ---- */

int craft_fuse(int idx1, int idx2, int idx3) {
    LootItem* a = inventory_get(idx1);
    LootItem* b = inventory_get(idx2);
    LootItem* c = inventory_get(idx3);
    if (!a || !b || !c) return 0;
    if (idx1 == idx2 || idx1 == idx3 || idx2 == idx3) return 0;

    /* All must be same rarity */
    if (a->rarity != b->rarity || b->rarity != c->rarity) return 0;
    /* Can't fuse Mythic */
    if (a->rarity >= RARITY_LEGENDARY) return 0;

    /* Generate new item at next rarity */
    int new_rarity = a->rarity + 1;
    int tier = (int)a->level / 2 + 1;
    LootItem result;
    loot_generate_any(&result, tier, new_rarity, 0);
    result.rarity = (u8)new_rarity;

    /* Unequip consumed items before removal */
    b->flags &= (u8)~LOOT_FLAG_EQUIPPED;
    c->flags &= (u8)~LOOT_FLAG_EQUIPPED;

    /* Remove consumed items, place result */
    u8 was_equipped = a->flags & LOOT_FLAG_EQUIPPED;
    inventory_remove(idx2);
    inventory_remove(idx3);
    inv_items[idx1] = result;
    if (was_equipped) inv_items[idx1].flags |= LOOT_FLAG_EQUIPPED;

    audio_play_sfx(SFX_CRAFT_SUCCESS);
    hud_notify("FUSED!", 60);
    if (game_stats.items_crafted < 65535) game_stats.items_crafted++;
    if (game_stats.items_crafted >= 10) ach_unlock_celebrate(ACH_CRAFTSMAN);
    if (new_rarity >= RARITY_LEGENDARY) ach_unlock_celebrate(ACH_MASTER_CRAFTER);
    return 1;
}

int craft_salvage(int idx) {
    LootItem* item = inventory_get(idx);
    if (!item) return 0;
    if (item->flags & LOOT_FLAG_EQUIPPED) return 0; /* Can't salvage equipped */

    int shards = loot_salvage_value(item);
    inventory_remove(idx);
    audio_play_sfx(SFX_CRAFT_SUCCESS);
    hud_notify("SALVAGED!", 60);
    return shards;
}

int craft_forge(int idx, u16* shards) {
    LootItem* item = inventory_get(idx);
    if (!item) return 0;

    /* Cost: 5 + rarity * 5 shards */
    int cost = 5 + item->rarity * 5;
    if (*shards < (u16)cost) return 0;

    *shards -= (u16)cost;

    /* Reroll stats but keep type, rarity, level */
    u8 old_type = item->type;
    u8 old_rarity = item->rarity;
    u8 old_level = item->level;
    u8 old_flags = item->flags;
    int tier = old_level / 2 + 1;

    int cat = LOOT_CATEGORY(old_type);
    switch (cat) {
    case LOOT_CAT_ARMOR:
        loot_generate_armor(item, tier, old_rarity, 0);
        break;
    case LOOT_CAT_ACCESSORY:
        loot_generate_accessory(item, tier, old_rarity, 0);
        break;
    default:
        loot_generate(item, tier, old_rarity, 0);
        break;
    }

    /* Restore type, rarity, level, flags */
    item->type = old_type;
    item->rarity = old_rarity;
    item->level = old_level;
    item->flags = old_flags;

    /* Recalculate stat2 for weapons — loot_generate used a random subtype */
    if (cat == LOOT_CAT_WEAPON) {
        int sub = LOOT_SUBTYPE(old_type);
        int base_rate;
        switch (sub) {
        case WEAPON_RAPID:   base_rate = 4; break;
        case WEAPON_SPREAD:  base_rate = 12; break;
        case WEAPON_CHARGER: base_rate = 20; break;
        case WEAPON_BEAM:    base_rate = 2; break;
        case WEAPON_LASER:   base_rate = 3; break;
        case WEAPON_HOMING:  base_rate = 15; break;
        case WEAPON_NOVA:    base_rate = 18; break;
        default:             base_rate = 8; break;
        }
        int rate = base_rate + (int)rand_range(4) - old_rarity;
        if (rate < 1) rate = 1;
        item->stat2 = (u8)rate;
    }

    audio_play_sfx(SFX_CRAFT_SUCCESS);
    hud_notify("FORGED!", 60);
    if (game_stats.items_crafted < 65535) game_stats.items_crafted++;
    if (game_stats.items_crafted >= 10) ach_unlock_celebrate(ACH_CRAFTSMAN);
    return 1;
}
