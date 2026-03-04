/*
 * mgba debug logging implementation.
 * Public domain / CC0.
 */
#include "mgba/mgba.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int mgba_is_open = 0;

int mgba_open(void) {
    REG_DEBUG_ENABLE = 0xC0DE;
    mgba_is_open = (REG_DEBUG_ENABLE == 0x1DEA);
    return mgba_is_open;
}

void mgba_close(void) {
    REG_DEBUG_ENABLE = 0;
    mgba_is_open = 0;
}

void mgba_printf(int level, const char* fmt, ...) {
    if (!mgba_is_open) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(REG_DEBUG_STRING, 256, fmt, args);
    va_end(args);

    REG_DEBUG_FLAGS = (u16)(level | 0x100);
}
