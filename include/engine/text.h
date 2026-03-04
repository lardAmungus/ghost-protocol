#ifndef ENGINE_TEXT_H
#define ENGINE_TEXT_H

#include <tonc.h>

/*
 * Fixed-width 8x8 font rendering to BG0.
 * Characters map directly to tile indices in BG0's character base.
 * A 30x20 character grid fits the 240x160 screen exactly.
 */

/* Dialogue font base tile index (opaque background, tiles 128-222 in CBB0) */
#define DBOX_FONT_BASE 128

/* Load the built-in 8x8 font tiles into VRAM for BG0. */
void text_init(void);

/* Print a string at tile position (tx, ty) on BG0. */
void text_print(int tx, int ty, const char* str);

/* Clear a rectangular region on BG0. */
void text_clear_rect(int tx, int ty, int w, int h);

/* Print a decimal number at tile position. */
void text_print_int(int tx, int ty, int value);

/* Clear the entire BG0 text layer. */
void text_clear_all(void);

/* Write a single character directly to BG0 screenblock.
 * tx: 0-29, ty: 0-19, ch: ASCII printable character. */
INLINE void text_put_char(int tx, int ty, char ch) {
    if ((unsigned)tx < 30 && (unsigned)ty < 20)
        ((u16*)se_mem[31])[ty * 32 + tx] = (u16)(ch - ' ');
}

#endif /* ENGINE_TEXT_H */
