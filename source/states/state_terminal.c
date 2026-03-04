/*
 * Ghost Protocol — Terminal Hub State
 *
 * Menu-driven cyberpunk terminal. Real-world hub for contracts, shop, stats.
 * Shows story progression, contract details, and inventory.
 */
#include <tonc.h>
#include <string.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/save.h"
#include "engine/rng.h"
#include "game/terminal.h"
#include "game/shop.h"
#include "game/quest.h"
#include "game/player.h"
#include "game/loot.h"
#include "game/bugbounty.h"
#include "game/boss.h"
#include "game/hud.h"
#include "states/state_ids.h"
#include "states/state_terminal.h"

/* Terminal sub-states */
enum {
    TSUB_MAIN = 0,
    TSUB_CONTRACTS,
    TSUB_SHOP,
    TSUB_STATS,
    TSUB_INVENTORY,
    TSUB_SAVE,
    TSUB_BUGBOUNTY,
    TSUB_HELP,
    TSUB_BB_RESULTS,
    TSUB_STORY,
};

static int sub_state;
static int cursor;
static int initialized;
static int inv_scroll;       /* Inventory scroll offset */
static int save_slot;        /* Selected save slot (0-2) */
static int save_msg_timer;   /* "Saved!" message timer */
static int help_page;        /* Selected topic index (0-8) */
static int help_mode;        /* 0=topic index, 1=topic detail */
static int last_dialogue_mission; /* Last dialogue shown (avoid re-showing) */
static int story_page;       /* Current page in story dialogue */
static const StoryDialogue* active_dialogue; /* Current story dialogue being shown */

/* ---- Save/Load helpers ---- */

/* Compile-time validation that save array sizes match game constants */
_Static_assert(sizeof(((SaveData*)0)->shop_purchases) >= SHOP_ITEM_COUNT,
               "SaveData.shop_purchases too small for SHOP_ITEM_COUNT");
_Static_assert(sizeof(((SaveData*)0)->bb_high_scores) / sizeof(u16) >= BB_TIER_COUNT,
               "SaveData.bb_high_scores too small for BB_TIER_COUNT");

static void pack_save(SaveData* sd) {
    memset(sd, 0, sizeof(SaveData));
    sd->magic = SAVE_MAGIC;
    sd->player_class = player_state.player_class;
    sd->player_level = player_state.level;
    sd->player_hp = player_state.hp;
    sd->player_max_hp = player_state.max_hp;
    sd->player_xp = player_state.xp;
    sd->player_atk = player_state.atk;
    sd->player_def = player_state.def;
    sd->player_spd = player_state.spd;
    sd->player_lck = player_state.lck;
    sd->credits = player_state.credits;
    sd->ability_unlocks = player_state.ability_unlocks;
    sd->quest_act = quest_state.current_act;
    sd->story_mission = quest_state.story_mission;
    for (int i = 0; i < 5; i++) {
        sd->boss_defeated[i] = quest_state.boss_defeated[i];
    }
    sd->contracts_completed = quest_state.contracts_completed;

    /* Pack inventory */
    int count = 0;
    u8 equipped = 0xFF;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        LootItem* item = inventory_get(i);
        if (item) {
            sd->inventory[count] = *item;
            if (item->flags & LOOT_FLAG_EQUIPPED) {
                equipped = (u8)count;
            }
            count++;
        }
    }
    sd->inventory_count = (u8)count;
    sd->equipped_idx = equipped;

    /* Pack shop state */
    shop_get_purchases(sd->shop_purchases, SHOP_ITEM_COUNT);

    /* Pack bug bounty state */
    bugbounty_pack(sd->bb_high_scores, &sd->bb_highest_unlocked, &sd->bb_total_runs);
}

static void unpack_save(const SaveData* sd) {
    player_state.player_class = sd->player_class;
    player_state.level = sd->player_level;
    player_state.hp = sd->player_hp;
    player_state.max_hp = sd->player_max_hp;
    player_state.xp = sd->player_xp;
    player_state.atk = sd->player_atk;
    player_state.def = sd->player_def;
    player_state.spd = sd->player_spd;
    player_state.lck = sd->player_lck;
    player_state.credits = sd->credits;
    player_state.ability_unlocks = sd->ability_unlocks;

    quest_state.current_act = sd->quest_act;
    quest_state.story_mission = sd->story_mission;
    for (int i = 0; i < 5; i++) {
        quest_state.boss_defeated[i] = sd->boss_defeated[i];
    }
    quest_state.contracts_completed = sd->contracts_completed;

    /* Unpack inventory */
    inventory_init();
    for (int i = 0; i < sd->inventory_count && i < INVENTORY_SIZE; i++) {
        inventory_add(&sd->inventory[i]);
    }
    if (sd->equipped_idx < INVENTORY_SIZE && sd->equipped_idx < sd->inventory_count) {
        inventory_equip(sd->equipped_idx);
    }

    /* Restore shop state */
    shop_set_purchases(sd->shop_purchases, SHOP_ITEM_COUNT);

    /* Restore bug bounty state */
    bugbounty_restore(sd->bb_high_scores, sd->bb_highest_unlocked, sd->bb_total_runs);

    /* Regenerate contracts for current level */
    quest_generate_contracts(player_state.level);
    quest_get_story_contract(player_state.level);
}

/* ---- Helpers ---- */

/* Print a string truncated to max_len visible characters */
static void safe_print(int col, int row, const char* str, int max_len) {
    for (int i = 0; i < max_len && str[i] != '\0' && col + i < 30; i++) {
        text_put_char(col + i, row, str[i]);
    }
}

/* ---- State functions ---- */

void state_terminal_reset(void) {
    initialized = 0;
}

void state_terminal_preload_slot(int slot) {
    static EWRAM_BSS SaveData sd;
    if (!save_read_slot(&sd, slot)) return;
    unpack_save(&sd);
    /* Mark as initialized so terminal_enter won't call quest_init and wipe state */
    initialized = 1;
    /* Don't replay dialogues the player already saw */
    last_dialogue_mission = (int)sd.story_mission;
}

void state_terminal_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    text_clear_all();
    terminal_init_palette();

    sub_state = TSUB_MAIN;
    cursor = 0;
    inv_scroll = 0;
    save_slot = 0;
    save_msg_timer = 0;

    /* Initialize systems on first visit */
    if (!initialized) {
        shop_init();
        quest_init();
        bugbounty_init();
        quest_generate_contracts(player_state.level);
        last_dialogue_mission = -1; /* Show M0 dialogue on first visit */
        initialized = 1;
    } else {
        /* Regenerate contracts if needed (after completing missions) */
        quest_generate_contracts(player_state.level);
    }

    /* Auto-create story contract if player qualifies */
    quest_get_story_contract(player_state.level);

    /* Auto-select story contract when available */
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        Contract* sc = &quest_state.contracts[i];
        if (sc->active && sc->story_mission > 0 && !sc->completed) {
            quest_state.active_contract_idx = (u8)i;
            break;
        }
    }

    /* Show results if just completed a bug bounty run */
    if (bb_state.run_complete) {
        sub_state = TSUB_BB_RESULTS;
    }

    /* Check for story dialogue trigger */
    if (sub_state == TSUB_MAIN) {
        int check_m = quest_state.story_mission; /* highest completed */
        if (check_m != last_dialogue_mission) {
            active_dialogue = quest_get_story_dialogue(check_m);
            if (active_dialogue) {
                sub_state = TSUB_STORY;
                story_page = 0;
                last_dialogue_mission = check_m;
                text_clear_all();
            }
        }
    }

    audio_play_music(MUS_TERMINAL);
}

static void update_main(void) {
    if (save_msg_timer > 0) save_msg_timer--;

    if (input_hit(KEY_DOWN)) {
        cursor++;
        if (cursor >= TMENU_COUNT) cursor = 0;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_UP)) {
        cursor--;
        if (cursor < 0) cursor = TMENU_COUNT - 1;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_A)) {
        audio_play_sfx(SFX_MENU_SELECT);
        switch (cursor) {
        case TMENU_CONTRACTS:
            sub_state = TSUB_CONTRACTS;
            cursor = 0;
            text_clear_all();
            break;
        case TMENU_SHOP:
            sub_state = TSUB_SHOP;
            cursor = 0;
            text_clear_all();
            break;
        case TMENU_STATS:
            sub_state = TSUB_STATS;
            text_clear_all();
            break;
        case TMENU_INVENTORY:
            sub_state = TSUB_INVENTORY;
            cursor = 0;
            inv_scroll = 0;
            text_clear_all();
            break;
        case TMENU_JACK_IN:
        {
            /* If story complete, offer bug bounty choice */
            if (bugbounty_unlocked()) {
                sub_state = TSUB_BUGBOUNTY;
                cursor = 0;
                text_clear_all();
                break;
            }
            /* Select active contract and transition */
            Contract* c = quest_get_active();
            if (!c) {
                /* Auto-select first available */
                for (int i = 0; i < MAX_CONTRACTS; i++) {
                    if (quest_state.contracts[i].active && !quest_state.contracts[i].completed) {
                        quest_state.active_contract_idx = (u8)i;
                        c = &quest_state.contracts[i];
                        break;
                    }
                }
            }
            if (c) {
                game_request_state = STATE_NET;
            } else {
                text_print(2, 17, "No contracts!");
            }
            break;
        }
        case TMENU_SAVE:
            sub_state = TSUB_SAVE;
            save_slot = 0;
            text_clear_all();
            break;
        case TMENU_HELP:
            sub_state = TSUB_HELP;
            help_page = 0;
            help_mode = 0;
            text_clear_all();
            break;
        }
    }
    /* No B-button exit — terminal is the hub. Exiting resets progress. */
}

static void update_contracts(void) {
    if (input_hit(KEY_B)) {
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }

    /* Build sorted order: story first, then side (matches draw order) */
    int sorted[MAX_CONTRACTS];
    int max_c = 0;
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        if (quest_state.contracts[i].active && quest_state.contracts[i].story_mission > 0)
            sorted[max_c++] = i;
    }
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        if (quest_state.contracts[i].active && quest_state.contracts[i].story_mission == 0)
            sorted[max_c++] = i;
    }
    if (max_c == 0) return;

    if (input_hit(KEY_DOWN) && cursor < max_c - 1) {
        cursor++;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_UP) && cursor > 0) {
        cursor--;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_A)) {
        /* Select contract using sorted order */
        if (cursor < max_c) {
            quest_state.active_contract_idx = (u8)sorted[cursor];
            audio_play_sfx(SFX_MENU_SELECT);
            text_clear_all();
        }
    }
}

static void update_shop(void) {
    if (input_hit(KEY_B)) {
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }
    if (input_hit(KEY_DOWN) && cursor < SHOP_ITEM_COUNT - 1) {
        cursor++;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_UP) && cursor > 0) {
        cursor--;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_A)) {
        if (!shop_buy(cursor)) {
            audio_play_sfx(SFX_MENU_BACK);
            hud_notify("Not enough CR!", 45);
        }
        text_clear_all(); /* Redraw prices */
    }
}

static void update_inventory(void) {
    if (input_hit(KEY_B)) {
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }

    int count = inventory_count();
    if (count == 0) return;

    if (input_hit(KEY_DOWN)) {
        cursor++;
        if (cursor >= count) cursor = count - 1;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_UP) && cursor > 0) {
        cursor--;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }

    /* Scroll if needed */
    if (cursor < inv_scroll) inv_scroll = cursor;
    if (cursor >= inv_scroll + 6) inv_scroll = cursor - 5;

    if (input_hit(KEY_A)) {
        /* Equip selected item */
        int idx = 0;
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            LootItem* item = inventory_get(i);
            if (item) {
                if (idx == cursor) {
                    inventory_equip(i);
                    audio_play_sfx(SFX_PICKUP);
                    text_clear_all();
                    break;
                }
                idx++;
            }
        }
    }

    if (input_hit(KEY_R)) {
        /* Sell selected item */
        int idx = 0;
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            LootItem* item = inventory_get(i);
            if (item) {
                if (idx == cursor) {
                    int value = shop_sell(i);
                    if (value > 0) {
                        audio_play_sfx(SFX_PICKUP);
                        /* Adjust cursor if at end */
                        int new_count = inventory_count();
                        if (cursor >= new_count && new_count > 0) cursor = new_count - 1;
                    }
                    text_clear_all();
                    break;
                }
                idx++;
            }
        }
    }
}

static void update_save(void) {
    if (input_hit(KEY_B)) {
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }

    if (input_hit(KEY_DOWN)) {
        save_slot = (save_slot + 1) % SAVE_SLOTS;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_UP)) {
        save_slot = (save_slot + SAVE_SLOTS - 1) % SAVE_SLOTS;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_A)) {
        /* Save to selected slot (EWRAM to avoid 512B on stack) */
        static EWRAM_BSS SaveData sd_save;
        pack_save(&sd_save);
        save_write_slot(&sd_save, save_slot);
        audio_play_sfx(SFX_SAVE);
        save_msg_timer = 60;
        text_clear_all();
    }
    if (input_hit(KEY_R)) {
        /* Load from selected slot (EWRAM to avoid 512B on stack) */
        static EWRAM_BSS SaveData sd_load;
        if (save_read_slot(&sd_load, save_slot)) {
            unpack_save(&sd_load);
            audio_play_sfx(SFX_PICKUP);
            save_msg_timer = 60;
            text_clear_all();
        } else {
            audio_play_sfx(SFX_MENU_BACK);
        }
    }
}

static void update_bugbounty(void) {
    if (input_hit(KEY_B)) {
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }
    /* Clamp cursor to highest unlocked tier */
    int max_tier = bb_state.highest_unlocked;
    if (max_tier >= BB_TIER_COUNT) max_tier = BB_TIER_COUNT - 1;
    if (input_hit(KEY_DOWN) && cursor < max_tier) {
        cursor++;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_UP) && cursor > 0) {
        cursor--;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_A)) {
        /* Tier lock check */
        if (!bugbounty_tier_available(cursor)) {
            audio_play_sfx(SFX_MENU_BACK);
            return;
        }
        /* Start bug bounty at selected tier */
        bugbounty_start(cursor);
        /* Create a contract for the bounty run */
        {
            int found_slot = 0;
            for (int i = 0; i < MAX_CONTRACTS; i++) {
                Contract* c = &quest_state.contracts[i];
                if (!c->active) {
                    /* Tiers 0-2: exterminate, tiers 3-4: boss hunt */
                    if (cursor >= 3) {
                        c->type = CONTRACT_BOUNTY;
                        c->kill_target = 1;
                    } else {
                        c->type = CONTRACT_EXTERMINATE;
                        c->kill_target = (u8)(10 + cursor * 5);
                    }
                    c->tier = (u8)(BB_BASE_TIER + cursor);
                    c->active = 1;
                    c->completed = 0;
                    c->reward_credits = (u16)(200 + cursor * 100);
                    c->seed = (u16)(rand_next() & 0xFFFF);
                    c->reward_rarity = bb_state.rarity_floor;
                    c->story_mission = 0;
                    c->kills = 0;
                    quest_state.active_contract_idx = (u8)i;
                    found_slot = 1;
                    break;
                }
            }
            if (!found_slot) {
                audio_play_sfx(SFX_MENU_BACK);
                return;
            }
        }
        audio_play_sfx(SFX_TRANSITION);
        game_request_state = STATE_NET;
    }
}

static void update_help(void) {
    if (help_mode == 0) {
        /* Index selection */
        if (input_hit(KEY_B)) {
            sub_state = TSUB_MAIN;
            cursor = 0;
            text_clear_all();
            return;
        }
        if (input_hit(KEY_DOWN)) {
            if (help_page < 8) { help_page++; audio_play_sfx(SFX_MENU_SELECT); }
        }
        if (input_hit(KEY_UP)) {
            if (help_page > 0) { help_page--; audio_play_sfx(SFX_MENU_SELECT); }
        }
        if (input_hit(KEY_A)) {
            help_mode = 1;
            audio_play_sfx(SFX_MENU_SELECT);
            text_clear_all();
        }
    } else {
        /* Detail view: any confirm/back returns to index */
        if (input_hit(KEY_A) || input_hit(KEY_B)) {
            help_mode = 0;
            audio_play_sfx(SFX_MENU_BACK);
            text_clear_all();
        }
    }
}

static void update_story(void) {
    if (!active_dialogue) {
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }
    if (input_hit(KEY_A) || input_hit(KEY_B)) {
        story_page++;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
        if (story_page >= active_dialogue->num_pages) {
            /* Dialogue complete */
            active_dialogue = NULL;
            sub_state = TSUB_MAIN;
            cursor = 0;
        }
    }
}

static void draw_story(void) {
    if (!active_dialogue || story_page >= active_dialogue->num_pages) return;

    /* Dark border frame */
    terminal_print_pal(2, 1, ">> INCOMING SIGNAL <<", TPAL_AMBER);

    /* Speaker-colored text */
    int pal = active_dialogue->speaker_pal[story_page];
    const char* text = active_dialogue->pages[story_page];

    /* Render multiline text (split on \n) */
    int row = 5;
    int col = 3;
    for (int i = 0; text[i] != '\0' && row < 16; i++) {
        if (text[i] == '\n') {
            row++;
            col = 3;
        } else {
            if (col < 29) {
                /* Use terminal_print_pal for colored single char */
                char buf[2] = { text[i], '\0' };
                terminal_print_pal(col, row, buf, pal);
                col++;
            }
        }
    }

    /* Page indicator */
    text_print(3, 18, "A:Next");
    text_print(18, 18, "Page");
    text_print_int(23, 18, story_page + 1);
    text_put_char(25, 18, '/');
    text_print_int(26, 18, active_dialogue->num_pages);
}

static void update_bb_results(void) {
    if (input_hit(KEY_A) || input_hit(KEY_B)) {
        bb_state.run_complete = 0;
        sub_state = TSUB_BUGBOUNTY;
        cursor = 0;
        text_clear_all();
    }
}

void state_terminal_update(void) {
    terminal_tick();
    switch (sub_state) {
    case TSUB_MAIN:       update_main(); break;
    case TSUB_CONTRACTS:  update_contracts(); break;
    case TSUB_SHOP:       update_shop(); break;
    case TSUB_INVENTORY:  update_inventory(); break;
    case TSUB_SAVE:       update_save(); break;
    case TSUB_BUGBOUNTY:  update_bugbounty(); break;
    case TSUB_HELP:       update_help(); break;
    case TSUB_BB_RESULTS: update_bb_results(); break;
    case TSUB_STORY:      update_story(); break;
    case TSUB_STATS:
        if (input_hit(KEY_B)) {
            sub_state = TSUB_MAIN;
            cursor = 0;
            text_clear_all();
        }
        break;
    }
}

/* Map contract type to palette bank for color coding */
static int contract_type_pal(int type) {
    switch (type) {
    case CONTRACT_STORY:       return TPAL_AMBER; /* Gold for story */
    case CONTRACT_BOUNTY:      return TPAL_RED;   /* Red for bounty */
    case CONTRACT_EXTERMINATE: return TPAL_RED;   /* Red for kill missions */
    case CONTRACT_SURVIVAL:    return TPAL_CYAN;  /* Cyan for survival */
    case CONTRACT_RETRIEVAL:   return TPAL_GREEN; /* Green for retrieval */
    default:                   return TPAL_GREEN;
    }
}

/* Draw persistent status bar at row 19 */
static void draw_status_bar(void) {
    static const char* const cls_short[] = { "ASL", "INF", "TEC" };
    int cls = player_state.player_class % CLASS_COUNT;
    terminal_print_pal(1, 19, cls_short[cls], TPAL_CYAN);
    text_print(5, 19, "Lv");
    text_print_int(7, 19, player_state.level);
    text_print(10, 19, "Cr:");
    text_print_int(13, 19, player_state.credits);
    text_print(19, 19, "Act:");
    text_print_int(23, 19, quest_state.current_act);
}

static void draw_main(void) {
    terminal_draw_menu(cursor);

    /* Show current story act */
    text_print(2, 13, "Act:");
    terminal_print_pal(7, 13, quest_get_act_name(quest_state.current_act), TPAL_AMBER);

    /* Show active contract indicator */
    Contract* ac = quest_get_active();
    if (ac) {
        text_print(2, 14, "Active:");
        terminal_print_pal(10, 14, quest_get_type_name(ac->type), contract_type_pal(ac->type));
    }

    if (save_msg_timer > 0) {
        terminal_print_pal(2, 16, "Saved!", TPAL_CYAN);
    }

    draw_status_bar();
}

static void draw_contracts(void) {
    terminal_print_pal(2, 1, ">> AVAILABLE CONTRACTS <<", TPAL_AMBER);

    /* Story briefing for next mission */
    int next_m = quest_state.story_mission + 1;
    if (next_m <= STORY_MISSIONS) {
        terminal_print_pal(2, 2, quest_get_story_brief(next_m), TPAL_CYAN);
    } else {
        terminal_print_pal(2, 2, "All missions complete.", TPAL_CYAN);
    }

    /* Build sorted order: story contracts first, then side contracts */
    int sorted[MAX_CONTRACTS];
    int sorted_count = 0;
    /* First pass: story contracts */
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        if (quest_state.contracts[i].active && quest_state.contracts[i].story_mission > 0)
            sorted[sorted_count++] = i;
    }
    /* Second pass: side contracts */
    for (int i = 0; i < MAX_CONTRACTS; i++) {
        if (quest_state.contracts[i].active && quest_state.contracts[i].story_mission == 0)
            sorted[sorted_count++] = i;
    }

    int row = 4;
    int idx = 0;
    for (int si = 0; si < sorted_count && row < 16; si++) {
        int i = sorted[si];
        Contract* c = &quest_state.contracts[i];

        /* Cursor */
        text_print(2, row, (idx == cursor) ? ">" : " ");

        /* Active indicator */
        if (i == quest_state.active_contract_idx) {
            text_print(3, row, "*");
        }

        /* Story tag or type — color-coded */
        if (c->story_mission > 0) {
            int act = quest_mission_to_act(c->story_mission);
            terminal_print_pal(5, row, "STORY", TPAL_AMBER);
            safe_print(11, row, quest_get_act_name(act), 8);
            /* Show boss name for boss missions */
            if (quest_is_boss_mission(c->story_mission)) {
                int boss_idx = c->story_mission / MISSIONS_PER_ACT - 1;
                if (boss_idx >= 0 && boss_idx < BOSS_TYPE_COUNT) {
                    row++;
                    terminal_print_pal(5, row, "Boss:", TPAL_RED);
                    safe_print(11, row, boss_get_name(boss_idx), 14);
                }
            }
        } else {
            int cpal = contract_type_pal(c->type);
            terminal_print_pal(5, row, quest_get_type_name(c->type), cpal);
        }

        /* Tier and reward */
        text_print(20, row, "T");
        text_print_int(21, row, c->tier);
        row++;

        /* Detail line */
        text_print(5, row, "Reward:");
        text_print_int(13, row, c->reward_credits);
        terminal_print_pal(18, row, "cr", TPAL_CYAN);

        if (c->type == CONTRACT_EXTERMINATE || c->type == CONTRACT_SURVIVAL) {
            text_print(21, row, "K:");
            text_print_int(23, row, c->kill_target);
        }

        row++;
        /* Decorative separator between contracts */
        if (row < 16) {
            text_print(3, row, "..........................");
        }
        row++;
        idx++;
    }

    if (idx == 0) {
        text_print(5, 5, "No contracts available");
    }

    text_print(2, 18, "A:Select  B:Back");
    draw_status_bar();
}

static void draw_stats(void) {
    static const char* const class_names[] = { "ASSAULT", "INFILTRATOR", "TECHNOMANCER" };

    terminal_print_pal(2, 1, ">> OPERATOR STATS <<", TPAL_AMBER);
    text_print(2, 3, "Class:");
    text_print(10, 3, class_names[player_state.player_class % CLASS_COUNT]);

    text_print(2, 5, "Level:");
    text_print_int(10, 5, player_state.level);

    text_print(2, 7, "HP:");
    text_print_int(7, 7, player_state.hp);
    text_print(11, 7, "/");
    text_print_int(12, 7, player_state.max_hp);

    text_print(2, 9, "ATK:");
    text_print_int(7, 9, player_state.atk);
    text_print(15, 9, "DEF:");
    text_print_int(20, 9, player_state.def);
    text_print(2, 10, "SPD:");
    text_print_int(7, 10, player_state.spd);
    text_print(15, 10, "LCK:");
    text_print_int(20, 10, player_state.lck);

    text_print(2, 12, "Credits:");
    text_print_int(11, 12, player_state.credits);

    text_print(2, 13, "XP:");
    text_print_int(6, 13, player_state.xp);
    text_print(12, 13, "/");
    text_print_int(13, 13, player_xp_to_next());

    /* Story progress */
    text_print(2, 15, "Mission:");
    text_print_int(11, 15, quest_state.story_mission);
    text_print(14, 15, "/20 Act:");
    text_print_int(22, 15, quest_state.current_act);

    /* Bosses defeated */
    int boss_count = 0;
    for (int i = 0; i < 5; i++) {
        if (quest_state.boss_defeated[i]) boss_count++;
    }
    text_print(2, 16, "Bosses:");
    text_print_int(10, 16, boss_count);
    text_print(11, 16, "/5");

    text_print(2, 18, "B:Back");
    draw_status_bar();
}

static void draw_inventory(void) {
    terminal_print_pal(2, 1, ">> INVENTORY <<", TPAL_AMBER);

    int count = inventory_count();
    if (count == 0) {
        text_print(5, 5, "Empty");
        text_print(2, 18, "B:Back");
        return;
    }

    /* Equipped weapon info */
    LootItem* equipped = inventory_get_equipped();
    if (equipped) {
        text_print(2, 2, "Equipped:");
        safe_print(12, 2, loot_get_name(equipped), 17);
    }

    /* Item list */
    int row = 4;
    int idx = 0;
    for (int i = 0; i < INVENTORY_SIZE && row < 16; i++) {
        LootItem* item = inventory_get(i);
        if (!item) continue;

        if (idx < inv_scroll) { idx++; continue; }

        /* Cursor */
        text_print(2, row, (idx == cursor) ? ">" : " ");

        /* Equipped marker */
        if (item->flags & LOOT_FLAG_EQUIPPED) {
            text_print(3, row, "E");
        }

        /* Item name (truncate to 20 chars to avoid overflow) */
        safe_print(5, row, loot_get_name(item), 20);

        row++;

        /* Stats line */
        text_print(5, row, "DMG:");
        text_print_int(9, row, item->stat1);
        text_print(13, row, "SPD:");
        text_print_int(17, row, item->stat2);
        text_print(21, row, loot_get_rarity_name(item->rarity));

        row += 2;
        idx++;
    }

    /* Scroll indicators */
    if (inv_scroll > 0) {
        text_print(28, 4, "^");
    }
    if (idx < count) {
        text_print(28, 15, "v");
    }

    text_print(2, 17, "Items:");
    text_print_int(9, 17, count);
    text_print(12, 17, "/");
    text_print_int(13, 17, INVENTORY_SIZE);
    /* Show sell value for selected item */
    {
        int sidx = 0;
        for (int i = 0; i < INVENTORY_SIZE; i++) {
            LootItem* item = inventory_get(i);
            if (!item) continue;
            if (sidx == cursor) {
                if (!(item->flags & LOOT_FLAG_EQUIPPED)) {
                    int val = loot_sell_value(item);
                    text_print(17, 17, "Sell:");
                    text_print_int(22, 17, val);
                    text_print(25, 17, "cr");
                }
                break;
            }
            sidx++;
        }
    }
    text_print(2, 18, "A:Equip R:Sell B:Back");
    draw_status_bar();
}

static void draw_save(void) {
    terminal_print_pal(2, 1, ">> SAVE / LOAD <<", TPAL_AMBER);

    for (int i = 0; i < SAVE_SLOTS; i++) {
        int row = 3 + i * 4;

        text_print(2, row, (i == save_slot) ? ">" : " ");
        text_print(4, row, "Slot");
        text_print_int(9, row, i + 1);

        if (save_slot_exists(i)) {
            /* Preview: read slot header (EWRAM to avoid 512B on stack) */
            static EWRAM_BSS SaveData sd;
            if (save_read_slot(&sd, i)) {
                static const char* const cn[] = { "ASL", "INF", "TEC" };
                text_print(12, row, cn[sd.player_class % 3]);
                text_print(16, row, "Lv");
                text_print_int(18, row, sd.player_level);
                text_print(4, row + 1, "Act:");
                text_print_int(9, row + 1, sd.quest_act);
                text_print(12, row + 1, "Cr:");
                text_print_int(15, row + 1, sd.credits);
            }
        } else {
            text_print(12, row, "- Empty -");
        }
    }

    if (save_msg_timer > 0) {
        text_print(10, 16, "Done!");
    }

    text_print(2, 18, "A:Save R:Load B:Back");
    draw_status_bar();
}

static void draw_bugbounty(void) {
    /* Stat scale strings (x256 fixed-point → display) */
    static const char* const scale_str[BB_TIER_COUNT] = {
        "x1.5", "x1.8", "x2.0", "x2.5", "x3.0",
    };

    terminal_print_pal(2, 1, ">> BUG BOUNTY <<", TPAL_AMBER);
    terminal_print_pal(2, 2, "High-risk contracts.", TPAL_RED);

    for (int i = 0; i < BB_TIER_COUNT; i++) {
        int row = 4 + i * 2;
        text_print(2, row, (i == cursor) ? ">" : " ");

        /* Locked tiers show "???" */
        if (bugbounty_tier_available(i)) {
            text_print(4, row, bugbounty_tier_name(i));

            /* Per-tier high score */
            u16 hs = bugbounty_get_high_score(i);
            if (hs > 0) {
                text_print(19, row, "Hi:");
                text_print_int(22, row, hs);
            }
        } else {
            text_print(4, row, "???");
        }
    }

    /* Selected tier detail */
    if (bugbounty_tier_available(cursor)) {
        int rfloor = bugbounty_get_rarity_floor(cursor);
        text_print(2, 15, "Scale:");
        text_print(9, 15, scale_str[cursor]);
        text_print(15, 15, "Drop:");
        text_print(21, 15, loot_get_rarity_name(rfloor));
    }

    /* Total runs */
    text_print(2, 16, "Runs:");
    text_print_int(8, 16, bb_state.total_runs);

    text_print(2, 18, "A:Start  B:Back");
    draw_status_bar();
}

static void draw_help(void) {
    static const char* const topic_names[9] = {
        "Controls & Movement",
        "Combat & Weapons",
        "Abilities & Classes",
        "Loot & Equipment",
        "Contracts & Missions",
        "Bosses & Story",
        "Shop & Credits",
        "Bug Bounty",
        "Tips & Tricks",
    };

    if (help_mode == 0) {
        /* ---- Topic index ---- */
        terminal_print_pal(3, 1, ">> HELP & REFERENCE <<", TPAL_AMBER);
        for (int i = 0; i < 9; i++) {
            int row = 3 + i;
            text_put_char(2, row, (i == help_page) ? '>' : ' ');
            text_put_char(4, row, (char)('1' + i));
            text_put_char(5, row, '.');
            text_print(7, row, topic_names[i]);
        }
        text_print(2, 18, "UP/DN:Select  A:View  B:Back");
        draw_status_bar();
        return;
    }

    /* ---- Topic detail ---- */
    int r = 3;
    switch (help_page) {
    case 0: /* Controls & Movement */
        terminal_print_pal(2, 1, "-- Controls & Movement --", TPAL_AMBER);
        text_print(2, r++, "A:Jump");
        text_print(2, r++, "B:Shoot");
        text_print(2, r++, "D-Pad:Move left / right");
        text_print(2, r++, "R:Use ability  L:Cycle");
        text_print(2, r++, "DOWN:Fast fall / drop thru");
        text_print(2, r++, "START:Pause");
        r++;
        text_print(2, r++, "Wall Jump: jump near wall,");
        text_print(2, r++, "then press A to leap off it");
        text_print(2, r++, "in the opposite direction.");
        break;
    case 1: /* Combat & Weapons */
        terminal_print_pal(2, 1, "-- Combat & Weapons --", TPAL_AMBER);
        text_print(2, r++, "B:Shoot your equipped weapon");
        r++;
        text_print(2, r++, "Weapon types:");
        text_print(2, r++, "Buster: slow, high damage");
        text_print(2, r++, "Rapid:  fast, spread burst");
        text_print(2, r++, "Matrix: 3-way diagonal aim");
        text_print(2, r++, "Charger: hold B to charge");
        text_print(2, r++, "Beam:   continuous stream");
        r++;
        text_print(2, r++, "Kills: XP + loot");
        text_print(2, r++, "Equip in INVENTORY for +DMG");
        break;
    case 2: /* Abilities & Classes */
        terminal_print_pal(2, 1, "-- Abilities & Classes --", TPAL_AMBER);
        text_print(2, r++, "L:Cycle  R:Use  Unlock=Level");
        r++;
        text_print(2, r++, "ASSAULT (tank & punish)");
        text_print(2, r++, " L3:Charged Shot");
        text_print(2, r++, " L7:Burst  L12:Heavy Shell");
        text_print(2, r++, " L17:Overclock (2x rate)");
        text_print(2, r++, "INFILTRATOR (speed)");
        text_print(2, r++, " L3:Air Dash  L7:Phase Shot");
        text_print(2, r++, " L12:Fan Fire  L17:Overload");
        text_print(2, r++, "TECHNOMANCER (control)");
        text_print(2, r++, " L3:Turret    L7:Scan Pulse");
        text_print(2, r++, " L12:Data Shield (half dmg)");
        text_print(2, r++, " L17:Sys Crash (stun all)");
        break;
    case 3: /* Loot & Equipment */
        terminal_print_pal(2, 1, "-- Loot & Equipment --", TPAL_AMBER);
        text_print(2, r++, "Rarity tiers:");
        terminal_print_pal(2, r++, "Common < Uncommon < Rare", TPAL_GREEN);
        terminal_print_pal(2, r++, "Epic < Legendary", TPAL_AMBER);
        r++;
        text_print(2, r++, "Open INVENTORY at terminal");
        text_print(2, r++, "A:Equip weapon");
        text_print(2, r++, "R:Sell for credits");
        r++;
        text_print(2, r++, "DMG=base damage per shot");
        text_print(2, r++, "SPD=fire rate (lower=faster)");
        r++;
        text_print(2, r++, "Inventory: 20 item capacity");
        text_print(2, r++, "All enemies can drop loot");
        break;
    case 4: /* Contracts & Missions */
        terminal_print_pal(2, 1, "-- Contracts & Missions --", TPAL_AMBER);
        text_print(2, r++, "Pick contract, then JACK IN");
        r++;
        text_print(2, r++, "Contract types:");
        text_print(2, r++, "STORY:advance main plot");
        text_print(2, r++, "EXTERMINATE:kill N enemies");
        text_print(2, r++, "RETRIEVAL:reach the exit");
        text_print(2, r++, "SURVIVAL:survive N kills");
        text_print(2, r++, "BOSS:defeat named target");
        r++;
        text_print(2, r++, "Reach exit gate to extract");
        text_print(2, r++, "Fail = lose in-run loot");
        break;
    case 5: /* Bosses & Story */
        terminal_print_pal(2, 1, "-- Bosses & Story --", TPAL_AMBER);
        text_print(2, r++, "5 acts, 4 missions each");
        text_print(2, r++, "Boss on mission 4,8,12,16,20");
        r++;
        text_print(2, r++, "Boss phases:");
        text_print(2, r++, "ATTACK: learn the pattern");
        text_print(2, r++, "VULNERABLE: max damage!");
        text_print(2, r++, "Boss flashing = hit it NOW!");
        r++;
        text_print(2, r++, "Bosses enrage at <50% HP");
        r++;
        text_print(2, r++, "Level gates:");
        text_print(2, r++, "Act2:Lv7    Act3:Lv15");
        text_print(2, r++, "Act4:Lv23   Act5:Lv27");
        text_print(2, r++, "Final boss: Lv30");
        break;
    case 6: /* Shop & Credits */
        terminal_print_pal(2, 1, "-- Shop & Credits --", TPAL_AMBER);
        text_print(2, r++, "Credits (cr) = your currency");
        r++;
        text_print(2, r++, "Earn credits:");
        text_print(2, r++, " Killing enemies");
        text_print(2, r++, " Completing contracts");
        text_print(2, r++, " Selling loot in Inventory");
        r++;
        text_print(2, r++, "SHOP: A to buy");
        text_print(2, r++, "Stock refreshes each visit");
        r++;
        text_print(2, r++, "Tip: sell duplicate gear to");
        text_print(2, r++, "fund upgrades and rerolls.");
        break;
    case 7: /* Bug Bounty */
        terminal_print_pal(2, 1, "-- Bug Bounty --", TPAL_AMBER);
        text_print(2, r++, "Unlocks after story complete");
        r++;
        text_print(2, r++, "5 tiers of escalating risk:");
        text_print(2, r++, " Zero-Day       x1.5 score");
        text_print(2, r++, " Kernel Panic   x1.8 score");
        text_print(2, r++, " Buffer Overflow  x2.0");
        text_print(2, r++, " Stack Smash     x2.5 score");
        text_print(2, r++, " Ring Zero       x3.0 score");
        r++;
        text_print(2, r++, "Complete tier = unlock next");
        text_print(2, r++, "High scores saved per tier");
        text_print(2, r++, "Ring Zero: Hunters only");
        break;
    case 8: /* Tips & Tricks */
        terminal_print_pal(2, 1, "-- Tips & Tricks --", TPAL_AMBER);
        text_print(2, r++, "Wall jump: jump near wall,");
        text_print(2, r++, "then A again to leap off it");
        r++;
        text_print(2, r++, "DROP thru platforms: DOWN");
        r++;
        text_print(2, r++, "Level up heals 75% HP --");
        text_print(2, r++, "grind before tough bosses");
        r++;
        text_print(2, r++, "Data Shield: halves damage");
        text_print(2, r++, "Equip best weapon before run");
        r++;
        text_print(2, r++, "Save often: 3 SRAM slots");
        text_print(2, r++, "Higher tier = better loot");
        break;
    }
    (void)r;

    /* Topic indicator and nav hint */
    text_print(2, 17, "Topic ");
    text_put_char(8, 17, (char)('1' + help_page));
    text_put_char(9, 17, '/');
    text_put_char(10, 17, '9');
    text_print(2, 18, "B:Back to index");
    draw_status_bar();
}

static void draw_bb_results(void) {
    terminal_print_pal(4, 1, ">> RUN COMPLETE <<", TPAL_AMBER);

    text_print(4, 3, "Tier:");
    text_print(10, 3, bugbounty_tier_name(bb_state.tier));

    text_print(4, 5, "Score:");
    text_print_int(11, 5, bb_state.score);

    text_print(4, 7, "Kills:");
    text_print_int(11, 7, bb_state.kills);

    /* Check if this is the new high score (complete() already stored it) */
    u16 hs = bugbounty_get_high_score(bb_state.tier);
    if (bb_state.score >= hs && bb_state.score > 0) {
        text_print(4, 9, "** NEW HIGH SCORE! **");
    } else {
        text_print(4, 9, "Best:");
        text_print_int(10, 9, hs);
    }

    /* Credits earned */
    text_print(4, 11, "Credits: +");
    text_print_int(14, 11, 200 + bb_state.tier * 100);

    /* Show tier unlock if applicable */
    int next = bb_state.tier + 1;
    if (next < BB_TIER_COUNT && next <= bb_state.highest_unlocked) {
        text_print(4, 13, "Unlocked:");
        text_print(14, 13, bugbounty_tier_name(next));
    }

    text_print(4, 18, "A:Continue  B:Back");
    draw_status_bar();
}

void state_terminal_draw(void) {
    switch (sub_state) {
    case TSUB_MAIN:
        draw_main();
        break;
    case TSUB_CONTRACTS:
        draw_contracts();
        break;
    case TSUB_SHOP:
        shop_draw(cursor);
        break;
    case TSUB_STATS:
        draw_stats();
        break;
    case TSUB_INVENTORY:
        draw_inventory();
        break;
    case TSUB_SAVE:
        draw_save();
        break;
    case TSUB_BUGBOUNTY:
        draw_bugbounty();
        break;
    case TSUB_HELP:
        draw_help();
        break;
    case TSUB_BB_RESULTS:
        draw_bb_results();
        break;
    case TSUB_STORY:
        draw_story();
        break;
    }
}

void state_terminal_exit(void) {
    text_clear_all();
    /* Clean up blend registers to prevent dark overlay bleeding into next state */
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
