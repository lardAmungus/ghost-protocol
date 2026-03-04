/*
 * Ghost Protocol — Class Abilities
 *
 * Per-class ability activation and effects.
 */
#include "game/abilities.h"
#include "game/player.h"
#include "game/projectile.h"
#include "game/enemy.h"
#include "game/hud.h"
#include "engine/audio.h"
#include "engine/video.h"

/* ---- Ability name tables ---- */
static const char* const assault_names[4] = {
    "Charged Shot", "Burst Fire", "Heavy Shell", "Overclock"
};
static const char* const infiltrator_names[4] = {
    "Air Dash", "Phase Shot", "Fan Fire", "Overload"
};
static const char* const technomancer_names[4] = {
    "Turret Deploy", "Scan Pulse", "Data Shield", "Sys Crash"
};

static const int assault_cd[4]      = { AB_CD_SHORT, AB_CD_MEDIUM, AB_CD_LONG, AB_CD_ULTRA };
static const int infiltrator_cd[4]  = { AB_CD_SHORT, AB_CD_SHORT, AB_CD_LONG, AB_CD_ULTRA };
static const int technomancer_cd[4] = { AB_CD_MEDIUM, AB_CD_SHORT, AB_CD_MEDIUM, AB_CD_ULTRA };

/* ---- Active effect state ---- */
static int overclock_timer = 0;   /* Assault ability 4: double fire rate */
static int data_shield_timer = 0; /* Technomancer ability 3: damage reduction */

void ability_reset(void) {
    overclock_timer = 0;
    data_shield_timer = 0;
    /* Clear cooldowns so abilities are fresh each level */
    for (int i = 0; i < 4; i++) {
        player_state.cooldown_ability[i] = 0;
    }
}

static void activate_assault(int slot) {
    Entity* pe = player_get();
    if (!pe) return;

    switch (slot) {
    case 0: /* Charged Shot — fire a 3x damage projectile */
    {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)(player_state.atk * 3), SUBTYPE_PROJ_CHARGE, 0, 0);
        break;
    }
    case 1: /* Burst Fire — 3 rapid shots */
        for (int i = 0; i < 3; i++) {
            s16 dir = pe->facing ? -448 : 448;
            s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
            s32 sy = pe->y + FP8(2) + FP8(i * 3);
            projectile_spawn(sx, sy, dir, 0,
                             player_state.atk, SUBTYPE_PROJ_RAPID, 0, 0);
        }
        break;
    case 2: /* Heavy Shell — slow piercing shot that hits all enemies in path */
    {
        s16 dir = pe->facing ? -320 : 320;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)(player_state.atk * 3), SUBTYPE_PROJ_BUSTER, PROJ_PIERCE, 0);
        break;
    }
    case 3: /* Overclock — double fire rate for 3 seconds */
        overclock_timer = 180;
        hud_notify("OVERCLOCK!", 60);
        break;
    }
}

static void activate_infiltrator(int slot) {
    Entity* pe = player_get();
    if (!pe) return;

    switch (slot) {
    case 0: /* Air Dash — quick horizontal burst */
    {
        s16 dash_v = pe->facing ? -768 : 768;
        pe->vx = dash_v;
        pe->vy = 0;
        player_state.dash_timer = 8;
        player_state.state = PSTATE_DASH;
        player_state.dash_cooldown = 30;
        audio_play_sfx(SFX_DASH);
        break;
    }
    case 1: /* Phase Shot — passes through walls */
    {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         player_state.atk, SUBTYPE_PROJ_BUSTER, PROJ_PHASE, 0);
        break;
    }
    case 2: /* Fan Fire — 3 projectiles in a forward spread */
    {
        s16 dir = pe->facing ? -384 : 384;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        s16 side_dmg = (s16)(player_state.atk * 2 / 3);
        if (side_dmg < 1) side_dmg = 1;
        /* Main forward */
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         player_state.atk, SUBTYPE_PROJ_RAPID, 0, 0);
        /* Upper diagonal */
        projectile_spawn(sx, pe->y, dir, -128,
                         side_dmg, SUBTYPE_PROJ_RAPID, 0, 0);
        /* Lower diagonal */
        projectile_spawn(sx, pe->y + FP8(8), dir, 128,
                         side_dmg, SUBTYPE_PROJ_RAPID, 0, 0);
        break;
    }
    case 3: /* Overload — EMP stun: damage and stun all enemies on screen */
        enemy_stun_all(player_state.atk * 2);
        video_shake(10, 3);
        hud_notify("EMP OVERLOAD!", 60);
        break;
    }
}

static void activate_technomancer(int slot) {
    Entity* pe = player_get();
    if (!pe) return;

    switch (slot) {
    case 0: /* Turret Deploy — fire 6 projectiles in a forward cone volley */
    {
        s16 dir = pe->facing ? -384 : 384;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        /* 3 speeds x 2 angles = 6 projectiles */
        projectile_spawn(sx, pe->y + FP8(2), dir, -96,
                         player_state.atk, SUBTYPE_PROJ_BUSTER, 0, 0);
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         player_state.atk, SUBTYPE_PROJ_BUSTER, 0, 0);
        projectile_spawn(sx, pe->y + FP8(6), dir, 96,
                         player_state.atk, SUBTYPE_PROJ_BUSTER, 0, 0);
        projectile_spawn(sx, pe->y + FP8(2), (s16)(dir * 3 / 4), -64,
                         player_state.atk, SUBTYPE_PROJ_RAPID, 0, 0);
        projectile_spawn(sx, pe->y + FP8(4), (s16)(dir * 3 / 4), 0,
                         player_state.atk, SUBTYPE_PROJ_RAPID, 0, 0);
        projectile_spawn(sx, pe->y + FP8(6), (s16)(dir * 3 / 4), 64,
                         player_state.atk, SUBTYPE_PROJ_RAPID, 0, 0);
        break;
    }
    case 1: /* Scan Pulse — wide piercing beam that hits all enemies in path */
    {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        /* 5 piercing projectiles in a wide spread */
        projectile_spawn(sx, pe->y - FP8(4), dir, -64,
                         (s16)(player_state.atk / 2), SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        projectile_spawn(sx, pe->y, dir, -32,
                         (s16)(player_state.atk / 2), SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         player_state.atk, SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        projectile_spawn(sx, pe->y + FP8(8), dir, 32,
                         (s16)(player_state.atk / 2), SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        projectile_spawn(sx, pe->y + FP8(12), dir, 64,
                         (s16)(player_state.atk / 2), SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        break;
    }
    case 2: /* Data Shield — reduce damage for 3 seconds */
        data_shield_timer = 180;
        hud_notify("DATA SHIELD!", 60);
        break;
    case 3: /* System Crash — screen nuke (4 cardinal directions) */
    {
        static const s16 dx[4] = { 384, 0, -384, 0 };
        static const s16 dy[4] = { 0, -384, 0, 384 };
        for (int d = 0; d < 4; d++) {
            projectile_spawn(pe->x + FP8(4), pe->y + FP8(4),
                             dx[d], dy[d],
                             (s16)(player_state.atk * 2),
                             SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        }
        break;
    }
    }
}

int ability_activate(int player_class, int slot) {
    if (slot < 0 || slot >= 4) return 0;

    /* Check if unlocked */
    u8 mask = (u8)(1 << slot);
    if (!(player_state.ability_unlocks & mask)) return 0;

    /* Check cooldown */
    if (player_state.cooldown_ability[slot] > 0) {
        audio_play_sfx(SFX_MENU_BACK);
        return 0;
    }

    /* Check projectile pool has enough slots for multi-projectile abilities
     * (Technomancer Turret Deploy spawns 6, Scan Pulse spawns 5) */
    int active = projectile_active_count();
    if (active > MAX_PROJECTILES - 6) {
        audio_play_sfx(SFX_MENU_BACK);
        return 0;
    }

    /* Set cooldown */
    int cd = ability_get_cooldown(player_class, slot);
    player_state.cooldown_ability[slot] = (u16)cd;

    audio_play_sfx(SFX_ABILITY);

    switch (player_class) {
    case CLASS_ASSAULT:      activate_assault(slot); break;
    case CLASS_INFILTRATOR:  activate_infiltrator(slot); break;
    case CLASS_TECHNOMANCER: activate_technomancer(slot); break;
    }

    return 1;
}

void ability_update(void) {
    /* Tick cooldowns */
    for (int i = 0; i < 4; i++) {
        if (player_state.cooldown_ability[i] > 0) {
            player_state.cooldown_ability[i]--;
        }
    }

    /* Tick active effects */
    if (overclock_timer > 0) overclock_timer--;
    if (data_shield_timer > 0) data_shield_timer--;
}

const char* ability_get_name(int player_class, int slot) {
    if (slot < 0 || slot >= 4) return "";
    switch (player_class) {
    case CLASS_ASSAULT:      return assault_names[slot];
    case CLASS_INFILTRATOR:  return infiltrator_names[slot];
    case CLASS_TECHNOMANCER: return technomancer_names[slot];
    default: return "";
    }
}

int ability_get_cooldown(int player_class, int slot) {
    if (slot < 0 || slot >= 4) return 0;
    switch (player_class) {
    case CLASS_ASSAULT:      return assault_cd[slot];
    case CLASS_INFILTRATOR:  return infiltrator_cd[slot];
    case CLASS_TECHNOMANCER: return technomancer_cd[slot];
    default: return 0;
    }
}

int ability_is_overclock_active(void) {
    return overclock_timer > 0;
}

int ability_is_data_shield_active(void) {
    return data_shield_timer > 0;
}
