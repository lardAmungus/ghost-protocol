/*
 * Ghost Protocol — Title Screen
 *
 * Displays game title with cyberpunk green-on-black terminal aesthetic.
 * Shows "NEW GAME / CONTINUE" when saves exist; NEW GAME → charsel,
 * CONTINUE → slot picker → terminal (skips charsel).
 */
#include <tonc.h>
#include <string.h>
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
static int scroll_timer;     /* BG1 circuit scroll */
static int type_timer;       /* Typewriter reveal timer */
static int type_phase;       /* 0=title, 1=subtitle, 2=done */
static int type_pos;         /* Current char position in typewriter */
static int fade_timer;       /* >0 = fading out before state switch */
static int fade_target;      /* STATE_* to switch to after fade */
static int fade_in_timer;    /* >0 = fading in on enter */

/* ---- Circuit BG tiles (reused from terminal) ---- */
static const u32 circuit_tiles[6][8] = {
    /* 0: dark fill */
    { 0x00000000, 0x00000000, 0x00000000, 0x00000000,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    /* 1: horizontal trace */
    { 0x00000000, 0x00000000, 0x00000000, 0x11111111,
      0x11111111, 0x00000000, 0x00000000, 0x00000000 },
    /* 2: vertical trace */
    { 0x00010000, 0x00010000, 0x00010000, 0x00010000,
      0x00010000, 0x00010000, 0x00010000, 0x00010000 },
    /* 3: node (junction dot) */
    { 0x00000000, 0x00000000, 0x00111000, 0x00111000,
      0x00111000, 0x00000000, 0x00000000, 0x00000000 },
    /* 4: corner (L-shape) */
    { 0x00010000, 0x00010000, 0x00010000, 0x00011111,
      0x00000000, 0x00000000, 0x00000000, 0x00000000 },
    /* 5: cross */
    { 0x00010000, 0x00010000, 0x00010000, 0x11111111,
      0x00010000, 0x00010000, 0x00010000, 0x00010000 },
};

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

/* Typewriter text strings */
static const char* type_lines[] = {
    "G H O S T",           /* row 3, col 5 */
    "P R O T O C O L",     /* row 5, col 3 */
};
static const int type_cols[] = { 5, 3 };
static const int type_rows[] = { 3, 5 };
#define TYPE_LINE_COUNT 2
#define TYPE_SPEED 3  /* frames per character */
#define FADE_FRAMES 15

static void start_fade_out(int target_state) {
    if (fade_timer > 0) return; /* already fading */
    fade_timer = FADE_FRAMES;
    fade_target = target_state;
    REG_BLDCNT = BLD_BLACK | BLD_ALL;
}

static void load_circuit_bg(void) {
    /* Load 6 circuit tiles into CBB1 (tile indices 0-5) */
    for (int t = 0; t < 6; t++) {
        memcpy16(&tile_mem[1][t], circuit_tiles[t], sizeof(circuit_tiles[0]) / 2);
    }

    /* Set circuit palette (bank 4): dark green scheme */
    pal_bg_mem[4 * 16 + 0] = RGB15(0, 0, 0);     /* transparent/dark */
    pal_bg_mem[4 * 16 + 1] = RGB15(0, 6, 2);      /* dim green traces */

    /* Fill SBB28 with circuit pattern */
    u16* sbb = (u16*)se_mem[28];
    u16 pal_bits = (4 << 12);
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            int tile;
            int px = col % 8;
            int py = row % 8;
            if (px == 0 && py == 0) tile = 5;
            else if (px == 4 && py == 4) tile = 3;
            else if (py == 0) tile = 1;
            else if (px == 0) tile = 2;
            else if (px == 4 && py < 4) tile = 2;
            else if (py == 4 && px < 4) tile = 1;
            else tile = 0;
            sbb[row * 32 + col] = (u16)(tile | pal_bits);
        }
    }

    REG_BG1CNT = BG_CBB(1) | BG_SBB(28) | BG_4BPP | BG_REG_32x32 | BG_PRIO(3);
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
}

void state_title_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;
    blink_timer = 0;
    menu_cursor = 0;
    slot_mode   = 0;
    slot_cursor = 0;
    scroll_timer = 0;
    type_timer = 0;
    type_phase = 0;
    type_pos = 0;
    fade_timer = 0;
    fade_target = 0;
    fade_in_timer = FADE_FRAMES;
    REG_BLDCNT = BLD_BLACK | BLD_ALL;
    REG_BLDY = 16; /* Start fully dark, fade in */
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

    /* Load animated circuit board background on BG1 */
    load_circuit_bg();

    /* Static elements (visible immediately) */
    text_print(2, 7, "--------------------------");
    text_print(8, 12, "v1.0 // 2026");

    /* Subtitle appears after title typewriter finishes */

    audio_play_music(MUS_TITLE);
}

void state_title_update(void) {
    blink_timer++;

    /* Handle fade-out → state transition */
    if (fade_timer > 0) {
        fade_timer--;
        REG_BLDY = (u16)(16 - (fade_timer * 16 / FADE_FRAMES));
        if (fade_timer == 0) {
            game_request_state = fade_target;
        }
        return;
    }

    /* Handle fade-in */
    if (fade_in_timer > 0) {
        fade_in_timer--;
        REG_BLDY = (u16)(fade_in_timer * 16 / FADE_FRAMES);
        if (fade_in_timer == 0) {
            REG_BLDCNT = 0;
            REG_BLDY = 0;
        }
    }

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
                start_fade_out(STATE_TERMINAL);
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
            start_fade_out(STATE_CHARSEL);
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
            start_fade_out(STATE_CHARSEL);
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
    /* Scroll circuit BG */
    scroll_timer++;
    REG_BG1HOFS = (u16)(scroll_timer >> 1);
    REG_BG1VOFS = (u16)(scroll_timer >> 2);

    /* Typewriter text reveal */
    if (type_phase < TYPE_LINE_COUNT) {
        type_timer++;
        if (type_timer >= TYPE_SPEED) {
            type_timer = 0;
            const char* line = type_lines[type_phase];
            int len = 0;
            while (line[len]) len++;
            if (type_pos < len) {
                text_put_char(type_cols[type_phase] + type_pos, type_rows[type_phase], line[type_pos]);
                type_pos++;
            } else {
                /* Line complete, advance to next */
                type_phase++;
                type_pos = 0;
                if (type_phase >= TYPE_LINE_COUNT) {
                    /* All title lines done — show subtitle */
                    text_print(4, 9, ">> Jack in. Get out. <<");
                }
            }
        }
    }

    draw_menu();
}

void state_title_exit(void) {
    text_clear_all();
    /* Clean up blend registers */
    REG_BLDCNT = 0;
    REG_BLDY = 0;
    /* Clean up BG1 */
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
}
