#ifndef GAME_SHOP_H
#define GAME_SHOP_H

#include <tonc.h>

/*
 * Ghost Protocol — Shop System
 *
 * 2 instant consumables + 4 charge-based buffs.
 * Charges consumed on mission end/death/exit.
 */

/* Shop item types */
enum {
    SHOP_HEALTH_PACK = 0,   /* Restore 20 HP — instant */
    SHOP_CD_RESET,          /* Reset skill cooldown — instant */
    SHOP_XP_BOOSTER,        /* +15% XP gain — charge-based */
    SHOP_SHIELD_CHARGE,     /* +5 DEF — charge-based */
    SHOP_CREDIT_FINDER,     /* +15% credit drops — charge-based */
    SHOP_LOOT_MAGNET,       /* Increased rare drops — charge-based */
    SHOP_ITEM_COUNT
};

#define SHOP_CHARGE_MAX 5

/* Charge indices (for buff_charges[] array) */
#define CHARGE_XP_BOOSTER    0
#define CHARGE_SHIELD        1
#define CHARGE_CREDIT_FINDER 2
#define CHARGE_LOOT_MAGNET   3
#define CHARGE_TYPE_COUNT    4

typedef struct {
    const char* name;
    u16 base_cost;
} ShopItem;

/* Initialize the shop. */
void shop_init(void);

/* Draw the shop UI. */
void shop_draw(int cursor);

/* Attempt to buy item at cursor. Returns 1 if purchased. */
int shop_buy(int cursor);

/* Sell inventory item at index. Returns credit value or 0 if failed. */
int shop_sell(int inv_idx);

/* Get current price for item (fixed, no scaling). */
int shop_get_price(int item_idx);

/* Get/set purchase counts for save/load. */
void shop_get_purchases(u8* out, int max);
void shop_set_purchases(const u8* in, int max);

/* Get current charge count for a charge type. */
int shop_get_charges(int charge_type);

/* Consume one charge of each active buff. Called on mission end/death/exit. */
void shop_consume_charges(void);

/* Check if a charge-based buff is active (charges > 0). */
int shop_buff_active(int charge_type);

/* Pack/unpack charge array for save serialization. */
void shop_get_charges_array(u8* out4);  /* Writes 4 bytes */
void shop_set_charges_array(const u8* in4);  /* Reads 4 bytes */

#endif /* GAME_SHOP_H */
