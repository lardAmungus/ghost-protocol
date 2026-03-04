/*
 * Ghost Protocol — Title Screen
 *
 * Displays game title with cyberpunk green-on-black terminal aesthetic.
 * Shows "NEW GAME / CONTINUE" when saves exist; NEW GAME → charsel,
 * CONTINUE → slot picker → terminal (skips charsel).
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/save.h"
#include "states/state_ids.h"
#include "states/state_title.h"
#include "states/state_terminal.h"

/* ---- State ---- */
static int blink_timer;
static int has_save;         /* non-zero if at least one slot has data */
static int menu_cursor;      /* 0=NEW GAME, 1=CONTINUE (only used when has_save) */
static int slot_mode;        /* 0=main menu, 1=slot picker */
static int slot_cursor;      /* 0-2 when in slot picker */

/* ---- Helpers ---- */

static void draw_slot_row(int s, int row) {
    text_clear_rect(0, row, 30, 1);
    int sel = (s == slot_cursor);
    text_put_char(4, row, sel ? '>' : ' ');
    text_print(6, row, "SLOT");
    text_put_char(11, row, (char)('1' + s));
    text_print(13, row, ":");
    if (save_slot_exists(s)) {
        static EWRAM_BSS SaveData sd;
        if (save_read_slot(&sd, s)) {
            static const char* const cn[] = { "ASL", "INF", "TEC" };
            text_print(15, row, cn[sd.player_class % 3]);
            text_print(19, row, "Lv");
            text_print_int(22, row, sd.player_level);
            text_print(25, row, "M");
            text_print_int(26, row, sd.story_mission);
        }
    } else {
        text_print(15, row, "- EMPTY -");
    }
}

static void draw_menu(void) {
    text_clear_rect(0, 14, 30, 6);
    if (!has_save) {
        /* No saves: simple PRESS START */
        if ((blink_timer >> 4) & 1) {
            text_print(9, 16, "PRESS START");
        }
        return;
    }

    if (slot_mode) {
        text_print(4, 13, ">> SELECT SAVE SLOT <<");
        draw_slot_row(0, 15);
        draw_slot_row(1, 16);
        draw_slot_row(2, 17);
        text_print(5, 19, "A:Load  B:Back");
        return;
    }

    /* Main menu: NEW GAME / CONTINUE */
    text_put_char(6, 14, (menu_cursor == 0) ? '>' : ' ');
    text_print(8, 14, "NEW GAME");
    text_put_char(6, 15, (menu_cursor == 1) ? '>' : ' ');
    text_print(8, 15, "CONTINUE");
    text_print(5, 17, "UP/DN:Select  A:Confirm");
}

/* ---- State callbacks ---- */

void state_title_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    blink_timer = 0;
    menu_cursor = 0;
    slot_mode   = 0;
    slot_cursor = 0;
    state_terminal_reset(); /* Clear stale session state for new game */

    /* Check for existing saves */
    has_save = 0;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (save_slot_exists(i)) { has_save = 1; break; }
    }

    /* Set BG palette 0 to green-on-black (CRT terminal) */
    pal_bg_mem[0] = RGB15(0, 0, 0);     /* black background */
    pal_bg_mem[1] = RGB15(0, 28, 0);    /* bright green text */

    text_clear_all();

    /* Title text */
    text_print(5, 3, "G H O S T");
    text_print(3, 5, "P R O T O C O L");

    /* Decorative line */
    text_print(2, 7, "--------------------------");

    /* Subtitle */
    text_print(4, 9, ">> Jack in. Get out. <<");

    /* Version / flavor */
    text_print(8, 12, "v1.0 // 2026");

    audio_play_music(MUS_TITLE);
}

void state_title_update(void) {
    blink_timer++;

    if (slot_mode) {
        /* Slot picker navigation */
        if (input_hit(KEY_DOWN)) {
            if (slot_cursor < SAVE_SLOTS - 1) slot_cursor++;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_UP)) {
            if (slot_cursor > 0) slot_cursor--;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A)) {
            if (save_slot_exists(slot_cursor)) {
                state_terminal_preload_slot(slot_cursor);
                audio_play_sfx(SFX_MENU_SELECT);
                game_request_state = STATE_TERMINAL;
            } else {
                audio_play_sfx(SFX_MENU_BACK);
            }
        }
        if (input_hit(KEY_B)) {
            slot_mode = 0;
            audio_play_sfx(SFX_MENU_BACK);
        }
        return;
    }

    if (!has_save) {
        /* No saves — any confirm goes to charsel */
        if (input_hit(KEY_START) || input_hit(KEY_A)) {
            audio_play_sfx(SFX_MENU_SELECT);
            game_request_state = STATE_CHARSEL;
        }
        return;
    }

    /* Main menu */
    if (input_hit(KEY_DOWN) || input_hit(KEY_UP)) {
        menu_cursor ^= 1;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_START) || input_hit(KEY_A)) {
        audio_play_sfx(SFX_MENU_SELECT);
        if (menu_cursor == 0) {
            /* New Game */
            state_terminal_reset();
            game_request_state = STATE_CHARSEL;
        } else {
            /* Continue — enter slot picker */
            slot_mode = 1;
            /* Pre-select first valid slot */
            slot_cursor = 0;
            for (int i = 0; i < SAVE_SLOTS; i++) {
                if (save_slot_exists(i)) { slot_cursor = i; break; }
            }
        }
    }
}

void state_title_draw(void) {
    draw_menu();
}

void state_title_exit(void) {
    text_clear_all();
    /* Clean up blend registers */
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
