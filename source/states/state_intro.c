/*
 * Ghost Protocol — Intro Prologue State
 *
 * 6-screen narrative prologue between title and charsel (new-game only).
 * Circuit board BG, typewriter text, two-stage advance, START to skip.
 */
#include <tonc.h>
#include <string.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/audio.h"
#include "game/terminal.h"
#include "states/state_ids.h"
#include "states/state_intro.h"

/* ---- Prologue content (6 screens, 26-char line width) ---- */

#define INTRO_PAGES 6
#define INTRO_TYPE_SPEED 2  /* frames per character */
#define FADE_FRAMES 15

static const char* const intro_pages[INTRO_PAGES] = {
    /* Screen 1 — Terminal boot */
    "[GHOST PROTOCOL v2.1]\n"
    "Secure link established.\n"
    "Connection verified.\n"
    "Status: UNREGISTERED",

    /* Screen 2 — ZERO introduces themselves */
    "ZERO> You're in. Took a\n"
    "while to find you.\n"
    "I'm ZERO -- your handler\n"
    "from here on out.",

    /* Screen 3 — World situation */
    "ZERO> The corps own the\n"
    "net now. MICROSLOP, GOGOL\n"
    "AMAZOMB -- they carved it\n"
    "up and locked it down.",

    /* Screen 4 — Your role */
    "ZERO> But you -- you're a\n"
    "Ghost. No registration,\n"
    "no trace. You don't exist\n"
    "in their systems.",

    /* Screen 5 — The mission */
    "ZERO> We need you to jack\n"
    "in. Extract data, break\n"
    "their grip. Take them\n"
    "apart from the inside.",

    /* Screen 6 — Call to action */
    "ZERO> First -- customize\n"
    "your rig. Then we'll get\n"
    "you started. First job's\n"
    "waiting.",
};

/* Speaker palette per page: page 0 is system green, rest are cyan (ZERO) */
static const int intro_pal[INTRO_PAGES] = {
    TPAL_GREEN,  /* boot text */
    TPAL_CYAN,   /* ZERO */
    TPAL_CYAN,   /* ZERO */
    TPAL_CYAN,   /* ZERO */
    TPAL_CYAN,   /* ZERO */
    TPAL_CYAN,   /* ZERO */
};

/* ---- State ---- */

static int intro_page;       /* Current page (0-5) */
static int char_pos;         /* Typewriter: chars revealed */
static int char_timer;       /* Typewriter: frame counter */
static int fade_timer;       /* >0 = fading out */
static int fade_target;      /* State to switch to after fade */
static int fade_in_timer;    /* >0 = fading in */
static int skip_confirm;     /* 1 = showing "Skip intro?" prompt */
static int scroll_timer;     /* BG1 circuit scroll */

static void start_fade_out(int target_state) {
    if (fade_timer > 0) return;
    fade_timer = FADE_FRAMES;
    fade_target = target_state;
    REG_BLDCNT = BLD_BLACK | BLD_ALL;
}

void state_intro_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_BG0 | DCNT_BG1;

    intro_page = 0;
    char_pos = 0;
    char_timer = 0;
    fade_timer = 0;
    fade_target = 0;
    fade_in_timer = FADE_FRAMES;
    skip_confirm = 0;
    scroll_timer = 0;

    REG_BLDCNT = BLD_BLACK | BLD_ALL;
    REG_BLDY = 16; /* Start fully dark */

    /* Terminal palettes for text rendering */
    terminal_init_palette();

    /* Animated circuit board BG on BG1 */
    terminal_load_bg();

    text_clear_all();

    audio_play_music(MUS_EX_CUTSCENE_INTRO);
}

void state_intro_update(void) {
    /* Handle fade-out */
    if (fade_timer > 0) {
        fade_timer--;
        REG_BLDY = (u16)(16 - (fade_timer * 16 / FADE_FRAMES));
        if (fade_timer == 0) {
            game_request_state = fade_target;
        }
        return;
    }

    /* Handle fade-in */
    if (fade_in_timer > 0) {
        fade_in_timer--;
        REG_BLDY = (u16)(fade_in_timer * 16 / FADE_FRAMES);
        if (fade_in_timer == 0) {
            REG_BLDCNT = 0;
            REG_BLDY = 0;
        }
    }

    /* Skip confirm prompt */
    if (skip_confirm) {
        if (input_hit(KEY_A)) {
            /* Yes — skip intro */
            audio_play_sfx(SFX_MENU_SELECT);
            start_fade_out(STATE_CHARSEL);
            skip_confirm = 0;
            return;
        }
        if (input_hit(KEY_B)) {
            /* No — cancel */
            audio_play_sfx(SFX_MENU_BACK);
            skip_confirm = 0;
            text_clear_rect(3, 17, 26, 2);
            return;
        }
        return; /* Block other input while prompt showing */
    }

    /* START opens skip confirmation */
    if (input_hit(KEY_START)) {
        skip_confirm = 1;
        audio_play_sfx(SFX_MENU_SELECT);
        return;
    }

    /* Typewriter advance */
    if (intro_page < INTRO_PAGES) {
        const char* text = intro_pages[intro_page];
        int len = (int)strlen(text);
        if (char_pos < len) {
            char_timer++;
            if (char_timer >= INTRO_TYPE_SPEED) {
                char_timer = 0;
                char_pos++;
            }
        }
    }

    /* Two-stage text advance: A/B */
    if (input_hit(KEY_A) || input_hit(KEY_B)) {
        if (intro_page < INTRO_PAGES) {
            const char* text = intro_pages[intro_page];
            int len = (int)strlen(text);
            if (char_pos < len) {
                /* Stage 1: complete current page instantly */
                char_pos = len;
                return;
            }
        }
        /* Stage 2: advance to next page */
        intro_page++;
        char_pos = 0;
        char_timer = 0;
        audio_play_sfx(SFX_MENU_SELECT);
        text_clear_all();

        if (intro_page >= INTRO_PAGES) {
            /* Prologue complete — go to charsel */
            start_fade_out(STATE_CHARSEL);
        }
    }
}

void state_intro_draw(void) {
    /* Scroll circuit BG */
    scroll_timer++;
    REG_BG1HOFS = (u16)(scroll_timer >> 1);
    REG_BG1VOFS = (u16)(scroll_timer >> 2);

    if (intro_page >= INTRO_PAGES) return;

    /* Header */
    terminal_print_pal(2, 1, ">> INCOMING SIGNAL <<", TPAL_AMBER);

    /* Render typewriter text */
    const char* text = intro_pages[intro_page];
    int pal = intro_pal[intro_page];
    int row = 5;
    int col = 3;
    int chars_shown = 0;
    for (int i = 0; text[i] != '\0' && row < 16; i++) {
        if (chars_shown >= char_pos) break;
        if (text[i] == '\n') {
            row++;
            col = 3;
        } else {
            if (col < 29) {
                char buf[2] = { text[i], '\0' };
                terminal_print_pal(col, row, buf, pal);
                col++;
            }
        }
        chars_shown++;
    }

    /* Page indicator (bottom-right) */
    text_print_int(25, 18, intro_page + 1);
    text_put_char(26, 18, '/');
    text_put_char(27, 18, '6');

    /* Skip confirm prompt */
    if (skip_confirm) {
        terminal_print_pal(3, 17, "Skip intro?", TPAL_AMBER);
        terminal_print_pal(3, 18, "A:Yes  B:No", TPAL_GREEN);
    }
}

void state_intro_exit(void) {
    text_clear_all();
    REG_BLDCNT = 0;
    REG_BLDY = 0;
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
}
