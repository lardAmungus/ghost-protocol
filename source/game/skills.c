#include "game/skills.h"
#include "game/common.h"

/* ---- Skill definitions table (ROM) ---- */
/* Fields: name, cooldown_base, cooldown_rank_reduce, damage_scale, damage_per_rank,
 *         duration_base, duration_per_rank, type, tier */
const SkillDef skill_defs[SKILL_ID_COUNT] = {
    /* ---- T1 ---- */
    /* SKILL_CHARGED_SHOT */
    { "Charged Shot",    90,  5, 300, 10,   0,  0, STYPE_PROJECTILE, 1 },
    /* SKILL_IRON_SKIN */
    { "Iron Skin",      180,  8,   0,  0, 180, 15, STYPE_BUFF,       1 },
    /* SKILL_SHADOW_STEP */
    { "Shadow Step",    120,  6, 150,  8,   0,  0, STYPE_DASH,       1 },
    /* SKILL_PHASE_SHOT */
    { "Phase Shot",      60,  3, 200, 10,   0,  0, STYPE_PROJECTILE, 1 },
    /* SKILL_TURRET_DEPLOY */
    { "Turret Deploy",  240, 10, 150,  8, 180, 10, STYPE_DEPLOY,     1 },
    /* SKILL_SCAN_PULSE */
    { "Scan Pulse",     150,  7, 180, 10,   0,  0, STYPE_PROJECTILE, 1 },

    /* ---- T2: Trojan→Juggernaut ---- */
    /* SKILL_IRON_PROTOCOL */
    { "Iron Protocol",  180,  6,   0,  0, 240, 12, STYPE_BUFF,       2 },
    /* SKILL_SEISMIC_STOMP */
    { "Seismic Stomp",  150,  6, 250, 10,   0,  0, STYPE_AOE,        2 },
    /* SKILL_WARCRY_PULSE */
    { "Warcry Pulse",   200,  7,   0,  0, 180, 12, STYPE_BUFF,       2 },

    /* ---- T2: Trojan→Berserker ---- */
    /* SKILL_CHROME_RAGE */
    { "Chrome Rage",    240,  8,   0,  0, 300, 12, STYPE_BUFF,       2 },
    /* SKILL_TYPHOON_BURST */
    { "Typhoon Burst",  180,  7, 350, 10,   0,  0, STYPE_AOE,        2 },
    /* SKILL_ADRENALINE_SURGE */
    { "Adren. Surge",   150,  6,   0,  0, 120, 10, STYPE_BUFF,       2 },

    /* ---- T2: Infiltrator→Specter ---- */
    /* SKILL_PHASE_CLOAK */
    { "Phase Cloak",    240,  8,   0,  0, 240, 12, STYPE_BUFF,       2 },
    /* SKILL_DECOY_HOLOGRAM */
    { "Decoy Hologram", 200,  7,   0,  0, 300, 12, STYPE_DEPLOY,     2 },
    /* SKILL_SHADOW_STRIKE */
    { "Shadow Strike",  120,  5, 300, 10,   0,  0, STYPE_DASH,       2 },

    /* ---- T2: Infiltrator→Edge Runner ---- */
    /* SKILL_OVERCLOCK_REFLEXES */
    { "Overclock Ref.", 200,  7,   0,  0, 300, 12, STYPE_BUFF,       2 },
    /* SKILL_BLADE_STORM */
    { "Blade Storm",    180,  6, 250, 10,   0,  0, STYPE_DASH,       2 },
    /* SKILL_RICOCHET_SHOT */
    { "Ricochet Shot",  100,  5, 200,  8,   0,  0, STYPE_PROJECTILE, 2 },

    /* ---- T2: Technomancer→Architect ---- */
    /* SKILL_SENTRY_NETWORK */
    { "Sentry Network", 300,  8, 120,  8, 360, 12, STYPE_DEPLOY,     2 },
    /* SKILL_TESLA_MINE */
    { "Tesla Mine",     150,  6, 280, 10,   0,  0, STYPE_DEPLOY,     2 },
    /* SKILL_REPAIR_PULSE */
    { "Repair Pulse",   240,  7,   0,  0,  60, 10, STYPE_BUFF,       2 },

    /* ---- T2: Technomancer→Netweaver ---- */
    /* SKILL_CONTAGION */
    { "Contagion",      180,  6, 150,  8, 300, 12, STYPE_DEBUFF,     2 },
    /* SKILL_SYNAPSE_BURN */
    { "Synapse Burn",   120,  5, 400, 12,   0,  0, STYPE_PROJECTILE, 2 },
    /* SKILL_SYSTEM_CRASH */
    { "System Crash",   300,  8,   0,  0, 120, 12, STYPE_DEBUFF,     2 },

    /* ---- T3: Trojan→Juggernaut→Bastion ---- */
    /* SKILL_FORTRESS_MODE */
    { "Fortress Mode",  360,  8,   0,  0, 300, 15, STYPE_SPECIAL,    3 },
    /* SKILL_AEGIS_FIELD */
    { "Aegis Field",    240,  7,   0,  0, 240, 12, STYPE_DEPLOY,     3 },

    /* ---- T3: Trojan→Juggernaut→Warlord ---- */
    /* SKILL_CONCUSSIVE_CHARGE */
    { "Concussive Chg", 150,  6, 350, 10,   0,  0, STYPE_DASH,       3 },
    /* SKILL_WARPATH */
    { "Warpath",        200,  7,   0,  0, 360, 15, STYPE_BUFF,       3 },

    /* ---- T3: Trojan→Berserker→Reaver ---- */
    /* SKILL_BLOOD_CIRCUIT */
    { "Blood Circuit",  240,  8,   0,  0, 480, 15, STYPE_BUFF,       3 },
    /* SKILL_LAST_STAND */
    { "Last Stand",     300,  8,   0,  0, 360, 15, STYPE_BUFF,       3 },

    /* ---- T3: Trojan→Berserker→Demolisher ---- */
    /* SKILL_SHOCKWAVE_SLAM */
    { "Shockwave Slam", 180,  6, 400, 12,   0,  0, STYPE_AOE,        3 },
    /* SKILL_PAYLOAD_ROUNDS */
    { "Payload Rounds", 200,  7, 150, 10, 300, 12, STYPE_BUFF,       3 },

    /* ---- T3: Infiltrator→Specter→Wraith ---- */
    /* SKILL_PHASE_WALK */
    { "Phase Walk",     240,  7,   0,  0, 180, 12, STYPE_SPECIAL,    3 },
    /* SKILL_MARKED_FOR_DEATH */
    { "Marked f/Death", 180,  6,   0,  0, 360, 15, STYPE_DEBUFF,     3 },

    /* ---- T3: Infiltrator→Specter→Shade ---- */
    /* SKILL_SMOKE_CLOUD */
    { "Smoke Cloud",    200,  7,   0,  0, 360, 12, STYPE_DEPLOY,     3 },
    /* SKILL_WIRE_TRAP */
    { "Wire Trap",      120,  5,   0,  0,   0,  0, STYPE_DEPLOY,     3 },

    /* ---- T3: Infiltrator→Edge Runner→Razor ---- */
    /* SKILL_NERVE_STRIKE */
    { "Nerve Strike",   100,  5, 300, 10,   0,  0, STYPE_DASH,       3 },
    /* SKILL_FLURRY */
    { "Flurry",         150,  6, 120,  8,   0,  0, STYPE_PROJECTILE, 3 },

    /* ---- T3: Infiltrator→Edge Runner→Chrome Phantom ---- */
    /* SKILL_BULLET_TIME */
    { "Bullet Time",    300,  8,   0,  0, 240, 12, STYPE_SPECIAL,    3 },
    /* SKILL_SHURIKEN_BARRAGE */
    { "Shuriken Barrage",120, 5, 180,  8,   0,  0, STYPE_PROJECTILE, 3 },

    /* ---- T3: Technomancer→Architect→Machinist ---- */
    /* SKILL_DRONE_SWARM */
    { "Drone Swarm",    240,  7, 250, 10, 300, 12, STYPE_DEPLOY,     3 },
    /* SKILL_BASTILLE_FIELD */
    { "Bastille Field", 200,  7,   0,  0, 240, 12, STYPE_DEPLOY,     3 },

    /* ---- T3: Technomancer→Architect→Conduit ---- */
    /* SKILL_SHIELD_SIPHON */
    { "Shield Siphon",  180,  6,   0,  0, 300, 12, STYPE_BUFF,       3 },
    /* SKILL_TEMPORAL_ANCHOR */
    { "Temporal Anchor",360,  8,   0,  0, 300, 15, STYPE_SPECIAL,    3 },

    /* ---- T3: Technomancer→Netweaver→Daemon ---- */
    /* SKILL_OVERCLOCK_PROTOCOL */
    { "Overclock Proto",300,  8,   0,  0, 360, 15, STYPE_SPECIAL,    3 },
    /* SKILL_CASCADE_FAILURE */
    { "Cascade Failure",180,  6, 350, 12,   0,  0, STYPE_PROJECTILE, 3 },

    /* ---- T3: Technomancer→Netweaver→Gridrunner ---- */
    /* SKILL_EXPLOIT */
    { "Exploit",        150,  6,   0,  0, 360, 15, STYPE_DEBUFF,     3 },
    /* SKILL_FEEDBACK_LOOP */
    { "Feedback Loop",  300,  8,   0,  0, 480, 15, STYPE_SPECIAL,    3 },
};

/* ---- skills_get_unlocked ---- */
int skills_get_unlocked(int t1_class, int t2_spec, int t3_spec, u8* out_ids) {
    int n = 0;
    if (t1_class == 0xFF) return 0;  /* Classless — no skills */

    /* T1: 2 skills per class */
    switch (t1_class) {
        case CLASS_TROJAN:
            out_ids[n++] = SKILL_CHARGED_SHOT;
            out_ids[n++] = SKILL_IRON_SKIN;
            break;
        case CLASS_INFILTRATOR:
            out_ids[n++] = SKILL_SHADOW_STEP;
            out_ids[n++] = SKILL_PHASE_SHOT;
            break;
        case CLASS_TECHNOMANCER:
            out_ids[n++] = SKILL_TURRET_DEPLOY;
            out_ids[n++] = SKILL_SCAN_PULSE;
            break;
        default:
            break;
    }

    if (t2_spec == 0xFF) return n;

    /* T2: 3 skills per spec */
    switch (t2_spec) {
        case SPEC_JUGGERNAUT:
            out_ids[n++] = SKILL_IRON_PROTOCOL;
            out_ids[n++] = SKILL_SEISMIC_STOMP;
            out_ids[n++] = SKILL_WARCRY_PULSE;
            break;
        case SPEC_BERSERKER:
            out_ids[n++] = SKILL_CHROME_RAGE;
            out_ids[n++] = SKILL_TYPHOON_BURST;
            out_ids[n++] = SKILL_ADRENALINE_SURGE;
            break;
        case SPEC_SPECTER:
            out_ids[n++] = SKILL_PHASE_CLOAK;
            out_ids[n++] = SKILL_DECOY_HOLOGRAM;
            out_ids[n++] = SKILL_SHADOW_STRIKE;
            break;
        case SPEC_EDGE_RUNNER:
            out_ids[n++] = SKILL_OVERCLOCK_REFLEXES;
            out_ids[n++] = SKILL_BLADE_STORM;
            out_ids[n++] = SKILL_RICOCHET_SHOT;
            break;
        case SPEC_ARCHITECT:
            out_ids[n++] = SKILL_SENTRY_NETWORK;
            out_ids[n++] = SKILL_TESLA_MINE;
            out_ids[n++] = SKILL_REPAIR_PULSE;
            break;
        case SPEC_NETWEAVER:
            out_ids[n++] = SKILL_CONTAGION;
            out_ids[n++] = SKILL_SYNAPSE_BURN;
            out_ids[n++] = SKILL_SYSTEM_CRASH;
            break;
        default:
            break;
    }

    if (t3_spec == 0xFF) return n;

    /* T3: 2 skills per spec */
    switch (t3_spec) {
        case SPEC3_BASTION:
            out_ids[n++] = SKILL_FORTRESS_MODE;
            out_ids[n++] = SKILL_AEGIS_FIELD;
            break;
        case SPEC3_WARLORD:
            out_ids[n++] = SKILL_CONCUSSIVE_CHARGE;
            out_ids[n++] = SKILL_WARPATH;
            break;
        case SPEC3_REAVER:
            out_ids[n++] = SKILL_BLOOD_CIRCUIT;
            out_ids[n++] = SKILL_LAST_STAND;
            break;
        case SPEC3_DEMOLISHER:
            out_ids[n++] = SKILL_SHOCKWAVE_SLAM;
            out_ids[n++] = SKILL_PAYLOAD_ROUNDS;
            break;
        case SPEC3_WRAITH:
            out_ids[n++] = SKILL_PHASE_WALK;
            out_ids[n++] = SKILL_MARKED_FOR_DEATH;
            break;
        case SPEC3_SHADE:
            out_ids[n++] = SKILL_SMOKE_CLOUD;
            out_ids[n++] = SKILL_WIRE_TRAP;
            break;
        case SPEC3_RAZOR:
            out_ids[n++] = SKILL_NERVE_STRIKE;
            out_ids[n++] = SKILL_FLURRY;
            break;
        case SPEC3_CHROME_PHANTOM:
            out_ids[n++] = SKILL_BULLET_TIME;
            out_ids[n++] = SKILL_SHURIKEN_BARRAGE;
            break;
        case SPEC3_MACHINIST:
            out_ids[n++] = SKILL_DRONE_SWARM;
            out_ids[n++] = SKILL_BASTILLE_FIELD;
            break;
        case SPEC3_CONDUIT:
            out_ids[n++] = SKILL_SHIELD_SIPHON;
            out_ids[n++] = SKILL_TEMPORAL_ANCHOR;
            break;
        case SPEC3_DAEMON:
            out_ids[n++] = SKILL_OVERCLOCK_PROTOCOL;
            out_ids[n++] = SKILL_CASCADE_FAILURE;
            break;
        case SPEC3_GRIDRUNNER:
            out_ids[n++] = SKILL_EXPLOIT;
            out_ids[n++] = SKILL_FEEDBACK_LOOP;
            break;
        default:
            break;
    }

    return n;
}

/* ---- Helper functions ---- */

const SkillDef* skill_get_def(int skill_id) {
    if (skill_id < 0 || skill_id >= SKILL_ID_COUNT) return &skill_defs[0];
    return &skill_defs[skill_id];
}

int skill_get_cooldown(int skill_id, int rank) {
    const SkillDef* sd = skill_get_def(skill_id);
    int cd = (int)sd->cooldown_base - rank * (int)sd->cooldown_rank_reduce;
    if (cd < 30) cd = 30;  /* Minimum 30 frames (0.5s) */
    return cd;
}

int skill_get_damage(int skill_id, int rank, int player_atk) {
    const SkillDef* sd = skill_get_def(skill_id);
    int scale = (int)sd->damage_scale + rank * (int)sd->damage_per_rank;
    return player_atk * scale / 100;
}

int skill_get_duration(int skill_id, int rank) {
    const SkillDef* sd = skill_get_def(skill_id);
    return (int)sd->duration_base + rank * (int)sd->duration_per_rank;
}

/* ---- Tier name lookup ---- */

const char* tier_get_name(int tier, int choice) {
    static const char* const t1_names[] = { "Trojan", "Infiltrator", "Technomancer" };
    static const char* const t2_names[] = { "Juggernaut", "Berserker", "Specter", "Edge Runner", "Architect", "Netweaver" };
    static const char* const t3_names[] = { "Bastion", "Warlord", "Reaver", "Demolisher", "Wraith", "Shade", "Razor", "Chrome Phantom", "Machinist", "Conduit", "Daemon", "Gridrunner" };

    switch (tier) {
        case 1: return (choice >= 0 && choice < CLASS_COUNT)    ? t1_names[choice] : "???";
        case 2: return (choice >= 0 && choice < SPEC_T2_COUNT)  ? t2_names[choice] : "???";
        case 3: return (choice >= 0 && choice < SPEC3_COUNT)    ? t3_names[choice] : "???";
        default: break;
    }
    return "???";
}

/* ---- Tier option lookups ---- */

void tier2_get_options(int t1_class, int* out_a, int* out_b) {
    switch (t1_class) {
        case CLASS_TROJAN:       *out_a = SPEC_JUGGERNAUT; *out_b = SPEC_BERSERKER;  break;
        case CLASS_INFILTRATOR:  *out_a = SPEC_SPECTER;    *out_b = SPEC_EDGE_RUNNER; break;
        case CLASS_TECHNOMANCER: *out_a = SPEC_ARCHITECT;  *out_b = SPEC_NETWEAVER;  break;
        default:                 *out_a = 0;               *out_b = 1;               break;
    }
}

void tier3_get_options(int t2_spec, int* out_a, int* out_b) {
    switch (t2_spec) {
        case SPEC_JUGGERNAUT:  *out_a = SPEC3_BASTION;   *out_b = SPEC3_WARLORD;       break;
        case SPEC_BERSERKER:   *out_a = SPEC3_REAVER;    *out_b = SPEC3_DEMOLISHER;    break;
        case SPEC_SPECTER:     *out_a = SPEC3_WRAITH;    *out_b = SPEC3_SHADE;         break;
        case SPEC_EDGE_RUNNER: *out_a = SPEC3_RAZOR;     *out_b = SPEC3_CHROME_PHANTOM; break;
        case SPEC_ARCHITECT:   *out_a = SPEC3_MACHINIST; *out_b = SPEC3_CONDUIT;       break;
        case SPEC_NETWEAVER:   *out_a = SPEC3_DAEMON;    *out_b = SPEC3_GRIDRUNNER;    break;
        default:               *out_a = 0;               *out_b = 1;                   break;
    }
}
