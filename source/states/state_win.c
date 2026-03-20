/*
 * Ghost Protocol — Win State
 *
 * Displayed after defeating the final boss.
 * Circuit BG with palette shifts per phase (blue, amber, cyan, green).
 * Per-act recap before credits, separate stats phase from credits.
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "game/player.h"
#include "game/quest.h"
#include "game/common.h"
#include "game/terminal.h"
#include "states/state_ids.h"
#include "states/state_win.h"
#include "states/state_terminal.h"

enum {
    WIN_EPILOGUE = 0,
    WIN_RECAP,
    WIN_STATS,
    WIN_CREDITS,
    WIN_NGPLUS,
    WIN_END,
};

static int phase;
static int timer;
static int blink_timer;
static int ng_cursor;
static int recap_act;      /* Current act being recapped (0-5) */

/* Typewriter state for epilogue */
static int epi_line;
static int epi_char;
static int epi_timer;

/* Epilogue text data */
static const char* epi_text[] = {
    "GHOST PROTOCOL",
    "ACTIVATED",
    "",
    "Root Access defeated.",
    "The rogue AI is gone.",
    "The Net is free.",
    "",
    "Your legend grows...",
};
static const int epi_col[] = { 5, 7, 0, 3, 3, 3, 0, 3 };
static const int epi_row[] = { 3, 5, 0, 8, 10, 11, 0, 13 };
#define EPI_LINES 8
#define EPI_CHAR_SPEED 2
#define EPI_LINE_PAUSE 12

/* Credits scroll state */
static int credit_scroll_y;

/* Act recap data */
static const char* const act_names[6] = {
    "ACT 1: THE GLITCH",
    "ACT 2: TRACEBACK",
    "ACT 3: DEEP PACKET",
    "ACT 4: ZERO DAY",
    "ACT 5: GHOST PROTOCOL",
    "ACT 6: TRACE ROUTE",
};
static const char* const act_summaries[6] = {
    "The first anomalies detected.",
    "Tracing the rogue signals.",
    "Deep into enemy networks.",
    "Zero-day exploits deployed.",
    "The ghost protocol activated.",
    "Final trace. End of the line.",
};

/* Phase palette setters */
static void set_palette_blue(void) {
    pal_bg_mem[0] = RGB15(0, 0, 2);
    pal_bg_mem[1] = RGB15(8, 31, 20);
    /* BG1 circuit: blue tint */
    pal_bg_mem[4 * 16 + 0] = RGB15(0, 0, 2);
    pal_bg_mem[4 * 16 + 1] = RGB15(2, 8, 16);
}

static void set_palette_amber(void) {
    pal_bg_mem[0] = RGB15(1, 1, 0);
    pal_bg_mem[1] = RGB15(28, 22, 6);
    /* BG1 circuit: amber tint */
    pal_bg_mem[4 * 16 + 0] = RGB15(1, 1, 0);
    pal_bg_mem[4 * 16 + 1] = RGB15(12, 8, 2);
}

static void set_palette_cyan(void) {
    pal_bg_mem[0] = RGB15(0, 1, 2);
    pal_bg_mem[1] = RGB15(4, 26, 28);
    /* BG1 circuit: cyan tint */
    pal_bg_mem[4 * 16 + 0] = RGB15(0, 1, 2);
    pal_bg_mem[4 * 16 + 1] = RGB15(2, 10, 12);
}

static void set_palette_green(void) {
    pal_bg_mem[0] = RGB15(0, 0, 0);
    pal_bg_mem[1] = RGB15(4, 28, 8);
    /* BG1 circuit: green tint */
    pal_bg_mem[4 * 16 + 0] = RGB15(0, 1, 0);
    pal_bg_mem[4 * 16 + 1] = RGB15(1, 8, 2);
}

static int epilogue_typewriter(void) {
    if (epi_line >= EPI_LINES) return 1;

    epi_timer++;

    /* Empty lines — skip after short pause */
    if (epi_text[epi_line][0] == '\0') {
        if (epi_timer >= EPI_LINE_PAUSE) {
            epi_line++;
            epi_char = 0;
            epi_timer = 0;
        }
        return 0;
    }

    if (epi_timer >= EPI_CHAR_SPEED) {
        epi_timer = 0;
        char ch = epi_text[epi_line][epi_char];
        if (ch != '\0') {
            text_put_char(epi_col[epi_line] + epi_char, epi_row[epi_line], ch);
            epi_char++;
        } else {
            epi_timer = -EPI_LINE_PAUSE;
            epi_line++;
            epi_char = 0;
        }
    }
    return 0;
}

static void draw_recap(void) {
    terminal_draw_panel(2, 2, 17, 27);

    /* Act header */
    if (recap_act < 6) {
        terminal_print_pal(4, 3, act_names[recap_act], TPAL_AMBER);

        /* Boss defeated status */
        if (quest_state.boss_defeated[recap_act]) {
            terminal_print_pal(4, 5, "BOSS: DEFEATED", TPAL_CYAN);
        } else {
            text_print(4, 5, "BOSS: ---");
        }

        /* Act summary */
        text_print(4, 7, act_summaries[recap_act]);

        /* Page indicator */
        text_print(4, 15, "Act");
        text_print_int(8, 15, recap_act + 1);
        text_print(9, 15, "/6");
        text_print(14, 15, "A:Next");
    }
}

static void draw_stats(void) {
    terminal_draw_panel(1, 1, 18, 28);
    terminal_print_pal(4, 2, "MISSION COMPLETE", TPAL_AMBER);

    text_print(4, 4, "Level:");
    text_print_int(14, 4, player_state.level);

    int bosses = 0;
    for (int i = 0; i < 6; i++) {
        if (quest_state.boss_defeated[i]) bosses++;
    }
    text_print(4, 5, "Bosses:");
    text_print_int(14, 5, bosses);
    text_print(15, 5, "/6");

    text_print(4, 7, "Kills:");
    text_print_int(14, 7, (int)game_stats.total_kills);

    text_print(4, 8, "Deaths:");
    text_print_int(14, 8, (int)game_stats.total_deaths);

    text_print(4, 9, "Dmg Dealt:");
    text_print_int(16, 9, (int)game_stats.damage_dealt);

    text_print(4, 10, "Items Found:");
    text_print_int(17, 10, (int)game_stats.items_found);

    text_print(4, 11, "Best Combo:");
    text_print_int(16, 11, (int)game_stats.highest_combo);

    /* Play time */
    {
        int total_secs = (int)(game_stats.play_time_frames / 60);
        int hrs = total_secs / 3600;
        int mins = (total_secs % 3600) / 60;
        text_print(4, 13, "Time:");
        text_print_int(14, 13, hrs);
        text_put_char(14 + (hrs >= 10 ? 2 : 1), 13, 'h');
        text_print_int(14 + (hrs >= 10 ? 3 : 2), 13, mins);
        text_put_char(14 + (hrs >= 10 ? 3 : 2) + (mins >= 10 ? 2 : 1), 13, 'm');
    }

    /* Achievement count */
    {
        int ach_count = 0;
        for (int i = 0; i < ACH_COUNT; i++) {
            if (ach_unlocked(i)) ach_count++;
        }
        text_print(4, 15, "Achievements:");
        text_print_int(18, 15, ach_count);
        text_put_char(20, 15, '/');
        text_print_int(21, 15, ACH_COUNT);
    }

    terminal_print_pal(8, 17, "A:Continue", TPAL_GREEN);
}

static void draw_credits_scrolling(void) {
    if (timer == 1) {
        /* Write credits below visible area (rows 22-31 in 32-row BG) */
        {
            static const struct { int col; int row; const char* str; } cred[] = {
                {8, 22, "- CREDITS -"},
                {6, 24, "Ghost Protocol"},
                {4, 26, "A GBA Game by You"},
                {4, 28, "Engine: libtonc"},
                {4, 29, "Audio: Maxmod"},
                {4, 30, "Build: devkitARM"},
            };
            for (int c = 0; c < 6; c++) {
                const char* s = cred[c].str;
                int x = cred[c].col;
                int y = cred[c].row & 31;
                while (*s) {
                    int ch = *s - ' ';
                    if (ch < 0 || ch >= 95) ch = 0;
                    se_mem[31][y * 32 + x] = (u16)(ch);
                    x++; s++;
                }
            }
        }
    }
}

static void draw_ngplus(void) {
    terminal_draw_panel(2, 2, 17, 27);
    terminal_print_pal(5, 3, "WHAT COMES NEXT?", TPAL_AMBER);

    text_print(4, 6, (ng_cursor == 0) ? "> NEW GAME+" : "  NEW GAME+");
    text_print(6, 7, "Keep stats & gear,");
    text_print(6, 8, "enemies +50% HP/ATK");

    text_print(4, 10, (ng_cursor == 1) ? "> BUG BOUNTY" : "  BUG BOUNTY");
    text_print(6, 11, "Endless contracts,");
    text_print(6, 12, "scaling difficulty");

    text_print(4, 14, (ng_cursor == 2) ? "> TITLE SCREEN" : "  TITLE SCREEN");

    text_print(6, 16, "A:Select");
}

static void draw_end(void) {
    terminal_draw_panel(5, 4, 14, 25);
    terminal_print_pal(9, 7, "THE END", TPAL_AMBER);
    text_print(6, 9, "Thanks for playing!");
    terminal_print_pal(6, 11, "GHOST PROTOCOL", TPAL_GREEN);
}

void state_win_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;
    timer = 0;
    blink_timer = 0;
    phase = WIN_EPILOGUE;
    ng_cursor = 0;
    recap_act = 0;
    epi_line = 0;
    epi_char = 0;
    epi_timer = 0;
    credit_scroll_y = 0;
    text_clear_all();

    /* Terminal palettes for colored text */
    terminal_init_palette();

    /* Load circuit board BG */
    terminal_load_bg();

    /* Start with blue palette */
    set_palette_blue();

    audio_play_music(MUS_VICTORY);
}

void state_win_update(void) {
    timer++;
    blink_timer++;
    terminal_tick();

    if (phase == WIN_EPILOGUE) {
        int done = epilogue_typewriter();
        if (done && (timer > 180 || input_hit(KEY_A))) {
            phase = WIN_RECAP;
            timer = 0;
            recap_act = 0;
            text_clear_all();
            set_palette_amber();
        } else if (input_hit(KEY_START)) {
            /* Skip epilogue */
            for (int i = 0; i < EPI_LINES; i++) {
                if (epi_text[i][0] != '\0') {
                    text_print(epi_col[i], epi_row[i], epi_text[i]);
                }
            }
            epi_line = EPI_LINES;
        }
    } else if (phase == WIN_RECAP) {
        if (input_hit(KEY_A) || input_hit(KEY_START)) {
            recap_act++;
            text_clear_all();
            audio_play_sfx(SFX_MENU_SELECT);
            if (recap_act >= 6) {
                phase = WIN_STATS;
                timer = 0;
                text_clear_all();
                set_palette_cyan();
            }
        }
    } else if (phase == WIN_STATS) {
        if (input_hit(KEY_A) || (timer > 300)) {
            phase = WIN_CREDITS;
            timer = 0;
            text_clear_all();
            REG_BG0VOFS = 0;
            set_palette_amber();
        }
    } else if (phase == WIN_CREDITS) {
        /* Scroll credits upward */
        if ((timer & 1) == 0) credit_scroll_y++;
        REG_BG0VOFS = (u16)credit_scroll_y;

        if (timer > 240 || input_hit(KEY_A)) {
            phase = WIN_NGPLUS;
            timer = 0;
            ng_cursor = 0;
            text_clear_all();
            REG_BG0VOFS = 0;
            set_palette_green();
        }
    } else if (phase == WIN_NGPLUS) {
        if (input_hit(KEY_DOWN) && ng_cursor < 2) {
            ng_cursor++;
            audio_play_sfx(SFX_MENU_SELECT);
            text_clear_all();
        }
        if (input_hit(KEY_UP) && ng_cursor > 0) {
            ng_cursor--;
            audio_play_sfx(SFX_MENU_SELECT);
            text_clear_all();
        }
        if (input_hit(KEY_A)) {
            audio_play_sfx(SFX_MENU_SELECT);
            if (ng_cursor == 0) {
                /* NG+ */
                game_stats.ng_plus++;
                game_stats.endgame_unlocked = 1;
                quest_state.story_mission = 0;
                quest_state.current_act = 0;
                for (int i = 0; i < 6; i++) quest_state.boss_defeated[i] = 0;
                if (!ach_unlocked(ACH_NG_PLUS_CLEARED)) ach_unlock(ACH_NG_PLUS_CLEARED);
                state_terminal_save_current(0);
                game_request_state = STATE_TERMINAL;
            } else if (ng_cursor == 1) {
                /* Endless Bug Bounty */
                game_stats.endgame_unlocked = 1;
                if (!ach_unlocked(ACH_ENDGAME)) ach_unlock(ACH_ENDGAME);
                state_terminal_save_current(0);
                game_request_state = STATE_TERMINAL;
            } else {
                /* Title screen */
                phase = WIN_END;
                timer = 0;
                text_clear_all();
                set_palette_green();
            }
        }
    } else if (phase == WIN_END && (input_hit(KEY_START) || input_hit(KEY_A))) {
        audio_play_sfx(SFX_MENU_SELECT);
        game_request_state = STATE_TITLE;
    }
}

void state_win_draw(void) {
    /* Scroll circuit BG */
    terminal_scroll_bg();

    switch (phase) {
    case WIN_EPILOGUE:
        /* Typewriter handled in update */
        break;
    case WIN_RECAP:
        draw_recap();
        break;
    case WIN_STATS:
        draw_stats();
        break;
    case WIN_CREDITS:
        draw_credits_scrolling();
        break;
    case WIN_NGPLUS:
        draw_ngplus();
        break;
    case WIN_END:
        draw_end();
        if ((blink_timer >> 4) & 1) {
            text_print(7, 16, "PRESS START");
        } else {
            text_clear_rect(7, 16, 12, 1);
        }
        break;
    }
}

void state_win_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY = 0;
    REG_BG0VOFS = 0;
}
