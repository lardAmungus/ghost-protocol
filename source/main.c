/*
 * Ghost Protocol — Entry Point
 *
 * Main game loop with state machine.
 * States: Title -> CharSel -> Terminal -> Net -> Gameover/Win
 */
#include <tonc.h>
#include "mgba/mgba.h"
#include "engine/video.h"
#include "engine/input.h"
#include "engine/sprite.h"
#include "engine/audio.h"
#include "engine/text.h"
#include "engine/rng.h"
#include "game/common.h"
#include "states/state_title.h"
#include "states/state_charsel.h"
#include "states/state_terminal.h"
#include "states/state_net.h"
#include "states/state_gameover.h"
#include "states/state_win.h"

/* ---------- State machine ---------- */
#include "states/state_ids.h"

int game_request_state = STATE_NONE;

typedef struct {
    void (*enter)(void);
    void (*update)(void);
    void (*draw)(void);
    void (*exit)(void);
} GameState;

static const GameState state_table[] = {
    [STATE_TITLE] = {
        state_title_enter,
        state_title_update,
        state_title_draw,
        state_title_exit
    },
    [STATE_TERMINAL] = {
        state_terminal_enter,
        state_terminal_update,
        state_terminal_draw,
        state_terminal_exit
    },
    [STATE_NET] = {
        state_net_enter,
        state_net_update,
        state_net_draw,
        state_net_exit
    },
    [STATE_GAMEOVER] = {
        state_gameover_enter,
        state_gameover_update,
        state_gameover_draw,
        state_gameover_exit
    },
    [STATE_WIN] = {
        state_win_enter,
        state_win_update,
        state_win_draw,
        state_win_exit
    },
    [STATE_CHARSEL] = {
        state_charsel_enter,
        state_charsel_update,
        state_charsel_draw,
        state_charsel_exit
    },
};

static int current_state = STATE_NONE;

static void state_switch(int new_state) {
    if (new_state <= STATE_NONE || new_state >= STATE_COUNT) return;
    if (current_state != STATE_NONE && current_state < STATE_COUNT
        && state_table[current_state].exit) {
        state_table[current_state].exit();
    }
    current_state = new_state;
    if (state_table[current_state].enter) {
        state_table[current_state].enter();
    }
    game_request_state = STATE_NONE;
}

/* ---------- Headless test support ---------- */

#ifdef HEADLESS_TEST
static int headless_frame_count = 0;
#define HEADLESS_MAX_FRAMES 30

static void headless_check(void) {
    headless_frame_count++;

    /* Auto-transition to terminal state after 5 frames for testing */
    if (headless_frame_count == 5 && current_state == STATE_TITLE) {
        mgba_printf(MGBA_LOG_INFO, "Headless: auto-entering terminal state");
        game_request_state = STATE_TERMINAL;
    }

    if (headless_frame_count >= HEADLESS_MAX_FRAMES) {
        mgba_printf(MGBA_LOG_INFO, "Headless test complete: %d frames OK", headless_frame_count);
        Stop();
    }
}
#endif /* HEADLESS_TEST */

/* ---------- Main ---------- */

int main(void) {
    int mgba_ok = mgba_open();

    if (mgba_ok) {
        mgba_printf(MGBA_LOG_INFO, "=== Ghost Protocol boot ===");
    }

    /* Start hardware timers for RNG seeding — use TM2/TM3 to avoid
     * conflicting with Maxmod which owns Timer 0 for DMA audio. */
    REG_TM2CNT_H = TM_ENABLE;
    REG_TM3CNT_H = TM_ENABLE | TM_CASCADE;

    /* Initialize engine subsystems */
    video_init();
    sprite_init();
    text_init();
    audio_init();

    if (mgba_ok) {
        mgba_printf(MGBA_LOG_INFO, "All subsystems ready");
    }

    /* Enter title state */
    state_switch(STATE_TITLE);

    /* Main loop */
    for (;;) {
        input_poll();

        if (game_request_state != STATE_NONE) {
            state_switch(game_request_state);
        }

        if (state_table[current_state].update) {
            state_table[current_state].update();
        }

        if (state_table[current_state].draw) {
            state_table[current_state].draw();
        }

        game_stats.play_time_frames++;

        video_vsync();
        audio_update();
        audio_update_fade();

#ifdef HEADLESS_TEST
        if (mgba_ok) headless_check();
#endif
    }

    return 0;
}
