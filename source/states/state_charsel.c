/*
 * Ghost Protocol — Class Selection Screen
 *
 * Presents the 3 operator classes with stats and abilities before
 * the player enters the terminal hub. Also offers save file loading.
 */
#include <tonc.h>
#include "states/state_charsel.h"
#include "states/state_ids.h"
#include "states/state_terminal.h"
#include "game/player.h"
#include "game/common.h"
#include "game/quest.h"
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/save.h"
#include <string.h>

/* ---- Text palette shortcuts (matches terminal colours) ---- */
#define C_WHITE  0  /* palette index 0: default white text      */
#define C_AMBER  1  /* BG palette 1: amber/gold accent text      */
#define C_CYAN   2  /* BG palette 2: cyan/highlight text         */
#define C_DIM    0  /* same as white but intentionally lowercase  */

/* ---- Layout constants ---- */
#define SEL_COL   2     /* left margin                            */
#define BAR_COL   0     /* full-width separator col               */

/* ---- Class data ---- */
static const char* const class_names[CLASS_COUNT] = {
    "ASSAULT", "INFILTRATOR", "TECHNOMANCER"
};
static const char* const class_roles[CLASS_COUNT] = {
    "HEAVY ASSAULT OPS",        /* max 22 chars from col 8 */
    "STEALTH INFILTRATOR",
    "SYSTEMS HACKER"
};
static const char* const class_weapons[CLASS_COUNT] = {
    "BUSTER CANNON [CHG]",      /* max 20 chars from col 10 */
    "RAPID SCATTER [SPR]",
    "MATRIX RIFLE [AIM]"
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
/* Level 1 base stats: HP, ATK, DEF, SPD, LCK */
static const int class_stats[CLASS_COUNT][5] = {
    { 40, 10, 6, 4, 2 },
    { 30,  7, 4, 8, 5 },
    { 35,  8, 7, 5, 3 },
};

static int selected_class;
static int cursor;          /* 0 = class select, 1..3 = load slot 1-3 */
static int any_save;        /* non-zero if at least one slot has data  */
static int blink_timer;
static int fade_timer;
static int fade_target;
static int fade_in_timer;
#define FADE_FRAMES 15

/* ---- Helpers ---- */

static void draw_bar(int y) {
    text_print(BAR_COL, y, "==============================");
}

static void draw_class_page(int cls) {
    /* Clear content area (rows 2-17) */
    for (int r = 2; r <= 17; r++) {
        text_clear_rect(0, r, 30, 1);
    }

    /* Class number indicator */
    text_print(SEL_COL, 2, "OPERATOR CLASS:");
    /* Print class index "1/3" etc */
    text_put_char(18, 2, (char)('0' + cls + 1));
    text_put_char(19, 2, '/');
    text_put_char(20, 2, (char)('0' + CLASS_COUNT));

    /* Class name — larger emphasis via spacing */
    {
        /* Pad to center within 26 cols */
        const char* nm = class_names[cls];
        int len = 0;
        while (nm[len]) len++;
        int pad = (26 - len) / 2;
        if (pad < 0) pad = 0;
        text_print(SEL_COL + pad, 3, nm);
    }

    draw_bar(4);

    /* Role and weapon */
    text_print(SEL_COL, 5, "ROLE:");
    text_print(SEL_COL + 6, 5, class_roles[cls]);

    text_print(SEL_COL, 6, "WEAPON:");
    text_print(SEL_COL + 8, 6, class_weapons[cls]);

    draw_bar(7);

    /* Base stats with visual bars — 2 rows of layout */
    /* Max reference values for bar scaling */
    {
        /* Row 8: HP bar (full width since HP range is large) */
        text_print(SEL_COL, 8, "HP");
        {
            int val = class_stats[cls][0];
            int filled = val * 8 / 40; /* scale to 8 chars */
            if (filled > 8) filled = 8;
            text_put_char(SEL_COL + 3, 8, '[');
            for (int b = 0; b < 8; b++)
                text_put_char(SEL_COL + 4 + b, 8, (b < filled) ? '=' : '-');
            text_put_char(SEL_COL + 12, 8, ']');
            text_print_int(SEL_COL + 14, 8, val);
        }

        /* Row 9: ATK and DEF side by side */
        static const char* lab2[] = { "ATK", "DEF" };
        static const int max2[] = { 10, 7 };
        for (int s = 0; s < 2; s++) {
            int col_base = SEL_COL + s * 14;
            int val = class_stats[cls][1 + s];
            int filled = val * 5 / max2[s];
            if (filled > 5) filled = 5;
            text_print(col_base, 9, lab2[s]);
            text_put_char(col_base + 4, 9, '[');
            for (int b = 0; b < 5; b++)
                text_put_char(col_base + 5 + b, 9, (b < filled) ? '=' : '-');
            text_put_char(col_base + 10, 9, ']');
            text_print_int(col_base + 11, 9, val);
        }

        /* Row 10: SPD and LCK side by side */
        static const char* lab3[] = { "SPD", "LCK" };
        static const int max3[] = { 8, 5 };
        for (int s = 0; s < 2; s++) {
            int col_base = SEL_COL + s * 14;
            int val = class_stats[cls][3 + s];
            int filled = val * 5 / max3[s];
            if (filled > 5) filled = 5;
            text_print(col_base, 10, lab3[s]);
            text_put_char(col_base + 4, 10, '[');
            for (int b = 0; b < 5; b++)
                text_put_char(col_base + 5 + b, 10, (b < filled) ? '=' : '-');
            text_put_char(col_base + 10, 10, ']');
            text_print_int(col_base + 11, 10, val);
        }
    }

    draw_bar(11);

    /* Abilities — 8 in 2 columns x 4 rows */
    text_print(SEL_COL, 12, "ABILITIES:");
    for (int a = 0; a < 4; a++) {
        int row = 13 + a;
        /* Left column: abilities 1-4 */
        text_put_char(SEL_COL, row, (char)('1' + a));
        text_put_char(SEL_COL + 1, row, ':');
        text_print(SEL_COL + 2, row, class_abilities[cls][a]);
        /* Right column: abilities 5-8 */
        text_put_char(SEL_COL + 14, row, (char)('5' + a));
        text_put_char(SEL_COL + 15, row, ':');
        text_print(SEL_COL + 16, row, class_abilities[cls][4 + a]);
    }

    /* Playstyle */
    text_print(SEL_COL, 17, "STYLE:");
    text_print(SEL_COL + 7, 17, class_playstyle[cls]);
}

static void draw_nav_bar(void) {
    /* Row 18: save file info or blank */
    text_clear_rect(0, 18, 30, 1);
    if (any_save) {
        if (cursor == 0) {
            text_print(0, 18, "B:BACK  A:SELECT  L/R:SWITCH");
        } else {
            /* Show which save slot is highlighted */
            text_print(SEL_COL, 18, "[B]:CANCEL  [A]:LOAD SAVE");
        }
    }

    /* Row 19: main input legend + class arrows */
    text_clear_rect(0, 19, 30, 1);
    if (cursor == 0) {
        text_print(0, 19, "[L]/[R]:SWITCH  [A]:JACK IN");
    } else {
        /* In save-select mode: show save slot highlight */
        text_print(0, 19, "[UP]/[DN]:SLOT  [A]:LOAD");
    }
}

/* ---- State callbacks ---- */

void state_charsel_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;

    /* Green-on-black terminal palette (same as title) */
    pal_bg_mem[0] = RGB15(0, 0, 0);
    pal_bg_mem[1] = RGB15(0, 28, 0);   /* bright green */

    text_clear_all();

    selected_class = 0;
    cursor = 0;
    blink_timer = 0;
    fade_timer = 0;
    fade_target = 0;
    fade_in_timer = FADE_FRAMES;
    REG_BLDCNT = BLD_BLACK | BLD_ALL;
    REG_BLDY = 16;

    /* Check for any existing saves */
    any_save = 0;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (save_slot_exists(i)) { any_save = 1; break; }
    }

    /* Header */
    text_print(3, 0, ">> JACK-IN CONFIGURATION <<");
    draw_bar(1);

    draw_class_page(selected_class);
    draw_nav_bar();

    audio_play_sfx(SFX_MENU_SELECT);
}

void state_charsel_update(void) {
    blink_timer++;

    /* Fade-out → state transition */
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

    if (cursor == 0) {
        /* Class selection mode */
        if (input_hit(KEY_LEFT) || input_hit(KEY_L)) {
            selected_class--;
            if (selected_class < 0) selected_class = CLASS_COUNT - 1;
            draw_class_page(selected_class);
            draw_nav_bar();
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_RIGHT) || input_hit(KEY_R)) {
            selected_class++;
            if (selected_class >= CLASS_COUNT) selected_class = 0;
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
            /* Load save from selected slot — preload restores ALL state
             * (player, quest, inventory, skills, stats, shop, bounty) */
            int slot = cursor - 1;
            if (save_slot_exists(slot)) {
                state_terminal_preload_slot(slot);
                audio_play_sfx(SFX_MENU_SELECT);
                fade_timer = FADE_FRAMES;
                fade_target = STATE_TERMINAL;
                REG_BLDCNT = BLD_BLACK | BLD_ALL;
            } else {
                /* Slot empty — bounce back */
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
    /* Blink the selected class name on row 3 */
    /* blink_timer is already incremented in update — do not double-count */
    if (cursor == 0) {
        if ((blink_timer >> 4) & 1) {
            /* Show a cursor arrow before class name */
            text_put_char(SEL_COL - 1, 3, '>');
        } else {
            text_put_char(SEL_COL - 1, 3, ' ');
        }
    }

    /* If in save mode, show save slot list on row 18+ */
    if (cursor > 0) {
        /* Show save slots on rows 16-19: header + 3 slots */
        text_clear_rect(0, 16, 30, 4);
        draw_bar(16);
        for (int s = 0; s < SAVE_SLOTS; s++) {
            int row = 17 + s;
            int slot_sel = (cursor - 1 == s);
            text_put_char(0, row, slot_sel ? '>' : ' ');
            text_print(2, row, "SLOT");
            text_put_char(7, row, (char)('0' + s + 1));
            text_print(9, row, ": ");
            static EWRAM_BSS SaveData sd;
            if (save_read_slot(&sd, s)) {
                static const char* const cn3[] = { "ASL", "INF", "TEC" };
                text_print(11, row, cn3[sd.player_class % 3]);
                text_print(15, row, "Lv");
                text_print_int(18, row, sd.player_level);
            } else {
                text_print(11, row, "EMPTY");
            }
        }
    }
}

void state_charsel_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY   = 0;
}
