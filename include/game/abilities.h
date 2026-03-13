#ifndef GAME_ABILITIES_H
#define GAME_ABILITIES_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Class Abilities
 *
 * Each class has 8 abilities unlocked at levels 3, 5, 8, 10, 14, 18, 22, 26.
 * Assault:      Charged Shot, Burst Fire, Heavy Shell, Overclock,
 *               Rocket, Iron Skin, War Cry, Berserk
 * Infiltrator:  Air Dash, Phase Shot, Fan Fire, Overload,
 *               Smoke Bomb, Backstab, Clone, Time Warp
 * Technomancer: Turret Deploy, Scan Pulse, Data Shield, System Crash,
 *               Nanobots, Firewall, Overclock+, Upload
 */

#define AB_SLOT_COUNT 8

/* Ability indices (per class) */
#define AB_SLOT_1  0  /* Level 3 */
#define AB_SLOT_2  1  /* Level 5 */
#define AB_SLOT_3  2  /* Level 8 */
#define AB_SLOT_4  3  /* Level 10 */
#define AB_SLOT_5  4  /* Level 14 */
#define AB_SLOT_6  5  /* Level 18 */
#define AB_SLOT_7  6  /* Level 22 */
#define AB_SLOT_8  7  /* Level 26 */

/* Ability unlock level thresholds */
#define AB_UNLOCK_1  3
#define AB_UNLOCK_2  5
#define AB_UNLOCK_3  8
#define AB_UNLOCK_4  10
#define AB_UNLOCK_5  14
#define AB_UNLOCK_6  18
#define AB_UNLOCK_7  22
#define AB_UNLOCK_8  26

/* Cooldown frames per ability (varies by power) */
#define AB_CD_SHORT   60   /* 1 second */
#define AB_CD_MEDIUM  120  /* 2 seconds */
#define AB_CD_LONG    180  /* 3 seconds */
#define AB_CD_ULTRA   300  /* 5 seconds */

/* Try to activate ability slot (0-7) for the given class.
 * Returns 1 if activated, 0 if on cooldown or not unlocked. */
int ability_activate(int player_class, int slot);

/* Update ability effects (turrets, shields, etc). Call per frame. */
void ability_update(void);

/* Get the name string for an ability. */
const char* ability_get_name(int player_class, int slot);

/* Get cooldown duration for an ability. */
int ability_get_cooldown(int player_class, int slot);

/* Check if Overclock (Assault) is active — doubles fire rate. */
int ability_is_overclock_active(void);

/* Check if Data Shield (Technomancer) is active — halves incoming damage. */
int ability_is_data_shield_active(void);

/* Check if Iron Skin (Assault) is active — DEF doubled. */
int ability_is_iron_skin_active(void);

/* Check if Berserk (Assault) is active — ATK x1.5, DEF x0.5. */
int ability_is_berserk_active(void);

/* Check if Smoke Bomb (Infiltrator) is active — enemies lose tracking. */
int ability_is_smoke_active(void);

/* Check if Backstab (Infiltrator) is active — next hit from behind 3x. */
int ability_is_backstab_active(void);

/* Check if Time Warp (Infiltrator) is active — enemies half speed. */
int ability_is_time_warp_active(void);

/* Check if Nanobots (Technomancer) is active — HP regen. */
int ability_is_nanobots_active(void);

/* Check if Firewall (Technomancer) is active — damage reflection. */
int ability_is_firewall_active(void);

/* Check if Overclock+ (Technomancer) is active — all cooldowns halved. */
int ability_is_overclock_plus_active(void);

/* Check if Upload (Technomancer) is active — marked enemy takes 2x. */
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
