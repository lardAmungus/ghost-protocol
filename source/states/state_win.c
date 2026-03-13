/*
 * Ghost Protocol — Win State
 *
 * Displayed after defeating Root Access (Act 5 boss).
 * Shows epilogue (typewriter), credits, NG+ choice, and THE END.
 * Per-phase palette theming for visual pacing.
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "game/player.h"
#include "game/quest.h"
#include "game/common.h"
#include "states/state_ids.h"
#include "states/state_win.h"
#include "states/state_terminal.h"

enum {
    WIN_EPILOGUE = 0,
    WIN_CREDITS,
    WIN_NGPLUS,
    WIN_END,
};

static int phase;
static int timer;
static int blink_timer;
static int ng_cursor;

/* Typewriter state for epilogue */
static int epi_line;       /* current line being typed */
static int epi_char;       /* current char in line */
static int epi_timer;      /* frames since last char */

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
#define EPI_CHAR_SPEED 2  /* frames per character */
#define EPI_LINE_PAUSE 12 /* pause frames between lines */

/* Credits scroll state */
static int credit_scroll_y;  /* BG0 vertical scroll offset */

static void set_palette_epilogue(void) {
    pal_bg_mem[0] = RGB15(0, 0, 2);     /* deep dark blue */
    pal_bg_mem[1] = RGB15(8, 31, 20);   /* cool cyan-green */
}

static void set_palette_credits(void) {
    pal_bg_mem[0] = RGB15(1, 1, 0);     /* warm dark */
    pal_bg_mem[1] = RGB15(28, 22, 6);   /* warm amber */
}

static void set_palette_end(void) {
    pal_bg_mem[0] = RGB15(0, 0, 0);     /* black */
    pal_bg_mem[1] = RGB15(16, 20, 16);  /* dim green */
}

void state_win_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    timer = 0;
    blink_timer = 0;
    phase = WIN_EPILOGUE;
    ng_cursor = 0;
    epi_line = 0;
    epi_char = 0;
    epi_timer = 0;
    credit_scroll_y = 0;
    text_clear_all();

    set_palette_epilogue();

    /* Amber for highlights */
    pal_bg_mem[1 * 16 + 0] = RGB15(0, 0, 0);
    pal_bg_mem[1 * 16 + 1] = RGB15(28, 20, 0);

    audio_play_music(MUS_VICTORY);
}

static int epilogue_typewriter(void) {
    /* Returns 1 when all lines finished */
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
            /* Line done — pause then advance */
            epi_timer = -EPI_LINE_PAUSE; /* negative = pause */
            epi_line++;
            epi_char = 0;
        }
    }
    return 0;
}

static void draw_credits_scrolling(void) {
    /* Credits are placed at rows 22+ (off-screen bottom) and scroll up */
    /* We write all credit text once, then scroll BG0 */
    if (timer == 1) {
        /* Place credits below visible area (rows 22-31 wrap in 32-row BG) */
        text_print(6, 22, "- CREDITS -");
        text_print(6, 24, "Ghost Protocol");
        text_print(4, 26, "A GBA Game by You");
        text_print(4, 28, "Engine: libtonc");
        text_print(4, 29, "Audio: Maxmod");
        text_print(4, 30, "Build: devkitARM");

        /* Stats in visible area (rows 4-7) */
        text_print(4, 4, "MISSION COMPLETE");
        text_print(4, 6, "Level:");
        text_print_int(11, 6, player_state.level);

        int bosses = 0;
        for (int i = 0; i < 6; i++) {
            if (quest_state.boss_defeated[i]) bosses++;
        }
        text_print(4, 7, "Bosses:");
        text_print_int(12, 7, bosses);
        text_print(13, 7, "/6");

        text_print(4, 9, "Kills:");
        text_print_int(11, 9, (int)game_stats.total_kills);

        text_print(4, 10, "Deaths:");
        text_print_int(12, 10, (int)game_stats.total_deaths);

        /* Play time */
        {
            int total_secs = (int)(game_stats.play_time_frames / 60);
            int hrs = total_secs / 3600;
            int mins = (total_secs % 3600) / 60;
            text_print(4, 12, "Time:");
            text_print_int(10, 12, hrs);
            text_put_char(10 + (hrs >= 10 ? 2 : 1), 12, 'h');
            text_print_int(10 + (hrs >= 10 ? 3 : 2), 12, mins);
            text_put_char(10 + (hrs >= 10 ? 3 : 2) + (mins >= 10 ? 2 : 1), 12, 'm');
        }
    }
}

static void draw_ngplus(void) {
    text_print(5, 3, "WHAT COMES NEXT?");
    text_print(3, 6, (ng_cursor == 0) ? "> NEW GAME+" : "  NEW GAME+");
    text_print(5, 7, "Keep stats & gear,");
    text_print(5, 8, "enemies +50% HP/ATK");

    text_print(3, 10, (ng_cursor == 1) ? "> BUG BOUNTY" : "  BUG BOUNTY");
    text_print(5, 11, "Endless contracts,");
    text_print(5, 12, "scaling difficulty");

    text_print(3, 14, (ng_cursor == 2) ? "> TITLE SCREEN" : "  TITLE SCREEN");

    text_print(5, 18, "A:Select");
}

static void draw_end(void) {
    text_print(8, 8, "THE END");
    text_print(5, 10, "Thanks for playing!");
}

void state_win_update(void) {
    timer++;
    blink_timer++;

    if (phase == WIN_EPILOGUE) {
        int done = epilogue_typewriter();
        if (done && (timer > 180 || input_hit(KEY_A))) {
            phase = WIN_CREDITS;
            timer = 0;
            text_clear_all();
            REG_BG0VOFS = 0;
            set_palette_credits();
        } else if (input_hit(KEY_START)) {
            /* Skip epilogue — print all text immediately */
            for (int i = 0; i < EPI_LINES; i++) {
                if (epi_text[i][0] != '\0') {
                    text_print(epi_col[i], epi_row[i], epi_text[i]);
                }
            }
            epi_line = EPI_LINES;
        }
    } else if (phase == WIN_CREDITS) {
        /* Scroll credits upward — 1 pixel every 2 frames */
        if ((timer & 1) == 0) credit_scroll_y++;
        REG_BG0VOFS = (u16)credit_scroll_y;
    }
    if (phase == WIN_CREDITS && (timer > 240 || input_hit(KEY_A))) {
        phase = WIN_NGPLUS;
        timer = 0;
        ng_cursor = 0;
        text_clear_all();
        REG_BG0VOFS = 0;
        pal_bg_mem[0] = RGB15(0, 0, 0);
        pal_bg_mem[1] = RGB15(0, 31, 16);
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
                /* NG+: Keep stats, reset story progress */
                game_stats.ng_plus++;
                game_stats.endgame_unlocked = 1;
                quest_state.story_mission = 0;
                quest_state.current_act = 0;
                for (int i = 0; i < 6; i++) quest_state.boss_defeated[i] = 0;
                if (!ach_unlocked(ACH_NG_PLUS_CLEARED)) ach_unlock(ACH_NG_PLUS_CLEARED);
                state_terminal_save_current(0); /* Auto-save NG+ state */
                game_request_state = STATE_TERMINAL;
            } else if (ng_cursor == 1) {
                /* Endless Bug Bounty */
                game_stats.endgame_unlocked = 1;
                if (!ach_unlocked(ACH_ENDGAME)) ach_unlock(ACH_ENDGAME);
                state_terminal_save_current(0); /* Auto-save endgame state */
                game_request_state = STATE_TERMINAL;
            } else {
                /* Title screen */
                phase = WIN_END;
                timer = 0;
                text_clear_all();
                set_palette_end();
            }
        }
    } else if (phase == WIN_END && (input_hit(KEY_START) || input_hit(KEY_A))) {
        audio_play_sfx(SFX_MENU_SELECT);
        game_request_state = STATE_TITLE;
    }
}

void state_win_draw(void) {
    switch (phase) {
    case WIN_EPILOGUE:
        /* Typewriter handled in update */
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
