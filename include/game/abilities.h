#ifndef GAME_ABILITIES_H
#define GAME_ABILITIES_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Buff Timer & Skill Activation Bridge
 *
 * Buff timers tick down each frame via ability_update().
 * Getter functions expose buff state to combat/enemy/HUD systems.
 * skill_activate() dispatches skill effects by ID.
 */

/* Update ability effects (buff timers, nanobots regen). Call per frame. */
void ability_update(void);

/* Activate a skill by ID (dispatches to appropriate effect).
 * rank: current skill rank (0-10), player_atk: current ATK stat.
 * Returns 1 if activated, 0 if failed. */
int skill_activate(int skill_id, int rank, int player_atk);

/* Check if Overclock is active — doubles fire rate. */
int ability_is_overclock_active(void);

/* Check if Data Shield is active — halves incoming damage. */
int ability_is_data_shield_active(void);

/* Check if Iron Skin is active — DEF doubled. */
int ability_is_iron_skin_active(void);

/* Check if Berserk is active — ATK x1.5, DEF x0.5. */
int ability_is_berserk_active(void);

/* Check if Smoke is active — enemies lose tracking. */
int ability_is_smoke_active(void);

/* Check if Backstab is active — next hit from behind 3x. */
int ability_is_backstab_active(void);

/* Check if Time Warp is active — enemies half speed. */
int ability_is_time_warp_active(void);

/* Check if Nanobots is active — HP regen. */
int ability_is_nanobots_active(void);

/* Check if Firewall is active — damage reflection. */
int ability_is_firewall_active(void);

/* Check if Overclock+ is active — all cooldowns halved. */
int ability_is_overclock_plus_active(void);

/* Check if Upload is active — marked enemy takes 2x. */
int ability_is_upload_active(void);

/* Get remaining frames for active buff (for HUD timer display). */
int ability_get_overclock_timer(void);
int ability_get_iron_skin_timer(void);
int ability_get_berserk_timer(void);
int ability_get_data_shield_timer(void);
int ability_get_smoke_timer(void);
int ability_get_backstab_timer(void);
int ability_get_time_warp_timer(void);
int ability_get_nanobots_timer(void);

/* Reset active ability effects (call on player death/level transition). */
void ability_reset(void);

#endif /* GAME_ABILITIES_H */
