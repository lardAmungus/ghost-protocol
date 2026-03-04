#ifndef STATES_STATE_TERMINAL_H
#define STATES_STATE_TERMINAL_H

void state_terminal_enter(void);
void state_terminal_update(void);
void state_terminal_draw(void);
void state_terminal_exit(void);

/* Reset session statics (call on new-game from title). */
void state_terminal_reset(void);

/* Load a save slot and mark terminal as initialized (call before STATE_TERMINAL for CONTINUE). */
void state_terminal_preload_slot(int slot);

#endif /* STATES_STATE_TERMINAL_H */
