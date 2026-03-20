/*
 * Ghost Protocol — Buff Timers & Skill Activation Bridge
 *
 * Buff timer state, per-frame tick-down, and skill_activate() dispatch.
 * Old per-class ability tables/activators removed; skills.h provides data.
 */
#include "game/abilities.h"
#include "game/skills.h"
#include "game/player.h"
#include "game/projectile.h"
#include "game/enemy.h"
#include "game/hud.h"
#include "game/particle.h"
#include "game/levelgen.h"
#include "engine/audio.h"
#include "engine/video.h"
#include "engine/collision.h"
#include "engine/rng.h"

/* ---- Active effect state ---- */
static int overclock_timer = 0;
static int data_shield_timer = 0;
static int iron_skin_timer = 0;
static int berserk_timer = 0;
static int smoke_timer = 0;
static int backstab_timer = 0;
static int time_warp_timer = 0;
static int nanobots_timer = 0;
static int firewall_timer = 0;
static int overclock_plus_timer = 0;
static int upload_timer = 0;
static int nanobots_heal_tick = 0;

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
}

void ability_update(void) {
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
}

/* ---- Buff state getters ---- */
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

/* Timer getters for HUD status display */
int ability_get_overclock_timer(void) { return overclock_timer; }
int ability_get_iron_skin_timer(void) { return iron_skin_timer; }
int ability_get_berserk_timer(void) { return berserk_timer; }
int ability_get_data_shield_timer(void) { return data_shield_timer; }
int ability_get_smoke_timer(void) { return smoke_timer; }
int ability_get_backstab_timer(void) { return backstab_timer; }
int ability_get_time_warp_timer(void) { return time_warp_timer; }
int ability_get_nanobots_timer(void) { return nanobots_timer; }

/* ---- Skill activation dispatch ---- */

int skill_activate(int skill_id, int rank, int player_atk) {
    Entity* pe = player_get();
    if (!pe) return 0;

    const SkillDef* sd = skill_get_def(skill_id);
    int dmg = skill_get_damage(skill_id, rank, player_atk);
    int dur = skill_get_duration(skill_id, rank);

    switch (skill_id) {
    /* ---- T1: Trojan ---- */
    case SKILL_CHARGED_SHOT: {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)dmg, SUBTYPE_PROJ_CHARGE, 0, 0);
        break;
    }
    case SKILL_IRON_SKIN:
        iron_skin_timer = dur;
        hud_notify("IRON SKIN!", 60);
        break;

    /* ---- T1: Infiltrator ---- */
    case SKILL_SHADOW_STEP: {
        /* Teleport forward */
        s32 step_dist = FP8(32);
        s32 dest_x = pe->x + (pe->facing ? -step_dist : step_dist);
        /* Scan for clear spot */
        int found = 0;
        int step_dir = pe->facing ? 1 : -1;
        for (int i = 0; i <= 4; i++) {
            s32 cx = dest_x + (s32)(step_dir * i * 8 * 256);
            int px_c = (int)(cx >> 8) + pe->width / 2;
            int py_t = (int)(pe->y >> 8) + 2;
            int py_b = (int)(pe->y >> 8) + pe->height - 2;
            if (!collision_point_solid(px_c, py_t) &&
                !collision_point_solid(px_c, py_b)) {
                dest_x = cx;
                found = 1;
                break;
            }
        }
        if (found) {
            if (dest_x < 0) dest_x = 0;
            int max_x = (NET_MAP_PX - pe->width) << 8;
            if (dest_x > max_x) dest_x = max_x;
            particle_spawn(pe->x + FP8(6), pe->y + FP8(6), 0, 0, PART_ELECTRIC, 12);
            pe->x = dest_x;
            particle_spawn(pe->x + FP8(6), pe->y + FP8(6), 0, 0, PART_ELECTRIC, 12);
            audio_play_sfx(SFX_SHADOW_STEP);
        }
        break;
    }
    case SKILL_PHASE_SHOT: {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)dmg, SUBTYPE_PROJ_BUSTER, PROJ_PHASE, 0);
        break;
    }

    /* ---- T1: Technomancer ---- */
    case SKILL_TURRET_DEPLOY: {
        s16 dir = pe->facing ? -384 : 384;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        for (int i = 0; i < 6; i++) {
            s16 vy = (s16)((i - 2) * 48);
            s16 d = (s16)(dir * (75 + i * 5) / 100);
            projectile_spawn(sx, pe->y + FP8(2 + i), d, vy,
                             (s16)dmg, SUBTYPE_PROJ_BUSTER, 0, 0);
        }
        break;
    }
    case SKILL_SCAN_PULSE: {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        for (int i = 0; i < 5; i++) {
            s16 vy = (s16)((i - 2) * 32);
            projectile_spawn(sx, pe->y + FP8(4), dir, vy,
                             (s16)dmg, SUBTYPE_PROJ_BEAM, PROJ_PIERCE, 0);
        }
        break;
    }

    /* ---- T2/T3: Buff-type skills (use timer statics) ---- */
    case SKILL_IRON_PROTOCOL:
    case SKILL_WARCRY_PULSE:
    case SKILL_OVERCLOCK_REFLEXES:
    case SKILL_BLOOD_CIRCUIT:
    case SKILL_LAST_STAND:
    case SKILL_PAYLOAD_ROUNDS:
    case SKILL_SHIELD_SIPHON:
    case SKILL_WARPATH:
        /* Generic buff — set overclock_timer as a catch-all */
        overclock_timer = dur;
        hud_notify(sd->name, 60);
        if (pe) particle_burst(pe->x + FP8(6), pe->y + FP8(6), 3, PART_STAR, 160, 16);
        break;

    case SKILL_CHROME_RAGE:
        berserk_timer = dur;
        hud_notify("CHROME RAGE!", 60);
        break;

    case SKILL_PHASE_CLOAK:
        smoke_timer = dur;
        hud_notify("PHASE CLOAK!", 60);
        break;

    case SKILL_ADRENALINE_SURGE:
        backstab_timer = dur;
        hud_notify("ADRENALINE!", 60);
        break;

    case SKILL_REPAIR_PULSE:
        nanobots_timer = dur;
        nanobots_heal_tick = 0;
        hud_notify("REPAIR!", 60);
        break;

    case SKILL_OVERCLOCK_PROTOCOL:
        overclock_plus_timer = dur;
        hud_notify("OVERCLOCK!", 60);
        break;

    /* ---- Projectile-type skills ---- */
    case SKILL_TYPHOON_BURST:
    case SKILL_SHOCKWAVE_SLAM:
    case SKILL_CASCADE_FAILURE:
    case SKILL_RICOCHET_SHOT:
    case SKILL_SHURIKEN_BARRAGE: {
        s16 dir = pe->facing ? -512 : 512;
        s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)dmg, SUBTYPE_PROJ_CHARGE, PROJ_PIERCE, 0);
        if (pe) particle_burst(pe->x + FP8(6), pe->y + FP8(6), 4, PART_SPARK, 200, 12);
        break;
    }

    /* ---- AoE-type skills ---- */
    case SKILL_SEISMIC_STOMP:
    case SKILL_SYNAPSE_BURN:
    case SKILL_SYSTEM_CRASH:
        enemy_stun_all(dmg);
        video_shake(3, 1);
        hud_notify(sd->name, 60);
        break;

    /* ---- Dash-type skills ---- */
    case SKILL_CONCUSSIVE_CHARGE:
    case SKILL_NERVE_STRIKE:
    case SKILL_BLADE_STORM:
    case SKILL_SHADOW_STRIKE: {
        /* Simplified dash: teleport + damage */
        s32 step = FP8(32);
        s32 dest = pe->x + (pe->facing ? -step : step);
        if (dest < 0) dest = 0;
        int max_x2 = (NET_MAP_PX - pe->width) << 8;
        if (dest > max_x2) dest = max_x2;
        particle_spawn(pe->x + FP8(6), pe->y + FP8(6), 0, 0, PART_ELECTRIC, 12);
        pe->x = dest;
        particle_spawn(pe->x + FP8(6), pe->y + FP8(6), 0, 0, PART_ELECTRIC, 12);
        /* Damage nearby enemies */
        enemy_stun_all(dmg);
        break;
    }

    /* ---- Deploy-type skills ---- */
    case SKILL_DECOY_HOLOGRAM:
    case SKILL_TESLA_MINE:
    case SKILL_SENTRY_NETWORK:
    case SKILL_WIRE_TRAP:
    case SKILL_SMOKE_CLOUD:
    case SKILL_DRONE_SWARM:
    case SKILL_BASTILLE_FIELD:
    case SKILL_AEGIS_FIELD: {
        /* Placeholder: spawn projectiles as deployable stand-in */
        s16 dir = pe->facing ? -256 : 256;
        s32 sx = pe->x + (pe->facing ? -FP8(8) : FP8(16));
        projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                         (s16)dmg, SUBTYPE_PROJ_BUSTER, 0, 0);
        hud_notify(sd->name, 60);
        break;
    }

    /* ---- Debuff-type skills ---- */
    case SKILL_CONTAGION:
    case SKILL_MARKED_FOR_DEATH:
    case SKILL_EXPLOIT:
        upload_timer = dur;
        hud_notify(sd->name, 60);
        break;

    /* ---- Special-type skills ---- */
    case SKILL_FORTRESS_MODE:
        iron_skin_timer = dur;
        data_shield_timer = dur;
        hud_notify("FORTRESS!", 60);
        break;

    case SKILL_PHASE_WALK:
        smoke_timer = dur;
        hud_notify("PHASE WALK!", 60);
        break;

    case SKILL_BULLET_TIME:
        time_warp_timer = dur;
        hud_notify("BULLET TIME!", 60);
        break;

    case SKILL_TEMPORAL_ANCHOR:
    case SKILL_FEEDBACK_LOOP:
    case SKILL_FLURRY:
        /* Complex mechanics — placeholder as buff */
        overclock_timer = dur;
        hud_notify(sd->name, 60);
        break;

    default:
        /* Fallback: spawn a projectile or apply a buff */
        if (sd->type == STYPE_PROJECTILE || sd->type == STYPE_AOE) {
            s16 dir = pe->facing ? -512 : 512;
            s32 sx = pe->x + (pe->facing ? -FP8(4) : FP8(12));
            projectile_spawn(sx, pe->y + FP8(4), dir, 0,
                             (s16)dmg, SUBTYPE_PROJ_CHARGE, 0, 0);
        } else {
            overclock_timer = dur > 0 ? dur : 180;
            hud_notify(sd->name, 60);
        }
        break;
    }

    return 1;
}
