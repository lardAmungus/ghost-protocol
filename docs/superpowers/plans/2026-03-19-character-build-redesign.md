# Character Build & Leveling Redesign — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the 3-starting-class system with a single classless character that evolves through a 3-tier class tree, overhauling stat allocation, skills, shop, controls, crafting UI, and adding character customization.

**Architecture:** The redesign keeps the existing engine, entity, and loot systems intact. It replaces the player progression layer: `common.h` enums, `player.h/c` stat tables, `abilities.h/c` skill system, `shop.h/c` items, `save.h/c` persistence, and `state_charsel.c` flow. New files are created only for the skill data table (`skills.c/h`) and color presets (`colors.c/h`). All 48 skills reuse existing projectile/particle/buff timer infrastructure.

**Tech Stack:** C (libtonc), devkitARM, GBA Mode 0, fixed-point 8.8 math, SRAM save, Maxmod audio

**Spec:** `/opt/docker/projects/gba/src/docs/specs/2026-03-19-character-build-redesign.md`

**Build:** `docker exec gba-dev bash -c "cd /workspace && make"`
**Test:** `docker exec gba-dev bash -c "cd /workspace && make test"`

---

## File Map

### Files to Create
| File | Responsibility |
|------|---------------|
| `include/game/skills.h` | Skill enums (48 SKILL_* IDs), SkillDef struct, tier lookup API, max constants |
| `source/game/skills.c` | Global `skill_defs[]` table (48 entries), tier→skill lookup, skill activation dispatch, cooldown scaling |
| `include/game/colors.h` | Color preset count, palette application API |
| `source/game/colors.c` | 24 suit presets + 24 visor presets (RGB15 triplets), `colors_apply_player_palette()` |

### Files to Modify
| File | What Changes |
|------|-------------|
| `include/game/common.h` | Rename CLASS_ASSAULT→CLASS_TROJAN, add tier enums (TIER_*), remove old EVOLUTION_* |
| `include/game/player.h` | Replace skill_tree/evolution/ability_unlocks with tier_choices[3]/skill_ranks[7]/slotted_skill/suit_color/visor_color, remove old ability bitmask defines, update PSTATE enum |
| `source/game/player.c` | Rewrite stat allocation (classless L0-4, auto-alloc L5+), remove evolution/passive bonuses, new `player_init()` for classless start, palette application on enter_level, remove old class validation clamp, remove `skill_tree[]` XP bonus in `player_add_xp()` |
| `include/game/physics.h` | No struct changes — but reviewed for CLASS_COUNT usage |
| `source/game/physics.c` | Rename `[CLASS_ASSAULT]` → `[CLASS_TROJAN]`, set `max_jumps = 2` for ALL classes (double jump universal). **CRITICAL**: Add classless default physics (reuse Trojan params) — `player.c` reads `physics_class[player_state.player_class]` and classless players use index 0 = CLASS_TROJAN, which is safe since CLASS_TROJAN = 0 |
| `include/game/abilities.h` | Gut old 8-per-class API — redirect to skills.h (or inline the few remaining buff-query functions) |
| `source/game/abilities.c` | Remove 3×8 ability tables + activation switches. Keep buff timer state + `ability_update()` for active effects. Activation delegates to `skill_activate()` |
| `include/game/shop.h` | New enum (6 items), charge query/consume API, `shop_get_charges_array()`/`shop_set_charges_array()` for save serialization |
| `source/game/shop.c` | Replace 9 items with 6 (Health Pack, CD Reset, XP Booster, Shield Charge, Credit Finder, Loot Magnet), charge system, fixed prices |
| `include/engine/save.h` | New SAVE_MAGIC, replace `ability_unlocks`/`skill_tree[12]`/`evolution`/`evolution_pending`/`last_used_ability` with `tier_choices[3]`/`skill_ranks[7]`/`slotted_skill`/`suit_color`/`visor_color`/`buff_charges[4]`, resize `shop_purchases[9]` → `shop_purchases[6]`, adjust `reserved[]` |
| `source/engine/save.c` | Update `save_defaults()` for classless start |
| `source/states/state_charsel.c` | Replace class selection with: character customization (suit+visor color) → save load. Class chosen at L5 in-game, not at character creation |
| `source/states/state_title.c` | Update save slot preview — replace `cn[sd.player_class % 3]` with tier-aware abbreviation (handles classless `0xFF` case) |
| `source/states/state_terminal.c` | Rewrite TSUB_SKILLS (active skill ranking), replace TSUB_EVOLUTION with TSUB_TIER_SELECT (3-tier selection), update pack_save/unpack_save, call `player_recompute_stats()` after tier selection |
| `source/states/state_net.c` | Replace R-hold ability wheel with SELECT-opens/R-fires controls, wire charge consumption on level complete/death/exit |
| `source/game/hud.c` | Add slotted skill name + cooldown bar on HUD row 1, skill wheel overlay, remove old ability wheel rendering code |
| `source/game/loot.c` | Replace rarity name strings with letter grades (C/B/A/A+/S/S+). Existing rarity palette system already assigns colors per rarity — letter grades inherit these colors |
| `source/game/enemy.c` | Remove old `skill_tree[11]` credit bonus, add charge-based Credit Finder modifier via `shop_buff_active()` |
| `source/game/boss.c` | Same — remove old `skill_tree[11]` credit bonus, add charge-based modifier |

### Files with Trivial Changes
| File | What Changes |
|------|-------------|
| `source/game/itemdrop.c` | Loot Magnet charge check on drop rarity |
| `source/states/state_gameover.c` | Show tier name instead of class name |
| `source/states/state_win.c` | Show tier name instead of class name |
| `source/main.c` | No changes expected (state machine unchanged) |

### Design Decisions for Classless (L0-4) Players
- **Physics**: Classless players use `player_class = CLASS_TROJAN` (index 0) as a physics default. Since CLASS_TROJAN = 0 and all classes get `max_jumps = 2`, this is safe and gives balanced movement.
- **Skills**: R does nothing (slotted_skill = 0xFF), SELECT does nothing (checked via `player_has_class()`).
- **Stats**: Flat +1 to each stat per level, no class allocation.
- **PSTATE_CHARGE/PSTATE_TETHER**: These movement states are activated by specific skills (e.g. Concussive Charge → PSTATE_CHARGE). Classless players cannot activate them since they have no skills. The PSTATE enum values are kept; they become reachable only when the corresponding tier skill is activated.
- **Intro Scene**: Spec mentions "Intro Scene" between Title and Customization — deferred to a future task (cosmetic, not blocking gameplay systems).

### Charge Consumption Edge Cases
- `shop_consume_charges()` is called once per triggering event (death OR exit OR completion).
- "Retry after death, then exit" = 2 calls: once at NETSUB_DEATH transition, once at quit/exit. Each call decrements all active charges by 1.
- Charges cannot go negative — consumption is skipped when already 0.

---

## Task 1: Enums & Constants (common.h)

**Files:**
- Modify: `include/game/common.h`

- [ ] **Step 1: Rename CLASS_ASSAULT to CLASS_TROJAN**

In `include/game/common.h`, rename the enum value:

```c
enum {
    CLASS_TROJAN = 0,       /* Was CLASS_ASSAULT */
    CLASS_INFILTRATOR,
    CLASS_TECHNOMANCER,
    CLASS_COUNT
};
```

- [ ] **Step 2: Add tier enums and constants**

Below the class enum, add:

```c
/* ---- Tier system ---- */
#define TIER_NONE       0
#define TIER_1_LEVEL    5
#define TIER_2_LEVEL   25
#define TIER_3_LEVEL   55
#define MAX_LEVEL_BASE     40
#define MAX_LEVEL_ENDGAME  99

/* T2 specializations (stored in tier_choices[1]) */
enum {
    /* Trojan T2 */
    SPEC_JUGGERNAUT = 0,
    SPEC_BERSERKER,
    /* Infiltrator T2 */
    SPEC_SPECTER,
    SPEC_EDGE_RUNNER,
    /* Technomancer T2 */
    SPEC_ARCHITECT,
    SPEC_NETWEAVER,
    SPEC_T2_COUNT
};

/* T3 specializations (stored in tier_choices[2]) */
enum {
    /* Trojan→Juggernaut T3 */
    SPEC3_BASTION = 0,
    SPEC3_WARLORD,
    /* Trojan→Berserker T3 */
    SPEC3_REAVER,
    SPEC3_DEMOLISHER,
    /* Infiltrator→Specter T3 */
    SPEC3_WRAITH,
    SPEC3_SHADE,
    /* Infiltrator→Edge Runner T3 */
    SPEC3_RAZOR,
    SPEC3_CHROME_PHANTOM,
    /* Technomancer→Architect T3 */
    SPEC3_MACHINIST,
    SPEC3_CONDUIT,
    /* Technomancer→Netweaver T3 */
    SPEC3_DAEMON,
    SPEC3_GRIDRUNNER,
    SPEC3_COUNT
};
```

- [ ] **Step 3: Remove old EVOLUTION_* enum**

Delete the entire `EVOLUTION_NONE` through `EVOLUTION_COUNT` enum block. Remove `EVOLUTION_LEVEL` define.

- [ ] **Step 4: Fix all CLASS_ASSAULT references project-wide**

Search-and-replace `CLASS_ASSAULT` → `CLASS_TROJAN` in all `.c` and `.h` files. Key files: `player.c` (class_palettes, base_stats, level_gains), `physics.c` (`physics_class[]` array), `abilities.c` (assault_names, assault_cd), `state_charsel.c` (class_names, class_roles), `state_terminal.c` (save preview), `state_title.c` (save preview), `shop.c`, `hud.c`, `enemy.c`, `boss.c`.

- [ ] **Step 4b: Update physics.c for universal double jump**

In `source/game/physics.c`, set `max_jumps = 2` for ALL three class entries (Trojan, Infiltrator, Technomancer). Currently only Infiltrator has `max_jumps = 2`. Also rename `[CLASS_ASSAULT]` → `[CLASS_TROJAN]`.

- [ ] **Step 5: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Zero warnings, zero errors.

Run: `docker exec gba-dev bash -c "cd /workspace && make test"`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "refactor: rename CLASS_ASSAULT to CLASS_TROJAN, add tier enums, remove old evolution enums"
```

---

## Task 2: Skill System Data Layer (skills.h / skills.c)

**Files:**
- Create: `include/game/skills.h`
- Create: `source/game/skills.c`

- [ ] **Step 1: Create skills.h with enums and structures**

```c
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
    u8  damage_scale;       /* Base damage multiplier (100 = 1x ATK) */
    u8  damage_per_rank;    /* Additional % per rank (e.g. 10 = +10%/rank) */
    u8  duration_base;      /* Base duration in frames (buffs/debuffs) */
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
```

- [ ] **Step 2: Create skills.c with the global skill table**

Create `source/game/skills.c` with:
- The full `skill_defs[48]` const array (name, cooldown, damage, duration, type, tier for each).
- `skills_get_unlocked()` — switch on t1_class to write T1 skill IDs, then on t2_spec for T2, then on t3_spec for T3.
- `skill_get_def()`, `skill_get_cooldown()`, `skill_get_damage()`, `skill_get_duration()` — simple lookups with rank scaling.
- `tier_get_name()` — returns string for each tier choice.
- `tier2_get_options()`, `tier3_get_options()` — return the two branching options.

Cooldown formula: `base - rank * rank_reduce` (clamped to minimum 30 frames).
Damage formula: `player_atk * (damage_scale + rank * damage_per_rank) / 100`.
Duration formula: `duration_base + rank * duration_per_rank`.

Use realistic values from the spec — e.g. Charged Shot: 90 frame CD, 300% ATK base, +10%/rank. Iron Skin: 180 frame CD, 180 frame duration, +15 frames/rank.

- [ ] **Step 3: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Zero warnings. (skills.c compiles but isn't called yet.)

Run: `docker exec gba-dev bash -c "cd /workspace && make test"`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add include/game/skills.h source/game/skills.c
git commit -m "feat: add skill system data layer — 48 skill definitions, tier lookup API"
```

---

## Task 3: Color Presets (colors.h / colors.c)

**Files:**
- Create: `include/game/colors.h`
- Create: `source/game/colors.c`

- [ ] **Step 1: Create colors.h**

```c
#ifndef GAME_COLORS_H
#define GAME_COLORS_H

#include <tonc.h>

#define COLOR_PRESET_COUNT 24

/* Apply suit and visor colors to player OBJ palette bank 0.
 * suit_idx: 0-23, visor_idx: 0-23.
 * Writes to pal_obj_mem[0] indices 2-4 (suit) and 5-7 (visor). */
void colors_apply_player_palette(int suit_idx, int visor_idx);

/* Get preview RGB15 value for a suit preset (returns the "base" shade). */
u16 colors_get_suit_preview(int idx);

/* Get preview RGB15 value for a visor preset (returns the "bright" shade). */
u16 colors_get_visor_preview(int idx);

#endif /* GAME_COLORS_H */
```

- [ ] **Step 2: Create colors.c with 24 suit + 24 visor presets**

Each preset is 3 RGB15 values (dark/medium/bright for suit indices 2-4, dim/mid/bright for visor indices 5-7).

Design 24 suit colors spanning the full spectrum: red, crimson, orange, amber, gold, yellow, lime, green, emerald, teal, cyan, sky, blue, indigo, violet, purple, magenta, pink, white, silver, gunmetal, charcoal, brown, olive. Each has a coherent dark→medium→bright ramp.

Design 24 visor colors: similar spectrum with glowing quality (brighter, more saturated). Dim→mid→bright ramps that suggest illuminated glass.

`colors_apply_player_palette()` writes 6 palette entries via `pal_obj_mem[0*16 + idx]` using memcpy16.

- [ ] **Step 3: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Zero warnings.

Run: `docker exec gba-dev bash -c "cd /workspace && make test"`
Expected: PASS

- [ ] **Step 4: Commit**

```bash
git add include/game/colors.h source/game/colors.c
git commit -m "feat: add 24 suit + 24 visor color presets for character customization"
```

---

## Task 4: PlayerState Struct Redesign (player.h)

**Files:**
- Modify: `include/game/player.h`

- [ ] **Step 1: Replace old progression fields with new ones**

In the PERSISTENT section of `PlayerState`, replace:

```c
/* OLD — remove these: */
u8  player_class;     /* CLASS_ASSAULT / INFILTRATOR / TECHNOMANCER */
u8  evolution;
u8  skill_tree[SKILL_TREE_SIZE];
u8  skill_points;
u8  ability_unlocks;
u8  evolution_pending;
u8  last_used_ability;
```

With:

```c
/* NEW — tier-based progression */
u8  player_class;     /* CLASS_TROJAN / INFILTRATOR / TECHNOMANCER (0xFF = classless) */
u8  tier_choices[3];  /* [0]=T1 class, [1]=T2 SPEC_*, [2]=T3 SPEC3_* (0xFF=unchosen) */
u8  skill_ranks[MAX_PLAYER_SKILLS]; /* Rank 0-10 per unlocked skill */
u8  skill_points;     /* Unspent skill points (1/level, banked pre-T1) */
u8  slotted_skill;    /* Index into unlocked skills array (which skill R fires) */
u8  suit_color;       /* 0-23 palette preset */
u8  visor_color;      /* 0-23 palette preset */
```

- [ ] **Step 2: Remove old defines, update level-scope fields**

Remove: `SKILL_BRANCHES`, `SKILLS_PER_BRANCH`, `SKILL_TREE_SIZE`, `SKILL_MAX_RANK` (3), `SKILL_POINTS_PER_2_LEVELS`, `EVOLUTION_LEVEL`, `ABILITY_1..8` bitmask defines.

Update `MAX_LEVEL` → reference `MAX_LEVEL_BASE` and `MAX_LEVEL_ENDGAME` from common.h.

In the level-scope section, replace:

```c
/* OLD — remove: */
u8  selected_ability;
u16 cooldown_ability[8];
/* ...all the per-ability buff timers... */
u8  ability_wheel_open;
u8  ability_wheel_slot;
u8  ability_wheel_page;
```

With:

```c
/* NEW — single skill slot */
u16 skill_cooldown;       /* Cooldown frames remaining for slotted skill */
u8  skill_wheel_open;     /* 1=skill wheel overlay active */
u8  skill_wheel_cursor;   /* Cursor position in wheel */
/* Active buff timers (reused from old system, same names) */
u16 buff_timer_1;         /* Generic buff timer slots */
u16 buff_timer_2;
u16 buff_timer_3;
u16 buff_timer_4;
```

Note: Keep `charge_rush_timer`, `tether_timer`, `dash_timer` etc. for movement skills that are now tier-specific. They'll be activated via skill dispatch.

- [ ] **Step 3: Update function signatures**

Remove: `player_apply_evolution()`, `player_get_evolution_name()`, `player_skill_allocate()`, `player_get_skill_bonus()`, `player_ability_wheel_is_open()`, `player_ability_wheel_slot()`, `player_ability_wheel_page()`.

Add:

```c
/* Check if player has chosen T1 class yet. */
int player_has_class(void);

/* Get number of unlocked skills for current build path. */
int player_get_skill_count(void);

/* Get the skill ID at position idx in the player's unlocked skill list. */
int player_get_skill_id(int idx);

/* Get the player's current tier name string. */
const char* player_get_tier_name(void);
```

- [ ] **Step 4: Build (expect errors — dependents not yet updated)**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make 2>&1 | head -40"`
Expected: Compilation errors in files referencing removed fields/functions. This is expected — we fix them in subsequent tasks.

- [ ] **Step 5: Commit (WIP)**

```bash
git add include/game/player.h
git commit -m "wip: redesign PlayerState struct for tier-based progression"
```

---

## Task 5: SaveData Struct Redesign (save.h / save.c)

**Files:**
- Modify: `include/engine/save.h`
- Modify: `source/engine/save.c`

- [ ] **Step 1: Update SAVE_MAGIC**

Change: `#define SAVE_MAGIC 0x6750726F` → `#define SAVE_MAGIC 0x67507232` (new magic = old saves invalid).

- [ ] **Step 2: Update SaveData struct**

Replace old progression fields:

```c
/* OLD — remove: */
u8  ability_unlocks;
u8  skill_tree[12];
u8  evolution;
u8  evolution_pending;
u8  last_used_ability;
```

With:

```c
/* NEW — tier progression */
u8  tier_choices[3];    /* T1 class, T2 spec, T3 spec (0xFF=unchosen) */
u8  skill_ranks[7];     /* Per-skill rank (0-10) */
u8  skill_points;       /* Unspent points */
u8  slotted_skill;      /* Active skill slot index */
u8  suit_color;         /* 0-23 */
u8  visor_color;        /* 0-23 */
u8  buff_charges[4];    /* XP Booster, Shield Charge, Credit Finder, Loot Magnet */
```

Recalculate `reserved[]` size so `sizeof(SaveData) == 512`. The old fields used ~16 bytes; new fields use ~19 bytes, so shrink `reserved[]` by 3.

Keep the `_Static_assert(sizeof(SaveData) == SAVE_SLOT_SIZE, ...)`.

- [ ] **Step 3: Update save_defaults()**

In `save.c`, update `save_defaults()`:

```c
void save_defaults(SaveData* data) {
    memset(data, 0, sizeof(SaveData));
    data->magic = SAVE_MAGIC;
    data->player_class = 0xFF;  /* Classless start */
    data->player_level = 0;     /* Start at level 0 */
    data->player_hp = 20;
    data->player_max_hp = 20;
    data->player_atk = 5;
    data->player_def = 5;
    data->player_spd = 5;
    data->player_lck = 5;
    data->credits = 0;
    data->equipped_idx = 0xFF;
    data->tier_choices[0] = 0xFF;
    data->tier_choices[1] = 0xFF;
    data->tier_choices[2] = 0xFF;
    data->slotted_skill = 0xFF;
    /* ... checksum ... */
}
```

- [ ] **Step 4: Commit (WIP)**

```bash
git add include/engine/save.h source/engine/save.c
git commit -m "wip: redesign SaveData for tier-based progression, invalidate old saves"
```

---

## Task 6: Player Stat System Rewrite (player.c)

**Files:**
- Modify: `source/game/player.c`

- [ ] **Step 1: Replace base_stats and level_gains tables**

Remove the old `base_stats[CLASS_COUNT][5]` and `level_gains[CLASS_COUNT][5]` tables.

Replace with classless base stats and per-class allocation patterns:

```c
/* Classless base stats (level 0) */
#define CLASSLESS_HP   20
#define CLASSLESS_ATK   5
#define CLASSLESS_DEF   5
#define CLASSLESS_SPD   5
#define CLASSLESS_LCK   5

/* Per-level gains: classless (L0-4) = +1 to each stat */
#define CLASSLESS_GAIN  1

/* Per-class auto-allocation (L5+): 5 pts/level, spread 2+2+1 */
/*                         HP gain, ATK, DEF, SPD, LCK */
static const s16 class_alloc[CLASS_COUNT][5] = {
    [CLASS_TROJAN]       = { 3, 2, 2, 1, 0 },  /* Primary: ATK+DEF, Secondary: SPD */
    [CLASS_INFILTRATOR]  = { 2, 1, 0, 2, 2 },  /* Primary: SPD+LCK, Secondary: ATK */
    [CLASS_TECHNOMANCER] = { 2, 2, 1, 0, 2 },  /* Primary: ATK+LCK, Secondary: DEF */
};
```

- [ ] **Step 2: Rewrite player_get_base_stats()**

```c
void player_get_base_stats(int cls, int lvl, s16* hp, s16* atk, s16* def, s16* spd, s16* lck) {
    /* Start with classless base */
    *hp  = CLASSLESS_HP;
    *atk = CLASSLESS_ATK;
    *def = CLASSLESS_DEF;
    *spd = CLASSLESS_SPD;
    *lck = CLASSLESS_LCK;

    /* Classless levels (0-4): +1 each per level */
    int classless_levels = (lvl < TIER_1_LEVEL) ? lvl : (TIER_1_LEVEL - 1);
    *hp  += (s16)(classless_levels * 2); /* HP gains slightly more */
    *atk += (s16)(classless_levels * CLASSLESS_GAIN);
    *def += (s16)(classless_levels * CLASSLESS_GAIN);
    *spd += (s16)(classless_levels * CLASSLESS_GAIN);
    *lck += (s16)(classless_levels * CLASSLESS_GAIN);

    /* Class levels (5+): auto-allocated */
    if (lvl >= TIER_1_LEVEL && cls < CLASS_COUNT) {
        int class_levels = lvl - TIER_1_LEVEL + 1;
        *hp  += (s16)(class_levels * class_alloc[cls][0]);
        *atk += (s16)(class_levels * class_alloc[cls][1]);
        *def += (s16)(class_levels * class_alloc[cls][2]);
        *spd += (s16)(class_levels * class_alloc[cls][3]);
        *lck += (s16)(class_levels * class_alloc[cls][4]);
    }
}
```

- [ ] **Step 3: Remove apply_evolution_bonuses() and apply_skill_bonuses(), update compute_stats()**

Delete `apply_evolution_bonuses()` and `apply_skill_bonuses()` functions entirely. Then update `compute_stats()` (around line 314 in player.c) — remove the calls to these two deleted functions. The function should now call only `player_get_base_stats()` + `apply_equipment_bonuses()`:

```c
static void compute_stats(void) {
    int cls = player_state.player_class;
    int lvl = player_state.level;
    player_get_base_stats(cls, lvl,
                          &player_state.max_hp, &player_state.atk,
                          &player_state.def, &player_state.spd,
                          &player_state.lck);
    /* REMOVED: apply_evolution_bonuses() — old evolution system deleted */
    /* REMOVED: apply_skill_bonuses() — old passive skill tree deleted */
    apply_equipment_bonuses(&player_state.max_hp, &player_state.atk,
                            &player_state.def, &player_state.spd,
                            &player_state.lck);
    if (player_state.hp > player_state.max_hp)
        player_state.hp = player_state.max_hp;
}
```

- [ ] **Step 4: Rewrite player_init()**

**IMPORTANT**: Remove the old validation clamp at the top of player_init() that does `if (player_class < 0 || player_class >= CLASS_COUNT) player_class = CLASS_ASSAULT;` — this would clamp our 0xFF classless value to 0. The new function accepts 0xFF as valid:

```c
void player_init(int player_class) {
    /* NOTE: player_class is 0xFF for new game (classless). Do NOT clamp to CLASS_COUNT. */
    memset(&player_state, 0, sizeof(PlayerState));
    player_state.player_class = (u8)player_class;
    player_state.tier_choices[0] = 0xFF;
    player_state.tier_choices[1] = 0xFF;
    player_state.tier_choices[2] = 0xFF;
    player_state.slotted_skill = 0xFF;
    player_state.level = 0;
    player_state.max_hp = CLASSLESS_HP;
    player_state.hp = CLASSLESS_HP;
    player_state.atk = CLASSLESS_ATK;
    player_state.def = CLASSLESS_DEF;
    player_state.spd = CLASSLESS_SPD;
    player_state.lck = CLASSLESS_LCK;
    player_state.suit_color = 0;
    player_state.visor_color = 0;
    player_state.jumps_remaining = 2; /* Double jump universal */
}
```

For physics lookup safety: classless players use `player_class = 0xFF`. In `player_enter_level()` where `physics_class[player_state.player_class]` is accessed, add a bounds check: `int phys_cls = (player_state.player_class < CLASS_COUNT) ? player_state.player_class : CLASS_TROJAN;` and use `physics_class[phys_cls]`.

- [ ] **Step 5: Update player_add_xp() for new level-up flow**

Modify the level-up logic in `player_add_xp()`:
- Always grant 1 skill point per level (banked if classless).
- At level 5: set a flag to trigger T1 selection (handled by state_net or state_terminal).
- At level 25/55: similar tier-up flags.
- Call `player_recompute_stats()` after level-up.
- MAX_LEVEL check: use `game_stats.endgame_unlocked ? MAX_LEVEL_ENDGAME : MAX_LEVEL_BASE`.
- **IMPORTANT**: Remove the old `skill_tree[10]` XP bonus code at line ~1340: `int xp_bonus = player_state.skill_tree[10] * 5;`. The old passive skill tree is deleted. The new XP bonus comes from `shop_buff_active(CHARGE_XP_BOOSTER)` — add: `if (shop_buff_active(CHARGE_XP_BOOSTER)) amount = amount * 115 / 100;` at the top of `player_add_xp()`. Keep the `ACC_XP_BOOSTER` equipment check (equipment and charge-based buffs stack).

- [ ] **Step 6: Add new helper functions**

```c
int player_has_class(void) {
    return player_state.player_class != 0xFF;
}

int player_get_skill_count(void) {
    u8 ids[MAX_PLAYER_SKILLS];
    return skills_get_unlocked(
        player_state.tier_choices[0],
        player_state.tier_choices[1],
        player_state.tier_choices[2],
        ids);
}

int player_get_skill_id(int idx) {
    u8 ids[MAX_PLAYER_SKILLS];
    int count = skills_get_unlocked(
        player_state.tier_choices[0],
        player_state.tier_choices[1],
        player_state.tier_choices[2],
        ids);
    if (idx < 0 || idx >= count) return -1;
    return ids[idx];
}

const char* player_get_tier_name(void) {
    if (player_state.tier_choices[2] != 0xFF)
        return tier_get_name(3, player_state.tier_choices[2]);
    if (player_state.tier_choices[1] != 0xFF)
        return tier_get_name(2, player_state.tier_choices[1]);
    if (player_state.tier_choices[0] != 0xFF)
        return tier_get_name(1, player_state.tier_choices[0]);
    return "Classless";
}
```

- [ ] **Step 7: Update player palette loading in player_enter_level()**

After loading player sprite tiles, apply customization colors:

```c
#include "game/colors.h"
/* ... in player_enter_level(), after loading base palette: */
colors_apply_player_palette(player_state.suit_color, player_state.visor_color);
```

Remove the old `class_palettes[CLASS_COUNT][16]` static array. The base palette (outline, skin, circuit, AA, white) stays as a single const array applied first, then `colors_apply_player_palette()` overwrites indices 2-7.

- [ ] **Step 8: Remove old functions**

Delete: `player_apply_evolution()`, `player_get_evolution_name()`, `player_skill_allocate()`, `player_get_skill_bonus()`, `player_ability_wheel_is_open()`, `player_ability_wheel_slot()`, `player_ability_wheel_page()`, `player_skill_points_earned()`.

- [ ] **Step 9: Commit (WIP)**

```bash
git add source/game/player.c
git commit -m "wip: rewrite player stat system — classless start, auto-allocation, tier-based"
```

---

## Task 7: Abilities System Bridge (abilities.h / abilities.c)

**Files:**
- Modify: `include/game/abilities.h`
- Modify: `source/game/abilities.c`

The old abilities system has ~20 query functions (`ability_is_X_active()`) called from player.c, enemy.c, boss.c, hud.c. We need to keep the buff timer infrastructure but redirect activation through skills.c.

- [ ] **Step 1: Simplify abilities.h**

Remove: all `AB_SLOT_*`, `AB_UNLOCK_*`, `AB_CD_*` defines. Remove `ability_activate()`, `ability_get_name()`, `ability_get_cooldown()`, `ability_get_description()`.

Keep: all `ability_is_X_active()` and `ability_get_X_timer()` query functions, `ability_update()`, `ability_reset()`.

Add:

```c
/* Activate a skill by ID (dispatches to appropriate effect).
 * rank: current skill rank (0-10), player_atk: current ATK stat.
 * Returns 1 if activated, 0 if failed. */
int skill_activate(int skill_id, int rank, int player_atk);
```

- [ ] **Step 2: Rewrite abilities.c activation**

Remove the 3 name tables, 3 cooldown tables, `activate_assault()`, `activate_infiltrator()`, `activate_technomancer()`.

Add a single `skill_activate()` function that switches on `skill_id`:
- For projectile skills: spawn projectile with `skill_get_damage()`.
- For buff skills: set the corresponding timer to `skill_get_duration()`.
- For AoE skills: deal damage to nearby enemies.
- For dash/teleport: set player state (reuse existing dash/charge/tether logic).
- For deploy skills: spawn entity.

Keep all buff timer state variables and `ability_update()` (ticks down timers, applies per-tick effects like nanobot healing). Keep all `ability_is_X_active()` getters.

Many of the 48 skills will have placeholder implementations initially — simple projectile or buff timer with correct damage/duration from the skill table. Complex mechanics (Fortress Mode, Temporal Anchor, Feedback Loop) can be stubbed as buffs and refined later.

- [ ] **Step 3: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Errors in files still referencing old API. Note them for next tasks.

- [ ] **Step 4: Commit (WIP)**

```bash
git add include/game/abilities.h source/game/abilities.c
git commit -m "wip: bridge abilities to new skill system — keep buff timers, add skill_activate()"
```

---

## Task 8: Shop Redesign (shop.h / shop.c)

**Files:**
- Modify: `include/game/shop.h`
- Modify: `source/game/shop.c`

- [ ] **Step 1: Replace shop enums and data**

In `shop.h`:

```c
enum {
    SHOP_HEALTH_PACK = 0,   /* Restore 20 HP — instant */
    SHOP_CD_RESET,          /* Reset skill cooldown — instant */
    SHOP_XP_BOOSTER,        /* +15% XP gain — charge-based */
    SHOP_SHIELD_CHARGE,     /* +5 DEF — charge-based */
    SHOP_CREDIT_FINDER,     /* +15% credit drops — charge-based */
    SHOP_LOOT_MAGNET,       /* Increased rare drops — charge-based */
    SHOP_ITEM_COUNT
};

#define SHOP_CHARGE_MAX 5

/* Charge indices (for buff_charges[] array) */
#define CHARGE_XP_BOOSTER    0
#define CHARGE_SHIELD        1
#define CHARGE_CREDIT_FINDER 2
#define CHARGE_LOOT_MAGNET   3
#define CHARGE_TYPE_COUNT    4
```

Add charge query/consume API:

```c
/* Get current charge count for a charge type. */
int shop_get_charges(int charge_type);

/* Consume one charge of each active buff. Called on mission end/death/exit. */
void shop_consume_charges(void);

/* Check if a charge-based buff is active (charges > 0). */
int shop_buff_active(int charge_type);

/* Pack/unpack charge array for save serialization. */
void shop_get_charges_array(u8* out4);  /* Writes 4 bytes */
void shop_set_charges_array(const u8* in4);  /* Reads 4 bytes */
```

- [ ] **Step 2: Rewrite shop.c**

Replace shop_items table with 6 items, all fixed price:

```c
static const ShopItem shop_items[SHOP_ITEM_COUNT] = {
    { "Health Pack",    15 },
    { "CD Reset",       40 },
    { "XP Booster",     60 },
    { "Shield Chg",     25 },
    { "Credit Find",    50 },
    { "Loot Magnet",    50 },
};
```

Charge state is stored externally in `player_state` or via a static array synced with SaveData. The simplest approach: use a file-scope `static u8 buff_charges[CHARGE_TYPE_COUNT]` initialized from SaveData.

Rewrite `shop_buy()`:
- HEALTH_PACK/CD_RESET: instant effects (same as before).
- XP_BOOSTER/SHIELD_CHARGE/CREDIT_FINDER/LOOT_MAGNET: increment charges (capped at 5). Block purchase if already at max ("MAX" display).

Rewrite `shop_draw()`: show charge count for charge-based items instead of purchase count. Show "MAX" instead of price when at max charges.

Rewrite `shop_get_price()`: always return `shop_items[idx].base_cost` (no scaling).

Add `shop_consume_charges()`: decrement each active charge by 1 (min 0).

Add `shop_buff_active()`: return `buff_charges[type] > 0`.

Add `shop_get_charges()`: return `buff_charges[type]`.

- [ ] **Step 3: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`

- [ ] **Step 4: Commit**

```bash
git add include/game/shop.h source/game/shop.c
git commit -m "feat: redesign shop — 6 items, charge-based buffs, fixed pricing"
```

---

## Task 9: Character Creation Flow (state_charsel.c)

**Files:**
- Modify: `source/states/state_charsel.c`

- [ ] **Step 1: Replace class selection with character customization**

The new flow: New Game → Character Customization (suit + visor color) → Terminal.
Class is no longer chosen here — it's chosen in-game at level 5.

Replace `state_charsel_enter()`:
- Initialize `suit_cursor = 0`, `visor_cursor = 0`, `mode = 0` (0=suit, 1=visor).
- Load player sprite for live preview.
- Draw customization UI.

Replace `state_charsel_update()`:
- L/R: cycle through 24 color presets for current mode.
- UP/DOWN or A: toggle between suit/visor mode.
- A (when on visor mode): confirm both choices → set `player_state.suit_color`, `player_state.visor_color` → `player_init(0xFF)` (classless) → fade to STATE_TERMINAL.
- Keep the save-load overlay (same as before, with updated preview showing suit/visor instead of class).

Replace `state_charsel_draw()`:
- Draw color swatch grid or preview.
- Show player sprite with live-updated palette via `colors_apply_player_palette()`.
- Draw "SUIT COLOR" / "VISOR COLOR" labels.

- [ ] **Step 2: Update save slot preview**

Save preview now shows tier name + level instead of class abbreviation:

```c
text_print(8, row, player_get_tier_name_short(sd.tier_choices));
text_print(12, row, "Lv");
text_print_int(14, row, sd.player_level);
```

Add a local helper that reads tier_choices from SaveData and returns a 3-char abbreviation ("CLS" for classless, "TRO"/"INF"/"TEC" for T1, spec name for T2+).

- [ ] **Step 3: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Zero warnings.

- [ ] **Step 4: Commit**

```bash
git add source/states/state_charsel.c
git commit -m "feat: replace class selection with character customization (suit + visor colors)"
```

---

## Task 10: Terminal Skills & Tier Selection (state_terminal.c)

**Files:**
- Modify: `source/states/state_terminal.c`

- [ ] **Step 1: Update pack_save / unpack_save**

Replace old field packing:

```c
/* OLD */
sd->skill_tree[i] = player_state.skill_tree[i];
sd->evolution = player_state.evolution;
sd->evolution_pending = player_state.evolution_pending;
sd->last_used_ability = player_state.last_used_ability;

/* NEW */
for (int i = 0; i < 3; i++) sd->tier_choices[i] = player_state.tier_choices[i];
for (int i = 0; i < MAX_PLAYER_SKILLS; i++) sd->skill_ranks[i] = player_state.skill_ranks[i];
sd->skill_points = player_state.skill_points;
sd->slotted_skill = player_state.slotted_skill;
sd->suit_color = player_state.suit_color;
sd->visor_color = player_state.visor_color;
shop_get_charges_array(sd->buff_charges);
```

Mirror in `unpack_save()`.

- [ ] **Step 2: Replace TSUB_EVOLUTION with TSUB_TIER_SELECT**

Rename enum value. Rewrite `update_evolution()` → `update_tier_select()`:

- Determine which tier is being selected (1, 2, or 3) based on which `tier_choices[]` slot is 0xFF.
- Show 2 (or 3 for T1) options side-by-side with skill previews.
- D-pad left/right: toggle selection.
- A: confirm choice → write to `tier_choices[]`, unlock skills, auto-slot first new skill if no skill slotted.
- B: skip (can come back from terminal main menu).

Rewrite `draw_evolution()` → `draw_tier_select()` with new layout.

- [ ] **Step 3: Rewrite TSUB_SKILLS for active skill ranking**

Replace the old 3-branch passive tree UI with:
- List all unlocked skills with current rank (0-10).
- Cursor selects a skill.
- A: spend 1 skill point to increase rank (if < 10 and points available).
- Show skill stats at current vs next rank (damage, cooldown, duration).
- B: back to main menu.

- [ ] **Step 4: Update main menu indicators**

Replace evolution_pending indicator with tier-up indicator:
- If `player_state.level >= TIER_1_LEVEL && tier_choices[0] == 0xFF` → "CLASS!" blink.
- If `player_state.level >= TIER_2_LEVEL && tier_choices[1] == 0xFF` → "SPEC!" blink.
- If `player_state.level >= TIER_3_LEVEL && tier_choices[2] == 0xFF` → "T3!" blink.

Update stats page to show current tier path instead of old evolution name.

- [ ] **Step 5: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`

- [ ] **Step 6: Commit**

```bash
git add source/states/state_terminal.c
git commit -m "feat: tier selection UI + active skill ranking in terminal"
```

---

## Task 11: Gameplay Controls — Skill Wheel & R-Fire (state_net.c + player.c)

**Files:**
- Modify: `source/states/state_net.c`
- Modify: `source/game/player.c`

- [ ] **Step 1: Replace ability wheel controls in player.c**

Remove the entire R-hold ability wheel block (lines ~960-1015). Replace with:

```c
/* SELECT: open/close skill wheel */
if (input_hit(KEY_SELECT) && player_has_class()) {
    player_state.skill_wheel_open ^= 1;
    if (player_state.skill_wheel_open) {
        player_state.skill_wheel_cursor = player_state.slotted_skill;
        if (player_state.skill_wheel_cursor == 0xFF)
            player_state.skill_wheel_cursor = 0;
        audio_play_sfx(SFX_MENU_SELECT);
    }
}

/* D-pad in wheel */
if (player_state.skill_wheel_open) {
    int count = player_get_skill_count();
    if (input_hit(KEY_UP) && player_state.skill_wheel_cursor > 0)
        player_state.skill_wheel_cursor--;
    if (input_hit(KEY_DOWN) && player_state.skill_wheel_cursor < count - 1)
        player_state.skill_wheel_cursor++;
    if (input_hit(KEY_A)) {
        player_state.slotted_skill = player_state.skill_wheel_cursor;
        player_state.skill_wheel_open = 0;
        audio_play_sfx(SFX_MENU_SELECT);
    }
    if (input_hit(KEY_B) || input_hit(KEY_SELECT)) {
        player_state.skill_wheel_open = 0;
        audio_play_sfx(SFX_MENU_BACK);
    }
    return; /* Block other input while wheel is open */
}

/* R: fire slotted skill */
if (input_hit(KEY_R) && player_state.slotted_skill != 0xFF) {
    if (player_state.skill_cooldown == 0) {
        int sid = player_get_skill_id(player_state.slotted_skill);
        if (sid >= 0) {
            int rank = player_state.skill_ranks[player_state.slotted_skill];
            if (skill_activate(sid, rank, player_state.atk)) {
                player_state.skill_cooldown = (u16)skill_get_cooldown(sid, rank);
                audio_play_sfx(SFX_ABILITY);
            }
        }
    }
}
```

- [ ] **Step 2: Add cooldown tick to player_update()**

In the per-frame update section:

```c
if (player_state.skill_cooldown > 0)
    player_state.skill_cooldown--;
```

- [ ] **Step 3: Wire charge consumption in state_net.c**

At level completion (exit reached), death, and early exit, call `shop_consume_charges()`. Find the relevant code paths:
- `NETSUB_EXIT_ANIM` completion → `shop_consume_charges()`.
- `NETSUB_DEATH` → `shop_consume_charges()`.
- Pause menu "Quit" → `shop_consume_charges()`.

- [ ] **Step 4: Wire XP/credit modifiers**

In enemy/boss kill XP granting:

```c
int xp = base_xp;
if (shop_buff_active(CHARGE_XP_BOOSTER))
    xp = xp * 115 / 100;  /* +15% */
player_add_xp(xp);

int creds = base_credits;
if (shop_buff_active(CHARGE_CREDIT_FINDER))
    creds = creds * 115 / 100;
player_state.credits += (u32)creds;
```

- [ ] **Step 5: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`

Run: `docker exec gba-dev bash -c "cd /workspace && make test"`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add source/game/player.c source/states/state_net.c
git commit -m "feat: SELECT+R skill controls, charge consumption, XP/credit modifiers"
```

---

## Task 12: HUD Updates (hud.c)

**Files:**
- Modify: `source/game/hud.c`

- [ ] **Step 1: Add slotted skill display on HUD**

On HUD row 1 (below HP bar), show the slotted skill name and cooldown bar:

```c
/* Slotted skill name + cooldown */
if (player_state.slotted_skill != 0xFF) {
    int sid = player_get_skill_id(player_state.slotted_skill);
    if (sid >= 0) {
        const SkillDef* sd = skill_get_def(sid);
        text_print(0, 1, sd->name);
        /* Cooldown bar: [====----] */
        if (player_state.skill_cooldown > 0) {
            int rank = player_state.skill_ranks[player_state.slotted_skill];
            int max_cd = skill_get_cooldown(sid, rank);
            int filled = (max_cd > 0) ? (player_state.skill_cooldown * 6 / max_cd) : 0;
            /* Draw small bar */
            for (int i = 0; i < 6; i++)
                text_put_char(16 + i, 1, (i < 6 - filled) ? '=' : '-');
        } else {
            text_print(16, 1, "READY ");
        }
    }
}
```

- [ ] **Step 2: Add skill wheel overlay**

When `player_state.skill_wheel_open`, draw a panel listing all unlocked skills:

```c
if (player_state.skill_wheel_open) {
    /* Semi-transparent overlay on rows 4-14 */
    int count = player_get_skill_count();
    text_clear_rect(2, 4, 26, count + 2);
    terminal_draw_panel(3, 4, count + 2, 26);
    text_print(5, 4, "SKILLS");
    for (int i = 0; i < count; i++) {
        int sid = player_get_skill_id(i);
        const SkillDef* sd = skill_get_def(sid);
        int row = 5 + i;
        text_put_char(4, row, (i == player_state.skill_wheel_cursor) ? '>' : ' ');
        text_print(6, row, sd->name);
        /* Rank display */
        text_print(20, row, "R");
        text_print_int(21, row, player_state.skill_ranks[i]);
    }
}
```

- [ ] **Step 3: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`

- [ ] **Step 4: Commit**

```bash
git add source/game/hud.c
git commit -m "feat: HUD shows slotted skill + cooldown, skill wheel overlay"
```

---

## Task 13: Rarity Letter Grades (loot.c)

**Files:**
- Modify: `source/game/loot.c`

- [ ] **Step 1: Replace rarity name strings**

Find `loot_get_rarity_name()` and replace the full names:

```c
const char* loot_get_rarity_name(int rarity) {
    static const char* const names[] = {
        "C", "B", "A", "A+", "S", "S+"
    };
    if (rarity < 0 || rarity >= RARITY_COUNT) return "?";
    return names[rarity];
}
```

This changes display everywhere rarity is shown (inventory, loot drops, crafting, shop, HUD weapon tag).

- [ ] **Step 2: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`

- [ ] **Step 3: Commit**

```bash
git add source/game/loot.c
git commit -m "feat: replace rarity names with letter grades (C/B/A/A+/S/S+)"
```

---

## Task 14: Crafting Preview (state_terminal.c crafting section)

**Files:**
- Modify: `source/states/state_terminal.c` (TSUB_CRAFT section)

- [ ] **Step 1: Add preview step to fuse flow**

Before confirming a fuse, generate the result item and display it:

```c
/* In fuse confirmation: */
static LootItem fuse_preview;

/* When 3 items selected, generate preview */
loot_generate(&fuse_preview, ...);  /* Same call as actual fuse */

/* Draw preview panel */
text_print(3, 12, "Result:");
text_print(3, 13, loot_get_name(&fuse_preview));
text_print(3, 14, loot_get_rarity_name(fuse_preview.rarity));
text_print(3, 16, "A:Confirm  B:Cancel");

/* On A: commit the fuse (result already generated) */
/* On B: discard preview, return to selection */
```

- [ ] **Step 2: Add preview to salvage flow**

Show exact shard value before confirming:

```c
int shard_val = loot_salvage_value(inventory_get(craft_sel[0]));
text_print(3, 12, "Salvage for:");
text_print_int(3, 13, shard_val);
text_print(3 + digits, 13, " shards");
```

- [ ] **Step 3: Add preview to forge flow**

Show stat range preview (min-max for rerolled stats):

```c
text_print(3, 12, "Reroll stats:");
text_print(3, 13, "ATK: ");
/* Show range based on item level + rarity */
```

- [ ] **Step 4: Build and test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`

- [ ] **Step 5: Commit**

```bash
git add source/states/state_terminal.c
git commit -m "feat: crafting preview before confirm for fuse/salvage/forge"
```

---

## Task 15: Integration — Fix All Remaining Compile Errors

**Files:**
- Modify: multiple files with stale references

- [ ] **Step 1: Fix all CLASS_ASSAULT references**

Grep for any remaining `CLASS_ASSAULT` and replace with `CLASS_TROJAN`.

- [ ] **Step 2: Fix all evolution references**

Grep for `evolution`, `EVOLUTION_`, `evolution_pending`, `player_apply_evolution`, `player_get_evolution_name`. Remove or replace each:
- `player_state.evolution` → removed field, delete references.
- `player_state.evolution_pending` → check tier_choices instead.
- `ACH_EVOLVED` → change to `ACH_TIER_UP` (first tier choice made).

- [ ] **Step 3: Fix all old skill_tree references**

Grep for `skill_tree`, `SKILL_TREE_SIZE`, `player_skill_allocate`, `player_get_skill_bonus`, `SKILL_POINTS_PER_2_LEVELS`. Replace:
- `player_state.skill_tree[i]` → `player_state.skill_ranks[i]`.
- `SKILL_TREE_SIZE` → `MAX_PLAYER_SKILLS`.
- Remove `player_skill_allocate()` calls (replaced by terminal UI).

- [ ] **Step 4: Fix all old ability_unlocks references**

Grep for `ability_unlocks`, `ABILITY_1..8`. Remove — skill unlock is now tier-based.

- [ ] **Step 5: Fix ability_activate calls**

Grep for `ability_activate(`. Replace with `skill_activate()` calls using the new API.

- [ ] **Step 6: Fix old shop references**

Grep for `SHOP_ATK_UP`, `SHOP_DEF_UP`, `SHOP_SPD_UP`, `SHOP_LCK_UP`, `SHOP_SHIELD_PROG`, `SHOP_SPEED_PROG`. Remove any remaining references.

- [ ] **Step 7: Fix SaveData field references in state_terminal.c**

Ensure pack_save/unpack_save use new field names. Fix the `_Static_assert` for shop_purchases size (now 6, not 9).

- [ ] **Step 8: Fix state_gameover.c and state_win.c**

Replace class/evolution display with tier name:

```c
/* OLD: class_names[player_state.player_class] */
/* NEW: player_get_tier_name() */
```

- [ ] **Step 9: Full build**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Zero warnings, zero errors.

- [ ] **Step 10: Run headless test**

Run: `docker exec gba-dev bash -c "cd /workspace && make test"`
Expected: PASS

- [ ] **Step 11: Commit**

```bash
git add -A && git commit -m "fix: resolve all compile errors from character build redesign"
```

---

## Task 16: Smoke Test & Polish

**Files:**
- Various

- [ ] **Step 1: Verify new game flow**

Mentally trace: Title → Charsel (customization) → Terminal (classless, L0) → Net (gameplay). Verify `player_init(0xFF)` sets correct starting stats.

- [ ] **Step 2: Verify level-up flow**

Trace: Kill enemies → XP → level 5 → tier selection prompt. Verify tier_choices[0] gets set, skills unlock, first skill auto-slotted.

- [ ] **Step 3: Verify skill activation**

Trace: SELECT opens wheel → D-pad selects → A slots → R fires → cooldown starts → cooldown ticks → ready again.

- [ ] **Step 4: Verify save/load cycle**

Trace: Terminal → Save → reload → all tier_choices, skill_ranks, suit/visor colors, buff charges persisted correctly.

- [ ] **Step 5: Full clean build + test**

Run: `docker exec gba-dev bash -c "cd /workspace && make clean && make"`
Expected: Zero warnings.

Run: `docker exec gba-dev bash -c "cd /workspace && make test"`
Expected: PASS

- [ ] **Step 6: Commit**

```bash
git add -A && git commit -m "feat: complete character build redesign — classless start, 3-tier evolution, 48 active skills, charge-based shop, color customization"
```

---

## Deferred Work (Not In This Plan)

These items are explicitly deferred per the spec or are polish that should happen after the core redesign is validated through playtesting:

1. **Weapon visual sprites** — OBJ tile attachment per weapon type (spec section 6). Requires sprite art + OBJ tile budget planning. Implement after core systems are stable.
2. **Complex skill mechanics** — Fortress Mode (rooted, reflect), Temporal Anchor (position snapshot/rewind), Feedback Loop (damage accumulator). Implement as buff timers initially, refine per-skill after playtesting.
3. **XP curve tuning** — Spec says "exact tuning deferred to playtesting."
4. **L button binding** — Spec says "remains unbound, can be assigned in future."

---

## Key Gotchas for Implementers

1. **SRAM save is 8-bit only** — never use memcpy on SRAM addresses. Use byte-by-byte loop.
2. **SaveData must be exactly 512 bytes** — `_Static_assert` enforces this. Adjust `reserved[]` when adding/removing fields. Also update `shop_purchases[]` size from 9→6.
3. **PlayerState layout** — persistent fields above `_level_scope_start`, per-level fields below. New persistent fields go ABOVE the marker.
4. **Fixed-point math** — all positions/velocities are 8.8 FP. Use `FP8()` macro. GBA has NO FPU.
5. **No malloc** — all data is statically allocated. Skill table is const ROM data.
6. **16-bit VRAM writes** — never write single bytes to palette/OAM/VRAM. Use `memcpy16`.
7. **CLASS_TROJAN = 0** — ensure all array indices still work (was CLASS_ASSAULT = 0).
8. **Double jump is universal** — update `max_jumps = 2` in ALL physics_class entries (not just Infiltrator). Set `jumps_remaining = 2` in player_init.
9. **Ability buff query functions** — enemy.c, boss.c, player.c all call `ability_is_X_active()`. These must continue working through the bridge layer.
10. **wheel_suppress / r_hold_frames** — delete these statics entirely (old R-hold wheel removed).
11. **Classless physics safety** — `player_class = 0xFF` is out of bounds for `physics_class[CLASS_COUNT]`. Clamp to `CLASS_TROJAN` (index 0) when looking up physics params. Do NOT clamp in player_init() itself — only at the physics lookup site.
12. **Old skill_tree XP/credit bonuses** — `player.c:1340` has `skill_tree[10] * 5` XP bonus, `enemy.c:1611` and `boss.c:1380` have `skill_tree[11] * 5` credit bonus. These must be deleted (old skill tree removed). Replace with `shop_buff_active()` charge checks.
13. **ACH_EVOLVED trigger** — rename to ACH_TIER_UP. Old trigger at evolution selection must be replaced with trigger at T1 class selection. **ACH_SKILL_MASTER** — old trigger checked `skill_tree` branch fill. New trigger: check if any `skill_ranks[i] >= SKILL_MAX_RANK`.
14. **state_title.c save preview** — uses `sd.player_class % 3` for class abbreviation. Must handle `0xFF` (classless) case to avoid garbage display.
15. **Rarity colors** — existing palette system already assigns colors per rarity tier (BG palette banks 8-11). Letter grades inherit these colors. No additional palette work needed.
