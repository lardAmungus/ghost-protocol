#ifndef GAME_ABILITIES_H
#define GAME_ABILITIES_H

#include <tonc.h>
#include "game/common.h"

/*
 * Ghost Protocol — Class Abilities
 *
 * Each class has 4 abilities unlocked at levels 3, 7, 12, 17.
 * Assault:      Charged Shot, Burst Fire, Heavy Shell, Overclock
 * Infiltrator:  Air Dash, Phase Shot, Decoy, Overload
 * Technomancer: Turret Deploy, Scan Pulse, Data Shield, System Crash
 */

/* Ability indices (per class) */
#define AB_SLOT_1  0  /* Level 3 */
#define AB_SLOT_2  1  /* Level 7 */
#define AB_SLOT_3  2  /* Level 12 */
#define AB_SLOT_4  3  /* Level 17 */

/* Ability unlock level thresholds */
#define AB_UNLOCK_1  3
#define AB_UNLOCK_2  7
#define AB_UNLOCK_3  12
#define AB_UNLOCK_4  17

/* Cooldown frames per ability (varies by power) */
#define AB_CD_SHORT   60   /* 1 second */
#define AB_CD_MEDIUM  120  /* 2 seconds */
#define AB_CD_LONG    180  /* 3 seconds */
#define AB_CD_ULTRA   300  /* 5 seconds */

/* Try to activate ability slot (0-3) for the given class.
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

/* Reset active ability effects (call on player death/level transition). */
void ability_reset(void);

#endif /* GAME_ABILITIES_H */
