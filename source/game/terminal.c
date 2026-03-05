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
    "SKILLS",
    "CODEX",
    "CRAFT",
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
            /* Blinking cursor: visible 12 frames, hidden 4 frames (16-frame cycle) */
            if ((frame_counter & 15) >= 12) {
                text_print(3, row, " ");
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
        /* Only advance if character fits on screen — stop streaming at col 29 */
        if (tx + *char_pos < 30) {
            text_put_char(tx + *char_pos, ty, text[*char_pos]);
            (*char_pos)++;
        } else {
            /* Column full — skip remaining characters and signal done */
            *char_pos = len;
        }
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

/* ---- Animated circuit board background on BG1 ---- */

/* 6 simple 8x8 4bpp circuit tiles loaded into CBB1 */
static const u32 circuit_tiles[6][8] = {
    /* 0: dark fill (nearly empty) */
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

void terminal_load_bg(void) {
    /* Load circuit tiles into CBB1 (tile_mem[1]) */
    for (int t = 0; t < 6; t++) {
        memcpy(&tile_mem[1][t], circuit_tiles[t], 32);
    }

    /* Set up BG1 palette (bank 4): dark BG + dim green traces */
    pal_bg_mem[4 * 16 + 0] = RGB15(0, 0, 1);
    pal_bg_mem[4 * 16 + 1] = RGB15(1, 8, 2);

    /* Fill SBB28 with circuit pattern */
    u16* sbb = (u16*)se_mem[28];
    u16 pal_bits = (4 << 12); /* palette bank 4 */
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            int tile;
            /* Create a repeating circuit board pattern */
            int px = col % 8;
            int py = row % 8;
            if (px == 0 && py == 0) tile = 5;       /* cross at grid intersections */
            else if (px == 4 && py == 4) tile = 3;   /* node at midpoints */
            else if (py == 0) tile = 1;               /* horizontal trace on grid rows */
            else if (px == 0) tile = 2;               /* vertical trace on grid cols */
            else if (px == 4 && py < 4) tile = 2;    /* short vertical segment */
            else if (py == 4 && px < 4) tile = 1;    /* short horizontal segment */
            else tile = 0;                             /* dark fill */
            sbb[row * 32 + col] = (u16)(tile | pal_bits);
        }
    }

    /* Configure BG1: CBB1, SBB28, priority 3 (behind text) */
    REG_BG1CNT = BG_CBB(1) | BG_SBB(28) | BG_4BPP | BG_REG_32x32 | BG_PRIO(3);
}

void terminal_scroll_bg(void) {
    /* Slow diagonal scroll — 1px every 2 frames horizontal, 1px every 4 frames vertical */
    REG_BG1HOFS = (u16)(frame_counter >> 1);
    REG_BG1VOFS = (u16)(frame_counter >> 2);
}
