/*
 * Ghost Protocol — In-Game HUD
 *
 * Displays HP bar, level, ability cooldowns on BG0 row 0-1.
 * Boss HP bar on rows 18-19 when active.
 */
#include "game/hud.h"
#include "game/player.h"
#include "game/loot.h"
#include "game/common.h"
#include "engine/text.h"

/* Boss tracking */
static const char* boss_name;
static int boss_hp;
static int boss_max_hp;

/* Kill objective */
static int obj_kills;
static int obj_target;

/* Trace timer */
static int trace_frames;

/* Score (bug bounty) */
static u16 hud_score;

/* Notification */
static const char* notify_msg;
static int notify_timer;

void hud_init(void) {
    boss_name = NULL;
    boss_hp = 0;
    boss_max_hp = 0;
    obj_kills = 0;
    obj_target = 0;
    trace_frames = 0;
    hud_score = 0;
    notify_msg = NULL;
    notify_timer = 0;
}

void hud_draw(void) {
    /* Row 0: HP bar */
    text_print(0, 0, "HP");

    /* Draw simple ASCII HP bar */
    int hp = player_state.hp;
    int max_hp = player_state.max_hp;
    int bar_len = 10;
    int filled = 0;
    if (max_hp > 0) {
        filled = hp * bar_len / max_hp;
        if (filled < 0) filled = 0;
        if (filled > bar_len) filled = bar_len;
    }
    for (int i = 0; i < bar_len; i++) {
        if (i < filled) {
            text_put_char(3 + i, 0, '=');
        } else {
            text_put_char(3 + i, 0, '-');
        }
    }

    /* HP numeric — compact: digits flush against '/' */
    {
        int dhp = hp > 999 ? 999 : hp;
        int dmax = max_hp > 999 ? 999 : max_hp;
        int hdigits = 1;
        { int tmp = dhp; while (tmp >= 10) { hdigits++; tmp /= 10; } }
        text_clear_rect(14, 0, 7, 1); /* Clear old numeric area */
        text_print_int(14, 0, dhp);
        int scol = 14 + hdigits;
        text_put_char(scol, 0, '/');
        text_print_int(scol + 1, 0, dmax);
    }

    /* Level or Score — clear area first (stale digits from prev frame) */
    text_clear_rect(22, 0, 8, 1);
    if (hud_score > 0) {
        text_print(22, 0, "SC");
        text_print_int(24, 0, hud_score);
    } else {
        text_print(22, 0, "Lv");
        text_print_int(24, 0, player_state.level);
        /* XP percentage next to level — "Lv15 42%" */
        if (player_state.level < MAX_LEVEL) {
            int xp_need = player_xp_to_next();
            int pct = (xp_need > 0) ? (int)player_state.xp * 100 / xp_need : 0;
            if (pct > 99) pct = 99;
            /* Place after level digits */
            int ld = (player_state.level >= 10) ? 2 : 1;
            text_print_int(24 + ld, 0, pct);
            text_put_char(24 + ld + (pct >= 10 ? 2 : 1), 0, '%');
        }
    }

    /* Row 1: Ability cooldowns — clear slot first to avoid stale digits */
    for (int i = 0; i < 4; i++) {
        text_clear_rect(i * 4, 1, 3, 1);
        int cd = player_state.cooldown_ability[i];
        if (player_state.ability_unlocks & (1 << i)) {
            if (cd > 0) {
                /* Show cooldown in seconds (e.g. "3" for 180, "1" for <120) */
                int secs = (cd + 59) / 60; /* Ceiling division for responsive feel */
                text_print_int(i * 4, 1, secs);
            } else {
                /* Ready indicator */
                text_put_char(i * 4, 1, 'R');
            }
        }
    }

    /* Trace timer (right side of row 1) — bug bounty */
    if (trace_frames > 0) {
        int secs = trace_frames / 60;
        int mins = secs / 60;
        secs = secs % 60;
        if (mins > 99) mins = 99;
        text_print(20, 1, "T:");
        text_print_int(22, 1, mins);
        text_put_char(24, 1, ':');
        if (secs < 10) text_put_char(25, 1, '0');
        text_print_int(secs < 10 ? 26 : 25, 1, secs);
        /* Blink when low */
        if (trace_frames < 600 && !(trace_frames & 8)) {
            text_clear_rect(20, 1, 9, 1);
        }
    }
    /* Kill objective (right side of row 1) — only if no trace timer */
    else if (obj_target > 0) {
        /* Clear area first to prevent stale digits from digit-count changes */
        text_clear_rect(20, 1, 10, 1);
        /* Cap displayed values to 999 and position '/' dynamically */
        int dk = obj_kills > 999 ? 999 : obj_kills;
        int dt = obj_target > 999 ? 999 : obj_target;
        int kdigits = 1;
        { int tmp = dk; while (tmp >= 10) { kdigits++; tmp /= 10; } }
        text_print(20, 1, "K:");
        text_print_int(22, 1, dk);
        int slash_col = 22 + kdigits;
        if (slash_col > 28) slash_col = 28;
        text_put_char(slash_col, 1, '/');
        text_print_int(slash_col + 1, 1, dt);
    }

    /* Boss HP bar (rows 18-19) */
    if (boss_name && boss_hp > 0) {
        /* Clear name row first (prevents stale chars from longer previous name) */
        text_clear_rect(2, 18, 26, 1);
        /* Truncate boss name — col guard keeps within 30-col screen */
        for (int i = 0; i < 20 && boss_name[i] != '\0' && 2 + i < 28; i++) {
            text_put_char(2 + i, 18, boss_name[i]);
        }

        int bfilled = 0;
        int bbar_len = 20;
        if (boss_max_hp > 0) {
            bfilled = boss_hp * bbar_len / boss_max_hp;
            if (bfilled < 0) bfilled = 0;
            if (bfilled > bbar_len) bfilled = bbar_len;
        }
        for (int i = 0; i < bbar_len; i++) {
            if (i < bfilled) {
                text_put_char(2 + i, 19, '=');
            } else {
                text_put_char(2 + i, 19, '-');
            }
        }
    }

    /* Notification (centered on screen) */
    if (notify_msg && notify_timer > 0) {
        /* Blink in last 30 frames */
        if (notify_timer > 30 || (notify_timer & 4)) {
            int len = 0;
            while (notify_msg[len]) len++;
            int tx = (30 - len) / 2;
            if (tx < 0) tx = 0;
            text_print(tx, 10, notify_msg);
        }
    }
}

void hud_set_boss(const char* name, int hp, int max_hp) {
    boss_name = name;
    boss_hp = hp;
    boss_max_hp = max_hp;
}

void hud_notify(const char* msg, int duration) {
    notify_msg = msg;
    notify_timer = duration;
}

void hud_set_objective(int kills, int target) {
    obj_kills = kills;
    obj_target = target;
}

void hud_set_trace(int frames) {
    trace_frames = frames;
}

void hud_set_score(u16 score) {
    hud_score = score;
}

void hud_notify_update(void) {
    if (notify_timer > 0) {
        notify_timer--;
        if (notify_timer == 0) {
            notify_msg = NULL;
            /* Clear notification area (full width) */
            text_clear_rect(0, 10, 30, 1);
        }
    }
}
