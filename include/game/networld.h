#ifndef GAME_NETWORLD_H
#define GAME_NETWORLD_H

#include <tonc.h>

/*
 * Ghost Protocol — Net World Tile Management
 *
 * Handles BG1 tileset loading and column streaming for
 * side-scrolling levels wider than 64 tiles.
 */

/* Load the Net tileset and palette into VRAM (CBB1).
 * act: 0=freelance, 1-5=story act for themed palette. */
void networld_load_tileset(int act);

/* Stream columns into BG1 screenblocks as camera scrolls.
 * Call every frame with current camera pixel X position. */
void networld_stream_columns(int cam_px);

/* Force-load all visible columns (call on level entry). */
void networld_load_visible(int cam_px);

/* Load the parallax BG for Net levels. Seed varies skyline pattern.
 * act: 0=freelance, 1-5=story act for themed palette. */
void networld_load_parallax(u16 seed, int act);

#endif /* GAME_NETWORLD_H */
