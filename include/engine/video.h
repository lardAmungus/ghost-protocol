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

#endif /* ENGINE_VIDEO_H */
