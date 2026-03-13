#include "engine/video.h"
#include "engine/sprite.h"

#ifndef HEADLESS_TEST
#include <maxmod.h>
#endif

/* Screen shake state */
static int shake_timer = 0;
static int shake_intensity = 0;
static int shake_offset = 0;

static void vblank_isr(void) {
#ifndef HEADLESS_TEST
    /* Maxmod requires mmVBlank() called from VBlank ISR for audio buffer management */
    mmVBlank();
#endif
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

    /* BG2 parallax: half-speed horizontal, quarter-speed vertical for depth */
    REG_BG2HOFS = (u16)((px >> 1) + shake_offset);
    REG_BG2VOFS = (u16)(py >> 2);
}

void video_shake(int frames, int intensity) {
    /* Only override if new shake is stronger or current has faded */
    if (intensity > shake_intensity || shake_timer == 0) {
        shake_timer = frames;
        shake_intensity = intensity;
    }
}

int video_shake_update(void) {
    if (shake_timer > 0) {
        shake_timer--;
        /* Decaying intensity: halve every 8 frames for natural falloff */
        int current_intensity = shake_intensity;
        if (shake_timer < shake_intensity * 2) {
            current_intensity = 1 + shake_timer / 3;
            if (current_intensity > shake_intensity) current_intensity = shake_intensity;
        }
        /* Alternate left/right each frame */
        shake_offset = (shake_timer & 1) ? current_intensity : -current_intensity;
        if (shake_timer == 0) {
            shake_offset = 0;
            shake_intensity = 0;
        }
    }
    return shake_offset;
}

/* ---- Hit Flash (white palette pulse) ---- */

static int flash_bank = -1;     /* OBJ palette bank being flashed (-1 = none) */
static int flash_timer = 0;
static u16 flash_backup[16];    /* saved palette for restore */

void video_hit_flash_start(int pal_bank, int frames) {
    if (pal_bank < 0 || pal_bank >= 16) return;
    /* Save original palette and overwrite with white */
    if (flash_timer > 0 && flash_bank >= 0) {
        /* Restore previous flash first */
        memcpy16(&pal_obj_mem[flash_bank * 16], flash_backup, 16);
    }
    flash_bank = pal_bank;
    flash_timer = frames > 0 ? frames : 2;
    /* Backup original */
    memcpy16(flash_backup, &pal_obj_mem[pal_bank * 16], 16);
    /* Set all non-transparent entries to white */
    for (int i = 1; i < 16; i++) {
        pal_obj_mem[pal_bank * 16 + i] = RGB15(31, 31, 31);
    }
}

void video_hit_flash_update(void) {
    if (flash_timer <= 0) return;
    flash_timer--;
    if (flash_timer == 0 && flash_bank >= 0) {
        /* Restore original palette */
        memcpy16(&pal_obj_mem[flash_bank * 16], flash_backup, 16);
        flash_bank = -1;
    }
}

/* ---- Alpha blending ---- */

void video_blend_setup(u16 bldcnt, u16 bldalpha, u16 bldy) {
    REG_BLDCNT = bldcnt;
    REG_BLDALPHA = bldalpha;
    REG_BLDY = bldy;
}

void video_blend_clear(void) {
    REG_BLDCNT = 0;
    REG_BLDALPHA = 0;
    REG_BLDY = 0;
}

/* ---- Mosaic ---- */

/* REG_MOSAIC is write-only on GBA — must shadow the value in RAM */
static u16 mosaic_shadow = 0;

void video_mosaic_obj(int size) {
    mosaic_shadow = (u16)((mosaic_shadow & 0x00FF) | ((size & 0xF) << 8) | ((size & 0xF) << 12));
    REG_MOSAIC = mosaic_shadow;
}

void video_mosaic_bg(int size) {
    mosaic_shadow = (u16)((mosaic_shadow & 0xFF00) | ((size & 0xF) << 0) | ((size & 0xF) << 4));
    REG_MOSAIC = mosaic_shadow;
}

void video_mosaic_clear(void) {
    mosaic_shadow = 0;
    REG_MOSAIC = 0;
}

/* ---- Transitions ---- */

static int trans_type = 0;      /* 0=none, 1=fade-to-black, 2=flash */
static int trans_timer = 0;
static int trans_duration = 0;
static int flash_flash = 0;
static int flash_fade = 0;

void video_fade_start(int frames) {
    trans_type = 1;
    trans_timer = 0;
    trans_duration = frames > 0 ? frames : 1;
    REG_BLDCNT = BLD_TOP(BLD_BG0 | BLD_BG1 | BLD_BG2 | BLD_OBJ) | BLD_WHITE;
}

void video_fadein_start(int frames) {
    trans_type = 3;
    trans_timer = 0;
    trans_duration = frames > 0 ? frames : 1;
    REG_BLDCNT = BLD_TOP(BLD_BG0 | BLD_BG1 | BLD_BG2 | BLD_OBJ) | BLD_BLACK;
    REG_BLDY = 16; /* Start fully black */
}

void video_flash_start(int flash_frames, int fade_frames) {
    trans_type = 2;
    trans_timer = 0;
    flash_flash = flash_frames > 0 ? flash_frames : 1;
    flash_fade = fade_frames > 0 ? fade_frames : 1;
    trans_duration = flash_flash + flash_fade;
    REG_BLDCNT = BLD_TOP(BLD_BG0 | BLD_BG1 | BLD_BG2 | BLD_OBJ) | BLD_WHITE;
}

int video_transition_update(void) {
    if (trans_type == 0) return 0;

    trans_timer++;

    if (trans_type == 1) {
        /* Fade to white */
        int level = trans_timer * 16 / trans_duration;
        if (level > 16) level = 16;
        REG_BLDY = (u16)level;
        if (trans_timer >= trans_duration) {
            trans_type = 0;
            return 0;
        }
    } else if (trans_type == 2) {
        /* Flash then fade */
        if (trans_timer <= flash_flash) {
            REG_BLDY = 16; /* Full white */
        } else {
            int elapsed = trans_timer - flash_flash;
            int level = 16 - elapsed * 16 / flash_fade;
            if (level < 0) level = 0;
            REG_BLDY = (u16)level;
        }
        if (trans_timer >= trans_duration) {
            REG_BLDY = 0;
            REG_BLDCNT = 0;
            trans_type = 0;
            return 0;
        }
    } else if (trans_type == 3) {
        /* Fade from black */
        int level = 16 - trans_timer * 16 / trans_duration;
        if (level < 0) level = 0;
        REG_BLDY = (u16)level;
        if (trans_timer >= trans_duration) {
            REG_BLDY = 0;
            REG_BLDCNT = 0;
            trans_type = 0;
            return 0;
        }
    }

    return 1;
}

void video_reset_effects(void) {
    /* Restore palette if a hit flash was in progress */
    if (flash_timer > 0 && flash_bank >= 0 && flash_bank < 16) {
        memcpy16(&pal_obj_mem[flash_bank * 16], flash_backup, 16);
    }
    /* Clear all effect state */
    shake_timer = 0;
    shake_intensity = 0;
    shake_offset = 0;
    flash_bank = -1;
    flash_timer = 0;
    trans_type = 0;
    trans_timer = 0;
    trans_duration = 0;
    flash_flash = 0;
    flash_fade = 0;
    mosaic_shadow = 0;
    REG_MOSAIC = 0;
    REG_BLDCNT = 0;
    REG_BLDALPHA = 0;
    REG_BLDY = 0;
}
