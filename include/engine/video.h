#ifndef ENGINE_VIDEO_H
#define ENGINE_VIDEO_H

#include <tonc.h>

/* Initialize Mode 0 display with 4 BG layers and VBlank IRQ. */
void video_init(void);

/* Wait for VBlank and perform OAM DMA copy. Call at end of each frame. */
void video_vsync(void);

/* Update parallax scroll offsets based on camera position (8.8 fixed-point). */
void video_scroll_parallax(s32 cam_x, s32 cam_y);

/* Trigger screen shake (duration in frames, intensity in pixels). */
void video_shake(int frames, int intensity);

/* Tick shake timer (call once per frame). Returns current shake offset. */
int video_shake_update(void);

/* ---- Alpha blending ---- */

/* Set up hardware alpha blending. */
void video_blend_setup(u16 bldcnt, u16 bldalpha, u16 bldy);

/* Clear all blend registers. */
void video_blend_clear(void);

/* ---- Mosaic ---- */

/* Set OBJ mosaic size (0-15). 0 = disabled. */
void video_mosaic_obj(int size);

/* Set BG mosaic size (0-15). 0 = disabled. */
void video_mosaic_bg(int size);

/* Clear all mosaic. */
void video_mosaic_clear(void);

/* ---- Hit Flash ---- */

/* Flash an OBJ palette bank to white for N frames then restore. */
void video_hit_flash_start(int pal_bank, int frames);

/* Update hit flash timer. Call once per frame. */
void video_hit_flash_update(void);

/* ---- Transitions ---- */

/* Start a fade-to-black transition (duration in frames). */
void video_fade_start(int frames);

/* Start a fade-from-black transition (duration in frames). */
void video_fadein_start(int frames);

/* Start a brightness flash (white for flash_frames, then fade back). */
void video_flash_start(int flash_frames, int fade_frames);

/* Update transition effects. Call once per frame. Returns 1 while active. */
int video_transition_update(void);

#endif /* ENGINE_VIDEO_H */
