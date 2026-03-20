/*
 * Ghost Protocol — Shop System
 */
#include "game/shop.h"
#include "game/terminal.h"
#include "game/player.h"
#include "game/loot.h"
#include "game/hud.h"
#include "game/common.h"
#include "engine/text.h"
#include "engine/audio.h"

static const ShopItem shop_items[SHOP_ITEM_COUNT] = {
    { "Health Pack",    15 },   /* Restore 20 HP — instant */
    { "CD Reset",       40 },   /* Reset skill cooldown — instant */
    { "XP Booster",     60 },   /* +15% XP gain — charge-based */
    { "Shield Chg",     25 },   /* +5 DEF — charge-based */
    { "Credit Find",    50 },   /* +15% credit drops — charge-based */
    { "Loot Magnet",    50 },   /* Increased rare drops — charge-based */
};

static u8 purchase_count[SHOP_ITEM_COUNT];
static u8 buff_charges[CHARGE_TYPE_COUNT];

void shop_init(void) {
    int i;
    for (i = 0; i < SHOP_ITEM_COUNT; i++) {
        purchase_count[i] = 0;
    }
    for (i = 0; i < CHARGE_TYPE_COUNT; i++) {
        buff_charges[i] = 0;
    }
}

int shop_get_price(int item_idx) {
    if (item_idx < 0 || item_idx >= SHOP_ITEM_COUNT) return 9999;
    return (int)shop_items[item_idx].base_cost;
}

void shop_draw(int cursor) {
    int i;
    terminal_print_pal(2, 1, ">> NETRUNNER SHOP <<", TPAL_AMBER);

    for (i = 0; i < SHOP_ITEM_COUNT; i++) {
        int row = 3 + i * 2;

        /* Cursor indicator */
        if (i == cursor) {
            text_print(3, row, ">");
        } else {
            text_print(3, row, " ");
        }

        text_print(5, row, shop_items[i].name);

        /* Charge-based items show charge count and "MAX" when full */
        if (i >= SHOP_CD_RESET + 1) {
            /* Charge item: SHOP_XP_BOOSTER onward */
            int ci = i - (SHOP_CD_RESET + 1); /* charge index */
            int charges = (int)buff_charges[ci];
            if (charges >= SHOP_CHARGE_MAX) {
                text_print(20, row, "[MAX]");
            } else {
                /* Show [N/5] charge count */
                text_print(20, row, "[");
                text_print_int(21, row, charges);
                text_print(22, row, "/5]");

                /* Show price */
                {
                    int price = shop_get_price(i);
                    int digits = 1;
                    int tmp = price;
                    int pcol;
                    while (tmp >= 10) { digits++; tmp /= 10; }
                    pcol = 27 - digits;
                    if (pcol < 26) pcol = 26;
                    text_print_int(pcol, row, price);
                    text_print(27, row, "cr");
                }
            }
        } else {
            /* Instant consumable: show price */
            int price = shop_get_price(i);
            int digits = 1;
            int tmp = price;
            int pcol;
            while (tmp >= 10) { digits++; tmp /= 10; }
            pcol = 27 - digits;
            if (pcol < 26) pcol = 26;
            text_print_int(pcol, row, price);
            text_print(27, row, "cr");
        }
    }

    /* Player credits */
    text_print(2, 17, "Credits:");
    text_print_int(11, 17, (int)player_state.credits);

    text_print(2, 18, "A:Buy  B:Back");
}

int shop_buy(int cursor) {
    if (cursor < 0 || cursor >= SHOP_ITEM_COUNT) return 0;

    /* Check if charge item is already at max */
    if (cursor >= SHOP_CD_RESET + 1) {
        int ci = cursor - (SHOP_CD_RESET + 1);
        if (buff_charges[ci] >= SHOP_CHARGE_MAX) return 0;
    }

    int price = shop_get_price(cursor);
    if ((int)player_state.credits < price) return 0;

    player_state.credits -= (u32)price;
    if (purchase_count[cursor] < 255) purchase_count[cursor]++;

    switch (cursor) {
    case SHOP_HEALTH_PACK:
        /* Restore 20 HP instantly */
        player_state.hp += 20;
        if (player_state.hp > player_state.max_hp)
            player_state.hp = player_state.max_hp;
        hud_notify("HP +20!", 45);
        break;

    case SHOP_CD_RESET:
        /* Reset slotted skill cooldown */
        player_state.skill_cooldown = 0;
        hud_notify("CD RESET!", 45);
        break;

    case SHOP_XP_BOOSTER:
        buff_charges[CHARGE_XP_BOOSTER]++;
        hud_notify("XP Boost!", 45);
        break;

    case SHOP_SHIELD_CHARGE:
        buff_charges[CHARGE_SHIELD]++;
        /* Apply +5 DEF immediately from recomputed base to prevent stacking */
        player_recompute_stats();
        player_state.def += 5;
        hud_notify("DEF +5!", 45);
        break;

    case SHOP_CREDIT_FINDER:
        buff_charges[CHARGE_CREDIT_FINDER]++;
        hud_notify("Credits+!", 45);
        break;

    case SHOP_LOOT_MAGNET:
        buff_charges[CHARGE_LOOT_MAGNET]++;
        hud_notify("Loot+!", 45);
        break;
    }

    audio_play_sfx(SFX_PICKUP);

    /* Check millionaire achievement on credit accumulation */
    if (player_state.credits + (u32)price >= 10000) {
        ach_unlock_celebrate(ACH_MILLIONAIRE);
    }

    return 1;
}

int shop_sell(int inv_idx) {
    LootItem* item = inventory_get(inv_idx);
    if (!item) return 0;
    if (item->flags & LOOT_FLAG_EQUIPPED) return 0; /* Can't sell equipped */

    int value = loot_sell_value(item);
    player_state.credits += (u32)value;

    inventory_remove(inv_idx);
    audio_play_sfx(SFX_PICKUP);

    /* Sell notification with credit value */
    {
        static char sell_buf[12];
        int v = value;
        int digits = 0;
        char tmp[6];
        sell_buf[0] = '+';
        do { tmp[digits++] = (char)('0' + v % 10); v /= 10; } while (v > 0);
        {
            int j;
            for (j = 0; j < digits; j++) sell_buf[1 + j] = tmp[digits - 1 - j];
        }
        sell_buf[1 + digits] = 'c';
        sell_buf[2 + digits] = 'r';
        sell_buf[3 + digits] = '\0';
        hud_notify(sell_buf, 30);
    }

    /* Check millionaire achievement */
    if (player_state.credits >= 10000) {
        ach_unlock_celebrate(ACH_MILLIONAIRE);
    }

    return value;
}

void shop_get_purchases(u8* out, int max) {
    int i;
    for (i = 0; i < max && i < SHOP_ITEM_COUNT; i++) {
        out[i] = purchase_count[i];
    }
}

void shop_set_purchases(const u8* in, int max) {
    int i;
    for (i = 0; i < max && i < SHOP_ITEM_COUNT; i++) {
        purchase_count[i] = in[i];
    }
}

int shop_get_charges(int charge_type) {
    if (charge_type < 0 || charge_type >= CHARGE_TYPE_COUNT) return 0;
    return (int)buff_charges[charge_type];
}

void shop_consume_charges(void) {
    int i;
    for (i = 0; i < CHARGE_TYPE_COUNT; i++) {
        if (buff_charges[i] > 0) buff_charges[i]--;
    }
}

int shop_buff_active(int charge_type) {
    if (charge_type < 0 || charge_type >= CHARGE_TYPE_COUNT) return 0;
    return buff_charges[charge_type] > 0;
}

void shop_get_charges_array(u8* out4) {
    int i;
    for (i = 0; i < CHARGE_TYPE_COUNT; i++) {
        out4[i] = buff_charges[i];
    }
}

void shop_set_charges_array(const u8* in4) {
    int i;
    for (i = 0; i < CHARGE_TYPE_COUNT; i++) {
        buff_charges[i] = in4[i];
    }
}
