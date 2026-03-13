#ifndef GAME_SHOP_H
#define GAME_SHOP_H

#include <tonc.h>

/*
 * Ghost Protocol — Shop System
 *
 * Buy stat upgrades, consumable programs, sell loot.
 */

/* Shop item types */
enum {
    SHOP_ATK_UP = 0,
    SHOP_DEF_UP,
    SHOP_SPD_UP,
    SHOP_LCK_UP,
    SHOP_SHIELD_PROG,   /* Temporary damage reduction */
    SHOP_SPEED_PROG,    /* Temporary speed boost */
    SHOP_HEALTH_PACK,   /* Restore 20 HP instantly */
    SHOP_SHIELD_CHARGE, /* Temp +5 DEF for 120 frames */
    SHOP_CD_RESET,      /* Reset all ability cooldowns */
    SHOP_ITEM_COUNT
};

typedef struct {
    const char* name;
    u16 base_cost;      /* Cost increases with purchases */
    u8  type;
} ShopItem;

/* Initialize the shop. */
void shop_init(void);

/* Draw the shop UI. */
void shop_draw(int cursor);

/* Attempt to buy item at cursor. Returns 1 if purchased. */
int shop_buy(int cursor);

/* Sell inventory item at index. Returns credit value or 0 if failed. */
int shop_sell(int inv_idx);

/* Get current price for item (scales with purchases). */
int shop_get_price(int item_idx);

/* Get/set purchase counts for save/load. */
void shop_get_purchases(u8* out, int max);
void shop_set_purchases(const u8* in, int max);

#endif /* GAME_SHOP_H */
