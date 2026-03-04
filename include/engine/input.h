#ifndef ENGINE_INPUT_H
#define ENGINE_INPUT_H

#include <tonc.h>

/* Poll key state. Call once per frame before any input checks. */
void input_poll(void);

/* Returns non-zero if key was just pressed this frame. */
int input_hit(u16 key);

/* Returns non-zero if key is currently held. */
int input_held(u16 key);

/* Returns non-zero if key was just released this frame. */
int input_released(u16 key);

/* Returns non-zero if any key was pressed this frame. */
int input_any_hit(void);

#endif /* ENGINE_INPUT_H */
