#ifndef GAME_TERMINAL_H
#define GAME_TERMINAL_H

#include <tonc.h>

/*
 * Ghost Protocol — Terminal Menu System
 *
 * Green-on-black CRT terminal aesthetic.
 * Typewriter text effect, cursor navigation.
 */

/* Terminal menu items */
enum {
    TMENU_CONTRACTS = 0,
    TMENU_SHOP,
    TMENU_INVENTORY,
    TMENU_STATS,
    TMENU_JACK_IN,
    TMENU_SAVE,
    TMENU_HELP,
    TMENU_COUNT
};

/* Terminal palette banks (BG0) */
#define TPAL_GREEN   0   /* Green text on black */
#define TPAL_AMBER   1   /* Amber highlights */
#define TPAL_CYAN    2   /* Cyan system messages */
#define TPAL_RED     3   /* Red warnings */

/* Initialize terminal palettes. */
void terminal_init_palette(void);

/* Draw the main terminal menu with cursor (blinking cursor effect). */
void terminal_draw_menu(int cursor);

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

#endif /* GAME_TERMINAL_H */
