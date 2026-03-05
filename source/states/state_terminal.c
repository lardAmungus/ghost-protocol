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
#include "engine/video.h"
#include "engine/save.h"
#include "engine/rng.h"
#include "game/terminal.h"
#include "game/shop.h"
#include "game/quest.h"
#include "game/player.h"
#include "game/loot.h"
#include "game/bugbounty.h"
#include "game/boss.h"
#include "game/enemy.h"
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
    TSUB_SKILLS,
    TSUB_EVOLUTION,
    TSUB_CODEX,
    TSUB_CRAFT,
    TSUB_BRIEFING,
};

static int sub_state;
static int cursor;
static int initialized;
static int inv_scroll;       /* Inventory scroll offset */
static int save_slot;        /* Selected save slot (0-2) */
static int save_msg_timer;   /* "Saved!" message timer */
static int save_confirm;     /* 1=awaiting overwrite confirmation */
static int help_page;        /* Selected topic index (0-8) */
static int help_mode;        /* 0=topic index, 1=topic detail */
static int last_dialogue_mission; /* Last dialogue shown (avoid re-showing) */
static int story_page;       /* Current page in story dialogue */
static const StoryDialogue* active_dialogue; /* Current story dialogue being shown */
static int codex_category;   /* 0=enemies, 1=bosses, 2=weapons */
static int codex_mode;       /* 0=category list, 1=entry list */
static int stats_page;       /* 0=character, 1=combat stats */
static int boot_done;        /* Boot sequence completed flag */
static int boot_timer;       /* Boot sequence frame counter */
static int craft_mode;       /* 0=menu, 1=select items for fuse, 2=select for salvage, 3=select for forge */
static int craft_sel[3];     /* Selected inventory indices for fuse (up to 3) */
static int craft_sel_count;  /* Number of items selected for fuse */
static int choice_active;    /* 1 = showing choice menu */
static int choice_cursor;    /* 0 = option A, 1 = option B */

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
    for (int i = 0; i < 6; i++) {
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

    /* Pack skill tree & evolution */
    for (int i = 0; i < SKILL_TREE_SIZE; i++) {
        sd->skill_tree[i] = player_state.skill_tree[i];
    }
    sd->evolution = player_state.evolution;
    sd->skill_points = player_state.skill_points;
    sd->craft_shards = player_state.craft_shards;

    /* Pack game statistics */
    sd->ng_plus = game_stats.ng_plus;
    sd->choice_flags = game_stats.choice_flags;
    sd->play_time_frames = game_stats.play_time_frames;
    sd->total_kills = game_stats.total_kills;
    sd->total_deaths = game_stats.total_deaths;
    sd->damage_dealt = game_stats.damage_dealt;
    sd->damage_taken = game_stats.damage_taken;
    sd->items_found = game_stats.items_found;
    sd->items_crafted = game_stats.items_crafted;
    sd->highest_combo = game_stats.highest_combo;
    sd->contracts_done = game_stats.contracts_done;
    sd->bb_endgame_contracts = game_stats.bb_endgame_contracts;
    sd->bb_boss_contracts = game_stats.bb_boss_contracts;
    sd->bb_threat_level = game_stats.bb_threat_level;
    sd->bb_highest_level = game_stats.bb_highest_level;
    sd->endgame_unlocked = game_stats.endgame_unlocked;
    for (int i = 0; i < 3; i++) sd->achievements[i] = game_stats.achievements[i];
    for (int i = 0; i < 32; i++) sd->codex_unlocks[i] = game_stats.codex_unlocks[i];
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
    for (int i = 0; i < 6; i++) {
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

    /* Restore skill tree & evolution */
    for (int i = 0; i < SKILL_TREE_SIZE; i++) {
        player_state.skill_tree[i] = sd->skill_tree[i];
    }
    player_state.evolution = sd->evolution;
    player_state.skill_points = sd->skill_points;
    player_state.craft_shards = sd->craft_shards;

    /* Restore game statistics */
    game_stats.ng_plus = sd->ng_plus;
    game_stats.choice_flags = sd->choice_flags;
    game_stats.play_time_frames = sd->play_time_frames;
    game_stats.total_kills = sd->total_kills;
    game_stats.total_deaths = sd->total_deaths;
    game_stats.damage_dealt = sd->damage_dealt;
    game_stats.damage_taken = sd->damage_taken;
    game_stats.items_found = sd->items_found;
    game_stats.items_crafted = sd->items_crafted;
    game_stats.highest_combo = sd->highest_combo;
    game_stats.contracts_done = sd->contracts_done;
    game_stats.bb_endgame_contracts = sd->bb_endgame_contracts;
    game_stats.bb_boss_contracts = sd->bb_boss_contracts;
    game_stats.bb_threat_level = sd->bb_threat_level;
    game_stats.bb_highest_level = sd->bb_highest_level;
    game_stats.endgame_unlocked = sd->endgame_unlocked;
    for (int i = 0; i < 3; i++) game_stats.achievements[i] = sd->achievements[i];
    for (int i = 0; i < 32; i++) game_stats.codex_unlocks[i] = sd->codex_unlocks[i];

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
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;
    text_clear_all();
    terminal_init_palette();
    terminal_load_bg();

    sub_state = TSUB_MAIN;
    cursor = 0;
    inv_scroll = 0;
    save_slot = 0;
    save_msg_timer = 0;
    boot_done = 0;
    boot_timer = 0;
    choice_active = 0;
    choice_cursor = 0;
    video_fadein_start(30); /* Fade in from black over 0.5s */

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
            stats_page = 0;
            text_clear_all();
            break;
        case TMENU_INVENTORY:
            sub_state = TSUB_INVENTORY;
            cursor = 0;
            inv_scroll = 0;
            text_clear_all();
            break;
        case TMENU_SKILLS:
            /* Check if evolution is pending first */
            if (player_state.evolution_pending && player_state.evolution == EVOLUTION_NONE) {
                sub_state = TSUB_EVOLUTION;
                cursor = 0;
                text_clear_all();
            } else {
                sub_state = TSUB_SKILLS;
                cursor = 0;
                text_clear_all();
            }
            break;
        case TMENU_CODEX:
            sub_state = TSUB_CODEX;
            codex_mode = 0;
            cursor = 0;
            text_clear_all();
            break;
        case TMENU_CRAFT:
            sub_state = TSUB_CRAFT;
            craft_mode = 0;
            craft_sel_count = 0;
            cursor = 0;
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
                sub_state = TSUB_BRIEFING;
                text_clear_all();
            } else {
                text_print(2, 17, "No contracts!");
            }
            break;
        }
        case TMENU_SAVE:
            sub_state = TSUB_SAVE;
            save_slot = 0;
            save_confirm = 0;
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
        audio_play_sfx(SFX_MENU_BACK);
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
        audio_play_sfx(SFX_MENU_BACK);
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
        audio_play_sfx(SFX_MENU_BACK);
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

    /* Scroll if needed — 4 items visible (3 rows each: name+stats+gap, rows 4-15) */
    if (cursor < inv_scroll) inv_scroll = cursor;
    if (cursor >= inv_scroll + 4) inv_scroll = cursor - 3;

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
    /* Overwrite confirmation sub-state */
    if (save_confirm) {
        if (input_hit(KEY_A)) {
            /* Confirmed — save */
            static EWRAM_BSS SaveData sd_save;
            pack_save(&sd_save);
            save_write_slot(&sd_save, save_slot);
            audio_play_sfx(SFX_SAVE);
            save_msg_timer = 60;
            save_confirm = 0;
            text_clear_all();
        }
        if (input_hit(KEY_B)) {
            /* Cancel overwrite */
            audio_play_sfx(SFX_MENU_BACK);
            save_confirm = 0;
            text_clear_all();
        }
        return;
    }

    if (input_hit(KEY_B)) {
        audio_play_sfx(SFX_MENU_BACK);
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
        if (save_slot_exists(save_slot)) {
            /* Slot occupied — ask for confirmation */
            save_confirm = 1;
            audio_play_sfx(SFX_MENU_SELECT);
            text_clear_all();
        } else {
            /* Empty slot — save directly */
            static EWRAM_BSS SaveData sd_save;
            pack_save(&sd_save);
            save_write_slot(&sd_save, save_slot);
            audio_play_sfx(SFX_SAVE);
            save_msg_timer = 60;
            text_clear_all();
        }
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
        audio_play_sfx(SFX_MENU_BACK);
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
            audio_play_sfx(SFX_MENU_BACK);
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

    /* Choice menu active */
    if (choice_active) {
        if (input_hit(KEY_UP) || input_hit(KEY_DOWN)) {
            choice_cursor ^= 1;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A)) {
            /* Store choice in game_stats.choice_flags */
            int bit = active_dialogue->choice_bit;
            if (bit >= 1 && bit <= 4) {
                int shift = (bit - 1) * 2;
                /* Clear old bits, set new choice (0=A, 1=B) */
                game_stats.choice_flags = (u8)(
                    (game_stats.choice_flags & ~(3 << shift)) |
                    (choice_cursor << shift));
            }
            audio_play_sfx(SFX_TRANSITION);
            choice_active = 0;
            active_dialogue = NULL;
            sub_state = TSUB_MAIN;
            cursor = 0;
            text_clear_all();
        }
        return;
    }

    if (input_hit(KEY_A) || input_hit(KEY_B)) {
        story_page++;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
        if (story_page >= active_dialogue->num_pages) {
            /* Check for choice at end of dialogue */
            if (active_dialogue->choice_a != NULL) {
                choice_active = 1;
                choice_cursor = 0;
                text_clear_all();
            } else {
                /* Dialogue complete — no choice */
                active_dialogue = NULL;
                sub_state = TSUB_MAIN;
                cursor = 0;
            }
        }
    }
}

static void draw_story(void) {
    if (!active_dialogue) return;

    /* Choice screen */
    if (choice_active && active_dialogue->choice_a) {
        terminal_print_pal(2, 3, "== CHOOSE YOUR PATH ==", TPAL_AMBER);
        /* Option A */
        text_put_char(4, 8, choice_cursor == 0 ? '>' : ' ');
        terminal_print_pal(6, 8, active_dialogue->choice_a,
                           choice_cursor == 0 ? TPAL_CYAN : TPAL_GREEN);
        /* Option B */
        text_put_char(4, 11, choice_cursor == 1 ? '>' : ' ');
        terminal_print_pal(6, 11, active_dialogue->choice_b,
                           choice_cursor == 1 ? TPAL_CYAN : TPAL_GREEN);
        terminal_print_pal(3, 18, "[A]:Select  [UP/DN]:Switch", TPAL_GREEN);
        return;
    }

    if (story_page >= active_dialogue->num_pages) return;

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

/* Forward declaration — defined later in draw section */
static void draw_status_bar(void);

/* ---- Skill tree sub-state ---- */
static int skill_branch; /* 0=offense, 1=defense, 2=utility */

static void update_skills(void) {
    if (input_hit(KEY_B)) {
        audio_play_sfx(SFX_MENU_BACK);
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
        return;
    }
    /* LEFT/RIGHT to change branch */
    if (input_hit(KEY_LEFT) && skill_branch > 0) {
        skill_branch--;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    if (input_hit(KEY_RIGHT) && skill_branch < 2) {
        skill_branch++;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();
    }
    /* UP/DOWN to select skill in branch */
    if (input_hit(KEY_DOWN) && cursor < SKILLS_PER_BRANCH - 1) {
        cursor++;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_UP) && cursor > 0) {
        cursor--;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    /* A to allocate skill point */
    if (input_hit(KEY_A)) {
        int idx = skill_branch * SKILLS_PER_BRANCH + cursor;
        if (player_skill_allocate(idx)) {
            audio_play_sfx(SFX_MENU_SELECT);
            hud_notify("SKILL UP!", 45);
            text_clear_all();
        } else {
            audio_play_sfx(SFX_MENU_BACK);
        }
    }
}

static const char* const skill_names[CLASS_COUNT][SKILL_TREE_SIZE] = {
    [CLASS_ASSAULT] = {
        /* Offense */  "Crit Chance", "ATK Boost", "Charge Spd", "AoE Radius",
        /* Defense */  "Max HP", "DEF Boost", "HP Regen", "Resist",
        /* Utility */  "SPD Boost", "LCK Boost", "XP Gain", "Credit Gain",
    },
    [CLASS_INFILTRATOR] = {
        /* Offense */  "Crit Chance", "ATK Boost", "Fire Rate", "Pierce",
        /* Defense */  "Max HP", "DEF Boost", "Evasion", "Resist",
        /* Utility */  "SPD Boost", "LCK Boost", "XP Gain", "Credit Gain",
    },
    [CLASS_TECHNOMANCER] = {
        /* Offense */  "Crit Chance", "ATK Boost", "Proj Count", "Beam Width",
        /* Defense */  "Max HP", "DEF Boost", "Shield Dur", "Resist",
        /* Utility */  "SPD Boost", "LCK Boost", "XP Gain", "Credit Gain",
    },
};

static const char* const branch_names[3] = { "OFFENSE", "DEFENSE", "UTILITY" };

static void draw_skills(void) {
    int cls = player_state.player_class % CLASS_COUNT;
    terminal_print_pal(2, 1, "SKILL TREE", TPAL_AMBER);

    /* Show SP remaining */
    text_print(18, 1, "SP:");
    text_print_int(21, 1, player_state.skill_points);

    /* Show evolution if any */
    if (player_state.evolution != EVOLUTION_NONE) {
        text_print(2, 2, "Class:");
        terminal_print_pal(9, 2, player_get_evolution_name(), TPAL_CYAN);
    }

    /* Branch tabs */
    for (int b = 0; b < 3; b++) {
        int col = 2 + b * 9;
        if (b == skill_branch) {
            terminal_print_pal(col, 4, branch_names[b], TPAL_CYAN);
        } else {
            text_print(col, 4, branch_names[b]);
        }
    }
    text_print(2, 5, "----------------------------");

    /* Skills in current branch */
    for (int s = 0; s < SKILLS_PER_BRANCH; s++) {
        int row = 7 + s * 2;
        int idx = skill_branch * SKILLS_PER_BRANCH + s;
        int rank = player_state.skill_tree[idx];

        /* Cursor */
        if (s == cursor) {
            text_print(2, row, ">");
        } else {
            text_print(2, row, " ");
        }

        /* Skill name */
        const char* name = skill_names[cls][idx];
        text_print(4, row, name);

        /* Rank pips */
        for (int r = 0; r < SKILL_MAX_RANK; r++) {
            if (r < rank) {
                terminal_print_pal(18 + r * 2, row, "#", TPAL_AMBER);
            } else {
                text_print(18 + r * 2, row, ".");
            }
        }

        /* Bonus preview */
        int bonus = rank * 5; /* Each rank = +5% */
        if (bonus > 0) {
            text_print(25, row, "+");
            text_print_int(26, row, bonus);
            text_print(28, row, "%");
        }
    }

    text_print(2, 16, "A:Allocate L/R:Branch");
    text_print(2, 17, "B:Back");
    draw_status_bar();
}

/* ---- Codex sub-state ---- */

static const char* const codex_cat_names[] = { "ENEMIES", "BOSSES", "WEAPONS" };
#define CODEX_CAT_COUNT 3

static const char* const enemy_names[ENEMY_TYPE_COUNT] = {
    "Sentry", "Patrol", "Flyer", "Shield", "Spike", "Hunter",
    "Drone", "Turret", "Mimic", "Corruptor", "Ghost", "Bomber",
};

/* ---- Crafting ---- */

/* Get the real inventory index for the Nth non-empty slot */
static int craft_inv_index(int nth) {
    int idx = 0;
    for (int i = 0; i < INVENTORY_SIZE; i++) {
        if (inventory_get(i)) {
            if (idx == nth) return i;
            idx++;
        }
    }
    return -1;
}

static void update_craft(void) {
    if (input_hit(KEY_B)) {
        if (craft_mode > 0) {
            /* Back to craft menu */
            craft_mode = 0;
            craft_sel_count = 0;
            cursor = 0;
            text_clear_all();
            audio_play_sfx(SFX_MENU_BACK);
        } else {
            audio_play_sfx(SFX_MENU_BACK);
            sub_state = TSUB_MAIN;
            cursor = 0;
            text_clear_all();
        }
        return;
    }

    if (craft_mode == 0) {
        /* Craft menu: Fuse / Salvage / Forge */
        if (input_hit(KEY_DOWN)) { cursor++; if (cursor > 2) cursor = 0; audio_play_sfx(SFX_MENU_SELECT); text_clear_all(); }
        if (input_hit(KEY_UP)) { cursor--; if (cursor < 0) cursor = 2; audio_play_sfx(SFX_MENU_SELECT); text_clear_all(); }
        if (input_hit(KEY_A)) {
            craft_mode = cursor + 1; /* 1=fuse, 2=salvage, 3=forge */
            craft_sel_count = 0;
            cursor = 0;
            inv_scroll = 0;
            text_clear_all();
            audio_play_sfx(SFX_MENU_SELECT);
        }
    } else {
        /* Item selection mode */
        int count = inventory_count();
        if (count == 0) return;
        if (input_hit(KEY_DOWN)) { cursor++; if (cursor >= count) cursor = count - 1; audio_play_sfx(SFX_MENU_SELECT); text_clear_all(); }
        if (input_hit(KEY_UP) && cursor > 0) { cursor--; audio_play_sfx(SFX_MENU_SELECT); text_clear_all(); }
        if (cursor < inv_scroll) inv_scroll = cursor;
        if (cursor >= inv_scroll + 6) inv_scroll = cursor - 5;

        if (input_hit(KEY_A)) {
            int real_idx = craft_inv_index(cursor);
            if (real_idx < 0) return;

            if (craft_mode == 1) {
                /* Fuse: select 3 items of same rarity */
                /* Check if already selected */
                int dup = 0;
                for (int i = 0; i < craft_sel_count; i++) {
                    if (craft_sel[i] == real_idx) { dup = 1; break; }
                }
                if (!dup && craft_sel_count < 3) {
                    craft_sel[craft_sel_count++] = real_idx;
                    audio_play_sfx(SFX_MENU_SELECT);
                }
                if (craft_sel_count == 3) {
                    if (craft_fuse(craft_sel[0], craft_sel[1], craft_sel[2])) {
                        /* Success — reset */
                        craft_sel_count = 0;
                        cursor = 0;
                        inv_scroll = 0;
                    } else {
                        hud_notify("Can't fuse!", 60);
                        craft_sel_count = 0;
                    }
                    text_clear_all();
                }
            } else if (craft_mode == 2) {
                /* Salvage: instant */
                int shards = craft_salvage(real_idx);
                if (shards > 0) {
                    player_state.craft_shards += (u16)shards;
                    cursor = 0;
                    inv_scroll = 0;
                } else {
                    hud_notify("Can't salvage!", 60);
                }
                text_clear_all();
            } else if (craft_mode == 3) {
                /* Forge: reroll stats */
                if (craft_forge(real_idx, &player_state.craft_shards)) {
                    hud_notify("REFORGED!", 60);
                    audio_play_sfx(SFX_CRAFT_SUCCESS);
                    if (game_stats.items_crafted < 65535) game_stats.items_crafted++;
                    if (game_stats.items_crafted >= 10) ach_unlock_celebrate(ACH_CRAFTSMAN);
                } else {
                    hud_notify("Not enough shards!", 60);
                }
                text_clear_all();
            }
        }
    }
}

static void draw_craft(void) {
    terminal_print_pal(2, 1, ">> CRAFTING <<", TPAL_AMBER);

    /* Shard count */
    text_print(2, 2, "Shards:");
    text_print_int(10, 2, player_state.craft_shards);

    if (craft_mode == 0) {
        /* Main craft menu */
        static const char* const craft_opts[3] = {
            "FUSE   (3 same rarity -> 1 better)",
            "SALVAGE (break -> shards)",
            "FORGE   (reroll stats, costs shards)",
        };
        for (int i = 0; i < 3; i++) {
            text_print(2, 4 + i * 2, (i == cursor) ? ">" : " ");
            /* Truncate to fit 30-col screen */
            for (int c = 0; craft_opts[i][c] && 4 + c < 29; c++)
                text_put_char(4 + c, 4 + i * 2, craft_opts[i][c]);
        }
        text_print(2, 18, "A:Select  B:Back");
    } else {
        /* Item list with selection markers */
        static const char* const mode_names[3] = { "FUSE", "SALVAGE", "FORGE" };
        terminal_print_pal(2, 3, mode_names[craft_mode - 1], TPAL_CYAN);

        if (craft_mode == 1) {
            text_print(8, 3, "Select 3 items:");
            /* Show selected count */
            text_print_int(24, 3, craft_sel_count);
            text_print(25, 3, "/3");
            /* Preview result rarity if items selected */
            if (craft_sel_count > 0) {
                LootItem* sel0 = inventory_get(craft_sel[0]);
                if (sel0 && sel0->rarity < RARITY_LEGENDARY) {
                    static const char* const rar_names[] = {
                        "Common", "Uncommon", "Rare", "Epic", "Legendary", "Mythic"
                    };
                    text_print(2, 4, "Result:");
                    int nr = sel0->rarity + 1;
                    int rp = (nr >= 3) ? TPAL_AMBER : TPAL_GREEN;
                    if (nr >= 4) rp = TPAL_RED;
                    terminal_print_pal(10, 4, rar_names[nr], rp);
                }
            }
        } else if (craft_mode == 2) {
            /* Show salvage value preview */
            int si = craft_inv_index(cursor);
            if (si >= 0) {
                LootItem* sit = inventory_get(si);
                if (sit) {
                    int sv = loot_salvage_value(sit);
                    text_print(8, 3, "Value:");
                    text_print_int(15, 3, sv);
                    text_print(17, 3, "shards");
                }
            }
        } else if (craft_mode == 3) {
            /* Show forge cost preview */
            int ri = craft_inv_index(cursor);
            if (ri >= 0) {
                LootItem* it = inventory_get(ri);
                if (it) {
                    int cost = 5 + it->rarity * 5;
                    text_print(8, 3, "Cost:");
                    text_print_int(14, 3, cost);
                }
            }
        }

        int row = 5;
        int idx = 0;
        for (int i = 0; i < INVENTORY_SIZE && row < 17; i++) {
            LootItem* item = inventory_get(i);
            if (!item) continue;
            if (idx < inv_scroll) { idx++; continue; }

            /* Cursor */
            text_print(2, row, (idx == cursor) ? ">" : " ");

            /* Selected marker for fuse */
            int selected = 0;
            for (int s = 0; s < craft_sel_count; s++) {
                if (craft_sel[s] == i) { selected = 1; break; }
            }
            text_put_char(3, row, selected ? '*' : ' ');

            /* Item name + rarity */
            const char* name = loot_get_name(item);
            int pal = TPAL_GREEN;
            if (item->rarity >= 3) pal = TPAL_AMBER; /* Epic+ */
            if (item->rarity >= 4) pal = TPAL_RED;   /* Legendary+ */
            terminal_print_pal(5, row, name, pal);

            /* Equipped tag */
            if (item->flags & LOOT_FLAG_EQUIPPED)
                text_put_char(28, row, 'E');

            row++;
            idx++;
        }

        text_print(2, 18, "A:Pick  B:Back");
    }
    draw_status_bar();
}

static void update_codex(void) {
    if (codex_mode == 0) {
        /* Category list */
        if (input_hit(KEY_DOWN) && cursor < CODEX_CAT_COUNT - 1) {
            cursor++;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_UP) && cursor > 0) {
            cursor--;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A)) {
            codex_category = cursor;
            codex_mode = 1;
            cursor = 0;
            text_clear_all();
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_B)) {
            audio_play_sfx(SFX_MENU_BACK);
            sub_state = TSUB_MAIN;
            cursor = 0;
            text_clear_all();
        }
    } else if (codex_mode == 1) {
        /* Entry list */
        int max_items = 0;
        if (codex_category == 0) max_items = ENEMY_TYPE_COUNT;
        else if (codex_category == 1) max_items = BOSS_TYPE_COUNT;
        else max_items = WEAPON_TYPE_COUNT;

        if (input_hit(KEY_DOWN) && cursor < max_items - 1) {
            cursor++;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_UP) && cursor > 0) {
            cursor--;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A)) {
            /* Only open detail if entry is unlocked */
            int codex_id = -1;
            if (codex_category == 0) codex_id = CODEX_ENEMY_BASE + cursor;
            else if (codex_category == 1) codex_id = CODEX_BOSS_BASE + cursor;
            else codex_id = CODEX_WEAPON_BASE + cursor;
            if (codex_id >= 0 && codex_unlocked(codex_id)) {
                codex_mode = 2;
                text_clear_all();
                audio_play_sfx(SFX_MENU_SELECT);
            }
        }
        if (input_hit(KEY_B)) {
            audio_play_sfx(SFX_MENU_BACK);
            codex_mode = 0;
            cursor = codex_category;
            text_clear_all();
        }
    } else {
        /* Detail view — B returns to list */
        if (input_hit(KEY_B)) {
            audio_play_sfx(SFX_MENU_BACK);
            codex_mode = 1;
            text_clear_all();
        }
    }
}

static void draw_codex(void) {
    if (codex_mode == 0) {
        /* Category selection */
        terminal_print_pal(2, 1, ">> CODEX <<", TPAL_AMBER);
        text_print(2, 2, "-----------");

        for (int i = 0; i < CODEX_CAT_COUNT; i++) {
            int row = 4 + i * 2;
            text_print(3, row, (i == cursor) ? ">" : " ");
            text_print(5, row, codex_cat_names[i]);
        }

        text_print(2, 18, "A:Select  B:Back");
    } else if (codex_mode == 2) {
        /* Detail view */
        if (codex_category == 0 && cursor < ENEMY_TYPE_COUNT) {
            /* Enemy detail */
            static const char* const enemy_descs[ENEMY_TYPE_COUNT] = {
                "Stationary turret. Fires when",
                "Walks patrol route. Chases on",
                "Flying unit. Erratic movement.",
                "Blocks frontal attacks. Slow.",
                "Leaps at player. High damage.",
                "Aggressive tracker. Fast rush.",
                "Swarm unit. Low HP, in groups.",
                "Fixed position. Aimed laser.",
                "Disguised as loot drop. Ambush",
                "Corrupts tiles to hazards.",
                "Phases through walls. Melee.",
                "Aerial bomber. Drops AoE.",
            };
            terminal_print_pal(2, 1, enemy_names[cursor], TPAL_AMBER);
            text_print(2, 3, "HP:");
            text_print_int(6, 3, enemy_info[cursor].hp);
            text_print(12, 3, "ATK:");
            text_print_int(17, 3, enemy_info[cursor].atk);
            text_print(2, 5, "XP:");
            text_print_int(6, 5, enemy_info[cursor].xp_reward);
            text_print(12, 5, "Range:");
            text_print_int(19, 5, enemy_info[cursor].detection_range);
            text_put_char(22, 5, 'p');
            text_put_char(23, 5, 'x');
            /* Size */
            text_print(2, 7, "Size:");
            text_print_int(8, 7, enemy_info[cursor].width);
            text_put_char(10, 7, 'x');
            text_print_int(11, 7, enemy_info[cursor].height);
            /* Description */
            terminal_print_pal(2, 9, "PROFILE:", TPAL_CYAN);
            text_print(2, 10, enemy_descs[cursor]);
        } else if (codex_category == 1 && cursor < BOSS_TYPE_COUNT) {
            /* Boss detail */
            static const char* const boss_corps[BOSS_TYPE_COUNT] = {
                "Microslop Corp", "Gogol Systems",
                "Amazomb Inc", "Crapple Ltd",
                "Faceplant Media", "AXIOM Failsafe",
            };
            static const char* const boss_descs[BOSS_TYPE_COUNT] = {
                "Firewall barrier with pulse.",
                "Search beam and blackout zones.",
                "Segmented worm. Body splits.",
                "Radial + homing shot volleys.",
                "Vine tendrils from all walls.",
                "Copies all previous bosses.",
            };
            terminal_print_pal(2, 1, boss_get_name(cursor), TPAL_RED);
            terminal_print_pal(2, 3, boss_corps[cursor], TPAL_AMBER);
            text_print(2, 5, "Phases: 3");
            text_print(12, 5, "100%/60%/30%");
            if (quest_state.boss_defeated[cursor])
                terminal_print_pal(2, 7, "STATUS: DEFEATED", TPAL_CYAN);
            else
                text_print(2, 7, "STATUS: ACTIVE");
            terminal_print_pal(2, 9, "PROFILE:", TPAL_CYAN);
            text_print(2, 10, boss_descs[cursor]);
        } else if (codex_category == 2 && cursor < WEAPON_TYPE_COUNT) {
            /* Weapon detail */
            static const char* const wep_descs[WEAPON_TYPE_COUNT] = {
                "Standard blaster. Balanced.",
                "Fast fire rate. Low damage.",
                "3-way spread. Short range.",
                "Hold to charge. High burst.",
                "Continuous stream. Pierces.",
                "Hold for ramping damage.",
                "Auto-tracking projectile.",
                "AoE burst around player.",
            };
            static const char* const wep_traits[WEAPON_TYPE_COUNT] = {
                "Trait: None",
                "Trait: +Fire Rate",
                "Trait: Multi-hit",
                "Trait: Charge damage x3",
                "Trait: Pierce enemies",
                "Trait: Ramp 1x to 2x",
                "Trait: Auto-aim",
                "Trait: Close-range AoE",
            };
            terminal_print_pal(2, 1, weapon_type_names[cursor], TPAL_AMBER);
            terminal_print_pal(2, 3, wep_descs[cursor], TPAL_GREEN);
            text_print(2, 5, wep_traits[cursor]);
        }
        text_print(2, 18, "B:Back");
    } else if (codex_category == 0) {
        /* Enemy entries — compact single-row with scroll */
        terminal_print_pal(2, 1, ">> ENEMIES <<", TPAL_AMBER);
        int scroll = cursor > 11 ? cursor - 11 : 0;
        for (int i = scroll; i < ENEMY_TYPE_COUNT; i++) {
            int row = 3 + (i - scroll);
            if (row >= 17) break;
            text_print(2, row, (i == cursor) ? ">" : " ");
            if (codex_unlocked(CODEX_ENEMY_BASE + i)) {
                terminal_print_pal(4, row, enemy_names[i], TPAL_GREEN);
                text_print(16, row, "HP:");
                text_print_int(19, row, enemy_info[i].hp);
                text_print(23, row, "A:");
                text_print_int(25, row, enemy_info[i].atk);
            } else {
                text_print(4, row, "???");
            }
        }
        text_print(2, 18, "A:Detail  B:Back");
    } else if (codex_category == 1) {
        /* Boss entries */
        terminal_print_pal(2, 1, ">> BOSSES <<", TPAL_AMBER);
        for (int i = 0; i < BOSS_TYPE_COUNT; i++) {
            int row = 3 + i * 2;
            if (row >= 17) break;
            text_print(2, row, (i == cursor) ? ">" : " ");
            if (codex_unlocked(CODEX_BOSS_BASE + i)) {
                terminal_print_pal(4, row, boss_get_name(i), TPAL_RED);
                if (quest_state.boss_defeated[i]) {
                    terminal_print_pal(18, row, "DEFEATED", TPAL_CYAN);
                } else {
                    text_print(18, row, "--------");
                }
            } else {
                text_print(4, row, "???");
            }
        }
        text_print(2, 18, "A:Detail  B:Back");
    } else {
        /* Weapon type entries */
        terminal_print_pal(2, 1, ">> WEAPONS <<", TPAL_AMBER);
        for (int i = 0; i < WEAPON_TYPE_COUNT; i++) {
            int row = 3 + i * 2;
            if (row >= 17) break;
            text_print(2, row, (i == cursor) ? ">" : " ");
            if (codex_unlocked(CODEX_WEAPON_BASE + i)) {
                terminal_print_pal(4, row, weapon_type_names[i], TPAL_GREEN);
            } else {
                text_print(4, row, "???");
            }
        }
        text_print(2, 18, "A:Detail  B:Back");
    }
    draw_status_bar();
}

/* ---- Evolution sub-state ---- */

static void update_evolution(void) {
    if (input_hit(KEY_DOWN) && cursor < 1) {
        cursor = 1;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_UP) && cursor > 0) {
        cursor = 0;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_A)) {
        if (player_apply_evolution(cursor + 1)) {
            audio_play_sfx(SFX_EVOLVE);
            video_flash_start(4, 16);
            hud_notify("EVOLVED!", 90);
            sub_state = TSUB_SKILLS;
            cursor = 0;
            text_clear_all();
        }
    }
    if (input_hit(KEY_B)) {
        /* Allow skipping evolution (can come back later) */
        audio_play_sfx(SFX_MENU_BACK);
        sub_state = TSUB_MAIN;
        cursor = 0;
        text_clear_all();
    }
}

static const char* const evo_names[CLASS_COUNT][2] = {
    [CLASS_ASSAULT]      = { "VANGUARD", "COMMANDO" },
    [CLASS_INFILTRATOR]  = { "PHANTOM",  "STRIKER" },
    [CLASS_TECHNOMANCER] = { "ARCHITECT","HACKER" },
};
static const char* const evo_desc[CLASS_COUNT][2] = {
    [CLASS_ASSAULT]      = { "HP+30% DEF+20%", "ATK+25% Crit+10%" },
    [CLASS_INFILTRATOR]  = { "Stealth SPD+15%", "SPD+30% DblJump" },
    [CLASS_TECHNOMANCER] = { "HP+15% DEF+15%", "ATK+20% Upload" },
};

static void draw_evolution(void) {
    int cls = player_state.player_class % CLASS_COUNT;
    terminal_print_pal(4, 2, "CLASS EVOLUTION", TPAL_AMBER);
    terminal_print_pal(4, 4, "Level 20 reached!", TPAL_CYAN);
    text_print(4, 5, "Choose your path:");

    for (int c = 0; c < 2; c++) {
        int row = 8 + c * 4;
        if (c == cursor) {
            terminal_print_pal(3, row, ">", TPAL_AMBER);
        } else {
            text_print(3, row, " ");
        }
        terminal_print_pal(5, row, evo_names[cls][c], TPAL_CYAN);
        text_print(5, row + 1, evo_desc[cls][c]);
    }

    text_print(4, 17, "A:Select  B:Skip");
    draw_status_bar();
}

void state_terminal_update(void) {
    terminal_tick();

    /* Boot sequence: blocks input for 60 frames on terminal entry */
    if (!boot_done) {
        boot_timer++;
        if (boot_timer >= 60) {
            boot_done = 1;
            text_clear_all();
        }
        return;
    }

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
    case TSUB_SKILLS:     update_skills(); break;
    case TSUB_EVOLUTION:  update_evolution(); break;
    case TSUB_CODEX:      update_codex(); break;
    case TSUB_CRAFT:      update_craft(); break;
    case TSUB_BRIEFING:
        if (input_hit(KEY_A)) {
            audio_play_sfx(SFX_TRANSITION);
            game_request_state = STATE_NET;
        }
        if (input_hit(KEY_B)) {
            audio_play_sfx(SFX_MENU_BACK);
            sub_state = TSUB_MAIN;
            text_clear_all();
        }
        break;
    case TSUB_STATS:
        if (input_hit(KEY_L) || input_hit(KEY_R)) {
            stats_page ^= 1;
            text_clear_all();
        }
        if (input_hit(KEY_B)) {
            audio_play_sfx(SFX_MENU_BACK);
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
    /* Show shards if any, otherwise show act */
    if (player_state.craft_shards > 0) {
        text_print(17, 19, "Sh:");
        text_print_int(20, 19, player_state.craft_shards);
    } else {
        text_print(19, 19, "Act:");
        text_print_int(23, 19, quest_state.current_act);
    }
}

static void draw_main(void) {
    terminal_draw_menu(cursor);

    /* Skill point / evolution available indicators */
    if (player_state.skill_points > 0) {
        static int sp_blink;
        sp_blink++;
        if (sp_blink & 8) {
            terminal_print_pal(18, 4 + TMENU_SKILLS, "SP!", TPAL_CYAN);
        }
    }
    if (player_state.evolution_pending && player_state.evolution == EVOLUTION_NONE) {
        static int evo_blink;
        evo_blink++;
        if (evo_blink & 4) {
            terminal_print_pal(18, 4 + TMENU_SKILLS, "EVO!", TPAL_AMBER);
        }
    }

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

    if (stats_page == 0) {
        terminal_print_pal(2, 1, ">> OPERATOR STATS <<", TPAL_AMBER);
        text_print(2, 3, "Class:");
        if (player_state.evolution != EVOLUTION_NONE) {
            terminal_print_pal(10, 3, player_get_evolution_name(), TPAL_CYAN);
        } else {
            text_print(10, 3, class_names[player_state.player_class % CLASS_COUNT]);
        }

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
        text_print(14, 15, "/30 Act:");
        text_print_int(22, 15, quest_state.current_act);

        /* Bosses defeated */
        int boss_count = 0;
        for (int i = 0; i < 6; i++) {
            if (quest_state.boss_defeated[i]) boss_count++;
        }
        text_print(2, 16, "Bosses:");
        text_print_int(10, 16, boss_count);
        text_print(11, 16, "/6");
    } else {
        terminal_print_pal(2, 1, ">> COMBAT RECORD <<", TPAL_AMBER);

        text_print(2, 3, "Kills:");
        text_print_int(12, 3, game_stats.total_kills);
        text_print(2, 4, "Deaths:");
        text_print_int(12, 4, game_stats.total_deaths);
        text_print(2, 5, "Dmg Dealt:");
        text_print_int(14, 5, game_stats.damage_dealt);
        text_print(2, 6, "Dmg Taken:");
        text_print_int(14, 6, game_stats.damage_taken);
        text_print(2, 7, "Best Combo:");
        text_print_int(14, 7, game_stats.highest_combo);
        text_print(2, 8, "Items Found:");
        text_print_int(16, 8, game_stats.items_found);
        text_print(2, 9, "Items Crafted:");
        text_print_int(17, 9, game_stats.items_crafted);
        text_print(2, 10, "Contracts:");
        text_print_int(13, 10, game_stats.contracts_done);

        /* Play time (convert frames to HH:MM) */
        u32 total_secs = game_stats.play_time_frames / 60;
        int hours = (int)(total_secs / 3600);
        int mins = (int)((total_secs % 3600) / 60);
        text_print(2, 12, "Play Time:");
        text_print_int(14, 12, hours);
        text_put_char(14 + (hours >= 10 ? 2 : 1), 12, 'h');
        text_print_int(14 + (hours >= 10 ? 3 : 2), 12, mins);
        text_put_char(14 + (hours >= 10 ? 3 : 2) + (mins >= 10 ? 2 : 1), 12, 'm');

        if (game_stats.ng_plus) {
            terminal_print_pal(2, 14, "NG+ Active", TPAL_CYAN);
        }
        if (game_stats.bb_threat_level > 0) {
            text_print(2, 15, "Threat Lv:");
            text_print_int(13, 15, game_stats.bb_threat_level);
        }
        /* Achievement count */
        {
            int ach_count = 0;
            for (int i = 0; i < ACH_COUNT; i++) {
                if (ach_unlocked(i)) ach_count++;
            }
            text_print(2, 16, "Achievements:");
            text_print_int(16, 16, ach_count);
            text_print(18, 16, "/");
            text_print_int(19, 16, ACH_COUNT);
        }
    }

    text_print(2, 17, "L/R:Page");
    text_print(14, 17, stats_page == 0 ? "[1/2]" : "[2/2]");
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
            } else {
                terminal_print_pal(12, row, "CORRUPTED", TPAL_RED);
            }
        } else {
            text_print(12, row, "- Empty -");
        }
    }

    if (save_confirm) {
        terminal_print_pal(4, 16, "Overwrite slot? A:Yes B:No", TPAL_RED);
    } else if (save_msg_timer > 0) {
        text_print(10, 16, "Done!");
    }

    text_print(2, 18, save_confirm ? "A:Confirm      B:Cancel" : "A:Save R:Load B:Back");
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

    /* Total runs */
    text_print(2, 16, "Runs:");
    text_print_int(8, 16, bb_state.total_runs);

    /* Endgame stats (post-story) */
    if (game_stats.endgame_unlocked) {
        text_clear_rect(2, 14, 26, 1);
        text_print(2, 14, "EC:");
        text_print_int(5, 14, game_stats.bb_endgame_contracts);
        /* Boss contract countdown */
        {
            int next_boss = 25 - ((int)game_stats.bb_endgame_contracts % 25);
            if (next_boss == 25) next_boss = 0;
            text_print(13, 14, "BOSS:");
            text_print_int(18, 14, next_boss);
        }
        text_clear_rect(2, 15, 26, 1);
        /* Threat level as stars */
        text_print(2, 15, "THREAT:");
        {
            int tl = game_stats.bb_threat_level;
            if (tl > 10) tl = 10;
            for (int i = 0; i < tl && i < 10; i++)
                text_put_char(10 + i, 15, '*');
        }
        /* Highest level reached */
        text_print(21, 15, "Lv");
        text_print_int(23, 15, game_stats.bb_highest_level);
    } else if (bugbounty_tier_available(cursor)) {
        int rfloor = bugbounty_get_rarity_floor(cursor);
        text_print(2, 15, "Scale:");
        text_print(9, 15, scale_str[cursor]);
        text_print(15, 15, "Drop:");
        text_print(21, 15, loot_get_rarity_name(rfloor));
    }

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
        terminal_print_pal(2, r++, "ASSAULT", TPAL_RED);
        text_print(2, r++, "ChrgShot Burst HvyShell Overcl");
        text_print(2, r++, "Rocket IronSkin WarCry Berserk");
        r++;
        terminal_print_pal(2, r++, "INFILTRATOR", TPAL_CYAN);
        text_print(2, r++, "AirDash PhaseShot FanFire Ovld");
        text_print(2, r++, "Smoke Backstab Clone TimeWarp");
        r++;
        terminal_print_pal(2, r++, "TECHNOMANCER", TPAL_AMBER);
        text_print(2, r++, "Turret Scan DShield SysCrash");
        text_print(2, r++, "Nanobots Firewall OC+ Upload");
        r++;
        text_print(2, r++, "Evolve at Lv20 (2 choices)");
        text_print(2, r++, "Skill tree: 1 SP per 2 levels");
        break;
    case 3: /* Loot & Equipment */
        terminal_print_pal(2, 1, "-- Loot & Equipment --", TPAL_AMBER);
        text_print(2, r++, "Rarity: Common<Unco<Rare<Epic");
        terminal_print_pal(2, r++, "Legendary < Mythic (boss)", TPAL_AMBER);
        r++;
        text_print(2, r++, "3 slots: Weapon+Armor+Acces");
        text_print(2, r++, "A:Equip  R:Sell");
        r++;
        text_print(2, r++, "8 weapon types:");
        text_print(2, r++, "Buster Rapid Spread Charger");
        text_print(2, r++, "Beam Laser Homing Nova(AoE)");
        r++;
        text_print(2, r++, "CRAFT menu at terminal:");
        text_print(2, r++, "Fuse 3 same rarity -> upgrade");
        text_print(2, r++, "Salvage item -> shards");
        text_print(2, r++, "Forge = reroll stats (shards)");
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
        text_print(2, r++, "6 acts, 5 missions each (30)");
        text_print(2, r++, "Boss on mission 5,10,15,20,25");
        r++;
        text_print(2, r++, "3-phase bosses (100/60/30%HP)");
        text_print(2, r++, "P1:Learn  P2:Harder  P3:Chaos");
        text_print(2, r++, "VULNERABLE: flashing=HIT NOW");
        r++;
        text_print(2, r++, "6 corps: Microslop Gogol");
        text_print(2, r++, "Amazomb Crapple Faceplant");
        text_print(2, r++, "Act 6: DAEMON (final boss)");
        r++;
        text_print(2, r++, "After story: NG+ or Endgame");
        text_print(2, r++, "Endgame: endless Bug Bounty");
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
    terminal_scroll_bg();

    /* Update fade-in transition (must run every frame, even during boot) */
    video_transition_update();

    /* Boot sequence display — line-by-line terminal messages */
    if (!boot_done) {
        int t = boot_timer;
        if (t >= 2)  terminal_print_pal(2, 3, "> INIT NEURAL LINK...", TPAL_GREEN);
        if (t >= 10) terminal_print_pal(2, 4, "> HANDSHAKE OK", TPAL_GREEN);
        if (t >= 18) terminal_print_pal(2, 5, "> ENCRYPTING CHANNEL", TPAL_GREEN);
        if (t >= 26) terminal_print_pal(2, 6, "> TRACE COUNTERMEASURES: ON", TPAL_GREEN);
        if (t >= 34) terminal_print_pal(2, 8, "> CONNECTING...", TPAL_CYAN);
        if (t >= 44) terminal_print_pal(2, 10, "> LINK ESTABLISHED", TPAL_AMBER);
        if (t >= 52) terminal_print_pal(4, 12, "WELCOME, OPERATOR", TPAL_AMBER);
        return;
    }

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
    case TSUB_SKILLS:
        draw_skills();
        break;
    case TSUB_EVOLUTION:
        draw_evolution();
        break;
    case TSUB_CODEX:
        draw_codex();
        break;
    case TSUB_CRAFT:
        draw_craft();
        break;
    case TSUB_BRIEFING:
    {
        Contract* bc = quest_get_active();
        terminal_print_pal(2, 1, "== MISSION BRIEFING ==", TPAL_AMBER);
        if (bc) {
            /* Mission name / story mission info */
            if (bc->story_mission > 0) {
                terminal_print_pal(2, 3, "STORY MISSION", TPAL_CYAN);
                text_print(2, 4, "Act:");
                text_print_int(7, 4, (int)quest_state.current_act);
                text_print(10, 4, "Mission:");
                text_print_int(19, 4, bc->story_mission);
            } else {
                terminal_print_pal(2, 3, "CONTRACT", TPAL_CYAN);
            }
            /* Objective type */
            text_print(2, 6, "OBJ:");
            static const char* const obj_names[] = {
                "KILL ALL", "SURVIVE", "BOSS HUNT",
                "RETRIEVAL", "ESCAPE", "STORY"
            };
            int ot = bc->type;
            if (ot >= 0 && ot < 6) text_print(7, 6, obj_names[ot]);
            /* Kill target if applicable */
            if (bc->kill_target > 0) {
                text_print(2, 7, "Target:");
                text_print_int(10, 7, bc->kill_target);
                text_print(14, 7, "kills");
            }
            /* Threat level (stars) */
            text_print(2, 9, "THREAT:");
            {
                int stars = bc->tier;
                if (stars > 10) stars = 10;
                for (int s = 0; s < stars && s < 10; s++)
                    text_put_char(10 + s, 9, '*');
            }
            /* Rewards */
            terminal_print_pal(2, 11, "REWARDS:", TPAL_AMBER);
            text_print(2, 12, "Credits:");
            text_print_int(11, 12, (int)bc->reward_credits);
            text_print(2, 13, "XP:");
            {
                int xp = 30 + bc->tier * 15;
                if (bc->story_mission > 0) xp += bc->story_mission * 10;
                text_print_int(6, 13, xp);
            }
            /* Brief text */
            {
                const char* brief = quest_get_story_brief(bc->story_mission);
                if (brief) {
                    terminal_print_pal(2, 15, brief, TPAL_GREEN);
                }
            }
        } else {
            text_print(2, 5, "No active contract.");
        }
        /* Controls */
        terminal_print_pal(2, 18, "[A]:JACK IN  [B]:CANCEL", TPAL_GREEN);
        break;
    }
    }
}

void state_terminal_exit(void) {
    text_clear_all();
    /* Clean up blend registers to prevent dark overlay bleeding into next state */
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
