/*
 * Ghost Protocol — Shop System
 */
#include "game/shop.h"
#include "game/terminal.h"
#include "game/player.h"
#include "game/loot.h"
#include "engine/text.h"
#include "engine/audio.h"

static const ShopItem shop_items[SHOP_ITEM_COUNT] = {
    { "ATK +1",      50, SHOP_ATK_UP },
    { "DEF +1",      50, SHOP_DEF_UP },
    { "SPD +1",      50, SHOP_SPD_UP },
    { "LCK +1",      50, SHOP_LCK_UP },
    { "Repair Prog", 30, SHOP_SHIELD_PROG },
    { "Overclock",   40, SHOP_SPEED_PROG },
};

static u8 purchase_count[SHOP_ITEM_COUNT];

void shop_init(void) {
    for (int i = 0; i < SHOP_ITEM_COUNT; i++) {
        purchase_count[i] = 0;
    }
}

int shop_get_price(int item_idx) {
    if (item_idx < 0 || item_idx >= SHOP_ITEM_COUNT) return 9999;
    /* Price increases by 25% per purchase for stat upgrades */
    int base = shop_items[item_idx].base_cost;
    int count = purchase_count[item_idx];
    if (item_idx < 4 || item_idx == SHOP_SPEED_PROG) {
        /* Stat upgrades and Overclock (+5 max HP): cost scales */
        return base + base * count / 4;
    }
    return base; /* Repair Prog: fixed price */
}

void shop_draw(int cursor) {
    terminal_print_pal(2, 1, ">> UPGRADE SHOP <<", TPAL_AMBER);

    for (int i = 0; i < SHOP_ITEM_COUNT; i++) {
        int price = shop_get_price(i);
        if (i == cursor) {
            text_print(3, 3 + i * 2, ">");
        } else {
            text_print(3, 3 + i * 2, " ");
        }
        text_print(5, 3 + i * 2, shop_items[i].name);

        /* Price display — right-aligned against "cr" at col 25 */
        {
            int digits = 1;
            int tmp = price;
            while (tmp >= 10) { digits++; tmp /= 10; }
            int pcol = 25 - digits;
            if (pcol < 16) pcol = 16;
            text_print_int(pcol, 3 + i * 2, price);
            text_print(25, 3 + i * 2, "cr");
        }
    }

    /* Player credits */
    text_print(2, 17, "Credits:");
    text_print_int(11, 17, player_state.credits);

    text_print(2, 18, "A:Buy  B:Back");
}

int shop_buy(int cursor) {
    if (cursor < 0 || cursor >= SHOP_ITEM_COUNT) return 0;

    int price = shop_get_price(cursor);
    if (player_state.credits < (u16)price) return 0;

    player_state.credits -= (u16)price;
    if (purchase_count[cursor] < 255) purchase_count[cursor]++;

    switch (cursor) {
    case SHOP_ATK_UP: if (player_state.atk < 999) player_state.atk += 1; break;
    case SHOP_DEF_UP: if (player_state.def < 999) player_state.def += 1; break;
    case SHOP_SPD_UP: if (player_state.spd < 999) player_state.spd += 1; break;
    case SHOP_LCK_UP: if (player_state.lck < 999) player_state.lck += 1; break;
    case SHOP_SHIELD_PROG:
        /* Heal to full HP (useful between missions) */
        player_state.hp = player_state.max_hp;
        break;
    case SHOP_SPEED_PROG:
        /* Permanent +5 max HP boost */
        player_state.max_hp += 5;
        player_state.hp += 5;
        break;
    }

    audio_play_sfx(SFX_PICKUP);
    return 1;
}

int shop_sell(int inv_idx) {
    LootItem* item = inventory_get(inv_idx);
    if (!item) return 0;
    if (item->flags & LOOT_FLAG_EQUIPPED) return 0; /* Can't sell equipped */

    int value = loot_sell_value(item);
    u32 total = (u32)player_state.credits + (u32)value;
    player_state.credits = (total > 0xFFFF) ? (u16)0xFFFF : (u16)total;

    inventory_remove(inv_idx);
    audio_play_sfx(SFX_PICKUP);
    return value;
}

void shop_get_purchases(u8* out, int max) {
    for (int i = 0; i < max && i < SHOP_ITEM_COUNT; i++) {
        out[i] = purchase_count[i];
    }
}

void shop_set_purchases(const u8* in, int max) {
    for (int i = 0; i < max && i < SHOP_ITEM_COUNT; i++) {
        purchase_count[i] = in[i];
    }
}
