#ifndef GAME_HUD_H
#define GAME_HUD_H

#include <tonc.h>

/*
 * Ghost Protocol — In-Game HUD
 *
 * BG0 overlay showing HP, level, ability cooldowns, boss HP.
 */

/* Initialize HUD (load UI tile graphics). */
void hud_init(void);

/* Draw HUD every frame. */
void hud_draw(void);

/* Show boss HP bar (name + HP). Call with NULL to hide. */
void hud_set_boss(const char* name, int hp, int max_hp);

/* Show a notification message (centered, duration in frames). */
void hud_notify(const char* msg, int duration);

/* Update notification timer. */
void hud_notify_update(void);

/* Set kill objective display (kills / target). 0 target = hide. */
void hud_set_objective(int kills, int target);

/* Set trace timer display (frames remaining). 0 = hide. */
void hud_set_trace(int frames);

/* Set score display (replaces Lv during bug bounty). 0 = show Lv. */
void hud_set_score(u16 score);

#endif /* GAME_HUD_H */
