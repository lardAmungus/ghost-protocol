/*
 * Ghost Protocol — Character Customization Screen
 *
 * Players choose suit and visor color before starting a new game.
 * Class selection has been removed — the player starts classless and
 * picks a T1 class at level 5 via the in-game terminal hub.
 * Also handles save-slot loading.
 */
#include <tonc.h>
#include "states/state_charsel.h"
#include "states/state_ids.h"
#include "states/state_terminal.h"
#include "game/player.h"
#include "game/common.h"
#include "game/terminal.h"
#include "game/colors.h"
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/save.h"
#include "engine/sprite.h"
#include <string.h>

/* ---- State ---- */
static int suit_cursor;     /* 0-23 suit color index */
static int visor_cursor;    /* 0-23 visor color index */
static int mode;            /* 0=suit, 1=visor, 2=confirm (unused, kept for future) */
static int cursor;          /* 0=customize, 1..SAVE_SLOTS=load slots */
static int any_save;
static int blink_timer;
static int fade_timer;
static int fade_target;
static int fade_in_timer;
static int preview_oam;    /* OAM index for player sprite preview (-1 = none) */
#define FADE_FRAMES 15
#define PREVIEW_X  112     /* Center of 240px screen - 16px sprite / 2 */
#define PREVIEW_Y   88     /* Roughly center vertically in the preview area */

/* ---- Local helpers ---- */

/* Return a short tier/class label for a save slot preview. */
static const char* tier_preview(const SaveData* sd) {
    if (sd->tier_choices[2] != 0xFF) return "T3";
    if (sd->tier_choices[1] != 0xFF) return "T2";
    if (sd->tier_choices[0] != 0xFF) {
        static const char* const t1n[] = { "TRO", "INF", "TEC" };
        if (sd->tier_choices[0] < 3) return t1n[sd->tier_choices[0]];
        return "T1";
    }
    return "CLS"; /* Classless */
}

/* Print two-digit zero-padded number at (tx, ty). */
static void print_two_digit(int tx, int ty, int val) {
    if (val < 0) val = 0;
    if (val > 99) val = 99;
    text_put_char(tx,     ty, (char)('0' + val / 10));
    text_put_char(tx + 1, ty, (char)('0' + val % 10));
}

static void refresh_preview(void) {
    if (preview_oam >= 0) {
        sprite_free(preview_oam);
        preview_oam = -1;
    }
    preview_oam = player_load_preview(PREVIEW_X, PREVIEW_Y, suit_cursor, visor_cursor);
}

static void draw_customization(void) {
    terminal_draw_panel(0, 0, 19, 29);

    terminal_print_pal(4, 0, "JACK-IN CONFIGURATION", TPAL_AMBER);
    text_print(4, 1, "---------------------");

    /* Suit color row */
    text_print(2, 3, "SUIT COLOR:");
    if (mode == 0) {
        /* Active — show >> XX << */
        text_print(14, 3, ">> ");
        print_two_digit(17, 3, suit_cursor);
        text_print(19, 3, " <<");
    } else {
        text_print(14, 3, "[ ");
        print_two_digit(16, 3, suit_cursor);
        text_print(18, 3, " ]");
    }

    /* Visor color row */
    text_print(2, 4, "VISOR COLOR:");
    if (mode == 1) {
        /* Active — show >> XX << */
        text_print(14, 4, ">> ");
        print_two_digit(17, 4, visor_cursor);
        text_print(19, 4, " <<");
    } else {
        text_print(14, 4, "[ ");
        print_two_digit(16, 4, visor_cursor);
        text_print(18, 4, " ]");
    }

    /* Color name labels */
    text_print(2, 6, "Suit: ");
    text_print_int(8, 6, suit_cursor);
    text_print(2, 7, "Visor:");
    text_print_int(8, 7, visor_cursor);

    /* Navigation hints */
    text_clear_rect(2, 16, 26, 3);
    text_print(2, 16, "L/R: Change Color");
    if (mode == 0) {
        text_print(2, 17, "A: Confirm Suit  B: Title");
        if (any_save) {
            text_print(2, 18, "DOWN: Load Save");
        }
    } else {
        text_print(2, 17, "A: Start Game    B: Back");
    }
}

static void draw_nav_bar(void) {
    text_clear_rect(2, 16, 26, 3);
    if (cursor == 0) {
        text_print(2, 16, "L/R: Change Color");
        if (mode == 0) {
            text_print(2, 17, "A: Confirm Suit  B: Title");
            if (any_save) {
                text_print(2, 18, "DOWN: Load Save");
            }
        } else {
            text_print(2, 17, "A: Start Game    B: Back");
        }
    } else {
        text_print(2, 17, "UP/DN:Slot  A:Load");
        text_print(2, 18, "B:Cancel");
    }
}

/* ---- State callbacks ---- */

void state_charsel_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ | DCNT_OBJ_1D;

    terminal_init_palette();
    terminal_load_bg();
    text_clear_all();

    suit_cursor  = 0;
    visor_cursor = 0;
    mode         = 0;
    cursor       = 0;
    blink_timer  = 0;
    fade_timer   = 0;
    fade_target  = 0;
    fade_in_timer = FADE_FRAMES;
    REG_BLDCNT = BLD_BLACK | BLD_ALL;
    REG_BLDY   = 16;

    any_save = 0;
    for (int i = 0; i < SAVE_SLOTS; i++) {
        if (save_slot_exists(i)) { any_save = 1; break; }
    }

    preview_oam = -1;
    refresh_preview();

    draw_customization();
    draw_nav_bar();

    audio_play_sfx(SFX_MENU_SELECT);
}

void state_charsel_update(void) {
    blink_timer++;
    terminal_tick();

    /* Fade-out: count down, then trigger transition */
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
            REG_BLDY   = 0;
        }
    }

    if (cursor == 0) {
        /* Customization mode */
        if (mode == 0) {
            /* Suit color selection */
            if (input_hit(KEY_LEFT) || input_hit(KEY_L)) {
                suit_cursor--;
                if (suit_cursor < 0) suit_cursor = COLOR_PRESET_COUNT - 1;
                draw_customization();
                refresh_preview();
                audio_play_sfx(SFX_MENU_SELECT);
            }
            if (input_hit(KEY_RIGHT) || input_hit(KEY_R)) {
                suit_cursor++;
                if (suit_cursor >= COLOR_PRESET_COUNT) suit_cursor = 0;
                draw_customization();
                refresh_preview();
                audio_play_sfx(SFX_MENU_SELECT);
            }
            if (input_hit(KEY_A)) {
                mode = 1;
                draw_customization();
                audio_play_sfx(SFX_MENU_SELECT);
            }
            if (input_hit(KEY_B)) {
                audio_play_sfx(SFX_MENU_BACK);
                fade_timer  = FADE_FRAMES;
                fade_target = STATE_TITLE;
                REG_BLDCNT  = BLD_BLACK | BLD_ALL;
            }
            if (any_save && (input_hit(KEY_DOWN) || input_hit(KEY_UP))) {
                cursor = 1;
                draw_nav_bar();
                audio_play_sfx(SFX_MENU_SELECT);
            }
        } else {
            /* Visor color selection (mode == 1) */
            if (input_hit(KEY_LEFT) || input_hit(KEY_L)) {
                visor_cursor--;
                if (visor_cursor < 0) visor_cursor = COLOR_PRESET_COUNT - 1;
                draw_customization();
                refresh_preview();
                audio_play_sfx(SFX_MENU_SELECT);
            }
            if (input_hit(KEY_RIGHT) || input_hit(KEY_R)) {
                visor_cursor++;
                if (visor_cursor >= COLOR_PRESET_COUNT) visor_cursor = 0;
                draw_customization();
                refresh_preview();
                audio_play_sfx(SFX_MENU_SELECT);
            }
            if (input_hit(KEY_A)) {
                /* Confirm — start new game classless */
                player_init(0xFF); /* classless; zeroes entire struct */
                player_state.suit_color  = (u8)suit_cursor;
                player_state.visor_color = (u8)visor_cursor;
                audio_play_sfx(SFX_MENU_SELECT);
                fade_timer  = FADE_FRAMES;
                fade_target = STATE_TERMINAL;
                REG_BLDCNT  = BLD_BLACK | BLD_ALL;
            }
            if (input_hit(KEY_B)) {
                mode = 0;
                draw_customization();
                audio_play_sfx(SFX_MENU_BACK);
            }
        }
    } else {
        /* Save-slot browse mode: cursor 1-SAVE_SLOTS = slot 0-(SAVE_SLOTS-1) */
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
                fade_timer  = FADE_FRAMES;
                fade_target = STATE_TERMINAL;
                REG_BLDCNT  = BLD_BLACK | BLD_ALL;
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
    /* Animate circuit board BG */
    terminal_scroll_bg();

    /* Blink cursor on active color value */
    if (cursor == 0) {
        int blink_col;
        int blink_row;
        if (mode == 0) {
            blink_col = 13;
            blink_row = 3;
        } else {
            blink_col = 13;
            blink_row = 4;
        }
        if ((blink_timer >> 4) & 1) {
            text_put_char(blink_col, blink_row, '>');
        } else {
            text_put_char(blink_col, blink_row, ' ');
        }
    }

    /* Save slot overlay */
    if (cursor > 0) {
        static EWRAM_BSS SaveData sd;
        text_clear_rect(1, 16, 28, 3);
        text_print(2, 16, "---  LOAD SAVE  ---");
        for (int s = 0; s < SAVE_SLOTS; s++) {
            int row = 17 + s;
            if (row > 18) break;
            int slot_sel = (cursor - 1 == s);
            text_put_char(2, row, slot_sel ? '>' : ' ');
            text_print(4, row, "S");
            text_put_char(5, row, (char)('0' + s + 1));
            text_put_char(6, row, ':');
            if (save_read_slot(&sd, s)) {
                text_print(8, row, tier_preview(&sd));
                text_print(12, row, "Lv");
                text_print_int(14, row, (int)sd.player_level);
            } else {
                text_print(8, row, "EMPTY");
            }
        }
    }
}

void state_charsel_exit(void) {
    if (preview_oam >= 0) {
        sprite_free(preview_oam);
        preview_oam = -1;
    }
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY   = 0;
}
