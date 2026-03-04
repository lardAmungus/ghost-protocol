#include "engine/input.h"

void input_poll(void) {
    key_poll();
}

int input_hit(u16 key) {
    return (int)key_hit(key);
}

int input_held(u16 key) {
    return (int)key_held(key);
}

int input_released(u16 key) {
    return (int)key_released(key);
}

int input_any_hit(void) {
    return (int)key_hit(KEY_MASK);
}
