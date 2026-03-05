/*
 * Ghost Protocol — Class Abilities
 *
 * Per-class ability activation and effects.
 * 8 abilities per class unlocked at levels 3, 5, 8, 10, 14, 18, 22, 26.
 */
#include "game/abilities.h"
#include "game/player.h"
#include "game/projectile.h"
#include "game/enemy.h"
#include "game/hud.h"
#include "game/particle.h"
#include "engine/audio.h"
#include "engine/video.h"

/* ---- Ability name tables ---- */
static const char* const assault_names[AB_SLOT_COUNT] = {
    "Charged Shot", "Burst Fire", "Heavy Shell", "Overclock",
    "Rocket", "Iron Skin", "War Cry", "Berserk"
};
static const char* const infiltrator_names[AB_SLOT_COUNT] = {
    "Air Dash", "Phase Shot", "Fan Fire", "Overload",
    "Smoke Bomb", "Backstab", "Clone", "Time Warp"
};
static const char* const technomancer_names[AB_SLOT_COUNT] = {
    "Turret Deploy", "Scan Pulse", "Data Shield", "Sys Crash",
    "Nanobots", "Firewall", "Overclock+", "Upload"
};

static const int assault_cd[AB_SLOT_COUNT] = {
    AB_CD_SHORT, AB_CD_MEDIUM, AB_CD_LONG, AB_CD_ULTRA,
    AB_CD_LONG, AB_CD_MEDIUM, AB_CD_LONG, AB_CD_MEDIUM
};
static const int infiltrator_cd[AB_SLOT_COUNT] = {
    AB_CD_SHORT, AB_CD_SHORT, AB_CD_LONG, AB_CD_ULTRA,
    AB_CD_MEDIUM, AB_CD_SHORT, AB_CD_LONG, AB_CD_LONG
};
static const int technomancer_cd[AB_SLOT_COUNT] = {
    AB_CD_MEDIUM, AB_CD_SHORT, AB_CD_MEDIUM, AB_CD_ULTRA,
    AB_CD_MEDIUM, AB_CD_LONG, AB_CD_LONG, AB_CD_MEDIUM
};

/* ---- Active effect state ---- */
static int overclock_timer = 0;     /* Assault slot 3: double fire rate */
static int data_shield_timer = 0;   /* Technomancer slot 2: damage reduction */
static int iron_skin_timer = 0;     /* Assault slot 5: DEF doubled */
static int berserk_timer = 0;       /* Assault slot 7: ATK x1.5, DEF x0.5 */
static int smoke_timer = 0;         /* Infiltrator slot 4: enemies lose tracking */
static int backstab_timer = 0;      /* Infiltrator slot 5: next hit 3x from behind */
static int time_warp_timer = 0;     /* Infiltrator slot 6: enemies half speed */
static int nanobots_timer = 0;      /* Technomancer slot 4: HP regen */
static int firewall_timer = 0;      /* Technomancer slot 5: damage reflection */
static int overclock_plus_timer = 0;/* Technomancer slot 6: cooldowns halved */
static int upload_timer = 0;        /* Technomancer slot 7: marked enemy 2x damage */
static int nanobots_heal_tick = 0;  /* Counter for nanobot heal ticks */
static int skill_regen_tick = 0;    /* Counter for passive skill regen */

void ability_reset(void) {
    overclock_timer = 0;
    data_shield_timer = 0;
    iron_skin_timer = 0;
    berserk_timer = 0;
    smoke_timer = 0;
    backstab_timer = 0;
    time_warp_timer = 0;
    nanobots_timer = 0;
    firewall_timer = 0;
    overclock_plus_timer = 0;
    upload_timer = 0;
    nanobots_heal_tick = 0;
    skill_regen_tick = 0;
    /* Clear cooldowns so abilities are fresh each level */
    for (int i = 0; i < AB_SLOT_COUNT; i++) {
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
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 4, PART_STAR, 180, 18);
        break;
    case 4: /* Rocket — AoE explosion at impact point */
    {
        s16 dir = pe->facing ? -384 : 384;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        /* Single large damage projectile that pierces (simulates AoE) */
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)(player_state.atk * 4), SUBTYPE_PROJ_CHARGE, PROJ_PIERCE, 0);
        video_shake(2, 1);
        particle_burst(sx, pe->y + FP8(4), 2, PART_SPARK, 200, 10);
        break;
    }
    case 5: /* Iron Skin — temp DEF x2 for 3 seconds */
        iron_skin_timer = 180;
        hud_notify("IRON SKIN!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 3, PART_STAR, 120, 16);
        break;
    case 6: /* War Cry — stun all enemies in 48px radius */
        enemy_stun_all(0); /* 0 damage, just stun */
        video_shake(3, 1);
        hud_notify("WAR CRY!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 8, PART_SPARK, 250, 15);
        break;
    case 7: /* Berserk — ATK x1.5 but DEF x0.5 for 5 seconds */
        berserk_timer = 300;
        hud_notify("BERSERK!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 3, PART_BURST, 200, 12);
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
        video_shake(4, 2);
        hud_notify("EMP OVERLOAD!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 4, PART_SPARK, 250, 14);
        break;
    case 4: /* Smoke Bomb — enemies lose tracking for 3 seconds */
        smoke_timer = 180;
        hud_notify("SMOKE BOMB!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 6, PART_BURST, 150, 24);
        break;
    case 5: /* Backstab — next hit from behind deals 3x */
        backstab_timer = 300;
        hud_notify("BACKSTAB!", 60);
        particle_spawn(pe->x + FP8(6), pe->y + FP8(4), 0, -64, PART_STAR, 20);
        break;
    case 6: /* Clone — decoy (fires 3 projectiles in opposite direction) */
    {
        /* Fire 3 backward distraction shots */
        s16 dir = pe->facing ? 384 : -384;
        s32 sx = pe->x + (pe->facing ? FP8(12) : -FP8(4));
        for (int i = 0; i < 3; i++) {
            s32 sy = pe->y + FP8(i * 4);
            projectile_spawn(sx, sy, dir, 0,
                             (s16)(player_state.atk / 2), SUBTYPE_PROJ_RAPID, 0, 0);
        }
        hud_notify("CLONE!", 60);
        break;
    }
    case 7: /* Time Warp — all enemies half speed for 3 seconds */
        time_warp_timer = 180;
        hud_notify("TIME WARP!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 6, PART_STAR, 160, 22);
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
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 6, PART_STAR, 140, 20);
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
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 4, PART_SPARK, 250, 12);
        video_shake(3, 1);
        break;
    }
    case 4: /* Nanobots — heal 3 HP/sec for 5 seconds */
        nanobots_timer = 300;
        nanobots_heal_tick = 0;
        hud_notify("NANOBOTS!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 4, PART_STAR, 120, 24);
        break;
    case 5: /* Firewall — damage reflection ring for 3 seconds */
        firewall_timer = 180;
        hud_notify("FIREWALL!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 4, PART_SPARK, 200, 12);
        break;
    case 6: /* Overclock+ — all cooldowns halved for 5 seconds */
        overclock_plus_timer = 300;
        hud_notify("OVERCLOCK+!", 60);
        particle_burst(pe->x + FP8(6), pe->y + FP8(6), 5, PART_STAR, 180, 18);
        break;
    case 7: /* Upload — marked enemy takes 2x damage for 5 seconds */
        upload_timer = 300;
        hud_notify("UPLOAD!", 60);
        particle_spawn(pe->x + FP8(6), pe->y - FP8(4), 0, -96, PART_STAR, 24);
        particle_spawn(pe->x + FP8(6), pe->y + FP8(12), 0, 96, PART_STAR, 24);
        break;
    }
}

int ability_activate(int player_class, int slot) {
    if (slot < 0 || slot >= AB_SLOT_COUNT) return 0;

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

    /* Skill tree: offense branch index 2 = charge speed (-10/20/30% cooldown) */
    int cd_reduce = player_state.skill_tree[2] * 10;
    if (cd_reduce > 0) cd = cd * (100 - cd_reduce) / 100;

    /* Overclock+ halves all cooldowns when active */
    if (overclock_plus_timer > 0) {
        cd = cd / 2;
        if (cd < 30) cd = 30;
    }

    player_state.cooldown_ability[slot] = (u16)cd;

    audio_play_sfx(SFX_ABILITY);

    switch (player_class) {
    case CLASS_ASSAULT:      activate_assault(slot); break;
    case CLASS_INFILTRATOR:  activate_infiltrator(slot); break;
    case CLASS_TECHNOMANCER: activate_technomancer(slot); break;
    }

    /* Visual feedback: particle burst on activation */
    {
        Entity* p = player_get();
        if (p) {
            particle_burst(p->x + ((s32)p->width << 7),
                           p->y + ((s32)p->height << 7),
                           4, PART_STAR, 200, 16);
        }
    }

    return 1;
}

void ability_update(void) {
    /* Tick cooldowns */
    for (int i = 0; i < AB_SLOT_COUNT; i++) {
        if (player_state.cooldown_ability[i] > 0) {
            player_state.cooldown_ability[i]--;
        }
    }

    /* Tick active effects */
    if (overclock_timer > 0) overclock_timer--;
    if (data_shield_timer > 0) data_shield_timer--;
    if (iron_skin_timer > 0) iron_skin_timer--;
    if (berserk_timer > 0) berserk_timer--;
    if (smoke_timer > 0) smoke_timer--;
    if (backstab_timer > 0) backstab_timer--;
    if (time_warp_timer > 0) time_warp_timer--;
    if (firewall_timer > 0) firewall_timer--;
    if (overclock_plus_timer > 0) overclock_plus_timer--;
    if (upload_timer > 0) upload_timer--;

    /* Nanobots HP regen: 3 HP/sec = 1 HP every 20 frames */
    if (nanobots_timer > 0) {
        nanobots_timer--;
        nanobots_heal_tick++;
        if (nanobots_heal_tick >= 20) {
            nanobots_heal_tick = 0;
            if (player_state.hp < player_state.max_hp) {
                player_state.hp++;
                if (player_state.hp > player_state.max_hp) {
                    player_state.hp = player_state.max_hp;
                }
            }
        }
    }

    /* Skill tree: defense branch index 2 = passive regen
     * Rank 1: 1 HP every 300 frames (5s)
     * Rank 2: 1 HP every 200 frames (3.3s)
     * Rank 3: 1 HP every 150 frames (2.5s) */
    {
        int regen_rank = player_state.skill_tree[6];
        if (regen_rank > 0 && player_state.hp > 0 && player_state.hp < player_state.max_hp) {
            static const int regen_intervals[] = { 0, 300, 200, 150 };
            skill_regen_tick++;
            if (skill_regen_tick >= regen_intervals[regen_rank]) {
                skill_regen_tick = 0;
                player_state.hp++;
                if (player_state.hp > player_state.max_hp)
                    player_state.hp = player_state.max_hp;
            }
        } else {
            skill_regen_tick = 0;
        }
    }
}

const char* ability_get_name(int player_class, int slot) {
    if (slot < 0 || slot >= AB_SLOT_COUNT) return "";
    switch (player_class) {
    case CLASS_ASSAULT:      return assault_names[slot];
    case CLASS_INFILTRATOR:  return infiltrator_names[slot];
    case CLASS_TECHNOMANCER: return technomancer_names[slot];
    default: return "";
    }
}

int ability_get_cooldown(int player_class, int slot) {
    if (slot < 0 || slot >= AB_SLOT_COUNT) return 0;
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

int ability_is_iron_skin_active(void) {
    return iron_skin_timer > 0;
}

int ability_is_berserk_active(void) {
    return berserk_timer > 0;
}

int ability_is_smoke_active(void) {
    return smoke_timer > 0;
}

int ability_is_backstab_active(void) {
    return backstab_timer > 0;
}

int ability_is_time_warp_active(void) {
    return time_warp_timer > 0;
}

int ability_is_nanobots_active(void) {
    return nanobots_timer > 0;
}

int ability_is_firewall_active(void) {
    return firewall_timer > 0;
}

int ability_is_overclock_plus_active(void) {
    return overclock_plus_timer > 0;
}

int ability_is_upload_active(void) {
    return upload_timer > 0;
}
