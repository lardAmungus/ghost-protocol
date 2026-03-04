/*
 * Ghost Protocol — Terminal Menu System
 */
#include "game/terminal.h"
#include "game/common.h"
#include "engine/text.h"
#include <string.h>

static const char* const menu_labels[TMENU_COUNT] = {
    "CONTRACTS",
    "SHOP",
    "INVENTORY",
    "STATS",
    "JACK IN",
    "SAVE/LOAD",
    "HELP/TIPS",
};

static int frame_counter;

void terminal_init_palette(void) {
    /* Bank 0: Phosphor green on dark — classic CRT feel */
    pal_bg_mem[0 * 16 + 0] = RGB15(1, 1, 2);
    pal_bg_mem[0 * 16 + 1] = RGB15(4, 28, 8);

    /* Bank 1: Amber highlights — warm accent */
    pal_bg_mem[1 * 16 + 0] = RGB15(1, 1, 2);
    pal_bg_mem[1 * 16 + 1] = RGB15(28, 22, 4);

    /* Bank 2: Cyan system messages — cool accent */
    pal_bg_mem[2 * 16 + 0] = RGB15(1, 1, 2);
    pal_bg_mem[2 * 16 + 1] = RGB15(4, 26, 28);

    /* Bank 3: Red warnings — alert */
    pal_bg_mem[3 * 16 + 0] = RGB15(1, 1, 2);
    pal_bg_mem[3 * 16 + 1] = RGB15(28, 8, 6);
}

void terminal_draw_menu(int cursor) {
    terminal_print_pal(2, 1, "GHOST PROTOCOL v1.0", TPAL_AMBER);
    text_print(2, 2, "-------------------");

    for (int i = 0; i < TMENU_COUNT; i++) {
        int row = 4 + i;
        if (i == cursor) {
            /* Blinking cursor: alternate ">" and "_" every 16 frames */
            if ((frame_counter >> 4) & 1) {
                text_print(3, row, "_");
            } else {
                text_print(3, row, ">");
            }
        } else {
            text_print(3, row, " ");
        }
        text_print(5, row, menu_labels[i]);
    }

    text_print(2, 18, "A:Select  B:Back");
}

int terminal_typewriter(int tx, int ty, const char* text, int* char_pos, int* timer) {
    int len = (int)strlen(text);
    if (*char_pos >= len) return 1;

    (*timer)++;
    if (*timer >= 2) {
        *timer = 0;
        /* Print next character */
        if (tx + *char_pos < 30) {
            text_put_char(tx + *char_pos, ty, text[*char_pos]);
        }
        (*char_pos)++;
    }
    return *char_pos >= len;
}

void terminal_clear(void) {
    text_clear_all();
}

void terminal_draw_border(int x, int y, int w, int h) {
    if (w < 2 || h < 2) return;
    /* Corners */
    text_put_char(x, y, '+');
    text_put_char(x + w - 1, y, '+');
    text_put_char(x, y + h - 1, '+');
    text_put_char(x + w - 1, y + h - 1, '+');
    /* Top and bottom edges */
    for (int i = 1; i < w - 1; i++) {
        text_put_char(x + i, y, '-');
        text_put_char(x + i, y + h - 1, '-');
    }
    /* Left and right edges */
    for (int j = 1; j < h - 1; j++) {
        text_put_char(x, y + j, '|');
        text_put_char(x + w - 1, y + j, '|');
    }
}

void terminal_print_pal(int tx, int ty, const char* str, int pal_bank) {
    u16 pal_bits = (u16)((pal_bank & 0xF) << 12);
    u16* sbb = (u16*)se_mem[31];
    int col = tx;
    int row = ty;
    int i = 0;
    while (str[i] != '\0' && (unsigned)row < 20) {
        if (str[i] == '\n') {
            row++;
            col = tx;
            i++;
            continue;
        }
        if ((unsigned)col < 30) {
            u16 tile_id = (u16)(str[i] - ' ');
            sbb[row * 32 + col] = tile_id | pal_bits;
        }
        col++;
        i++;
    }
}

int terminal_get_frame(void) {
    return frame_counter;
}

void terminal_tick(void) {
    frame_counter++;
}
