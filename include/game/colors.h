#ifndef GAME_COLORS_H
#define GAME_COLORS_H

#include <tonc.h>

#define COLOR_PRESET_COUNT 24

/* Apply suit and visor colors to player OBJ palette bank 0.
 * suit_idx: 0-23, visor_idx: 0-23.
 * Writes to pal_obj_mem[0] indices 2-4 (suit) and 5-7 (visor). */
void colors_apply_player_palette(int suit_idx, int visor_idx);

/* Get preview RGB15 value for a suit preset (returns the "base" shade). */
u16 colors_get_suit_preview(int idx);

/* Get preview RGB15 value for a visor preset (returns the "bright" shade). */
u16 colors_get_visor_preview(int idx);

#endif /* GAME_COLORS_H */
