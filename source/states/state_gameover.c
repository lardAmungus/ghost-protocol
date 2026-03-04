/*
 * Ghost Protocol — Game Over State
 *
 * Displayed on player death. Shows stats and return options.
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "game/player.h"
#include "game/quest.h"
#include "states/state_ids.h"
#include "states/state_gameover.h"

static int blink_timer;

void state_gameover_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    blink_timer = 0;
    text_clear_all();

    pal_bg_mem[0] = RGB15(2, 0, 0);     /* dark red background */
    pal_bg_mem[1] = RGB15(31, 8, 8);    /* red text */

    audio_play_music(MUS_GAMEOVER);

    text_print(6, 4, "CONNECTION LOST");
    text_print(7, 6, "TRACE COMPLETE");

    /* Player stats */
    text_print(5, 9, "Level:");
    text_print_int(12, 9, player_state.level);

    text_print(5, 10, "Act:");
    text_print(10, 10, quest_get_act_name(quest_state.current_act));

    text_print(5, 11, "Mission:");
    text_print_int(14, 11, quest_state.story_mission);
    text_put_char(14 + (quest_state.story_mission >= 10 ? 2 : 1), 11, '/');
    text_print_int(14 + (quest_state.story_mission >= 10 ? 3 : 2), 11, STORY_MISSIONS);

    text_print(5, 12, "Credits:");
    text_print_int(14, 12, player_state.credits);
}

void state_gameover_update(void) {
    blink_timer++;
    if (input_hit(KEY_START) || input_hit(KEY_A)) {
        audio_play_sfx(SFX_MENU_SELECT);
        game_request_state = STATE_TITLE;
    }
}

void state_gameover_draw(void) {
    if ((blink_timer >> 4) & 1) {
        text_print(7, 16, "PRESS START");
    } else {
        text_clear_rect(7, 16, 12, 1);
    }
}

void state_gameover_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
