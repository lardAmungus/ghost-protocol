/*
 * Ghost Protocol — Game Over State
 *
 * Displayed on player death. Shows stats and return options.
 * Circuit BG with corruption, glitch aesthetic, failure stats,
 * RETRY / TERMINAL cursor options.
 */
#include <tonc.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "engine/rng.h"
#include "game/player.h"
#include "game/quest.h"
#include "game/common.h"
#include "game/terminal.h"
#include "states/state_ids.h"
#include "states/state_gameover.h"

static int blink_timer;
static int intro_timer;     /* Counts up from 0 for sequential reveal */
static int glitch_done;     /* 1 = initial glitch sequence finished */
static int fade_timer;      /* >0 = fading out */
static int go_cursor;       /* 0 = RETRY, 1 = TERMINAL */
static int fade_target;     /* State to transition to */
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

/* Corrupt random tiles on BG1 (circuit board) for glitch effect */
static void corrupt_circuit_bg(int count) {
    u16* sbb = (u16*)se_mem[28]; /* BG1 screenblock */
    u16 pal_bits = (4 << 12);
    for (int i = 0; i < count; i++) {
        int col = (int)rand_range(32);
        int row = (int)rand_range(32);
        /* Overwrite with random circuit tile (0-5) */
        sbb[row * 32 + col] = (u16)(rand_range(6) | pal_bits);
    }
}

/* Corrupt a few random tiles on BG0 text layer */
static void tile_corrupt(int count) {
    u16* sbb = (u16*)se_mem[31]; /* BG0 screenblock */
    for (int i = 0; i < count; i++) {
        int col = (int)rand_range(30);
        int row = (int)rand_range(20);
        sbb[row * 32 + col] = (u16)rand_range(96); /* random font tile */
    }
}

void state_gameover_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;
    blink_timer = 0;
    intro_timer = 0;
    glitch_done = 0;
    fade_timer = 0;
    go_cursor = 0;
    fade_target = STATE_TITLE;
    text_clear_all();

    /* Terminal palettes for colored text */
    terminal_init_palette();

    /* Load circuit board BG on BG1 */
    terminal_load_bg();

    /* Override BG1 palette to red/amber for gameover feel */
    pal_bg_mem[4 * 16 + 0] = RGB15(2, 0, 0);
    pal_bg_mem[4 * 16 + 1] = RGB15(12, 4, 2);

    /* Text palette: red on dark */
    pal_bg_mem[0] = RGB15(2, 0, 0);
    pal_bg_mem[1] = RGB15(31, 8, 8);

    audio_play_music(MUS_GAMEOVER);
}

void state_gameover_update(void) {
    blink_timer++;
    intro_timer++;
    terminal_tick();

    /* Fade-out */
    if (fade_timer > 0) {
        fade_timer--;
        REG_BLDCNT = BLD_BLACK | BLD_ALL;
        REG_BLDY = (u16)(16 - (fade_timer * 16 / FADE_FRAMES));
        if (fade_timer == 0) {
            game_request_state = fade_target;
        }
        return;
    }

    /* Don't accept input until glitch intro finishes */
    if (!glitch_done) return;

    /* Cursor navigation */
    if (input_hit(KEY_DOWN) || input_hit(KEY_UP)) {
        go_cursor ^= 1;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_A) || input_hit(KEY_START)) {
        audio_play_sfx(SFX_MENU_SELECT);
        if (go_cursor == 0) {
            /* RETRY — restart from last save (go to terminal) */
            fade_target = STATE_NET;
            fade_timer = FADE_FRAMES;
        } else {
            /* TERMINAL — return to hub */
            fade_target = STATE_TERMINAL;
            fade_timer = FADE_FRAMES;
        }
    }
}

void state_gameover_draw(void) {
    /* Scroll circuit BG (slow) */
    terminal_scroll_bg();

    /* Phase 1 (frames 0-24): palette glitch + tile corruption */
    if (intro_timer < 24) {
        palette_glitch(intro_timer);
        if ((intro_timer & 3) == 0) {
            tile_corrupt(8);
            corrupt_circuit_bg(4);
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
        terminal_print_pal(6, 3, "CONNECTION LOST", TPAL_RED);
    }
    if (reveal >= 1 && intro_timer == 24 + 15) {
        terminal_print_pal(7, 4, "TRACE COMPLETE", TPAL_AMBER);
    }
    if (reveal >= 2 && intro_timer == 24 + 30) {
        /* Failure stats panel */
        terminal_draw_panel(6, 3, 13, 26);
        terminal_print_pal(5, 6, "MISSION REPORT", TPAL_RED);
    }
    if (reveal >= 3 && intro_timer == 24 + 45) {
        text_print(5, 7, "Kills:");
        text_print_int(14, 7, game_stats.total_kills);
    }
    if (reveal >= 4 && intro_timer == 24 + 60) {
        text_print(5, 8, "Dmg Taken:");
        text_print_int(16, 8, game_stats.damage_taken);
    }
    if (reveal >= 5 && intro_timer == 24 + 75) {
        text_print(5, 9, "Level:");
        text_print_int(14, 9, player_state.level);
    }
    if (reveal >= 6 && intro_timer == 24 + 90) {
        text_print(5, 10, "Act:");
        text_print(14, 10, quest_get_act_name(quest_state.current_act));
    }
    if (reveal >= 7 && intro_timer == 24 + 105) {
        /* Play time */
        {
            u32 total_secs = game_stats.play_time_frames / 60;
            int mins = (int)((total_secs % 3600) / 60);
            int secs = (int)(total_secs % 60);
            text_print(5, 11, "Time:");
            text_print_int(14, 11, mins);
            text_put_char(14 + (mins >= 10 ? 2 : 1), 11, 'm');
            text_print_int(14 + (mins >= 10 ? 3 : 2), 11, secs);
            text_put_char(14 + (mins >= 10 ? 3 : 2) + (secs >= 10 ? 2 : 1), 11, 's');
        }
        text_print(5, 12, "Credits:");
        text_print_int(14, 12, (int)player_state.credits);
        glitch_done = 1;
    }

    /* Ongoing subtle glitch: occasional palette flicker + circuit corruption */
    if (glitch_done) {
        if ((blink_timer % 120) < 4) {
            pal_bg_mem[0] = RGB15(3, 0, 1);
            corrupt_circuit_bg(2);
        } else {
            pal_bg_mem[0] = RGB15(2, 0, 0);
        }
    }

    /* RETRY / TERMINAL options after reveal complete */
    if (glitch_done) {
        text_print(8, 15, (go_cursor == 0) ? "> RETRY" : "  RETRY");
        text_print(8, 16, (go_cursor == 1) ? "> TERMINAL" : "  TERMINAL");

        /* Blink prompt */
        if ((blink_timer >> 4) & 1) {
            text_print(8, 18, "A:Select");
        } else {
            text_clear_rect(8, 18, 10, 1);
        }
    }
}

void state_gameover_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY = 0;
}
