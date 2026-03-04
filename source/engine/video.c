#include "engine/video.h"
#include "engine/sprite.h"

/* Screen shake state */
static int shake_timer = 0;
static int shake_intensity = 0;
static int shake_offset = 0;

static void vblank_isr(void) {
    /* Copy shadow OAM to hardware during VBlank */
    sprite_oam_update();
}

void video_init(void) {
    /* Mode 0: BG0 (UI) + BG1 (world) + BG2 (parallax) + sprites */
    REG_DISPCNT = DCNT_MODE0 | DCNT_OBJ | DCNT_OBJ_1D
                | DCNT_BG0 | DCNT_BG1 | DCNT_BG2;

    /*
     * BG layer setup:
     *   BG0 = UI/HUD text overlay (priority 0, highest)
     *   BG1 = Level tilemap   (priority 1)
     *   BG2 = Parallax        (priority 2)
     *   BG3 = Reserved        (priority 3, lowest)
     */
    REG_BG0CNT = BG_PRIO(0) | BG_4BPP | BG_SBB(31) | BG_CBB(0) | BG_REG_32x32;
    REG_BG1CNT = BG_PRIO(1) | BG_4BPP | BG_SBB(28) | BG_CBB(1) | BG_REG_64x32;
    REG_BG2CNT = BG_PRIO(2) | BG_4BPP | BG_SBB(30) | BG_CBB(2) | BG_REG_32x32;
    REG_BG3CNT = BG_PRIO(3) | BG_4BPP | BG_SBB(30) | BG_CBB(2) | BG_REG_32x32;

    /* Setup VBlank IRQ */
    irq_init(NULL);
    irq_add(II_VBLANK, vblank_isr);
}

void video_vsync(void) {
    VBlankIntrWait();
}

void video_scroll_parallax(s32 cam_x, s32 cam_y) {
    int px = (int)(cam_x >> 8);
    int py = (int)(cam_y >> 8);

    /* Apply screen shake offset to world layer */
    REG_BG1HOFS = (u16)(px + shake_offset);
    REG_BG1VOFS = (u16)py;

    /* BG2 parallax at half camera speed */
    REG_BG2HOFS = (u16)((px >> 1) + shake_offset);
    REG_BG2VOFS = (u16)(py >> 1);
}

void video_shake(int frames, int intensity) {
    shake_timer = frames;
    shake_intensity = intensity;
}

int video_shake_update(void) {
    if (shake_timer > 0) {
        shake_timer--;
        /* Alternate left/right each frame */
        shake_offset = (shake_timer & 1) ? shake_intensity : -shake_intensity;
        if (shake_timer == 0) shake_offset = 0;
    }
    return shake_offset;
}
