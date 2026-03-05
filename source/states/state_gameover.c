/*
 * Ghost Protocol — Game Over State
 *
 * Displayed on player death. Shows stats and return options.
 * Glitch/corruption effects for system failure aesthetic.
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/rng.h"
#include "game/player.h"
#include "game/quest.h"
#include "game/common.h"
#include "states/state_ids.h"
#include "states/state_gameover.h"

static int blink_timer;
static int intro_timer;     /* Counts up from 0 for sequential reveal */
static int glitch_done;     /* 1 = initial glitch sequence finished */
static int fade_timer;      /* >0 = fading out to title */
#define FADE_FRAMES 15

/* Glitch: flash palette between red and orange */
static void palette_glitch(int frame) {
    int cycle = frame % 8;
    if (cycle < 4) {
        pal_bg_mem[0] = RGB15(4, 1, 0);    /* orange flash */
        pal_bg_mem[1] = RGB15(31, 20, 4);   /* orange text */
    } else {
        pal_bg_mem[0] = RGB15(2, 0, 0);     /* dark red */
        pal_bg_mem[1] = RGB15(31, 8, 8);    /* red text */
    }
}

/* Glitch: corrupt a few random tiles on BG0 */
static void tile_corrupt(int count) {
    u16* sbb = (u16*)se_mem[31]; /* BG0 screenblock */
    for (int i = 0; i < count; i++) {
        int col = (int)rand_range(30);
        int row = (int)rand_range(20);
        sbb[row * 32 + col] = (u16)rand_range(96); /* random font tile */
    }
}

void state_gameover_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0;
    blink_timer = 0;
    intro_timer = 0;
    glitch_done = 0;
    fade_timer = 0;
    text_clear_all();

    pal_bg_mem[0] = RGB15(2, 0, 0);
    pal_bg_mem[1] = RGB15(31, 8, 8);

    audio_play_music(MUS_GAMEOVER);
}

void state_gameover_update(void) {
    blink_timer++;
    intro_timer++;

    /* Fade-out to title */
    if (fade_timer > 0) {
        fade_timer--;
        REG_BLDCNT = BLD_BLACK | BLD_ALL;
        REG_BLDY = (u16)(16 - (fade_timer * 16 / FADE_FRAMES));
        if (fade_timer == 0) {
            game_request_state = STATE_TITLE;
        }
        return;
    }

    /* Don't accept input until glitch intro finishes */
    if (!glitch_done) return;

    if (input_hit(KEY_START) || input_hit(KEY_A)) {
        audio_play_sfx(SFX_MENU_SELECT);
        fade_timer = FADE_FRAMES;
    }
}

void state_gameover_draw(void) {
    /* Phase 1 (frames 0-24): palette glitch + tile corruption */
    if (intro_timer < 24) {
        palette_glitch(intro_timer);
        if ((intro_timer & 3) == 0) {
            tile_corrupt(8);
        }
        return;
    }

    /* Phase 2 (frame 24): clean up corruption, set final palette */
    if (intro_timer == 24) {
        pal_bg_mem[0] = RGB15(2, 0, 0);
        pal_bg_mem[1] = RGB15(31, 8, 8);
        text_clear_all();
    }

    /* Phase 3: Sequential text reveal (one element per 15 frames) */
    int reveal = (intro_timer - 24) / 15;

    if (reveal >= 0 && intro_timer == 24) {
        text_print(6, 4, "CONNECTION LOST");
    }
    if (reveal >= 1 && intro_timer == 24 + 15) {
        text_print(7, 6, "TRACE COMPLETE");
    }
    if (reveal >= 2 && intro_timer == 24 + 30) {
        text_print(5, 9, "Level:");
        text_print_int(12, 9, player_state.level);
    }
    if (reveal >= 3 && intro_timer == 24 + 45) {
        text_print(5, 10, "Act:");
        text_print(10, 10, quest_get_act_name(quest_state.current_act));
    }
    if (reveal >= 4 && intro_timer == 24 + 60) {
        text_print(5, 11, "Mission:");
        text_print_int(14, 11, quest_state.story_mission);
        text_put_char(14 + (quest_state.story_mission >= 10 ? 2 : 1), 11, '/');
        text_print_int(14 + (quest_state.story_mission >= 10 ? 3 : 2), 11, STORY_MISSIONS);
    }
    if (reveal >= 5 && intro_timer == 24 + 75) {
        text_print(5, 12, "Credits:");
        text_print_int(14, 12, player_state.credits);
        glitch_done = 1;
    }

    /* Ongoing subtle glitch: occasional palette flicker */
    if (glitch_done && (blink_timer % 120) < 4) {
        pal_bg_mem[0] = RGB15(3, 0, 1);
    } else if (glitch_done) {
        pal_bg_mem[0] = RGB15(2, 0, 0);
    }

    /* Blink prompt after reveal complete */
    if (glitch_done) {
        if ((blink_timer >> 4) & 1) {
            text_print(7, 16, "PRESS START");
        } else {
            text_clear_rect(7, 16, 12, 1);
        }
    }
}

void state_gameover_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
