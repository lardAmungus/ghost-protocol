/*
 * mgba debug logging interface.
 * Minimal implementation for mGBA's debug registers.
 * These registers only exist in mGBA — calls are no-ops on hardware.
 * Public domain / CC0.
 */
#ifndef MGBA_H
#define MGBA_H

#include <tonc.h>

#define MGBA_LOG_FATAL  0
#define MGBA_LOG_ERROR  1
#define MGBA_LOG_WARN   2
#define MGBA_LOG_INFO   3
#define MGBA_LOG_DEBUG  4

#define REG_DEBUG_ENABLE (*(volatile u16*)0x04FFF780)
#define REG_DEBUG_FLAGS  (*(volatile u16*)0x04FFF700)
#define REG_DEBUG_STRING ((char*)0x04FFF600)

/*
 * Open the debug interface. Returns 1 if running in mGBA, 0 otherwise.
 */
int mgba_open(void);

/*
 * Close the debug interface.
 */
void mgba_close(void);

/*
 * Printf to mGBA's debug console at the given log level.
 */
void mgba_printf(int level, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* MGBA_H */
