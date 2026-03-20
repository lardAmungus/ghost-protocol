#ifndef GAME_TERMINAL_H
#define GAME_TERMINAL_H

#include <tonc.h>

/*
 * Ghost Protocol — Terminal Menu System
 *
 * Green-on-black CRT terminal aesthetic.
 * Typewriter text effect, cursor navigation.
 */

/* Terminal menu items — 8 options in 3 groups */
enum {
    TMENU_CONTRACTS = 0,
    TMENU_JACK_IN,
    TMENU_BUGBOUNTY_OPT,
    TMENU_SHOP,
    TMENU_INVENTORY,
    TMENU_CRAFT,
    TMENU_SKILLS,
    TMENU_SAVE,
    TMENU_STATS_CODEX,
    TMENU_COUNT
};

/* Group header rows (non-selectable) */
#define TMENU_GRP_OPS    0  /* OPERATIONS group */
#define TMENU_GRP_SYS    1  /* SYSTEMS group */
#define TMENU_GRP_DATA   2  /* DATA group */

/* Terminal palette banks (BG0) */
#define TPAL_GREEN   0   /* Green text on black */
#define TPAL_AMBER   1   /* Amber highlights */
#define TPAL_CYAN    2   /* Cyan system messages */
#define TPAL_RED     3   /* Red warnings */

/* Initialize terminal palettes. */
void terminal_init_palette(void);

/* Draw the main terminal menu with cursor (blinking cursor effect). */
void terminal_draw_menu(int cursor);

/* Get the screen row for a menu item index (for grouped layout). */
int terminal_menu_item_row(int idx);

/* Draw a typewriter-style text line.
 * Returns 1 when done typing. */
int terminal_typewriter(int tx, int ty, const char* text, int* char_pos, int* timer);

/* Clear the terminal display area. */
void terminal_clear(void);

/* Draw an ASCII box border using +, -, |. */
void terminal_draw_border(int x, int y, int w, int h);

/* Print text with a specific BG palette bank.
 * Writes directly to BG0 screenblock with palette bits. */
void terminal_print_pal(int tx, int ty, const char* str, int pal_bank);

/* Get terminal frame counter (incremented each draw cycle). */
int terminal_get_frame(void);

/* Increment terminal frame counter. Call once per frame. */
void terminal_tick(void);

/* Load animated circuit board BG on BG1. Call in terminal_enter. */
void terminal_load_bg(void);

/* Update BG1 scroll for animated circuit board effect. Call each frame. */
void terminal_scroll_bg(void);

/* Draw a bordered panel (ASCII box) filling the rectangle.
 * Corners '+', horizontal '-', vertical '|', interior filled with spaces.
 * top/left/bottom/right are tile-grid coordinates (0-19 rows, 0-29 cols). */
void terminal_draw_panel(int top, int left, int bottom, int right);

/* Enable overlay dimming: darkens BG1+BG2 behind BG0 text via alpha blend. */
void terminal_overlay_on(void);

/* Disable overlay dimming: clears blend registers. */
void terminal_overlay_off(void);

#endif /* GAME_TERMINAL_H */
