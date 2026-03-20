#ifndef GAME_SKILLS_H
#define GAME_SKILLS_H

#include <tonc.h>
#include "game/common.h"

#define MAX_PLAYER_SKILLS   7   /* Max skills per build path (2 T1 + 3 T2 + 2 T3) */
#define SKILL_MAX_RANK     10
#define SKILL_COUNT        48   /* Total unique skills across all classes/tiers */

/* Skill IDs — T1 (6 skills: 2 per class) */
enum {
    /* Trojan T1 */
    SKILL_CHARGED_SHOT = 0,
    SKILL_IRON_SKIN,
    /* Infiltrator T1 */
    SKILL_SHADOW_STEP,
    SKILL_PHASE_SHOT,
    /* Technomancer T1 */
    SKILL_TURRET_DEPLOY,
    SKILL_SCAN_PULSE,

    /* T2: Trojan→Juggernaut (3) */
    SKILL_IRON_PROTOCOL,
    SKILL_SEISMIC_STOMP,
    SKILL_WARCRY_PULSE,
    /* T2: Trojan→Berserker (3) */
    SKILL_CHROME_RAGE,
    SKILL_TYPHOON_BURST,
    SKILL_ADRENALINE_SURGE,
    /* T2: Infiltrator→Specter (3) */
    SKILL_PHASE_CLOAK,
    SKILL_DECOY_HOLOGRAM,
    SKILL_SHADOW_STRIKE,
    /* T2: Infiltrator→Edge Runner (3) */
    SKILL_OVERCLOCK_REFLEXES,
    SKILL_BLADE_STORM,
    SKILL_RICOCHET_SHOT,
    /* T2: Technomancer→Architect (3) */
    SKILL_SENTRY_NETWORK,
    SKILL_TESLA_MINE,
    SKILL_REPAIR_PULSE,
    /* T2: Technomancer→Netweaver (3) */
    SKILL_CONTAGION,
    SKILL_SYNAPSE_BURN,
    SKILL_SYSTEM_CRASH,

    /* T3: Trojan→Juggernaut→Bastion (2) */
    SKILL_FORTRESS_MODE,
    SKILL_AEGIS_FIELD,
    /* T3: Trojan→Juggernaut→Warlord (2) */
    SKILL_CONCUSSIVE_CHARGE,
    SKILL_WARPATH,
    /* T3: Trojan→Berserker→Reaver (2) */
    SKILL_BLOOD_CIRCUIT,
    SKILL_LAST_STAND,
    /* T3: Trojan→Berserker→Demolisher (2) */
    SKILL_SHOCKWAVE_SLAM,
    SKILL_PAYLOAD_ROUNDS,
    /* T3: Infiltrator→Specter→Wraith (2) */
    SKILL_PHASE_WALK,
    SKILL_MARKED_FOR_DEATH,
    /* T3: Infiltrator→Specter→Shade (2) */
    SKILL_SMOKE_CLOUD,
    SKILL_WIRE_TRAP,
    /* T3: Infiltrator→Edge Runner→Razor (2) */
    SKILL_NERVE_STRIKE,
    SKILL_FLURRY,
    /* T3: Infiltrator→Edge Runner→Chrome Phantom (2) */
    SKILL_BULLET_TIME,
    SKILL_SHURIKEN_BARRAGE,
    /* T3: Technomancer→Architect→Machinist (2) */
    SKILL_DRONE_SWARM,
    SKILL_BASTILLE_FIELD,
    /* T3: Technomancer→Architect→Conduit (2) */
    SKILL_SHIELD_SIPHON,
    SKILL_TEMPORAL_ANCHOR,
    /* T3: Technomancer→Netweaver→Daemon (2) */
    SKILL_OVERCLOCK_PROTOCOL,
    SKILL_CASCADE_FAILURE,
    /* T3: Technomancer→Netweaver→Gridrunner (2) */
    SKILL_EXPLOIT,
    SKILL_FEEDBACK_LOOP,

    SKILL_ID_COUNT  /* == 48 */
};

/* Skill type (determines activation behavior) */
enum {
    STYPE_PROJECTILE = 0,   /* Spawns projectile(s) */
    STYPE_BUFF,             /* Applies timed buff to player */
    STYPE_DEBUFF,           /* Applies debuff to target/area */
    STYPE_AOE,              /* Area damage around player */
    STYPE_DEPLOY,           /* Spawns deployable (turret, mine, etc.) */
    STYPE_DASH,             /* Movement skill (teleport, charge) */
    STYPE_SPECIAL,          /* Unique mechanics (Fortress Mode, Temporal Anchor, etc.) */
};

typedef struct {
    const char* name;       /* Display name (max ~14 chars for HUD) */
    u16 cooldown_base;      /* Base cooldown in frames */
    u8  cooldown_rank_reduce; /* Frames reduced per rank */
    u16 damage_scale;       /* Base damage multiplier (100 = 1x ATK) — up to ~500% */
    u8  damage_per_rank;    /* Additional % per rank (e.g. 10 = +10%/rank) */
    u16 duration_base;      /* Base duration in frames (buffs/debuffs) — up to ~500f */
    u8  duration_per_rank;  /* Additional frames per rank */
    u8  type;               /* STYPE_* */
    u8  tier;               /* 1, 2, or 3 */
} SkillDef;

extern const SkillDef skill_defs[SKILL_ID_COUNT];

/* Get the list of skill IDs unlocked by a given build path.
 * t1_class: CLASS_TROJAN/INFILTRATOR/TECHNOMANCER (or 0xFF if classless)
 * t2_spec: SPEC_* (or 0xFF if not yet chosen)
 * t3_spec: SPEC3_* (or 0xFF if not yet chosen)
 * out_ids: array to fill (must hold MAX_PLAYER_SKILLS entries)
 * Returns: number of skills written to out_ids */
int skills_get_unlocked(int t1_class, int t2_spec, int t3_spec, u8* out_ids);

/* Get the skill definition for a skill ID. */
const SkillDef* skill_get_def(int skill_id);

/* Get effective cooldown at a given rank (0-10). */
int skill_get_cooldown(int skill_id, int rank);

/* Get effective damage at a given rank, given player ATK. */
int skill_get_damage(int skill_id, int rank, int player_atk);

/* Get effective duration at a given rank. */
int skill_get_duration(int skill_id, int rank);

/* Get the display name for a tier choice.
 * tier: 1/2/3, choice: class/spec enum value */
const char* tier_get_name(int tier, int choice);

/* Get the T2 spec options for a given T1 class.
 * Returns via out_a/out_b the two SPEC_* values. */
void tier2_get_options(int t1_class, int* out_a, int* out_b);

/* Get the T3 spec options for a given T2 spec.
 * Returns via out_a/out_b the two SPEC3_* values. */
void tier3_get_options(int t2_spec, int* out_a, int* out_b);

#endif /* GAME_SKILLS_H */
