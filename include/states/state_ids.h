#ifndef STATES_STATE_IDS_H
#define STATES_STATE_IDS_H

/* Ghost Protocol — state identifiers */
#define STATE_NONE      0
#define STATE_TITLE     1
#define STATE_TERMINAL  2
#define STATE_NET       3
#define STATE_GAMEOVER  4
#define STATE_WIN       5
#define STATE_CHARSEL   6
#define STATE_COUNT     7

/* Global state transition request (defined in main.c) */
extern int game_request_state;

#endif /* STATES_STATE_IDS_H */
