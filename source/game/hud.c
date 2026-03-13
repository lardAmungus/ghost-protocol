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
#include "game/abilities.h"
#include "game/levelgen.h"
#include "engine/text.h"
#include "engine/audio.h"

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

/* Camera for minimap + floattext (8.8 fixed-point) */
static s32 hud_cam_x;
static s32 hud_cam_y;

/* Floating damage numbers */
#define MAX_FLOATS 4
#define FLOAT_DURATION 18
static struct {
    s32 wx, wy;       /* World position (8.8 fixed-point) */
    int value;         /* Damage/heal value to display */
    int timer;         /* Frames remaining (0=inactive) */
    int is_crit;       /* 1=critical hit (blinks faster) */
    s8 prev_sx, prev_sy; /* Previous screen tile position (for targeted clear) */
    u8 prev_digits;      /* Number of digits drawn last frame */
} floats[MAX_FLOATS];

/* Notification */
static const char* notify_msg;
static int notify_timer;

/* Damage direction */
static int dmg_dir_timer;   /* >0 = showing direction indicator */
static int dmg_dir_right;   /* 1 = damage from right side */

void hud_init(void) {
    boss_name = NULL;
    boss_hp = 0;
    boss_max_hp = 0;
    obj_kills = 0;
    obj_target = 0;
    trace_frames = 0;
    hud_score = 0;
    hud_cam_x = 0;
    hud_cam_y = 0;
    notify_msg = NULL;
    notify_timer = 0;
    dmg_dir_timer = 0;
    dmg_dir_right = 0;
    for (int i = 0; i < MAX_FLOATS; i++) floats[i].timer = 0;
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
    {
        static u8 prev_cd[4]; /* Track previous cooldown state for flash */
        static u8 flash_timer[4]; /* Flash countdown per slot */
        for (int i = 0; i < 4; i++) {
            text_clear_rect(i * 4, 1, 4, 1);
            int cd = player_state.cooldown_ability[i];
            if (player_state.ability_unlocks & (1 << i)) {
                /* Detect cooldown→ready transition */
                if (prev_cd[i] > 0 && cd == 0) {
                    flash_timer[i] = 16; /* Flash for 16 frames */
                    audio_play_sfx(SFX_MENU_SELECT);
                }
                prev_cd[i] = (u8)(cd > 255 ? 255 : cd);

                if (cd > 0) {
                    /* Dim→bright color: amber when >50% left, green when <50% */
                    int max_cd = 300; /* rough max */
                    int pal = (cd * 2 > max_cd) ? (1 << 12) : 0; /* amber or green */
                    int secs = (cd + 59) / 60;
                    u16 tile = (u16)(('0' + secs - ' ') | pal);
                    ((u16*)se_mem[31])[1 * 32 + i * 4] = tile;
                } else {
                    /* Ready indicator — blink 'R' with increasing speed */
                    if (flash_timer[i] > 0) {
                        flash_timer[i]--;
                        /* Blink speed increases: slow→fast as timer counts down */
                        int blink_rate = (flash_timer[i] > 8) ? 4 : 2;
                        if (flash_timer[i] % blink_rate < blink_rate / 2)
                            text_put_char(i * 4, 1, '*');
                        else
                            text_put_char(i * 4, 1, 'R');
                    } else {
                        text_put_char(i * 4, 1, 'R');
                    }
                }
            } else {
                prev_cd[i] = 0;
            }
        }
    }

    /* XP progress bar (cols 16-19 on row 1) */
    if (player_state.level < MAX_LEVEL) {
        int xp_need = player_xp_to_next();
        int xp_pct = (xp_need > 0) ? (int)player_state.xp * 100 / xp_need : 0;
        int xp_fill = (xp_need > 0) ? (int)player_state.xp * 4 / xp_need : 0;
        if (xp_fill > 4) xp_fill = 4;
        /* Pulse/blink when close to level-up (>90%) */
        static int xp_blink;
        xp_blink++;
        int show_bar = 1;
        if (xp_pct >= 90 && (xp_blink & 4)) show_bar = 0;
        for (int i = 0; i < 4; i++) {
            if (show_bar)
                text_put_char(16 + i, 1, i < xp_fill ? '|' : '.');
            else
                text_put_char(16 + i, 1, ' ');
        }
    } else {
        text_print(16, 1, "MAX!");
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
        int nc = 0;
        for (int i = 0; i < 20 && boss_name[i] != '\0' && 2 + i < 28; i++) {
            text_put_char(2 + i, 18, boss_name[i]);
            nc = i + 1;
        }
        /* HP percentage after name */
        {
            int pct = (boss_max_hp > 0) ? boss_hp * 100 / boss_max_hp : 0;
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            int col = 2 + nc + 1;
            if (col < 25) {
                if (pct >= 100) { text_put_char(col++, 18, '1'); text_put_char(col++, 18, '0'); text_put_char(col++, 18, '0'); }
                else if (pct >= 10) { text_put_char(col++, 18, (char)('0' + pct / 10)); text_put_char(col++, 18, (char)('0' + pct % 10)); }
                else { text_put_char(col++, 18, (char)('0' + pct)); }
                if (col < 28) text_put_char(col, 18, '%');
            }
        }

        int bfilled = 0;
        int bbar_len = 20;
        if (boss_max_hp > 0) {
            bfilled = boss_hp * bbar_len / boss_max_hp;
            if (bfilled < 0) bfilled = 0;
            if (bfilled > bbar_len) bfilled = bbar_len;
        }
        /* Phase markers at 60% and 30% HP thresholds */
        int mark_60 = bbar_len * 60 / 100; /* Position for phase 2 */
        int mark_30 = bbar_len * 30 / 100; /* Position for phase 3 */
        for (int i = 0; i < bbar_len; i++) {
            if (i == mark_60 || i == mark_30) {
                /* Phase marker — shows threshold */
                text_put_char(2 + i, 19, '|');
            } else if (i < bfilled) {
                text_put_char(2 + i, 19, '=');
            } else {
                text_put_char(2 + i, 19, '-');
            }
        }
    }

    /* Row 2: Section minimap (right side) */
    {
        /* Calculate current section from camera pixel position */
        int cam_px = (int)(hud_cam_x >> 8);
        int cur_section = cam_px / (16 * 8);  /* 16 tiles * 8px per tile = 128px per section */
        if (cur_section < 0) cur_section = 0;
        if (cur_section >= NUM_SECTIONS) cur_section = NUM_SECTIONS - 1;

        /* Draw 16 section markers starting at col 14, row 2 */
        for (int i = 0; i < NUM_SECTIONS; i++) {
            if (i == cur_section) {
                text_put_char(14 + i, 2, '#');
            } else {
                text_put_char(14 + i, 2, '.');
            }
        }
    }

    /* Row 2: Active status effects (left side, cols 0-13) */
    {
        int col = 0;
        text_clear_rect(0, 2, 13, 1);

        /* Check each buff — show 2-letter code + remaining seconds */
        struct { int active; int timer; char c0; char c1; } efx[] = {
            { ability_is_overclock_active(), ability_get_overclock_timer(), 'O', 'C' },
            { ability_is_iron_skin_active(), ability_get_iron_skin_timer(), 'I', 'S' },
            { ability_is_berserk_active(), ability_get_berserk_timer(), 'B', 'S' },
            { ability_is_data_shield_active(), ability_get_data_shield_timer(), 'D', 'S' },
            { ability_is_smoke_active(), ability_get_smoke_timer(), 'S', 'M' },
            { ability_is_backstab_active(), ability_get_backstab_timer(), 'B', 'K' },
            { ability_is_time_warp_active(), ability_get_time_warp_timer(), 'T', 'W' },
            { ability_is_nanobots_active(), ability_get_nanobots_timer(), 'N', 'B' },
        };
        for (int e = 0; e < 8 && col < 10; e++) {
            if (!efx[e].active) continue;
            text_put_char(col, 2, efx[e].c0);
            text_put_char(col + 1, 2, efx[e].c1);
            int secs = (efx[e].timer + 59) / 60;
            if (secs > 9) secs = 9;
            text_put_char(col + 2, 2, (char)('0' + secs));
            col += 4;
            col += 3;
        }
        if (ability_is_firewall_active() && col < 11) {
            text_put_char(col, 2, 'F');
            text_put_char(col + 1, 2, 'W');
            col += 3;
        }
        if (ability_is_overclock_plus_active() && col < 11) {
            text_put_char(col, 2, 'O');
            text_put_char(col + 1, 2, '+');
            col += 3;
        }
        if (ability_is_upload_active() && col < 11) {
            text_put_char(col, 2, 'U');
            text_put_char(col + 1, 2, 'L');
            col += 3;
        }
        (void)col; /* Suppress unused warning after last check */
    }

    /* Notification (centered on screen, row 10) */
    if (notify_msg && notify_timer > 0) {
        /* Blink in last 30 frames */
        if (notify_timer > 30 || (notify_timer & 4)) {
            int len = 0;
            while (notify_msg[len]) len++;
            if (len > 30) len = 30; /* Cap to screen width */
            int tx = (30 - len) / 2;
            if (tx < 0) tx = 0;
            text_print(tx, 10, notify_msg);
        } else {
            /* Clear on blink-off frames */
            text_clear_rect(0, 10, 30, 1);
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

void hud_set_camera_x(s32 cam_x_fp) {
    hud_cam_x = cam_x_fp;
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

    /* Damage direction indicator countdown */
    if (dmg_dir_timer > 0) {
        dmg_dir_timer--;
        /* Blink an arrow on the side damage came from */
        if (dmg_dir_timer & 2) {
            if (dmg_dir_right) {
                text_put_char(29, 9, '<');
            } else {
                text_put_char(0, 9, '>');
            }
        } else {
            if (dmg_dir_right) {
                text_put_char(29, 9, ' ');
            } else {
                text_put_char(0, 9, ' ');
            }
        }
        if (dmg_dir_timer == 0) {
            text_put_char(29, 9, ' ');
            text_put_char(0, 9, ' ');
        }
    }
}

void hud_damage_direction(int from_right) {
    dmg_dir_timer = 20;
    dmg_dir_right = from_right;
}

/* ---- Floating damage numbers ---- */

void hud_floattext_spawn(s32 wx, s32 wy, int value, int is_crit) {
    /* Find free slot */
    for (int i = 0; i < MAX_FLOATS; i++) {
        if (floats[i].timer <= 0) {
            floats[i].wx = wx;
            floats[i].wy = wy;
            floats[i].value = value;
            floats[i].timer = FLOAT_DURATION;
            floats[i].is_crit = is_crit;
            return;
        }
    }
    /* All slots full — overwrite oldest */
    int oldest = 0;
    for (int i = 1; i < MAX_FLOATS; i++) {
        if (floats[i].timer < floats[oldest].timer) oldest = i;
    }
    floats[oldest].wx = wx;
    floats[oldest].wy = wy;
    floats[oldest].value = value;
    floats[oldest].timer = FLOAT_DURATION;
    floats[oldest].is_crit = is_crit;
}

void hud_floattext_clear(void) {
    for (int i = 0; i < MAX_FLOATS; i++) floats[i].timer = 0;
}

void hud_floattext_update(void) {
    for (int i = 0; i < MAX_FLOATS; i++) {
        if (floats[i].timer > 0) {
            floats[i].timer--;
            /* Rise upward by 1px per frame (in 8.8 fixed-point) */
            floats[i].wy -= 256;
            /* No per-float clear needed — hud_floattext_draw clears region each frame */
        }
    }
}

void hud_floattext_draw(void) {
    /* Clear previous float positions (targeted clear instead of 270-tile blanket) */
    for (int i = 0; i < MAX_FLOATS; i++) {
        if (floats[i].prev_digits > 0 && floats[i].prev_sy >= 4 && floats[i].prev_sy <= 12) {
            for (int d = 0; d < floats[i].prev_digits; d++) {
                int cx = floats[i].prev_sx + d;
                if (cx >= 0 && cx < 30)
                    text_put_char(cx, floats[i].prev_sy, ' ');
            }
            floats[i].prev_digits = 0;
        }
    }

    for (int i = 0; i < MAX_FLOATS; i++) {
        if (floats[i].timer <= 0) continue;
        /* Blink in last 6 frames */
        if (floats[i].timer <= 6 && !(floats[i].timer & 1)) continue;

        /* Convert world→screen (pixels then to tiles) */
        int sx = (int)((floats[i].wx - hud_cam_x) >> 8) / 8;
        int sy = (int)((floats[i].wy - hud_cam_y) >> 8) / 8;
        /* Only draw in safe BG0 area (rows 4-12, cols 0-29) */
        if (sx < 0 || sx >= 28 || sy < 4 || sy > 12) continue;

        /* Print the number with color: red=damage, green=heal, yellow=crit */
        int v = floats[i].value;
        int pal_bits;
        if (floats[i].value < 0) pal_bits = 0; /* green for heal (pal 0) */
        else if (floats[i].is_crit) pal_bits = 1 << 12; /* amber/yellow for crit */
        else pal_bits = 3 << 12; /* red for normal damage */
        if (v < 0) v = -v;
        if (v > 999) v = 999;
        /* Render digits right-to-left */
        int digits = 0;
        int tmp = v;
        do { digits++; tmp /= 10; } while (tmp > 0);
        tmp = v;
        for (int d = digits - 1; d >= 0; d--) {
            if (sx + d < 30 && sx + d >= 0) {
                u16 tile = (u16)(('0' + tmp % 10 - ' ') | pal_bits);
                ((u16*)se_mem[31])[(sy) * 32 + sx + d] = tile;
            }
            tmp /= 10;
        }
        /* Save position for targeted clear next frame */
        floats[i].prev_sx = (s8)sx;
        floats[i].prev_sy = (s8)sy;
        floats[i].prev_digits = (u8)digits;
    }
}

void hud_set_camera(s32 cam_x_fp, s32 cam_y_fp) {
    hud_cam_x = cam_x_fp;
    hud_cam_y = cam_y_fp;
}
