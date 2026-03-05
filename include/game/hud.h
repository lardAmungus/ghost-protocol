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

/* Set camera X for section minimap (8.8 fixed-point). */
void hud_set_camera_x(s32 cam_x_fp);

/* Show damage direction indicator (0=left, 1=right). */
void hud_damage_direction(int from_right);

/* ---- Floating damage numbers ---- */

/* Spawn a floating damage number at world position (8.8 fixed-point). */
void hud_floattext_spawn(s32 wx, s32 wy, int value, int is_crit);

/* Clear all floating text entries. */
void hud_floattext_clear(void);

/* Update floating text timers and positions. Call once per frame. */
void hud_floattext_update(void);

/* Draw floating text on BG0. Call once per frame after update. */
void hud_floattext_draw(void);

/* Set camera position for world→screen conversion (8.8 fixed-point). */
void hud_set_camera(s32 cam_x_fp, s32 cam_y_fp);

#endif /* GAME_HUD_H */
