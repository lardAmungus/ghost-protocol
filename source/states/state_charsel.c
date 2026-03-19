/*
 * Ghost Protocol — Class Selection Screen
 *
 * Presents the 3 operator classes with stats and abilities before
 * the player enters the terminal hub. Also offers save file loading.
 * Redesigned with circuit board BG, colored stat bars, paginated abilities.
 */
#include <tonc.h>
#include "states/state_charsel.h"
#include "states/state_ids.h"
#include "states/state_terminal.h"
#include "game/player.h"
#include "game/common.h"
#include "game/quest.h"
#include "game/terminal.h"
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/save.h"
#include <string.h>

/* ---- Layout constants ---- */
#define SEL_COL   3     /* left margin inside panel */

/* ---- Class data ---- */
static const char* const class_names[CLASS_COUNT] = {
    "TROJAN", "INFILTRATOR", "TECHNOMANCER"
};
static const char* const class_roles[CLASS_COUNT] = {
    "HEAVY ASSAULT OPS",
    "STEALTH INFILTRATOR",
    "SYSTEMS HACKER"
};
static const char* const class_weapons[CLASS_COUNT] = {
    "BUSTER CANNON",
    "RAPID SCATTER",
    "MATRIX RIFLE"
};
static const char* const class_playstyle[CLASS_COUNT] = {
    "TANK & PUNISH",
    "SPEED & EVASION",
    "CONTROL & SUPPORT"
};
/* Ability names per class (8 abilities) */
static const char* const class_abilities[CLASS_COUNT][8] = {
    { "Charged Shot", "Burst Fire",  "Heavy Shell", "Overclock",
      "Rocket",       "Iron Skin",   "War Cry",     "Berserk"     },
    { "Air Dash",     "Phase Shot",  "Fan Fire",    "Overload",
      "Smoke Bomb",   "Backstab",    "Clone",       "Time Warp"   },
    { "Turret",       "Scan Pulse",  "Data Shield", "Sys Crash",
      "Nanobots",     "Firewall",    "Overclock+",  "Upload"      },
};
/* Short ability descriptions (1 line, max 12 chars for cols 16-27) */
static const char* const ability_descs[CLASS_COUNT][8] = {
    { "Hold B burst", "Triple rapid", "Armor pierce", "Speed boost",
      "Rocket blast", "DEF doubled",  "ATK buff",     "Pwr up DEF-" },
    { "Air dodge",    "Phase walls",  "Wide scatter",  "Ovld burst",
      "Break aggro",  "3x backstab",  "Spawn decoy",   "Slow enemies" },
    { "Auto sentry",  "Reveal foes",  "Halve dmg",     "AoE damage",
      "HP regen",     "Reflect dmg",  "Halve CDs",     "Mark 2x dmg" },
};
/* Level 1 base stats: HP, ATK, DEF, SPD, LCK */
static const int class_stats[CLASS_COUNT][5] = {
    { 40, 10, 6, 4, 2 },
    { 30,  7, 4, 8, 5 },
    { 35,  8, 7, 5, 3 },
};
static const char* const stat_labels[5] = { "HP", "ATK", "DEF", "SPD", "LCK" };
static const int stat_max[5] = { 40, 10, 7, 8, 5 };

static int selected_class;
static int cursor;          /* 0 = class select, 1..3 = load slot 1-3 */
static int any_save;        /* non-zero if at least one slot has data  */
static int blink_timer;
static int fade_timer;
static int fade_target;
static int fade_in_timer;
static int ability_page;    /* 0 = abilities 1-4, 1 = abilities 5-8 */
static int compare_mode;    /* 1 = holding SELECT for 3-class comparison */
#define FADE_FRAMES 15

/* ---- Helpers ---- */

/* Draw a colored stat bar: [========--] */
static void draw_stat_bar(int col, int row, int value, int max_ref, int bar_len) {
    int filled = (max_ref > 0) ? value * bar_len / max_ref : 0;
    if (filled > bar_len) filled = bar_len;
    if (filled < 0) filled = 0;
    text_put_char(col, row, '[');
    for (int b = 0; b < bar_len; b++) {
        text_put_char(col + 1 + b, row, (b < filled) ? '=' : '-');
    }
    text_put_char(col + 1 + bar_len, row, ']');
}

static void draw_class_page(int cls) {
    /* Full-screen panel */
    terminal_draw_panel(0, 0, 19, 29);

    /* Header */
    terminal_print_pal(SEL_COL, 0, "JACK-IN CONFIGURATION", TPAL_AMBER);

    /* Class number indicator */
    text_put_char(24, 0, '<');
    text_put_char(25, 0, (char)('0' + cls + 1));
    text_put_char(26, 0, '/');
    text_put_char(27, 0, (char)('0' + CLASS_COUNT));
    text_put_char(28, 0, '>');

    /* Class name — centered, amber */
    {
        const char* nm = class_names[cls];
        int len = 0;
        while (nm[len]) len++;
        int pad = (26 - len) / 2;
        if (pad < 2) pad = 2;
        terminal_print_pal(pad, 2, nm, TPAL_AMBER);
    }

    /* Role and weapon */
    text_print(SEL_COL, 3, "Role:");
    terminal_print_pal(SEL_COL + 6, 3, class_roles[cls], TPAL_CYAN);
    text_print(SEL_COL, 4, "Wpn:");
    text_print(SEL_COL + 5, 4, class_weapons[cls]);

    /* Stat bars with colors (rows 6-10) */
    for (int s = 0; s < 5; s++) {
        int row = 6 + s;
        int val = class_stats[cls][s];
        text_print(SEL_COL, row, stat_labels[s]);
        draw_stat_bar(SEL_COL + 4, row, val, stat_max[s], 8);
        text_print_int(SEL_COL + 14, row, val);
    }

    /* Abilities with pagination (rows 12-15) */
    terminal_print_pal(SEL_COL, 11, "ABILITIES", TPAL_AMBER);
    {
        int base = ability_page * 4;
        text_put_char(14, 11, ability_page == 0 ? '1' : '2');
        text_put_char(15, 11, '/');
        text_put_char(16, 11, '2');
        for (int a = 0; a < 4 && base + a < 8; a++) {
            int row = 12 + a;
            int ab_idx = base + a;
            text_put_char(SEL_COL, row, (char)('1' + ab_idx));
            text_put_char(SEL_COL + 1, row, ':');
            text_print(SEL_COL + 2, row, class_abilities[cls][ab_idx]);
            /* 1-line description on right side */
            {
                const char* d = ability_descs[cls][ab_idx];
                for (int c = 0; d[c] && 16 + c < 28; c++)
                    text_put_char(16 + c, row, d[c]);
            }
        }
    }

    /* Playstyle */
    text_print(SEL_COL, 16, "Style:");
    terminal_print_pal(SEL_COL + 7, 16, class_playstyle[cls], TPAL_GREEN);
}

static void draw_comparison(void) {
    /* 3-class side-by-side comparison */
    terminal_draw_panel(0, 0, 19, 29);
    terminal_print_pal(4, 0, "CLASS COMPARISON", TPAL_AMBER);

    /* Column headers */
    terminal_print_pal(3, 2, "ASL", TPAL_RED);
    terminal_print_pal(12, 2, "INF", TPAL_CYAN);
    terminal_print_pal(21, 2, "TEC", TPAL_AMBER);

    /* Stats for each class */
    for (int s = 0; s < 5; s++) {
        int row = 4 + s * 2;
        text_print(1, row, stat_labels[s]);
        for (int c = 0; c < 3; c++) {
            int col = 3 + c * 9;
            draw_stat_bar(col, row, class_stats[c][s], stat_max[s], 5);
            text_print_int(col + 7, row, class_stats[c][s]);
        }
    }

    terminal_print_pal(3, 16, "Release SELECT to return", TPAL_GREEN);
}

static void draw_nav_bar(void) {
    /* Row 17-18: navigation hints */
    text_clear_rect(1, 17, 28, 2);
    if (cursor == 0) {
        text_print(2, 17, "L/R:Class  SEL:Compare");
        text_print(2, 18, "A:Jack In  B:Title");
        if (any_save) {
            text_print(18, 18, "DN:Load");
        }
    } else {
        text_print(2, 17, "UP/DN:Slot  A:Load");
        text_print(2, 18, "B:Cancel");
    }
}

/* ---- State callbacks ---- */

void state_charsel_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;

    /* Terminal palettes for text */
    terminal_init_palette();

    /* Load circuit board BG on BG1 */
    terminal_load_bg();

    text_clear_all();

    selected_class = 0;
    cursor = 0;
    blink_timer = 0;
    fade_timer = 0;
    fade_target = 0;
    fade_in_timer = FADE_FRAMES;
    ability_page = 0;
    compare_mode = 0;
    REG_BLDCNT = BLD_BLACK | BLD_ALL;
    REG_BLDY = 16;

    /* Check for any existing saves */
    any_save = 0;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (save_slot_exists(i)) { any_save = 1; break; }
    }

    draw_class_page(selected_class);
    draw_nav_bar();

    audio_play_sfx(SFX_MENU_SELECT);
}

void state_charsel_update(void) {
    blink_timer++;
    terminal_tick();

    /* Fade-out to state transition */
    if (fade_timer > 0) {
        fade_timer--;
        REG_BLDY = (u16)(16 - (fade_timer * 16 / FADE_FRAMES));
        if (fade_timer == 0) {
            game_request_state = fade_target;
        }
        return;
    }

    /* Fade-in */
    if (fade_in_timer > 0) {
        fade_in_timer--;
        REG_BLDY = (u16)(fade_in_timer * 16 / FADE_FRAMES);
        if (fade_in_timer == 0) {
            REG_BLDCNT = 0;
            REG_BLDY = 0;
        }
    }

    /* Compare mode: only via long hold of SELECT (not short tap) */
    {
        static int sel_hold;
        if (input_held(KEY_SELECT)) {
            sel_hold++;
            if (sel_hold > 20 && !compare_mode) {
                compare_mode = 1;
                text_clear_all();
            }
        } else {
            sel_hold = 0;
            if (compare_mode) {
                compare_mode = 0;
                text_clear_all();
                draw_class_page(selected_class);
                draw_nav_bar();
            }
        }
    }

    if (compare_mode) return;

    if (cursor == 0) {
        /* Class selection mode */
        if (input_hit(KEY_LEFT) || input_hit(KEY_L)) {
            selected_class--;
            if (selected_class < 0) selected_class = CLASS_COUNT - 1;
            ability_page = 0;
            text_clear_all();
            draw_class_page(selected_class);
            draw_nav_bar();
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_RIGHT) || input_hit(KEY_R)) {
            selected_class++;
            if (selected_class >= CLASS_COUNT) selected_class = 0;
            ability_page = 0;
            text_clear_all();
            draw_class_page(selected_class);
            draw_nav_bar();
            audio_play_sfx(SFX_MENU_SELECT);
        }
        /* SELECT toggles ability page */
        if (input_hit(KEY_SELECT)) {
            ability_page ^= 1;
            text_clear_all();
            draw_class_page(selected_class);
            draw_nav_bar();
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (any_save && (input_hit(KEY_DOWN) || input_hit(KEY_UP))) {
            /* Enter save-slot selection */
            cursor = 1;
            draw_nav_bar();
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A)) {
            /* Confirm class selection — new game */
            player_state.player_class = (u8)selected_class;
            audio_play_sfx(SFX_MENU_SELECT);
            fade_timer = FADE_FRAMES;
            fade_target = STATE_TERMINAL;
            REG_BLDCNT = BLD_BLACK | BLD_ALL;
        }
        if (input_hit(KEY_B)) {
            audio_play_sfx(SFX_MENU_BACK);
            fade_timer = FADE_FRAMES;
            fade_target = STATE_TITLE;
            REG_BLDCNT = BLD_BLACK | BLD_ALL;
        }
    } else {
        /* Save-slot browse mode: cursor 1-3 = save slots 0-2 */
        if (input_hit(KEY_UP) || input_hit(KEY_LEFT)) {
            cursor--;
            if (cursor < 1) cursor = 1;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_DOWN) || input_hit(KEY_RIGHT)) {
            cursor++;
            if (cursor > SAVE_SLOTS) cursor = SAVE_SLOTS;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A)) {
            int slot = cursor - 1;
            if (save_slot_exists(slot)) {
                state_terminal_preload_slot(slot);
                audio_play_sfx(SFX_MENU_SELECT);
                fade_timer = FADE_FRAMES;
                fade_target = STATE_TERMINAL;
                REG_BLDCNT = BLD_BLACK | BLD_ALL;
            } else {
                cursor = 0;
                draw_nav_bar();
                audio_play_sfx(SFX_MENU_BACK);
            }
        }
        if (input_hit(KEY_B)) {
            cursor = 0;
            draw_nav_bar();
            audio_play_sfx(SFX_MENU_BACK);
        }
    }
}

void state_charsel_draw(void) {
    /* Scroll circuit BG */
    terminal_scroll_bg();

    if (compare_mode) {
        draw_comparison();
        return;
    }

    /* Blink cursor arrow on class name */
    if (cursor == 0) {
        int nm_len = 0;
        const char* nm = class_names[selected_class];
        while (nm[nm_len]) nm_len++;
        int pad = (26 - nm_len) / 2;
        if (pad < 2) pad = 2;
        if ((blink_timer >> 4) & 1) {
            text_put_char(pad - 1, 2, '>');
        } else {
            text_put_char(pad - 1, 2, ' ');
        }
    }

    /* Save slot list overlay when in save mode */
    if (cursor > 0) {
        text_clear_rect(1, 16, 28, 3);
        text_print(2, 16, "---  LOAD SAVE  ---");
        for (int s = 0; s < SAVE_SLOTS; s++) {
            int row = 17 + s;
            if (row > 18) break; /* Only 2 rows available */
            int slot_sel = (cursor - 1 == s);
            text_put_char(2, row, slot_sel ? '>' : ' ');
            text_print(4, row, "S");
            text_put_char(5, row, (char)('0' + s + 1));
            text_put_char(6, row, ':');
            static EWRAM_BSS SaveData sd;
            if (save_read_slot(&sd, s)) {
                static const char* const cn3[] = { "ASL", "INF", "TEC" };
                text_print(8, row, cn3[sd.player_class % 3]);
                text_print(12, row, "Lv");
                text_print_int(14, row, sd.player_level);
            } else {
                text_print(8, row, "EMPTY");
            }
        }
    }
}

void state_charsel_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY   = 0;
}
