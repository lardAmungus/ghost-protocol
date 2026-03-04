/*
 * Ghost Protocol — Win State
 *
 * Displayed after defeating Root Access (Act 5 boss).
 * Shows epilogue, credits, and return to title.
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "game/player.h"
#include "game/quest.h"
#include "states/state_ids.h"
#include "states/state_win.h"

enum {
    WIN_EPILOGUE = 0,
    WIN_CREDITS,
    WIN_END,
};

static int phase;
static int timer;
static int blink_timer;

void state_win_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    timer = 0;
    blink_timer = 0;
    phase = WIN_EPILOGUE;
    text_clear_all();

    pal_bg_mem[0] = RGB15(0, 0, 0);
    pal_bg_mem[1] = RGB15(0, 31, 16);   /* bright green */

    /* Amber for highlights */
    pal_bg_mem[1 * 16 + 0] = RGB15(0, 0, 0);
    pal_bg_mem[1 * 16 + 1] = RGB15(28, 20, 0);

    audio_play_music(MUS_VICTORY);
}

static void draw_epilogue(void) {
    text_print(5, 3, "GHOST PROTOCOL");
    text_print(7, 5, "ACTIVATED");
    text_print(3, 8, "Root Access defeated.");
    text_print(3, 10, "The rogue AI is gone.");
    text_print(3, 11, "The Net is free.");
    text_print(3, 13, "Your legend grows...");
}

static void draw_credits(void) {
    text_print(6, 2, "- CREDITS -");
    text_print(6, 5, "Ghost Protocol");
    text_print(4, 7, "A GBA Game by You");
    text_print(4, 9, "Engine: libtonc");
    text_print(4, 10, "Audio: Maxmod");
    text_print(4, 12, "Build: devkitARM");

    /* Stats */
    text_print(4, 15, "Level:");
    text_print_int(11, 15, player_state.level);

    int bosses = 0;
    for (int i = 0; i < 5; i++) {
        if (quest_state.boss_defeated[i]) bosses++;
    }
    text_print(4, 16, "Bosses:");
    text_print_int(12, 16, bosses);
    text_print(13, 16, "/5");
}

static void draw_end(void) {
    text_print(8, 8, "THE END");
    text_print(5, 10, "Thanks for playing!");
}

void state_win_update(void) {
    timer++;
    blink_timer++;

    if (phase == WIN_EPILOGUE && (timer > 180 || input_hit(KEY_A))) {
        phase = WIN_CREDITS;
        timer = 0;
        text_clear_all();
    } else if (phase == WIN_CREDITS && (timer > 240 || input_hit(KEY_A))) {
        phase = WIN_END;
        timer = 0;
        text_clear_all();
    } else if (phase == WIN_END && (input_hit(KEY_START) || input_hit(KEY_A))) {
        audio_play_sfx(SFX_MENU_SELECT);
        game_request_state = STATE_TITLE;
    }
}

void state_win_draw(void) {
    switch (phase) {
    case WIN_EPILOGUE:
        draw_epilogue();
        break;
    case WIN_CREDITS:
        draw_credits();
        break;
    case WIN_END:
        draw_end();
        if ((blink_timer >> 4) & 1) {
            text_print(7, 16, "PRESS START");
        } else {
            text_clear_rect(7, 16, 12, 1);
        }
        break;
    }
}

void state_win_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
