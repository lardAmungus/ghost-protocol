/*
 * Ghost Protocol — Net State (Main Gameplay)
 *
 * Side-scrolling action state. Generates a level from the active contract,
 * spawns player + enemies, runs combat, handles level exit or death.
 */
#include <tonc.h>
#include <string.h>
#include "engine/text.h"
#include "engine/input.h"
#include "engine/video.h"
#include "engine/entity.h"
#include "engine/sprite.h"
#include "engine/camera.h"
#include "engine/audio.h"
#include "engine/collision.h"
#include "engine/rng.h"
#include "game/common.h"
#include "game/player.h"
#include "game/physics.h"
#include "game/projectile.h"
#include "game/abilities.h"
#include "game/levelgen.h"
#include "game/networld.h"
#include "game/quest.h"
#include "game/hud.h"
#include "game/enemy.h"
#include "game/boss.h"
#include "game/loot.h"
#include "game/itemdrop.h"
#include "game/bugbounty.h"
#include "game/particle.h"
#include "states/state_ids.h"
#include "states/state_net.h"

/* Net sub-states */
enum {
    NETSUB_FADEIN = 0,
    NETSUB_PLAY,
    NETSUB_EXIT_ANIM,
    NETSUB_DEATH,
    NETSUB_PAUSE,
    NETSUB_BOSS_INTRO,
};

static Camera cam;
static int sub_state;
static int timer;
static int pause_cursor;
static int level_complete;

/* Enemy spawn tracking */
static int enemies_killed;
static u8 contract_boss_defeated; /* For bounty/story contracts */

/* Wave respawning for survival/exterminate contracts */
static int wave_timer;        /* Frames until next wave check */
#define WAVE_INTERVAL 300     /* ~5 seconds between wave checks */
#define WAVE_MIN_ALIVE 3      /* Spawn more when fewer than this alive */

/* Hazard tile damage cooldown (prevents instant death on hazard tiles) */
static int hazard_cd;

/* Ambient palette cycling timer */
static int ambient_timer;

/* Boss intro cinematic */
static int boss_intro_done;
static int boss_warning_shown;

/* Combo system */
static int combo_count;
static int combo_timer;   /* Frames remaining before combo resets */
static int prev_kills;    /* Kill count from previous frame */
static int milestone_shown; /* Bitmask: 1=50%, 2=90% milestone shown */
#define COMBO_WINDOW 180  /* 3 seconds to chain next kill */

/* Section tracking */
static int current_section;  /* Player's current section index */

/* Mission timer */
static int mission_frames;
static u16 mission_start_dmg_taken; /* Snapshot of damage_taken at mission start */
static int tesla_timer;     /* Toggles every 90 frames */
static int corrupt_timer;   /* Corruption drain every 60 frames */

/* Check player vs enemy projectile collision */
static void check_projectile_vs_player(void) {
    Projectile* pool = projectile_get_pool();
    Entity* player = player_get();
    if (!player || !player_is_alive()) return;
    if (player_state.invincible_timer > 0) return;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* p = &pool[i];
        if (!(p->flags & PROJ_ACTIVE)) continue;
        if (!(p->flags & PROJ_ENEMY)) continue;

        /* Simple AABB check */
        int px = p->x >> 8;
        int py = p->y >> 8;
        int ex = player->x >> 8;
        int ey = player->y >> 8;

        if (px >= ex && px <= ex + (int)player->width &&
            py >= ey && py <= ey + (int)player->height) {
            /* Hit! */
            s16 kb = (p->vx > 0) ? 128 : -128;
            player_take_damage(p->damage, kb, -64);
            /* player_take_damage handles SFX + shake internally */
            /* Deactivate projectile (hide OAM + free) */
            projectile_deactivate(p);
        }
    }
}

/* Check if player reached exit */
static int check_exit(void) {
    Entity* player = player_get();
    if (!player) return 0;

    int ptx = (player->x >> 8) / 8;
    int pty = (player->y >> 8) / 8;

    int dx = ptx - (int)level_data.exit_x;
    int dy = pty - (int)level_data.exit_y;

    return (dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1);
}

/* Spawn enemies at generated spawn points */
static void spawn_enemies(int tier) {
    enemy_init();
    enemies_killed = 0;
    milestone_shown = 0;

    /* Bug bounty stat scale (x256 fixed-point, 256 = 1.0x) */
    int bb_scale = bb_state.active ? bugbounty_stat_scale(bb_state.tier) : 256;

    for (int i = 0; i < (int)level_data.num_spawns; i++) {
        int tx = level_data.spawn_points[i][0];
        int ty = level_data.spawn_points[i][1];

        /* Pick enemy type based on tier */
        int subtype;
        int roll = (int)rand_range(100);

        if (bb_state.active) {
            /* Bug bounty: per-tier enemy composition */
            /*  T0 Zero-Day:       15/15/10/15/10/35 */
            /*  T1 Kernel Panic:   10/10/10/20/10/40 */
            /*  T2 Buffer Overflow: 5/10/ 5/20/15/45 */
            /*  T3 Stack Smash:     0/ 5/ 5/25/20/45 */
            /*  T4 Ring Zero:       0/ 0/ 0/ 0/ 0/100 (all hunters) */
            static const u8 bb_comp[BB_TIER_COUNT][6] = {
                { 15, 30, 40, 55, 65, 100 }, /* cumulative thresholds */
                { 10, 20, 30, 50, 60, 100 },
                {  5, 15, 20, 40, 55, 100 },
                {  0,  5, 10, 35, 55, 100 },
                {  0,  0,  0,  0,  0, 100 },
            };
            int bt = bb_state.tier;
            if (bt >= BB_TIER_COUNT) bt = BB_TIER_COUNT - 1;
            if (roll < bb_comp[bt][0]) subtype = ENEMY_SENTRY;
            else if (roll < bb_comp[bt][1]) subtype = ENEMY_PATROL;
            else if (roll < bb_comp[bt][2]) subtype = ENEMY_FLYER;
            else if (roll < bb_comp[bt][3]) subtype = ENEMY_SHIELD;
            else if (roll < bb_comp[bt][4]) subtype = ENEMY_SPIKE;
            else subtype = ENEMY_HUNTER;
        } else {
            /* Per-act enemy composition (6 types per act, cumulative %) */
            static const u8 act_types[7][6] = {
                /* Freelance */ { ENEMY_SENTRY, ENEMY_PATROL, ENEMY_DRONE, ENEMY_FLYER, ENEMY_SENTRY, ENEMY_PATROL },
                /* Act 1 */    { ENEMY_SENTRY, ENEMY_PATROL, ENEMY_DRONE, ENEMY_FLYER, ENEMY_TURRET, ENEMY_PATROL },
                /* Act 2 */    { ENEMY_PATROL, ENEMY_FLYER,  ENEMY_TURRET, ENEMY_SHIELD, ENEMY_CORRUPTOR, ENEMY_HUNTER },
                /* Act 3 */    { ENEMY_FLYER,  ENEMY_CORRUPTOR, ENEMY_GHOST, ENEMY_MIMIC, ENEMY_SPIKE, ENEMY_HUNTER },
                /* Act 4 */    { ENEMY_HUNTER, ENEMY_BOMBER, ENEMY_SHIELD, ENEMY_SPIKE, ENEMY_TURRET, ENEMY_DRONE },
                /* Act 5 */    { ENEMY_HUNTER, ENEMY_GHOST, ENEMY_CORRUPTOR, ENEMY_BOMBER, ENEMY_DRONE, ENEMY_SHIELD },
                /* Act 6 */    { ENEMY_HUNTER, ENEMY_GHOST, ENEMY_CORRUPTOR, ENEMY_BOMBER, ENEMY_TURRET, ENEMY_DRONE },
            };
            int act = quest_state.current_act;
            if (act < 0) act = 0;
            if (act > 6) act = 6;
            /* Weighted: first 2 types more common, last 2 less */
            int idx;
            if (roll < 25) idx = 0;
            else if (roll < 50) idx = 1;
            else if (roll < 65) idx = 2;
            else if (roll < 80) idx = 3;
            else if (roll < 90) idx = 4;
            else idx = 5;
            subtype = act_types[act][idx];
        }

        Entity* e = enemy_spawn(subtype, tx, ty, tier);

        /* Per-section difficulty scaling: enemies in later sections get tougher.
         * Section 0-3: no bonus. Section 4-7: +10% HP/ATK. 8-11: +20%. 12-15: +30%. */
        if (e) {
            int section = tx / 16;
            if (section > 3) {
                int sec_scale = 256 + (section / 4) * 26; /* ~+10% per 4 sections */
                e->hp = (s16)((e->hp * sec_scale) >> 8);
                enemy_scale_atk(e, sec_scale);
            }
        }

        /* Apply bug bounty stat scaling to HP and ATK */
        if (e && bb_scale != 256) {
            e->hp = (s16)((e->hp * bb_scale) >> 8);
            enemy_scale_atk(e, bb_scale);
        }
    }
}

void state_net_enter(void) {
    REG_DISPCNT = DCNT_MODE0 | DCNT_OBJ | DCNT_OBJ_1D | DCNT_BG0 | DCNT_BG1 | DCNT_BG2;
    text_clear_all();

    /* Get active contract */
    Contract* c = quest_get_active();
    u16 seed = c ? c->seed : 42;
    int tier = c ? c->tier : 1;
    int is_boss = (c && (
        (c->type == CONTRACT_STORY && quest_is_boss_mission(c->story_mission)) ||
        c->type == CONTRACT_BOUNTY)) ? 1 : 0;

    /* Initialize subsystems */
    entity_init();
    sprite_init();
    projectile_init();
    particle_init();
    hud_init();
    /* NOTE: Do NOT call inventory_init() here — it wipes persistent inventory */
    itemdrop_init();
    hazard_cd = 0;
    ambient_timer = 0;
    boss_intro_done = 0;
    boss_warning_shown = 0;
    combo_count = 0;
    combo_timer = 0;
    prev_kills = 0;
    current_section = -1;
    mission_frames = 0;
    mission_start_dmg_taken = game_stats.damage_taken;
    enemy_reset_chase_count();
    tesla_timer = 0;
    corrupt_timer = 0;
    collision_tesla_active = 1;

    /* Generate level */
    levelgen_generate(seed, tier, is_boss);

    /* Load graphics — act-themed palettes for visual variety */
    int act = (int)quest_state.current_act;
    networld_load_tileset(act);
    networld_load_parallax(seed, act);

    /* Initialize player at level spawn.
     * player_init() memsets player_state to 0, so save/restore persistent fields. */
    {
        u8  saved_class          = player_state.player_class;
        u8  saved_level          = player_state.level;
        u16 saved_xp             = player_state.xp;
        u16 saved_credits        = player_state.credits;
        u8  saved_unlocks        = player_state.ability_unlocks;
        /* Save combat stats to preserve shop upgrade bonuses across jack-ins */
        s16 saved_max_hp         = player_state.max_hp;
        s16 saved_atk            = player_state.atk;
        s16 saved_def            = player_state.def;
        s16 saved_spd            = player_state.spd;
        s16 saved_lck            = player_state.lck;
        int had_stats            = (saved_atk > 0); /* 0 = first jack-in, uninitialized */
        /* Save skill tree and evolution so compute_stats() stays correct after return */
        u8  saved_skill_tree[SKILL_TREE_SIZE];
        for (int i = 0; i < SKILL_TREE_SIZE; i++)
            saved_skill_tree[i] = player_state.skill_tree[i];
        u8  saved_evolution      = player_state.evolution;
        u8  saved_evo_pending    = player_state.evolution_pending;
        u8  saved_skill_points   = player_state.skill_points;
        u16 saved_craft_shards   = player_state.craft_shards;

        player_init((int)saved_class);

        /* Restore persistent fields wiped by player_init's memset */
        player_state.level            = saved_level;
        player_state.xp               = saved_xp;
        player_state.credits          = saved_credits;
        player_state.ability_unlocks  = saved_unlocks;
        player_state.evolution        = saved_evolution;
        player_state.evolution_pending = saved_evo_pending;
        player_state.skill_points     = saved_skill_points;
        player_state.craft_shards     = saved_craft_shards;
        for (int i = 0; i < SKILL_TREE_SIZE; i++)
            player_state.skill_tree[i] = saved_skill_tree[i];

        if (had_stats) {
            /* Subsequent jack-in: restore saved stats (includes shop upgrades) */
            player_state.max_hp = saved_max_hp;
            player_state.atk    = saved_atk;
            player_state.def    = saved_def;
            player_state.spd    = saved_spd;
            player_state.lck    = saved_lck;
        } else {
            /* First jack-in: compute base stats for this class/level */
            player_get_base_stats((int)saved_class, (int)saved_level,
                                  &player_state.max_hp, &player_state.atk,
                                  &player_state.def, &player_state.spd,
                                  &player_state.lck);
        }
        player_state.hp = player_state.max_hp; /* Full heal on jack-in */
    }
    Entity* player = player_get();
    if (player) {
        player->x = (s32)level_data.spawn_x * 8 * 256; /* tile -> 8.8 px */
        player->y = (s32)level_data.spawn_y * 8 * 256;
    }

    /* Setup camera */
    camera_init(&cam, 32, 24);
    if (player) {
        cam.x = player->x - (SCREEN_WIDTH / 2) * 256;
        cam.y = player->y - (SCREEN_HEIGHT / 2) * 256;
        cam.target_x = cam.x;
        cam.target_y = cam.y;
    }

    /* Load visible columns */
    networld_load_visible(cam.x >> 8);

    /* Setup BG registers for Net level */
    REG_BG1CNT = BG_PRIO(1) | BG_CBB(1) | BG_SBB(28) | BG_REG_64x32;
    REG_BG2CNT = BG_PRIO(2) | BG_CBB(2) | BG_SBB(30) | BG_REG_32x32;

    /* Spawn enemies */
    spawn_enemies(tier);

    /* Spawn boss based on contract type */
    boss_init();
    contract_boss_defeated = 0;
    if (c) {
        if (c->type == CONTRACT_STORY && quest_is_boss_mission(c->story_mission)) {
            /* Story boss matches act (mission 4,8,12,16,20 → boss 0,1,2,3,4) */
            int boss_type = c->story_mission / MISSIONS_PER_ACT - 1;
            if (boss_type >= 0 && boss_type < BOSS_TYPE_COUNT) {
                boss_spawn(boss_type, tier);
            }
        } else if (c->type == CONTRACT_BOUNTY) {
            /* Bounty: spawn a mini-boss (random type scaled to tier) */
            int boss_type = (int)rand_range(BOSS_TYPE_COUNT);
            boss_spawn(boss_type, tier);
        }

        /* Apply bug bounty stat scaling to boss */
        if (bb_state.active && boss_state.active) {
            int bb_scale = bugbounty_stat_scale(bb_state.tier);
            boss_state.hp = (s16)((boss_state.hp * bb_scale) >> 8);
            boss_state.max_hp = boss_state.hp;
            boss_state.atk = (s16)((boss_state.atk * bb_scale) >> 8);
        }
    }

    /* Exploration item drops — 1-3 pickups placed across the level */
    {
        int num_pickups = 1 + (int)(seed % 3); /* 1-3 based on seed */
        for (int i = 0; i < num_pickups; i++) {
            /* Pick a spawn point in the middle sections of the level */
            int sp_idx = (int)level_data.num_spawns / 2 + i;
            if (sp_idx >= (int)level_data.num_spawns) sp_idx = (int)level_data.num_spawns - 1;
            if (sp_idx < 0) break;
            int itx = level_data.spawn_points[sp_idx][0];
            int ity = level_data.spawn_points[sp_idx][1];
            s32 ix = (s32)itx * 8 * 256;
            s32 iy = (s32)ity * 8 * 256;
            /* Spawn a random loot item at this location */
            itemdrop_roll(ix, iy, tier, 0, (int)player_state.lck);
        }
    }

    /* Start music based on act and contract type */
    if (is_boss) {
        /* Boss music: corp bosses (acts 1-3), gate bosses (acts 4-5), daemon (act 6) */
        if (act >= 6) {
            audio_play_music(MUS_BOSS_DAEMON);
        } else if (act >= 4) {
            audio_play_music(MUS_BOSS_GATE);
        } else {
            audio_play_music(MUS_BOSS_CORP);
        }
    } else {
        /* Per-act gameplay music */
        static const u8 act_music[] = {
            MUS_ACT1,  /* freelance (act 0) */
            MUS_ACT1,  /* act 1 */
            MUS_ACT2,  /* act 2 */
            MUS_ACT3,  /* act 3 */
            MUS_ACT4,  /* act 4 */
            MUS_ACT5,  /* act 5 */
            MUS_ACT6,  /* act 6 */
        };
        int mi = act;
        if (mi < 0) mi = 0;
        if (mi > 6) mi = 6;
        audio_play_music(act_music[mi]);
    }

    /* Start with fade-in */
    sub_state = NETSUB_FADEIN;
    timer = 16;
    level_complete = 0;
    pause_cursor = 0;
    wave_timer = WAVE_INTERVAL;

    REG_BLDCNT = BLD_BUILD(BLD_BG0 | BLD_BG1 | BLD_BG2 | BLD_OBJ, 0, BLD_BLACK);
    REG_BLDY = 16;
}

static void update_play(void) {
    /* Pause check */
    if (input_hit(KEY_START)) {
        sub_state = NETSUB_PAUSE;
        pause_cursor = 0;
        audio_play_sfx(SFX_MENU_SELECT);
        return;
    }

    /* Player update */
    player_update();

    /* Check for death — SFX_PLAYER_DIE + shake already fired in player_take_damage */
    if (!player_is_alive()) {
        sub_state = NETSUB_DEATH;
        timer = 90;
        if (bb_state.active) bb_state.active = 0; /* End bounty run */
        /* Death particle explosion */
        {
            Entity* dp = player_get();
            if (dp) {
                particle_burst(dp->x, dp->y, 8, PART_BURST, 220, 20);
                particle_burst(dp->x, dp->y - FP8(4), 4, PART_STAR, 160, 24);
            }
        }
        return;
    }

    /* Bug bounty trace timer */
    if (bugbounty_update_trace()) {
        /* Traced! Instant death */
        sub_state = NETSUB_DEATH;
        timer = 90;
        hud_notify("TRACED!", 60);
        audio_play_sfx(SFX_PLAYER_DIE);
        video_shake(15, 3);
        return;
    }
    hud_set_trace(bb_state.active ? bb_state.trace_timer : 0);
    hud_set_score(bb_state.active ? bb_state.score : 0);

    /* Physics already handled in player_update via physics_update */

    /* Update enemies */
    Entity* p = player_get();
    s32 px = p ? p->x : 0;
    s32 py = p ? p->y : 0;
    enemy_update_all(px, py);

    /* Update boss */
    boss_update(px, py);

    /* Update projectiles + particles */
    projectile_update_all();
    particle_update();

    /* Check enemy projectiles vs player */
    check_projectile_vs_player();

    /* Check player projectiles vs enemies/boss */
    enemy_check_player_attack(p);
    boss_check_player_attack(p);

    /* Dynamic music intensity based on combat state */
    {
        int alive = enemy_count_alive();
        int intensity = 0;
        if (boss_state.active && boss_state.hp > 0 &&
            boss_state.hp * 100 / boss_state.max_hp < 30) {
            intensity = 2; /* Boss low HP — max intensity */
        } else if (alive >= 6 || boss_state.active) {
            intensity = 1; /* Many enemies or boss present */
        }
        audio_set_intensity(intensity);
    }

    /* Check player contact damage with enemies */
    if (p && player_is_alive() && player_state.invincible_timer == 0) {
        int hw = entity_get_high_water();
        for (int i = 0; i < hw; i++) {
            Entity* e = entity_get(i);
            if (!e || e->type != ENT_ENEMY || e->hp <= 0) continue;
            if (collision_aabb(p, e)) {
                s16 kb = (p->x > e->x) ? 128 : -128;
                player_take_damage(enemy_get_atk(e), kb, -64);
                /* SFX_PLAYER_HIT + shake handled inside player_take_damage */
                break;
            }
        }
    }

    /* Check boss contact damage */
    if (p && player_is_alive() && player_state.invincible_timer == 0 && boss_is_active()) {
        Entity* be = boss_get_entity();
        if (be && collision_aabb(p, be)) {
            s16 kb = (p->x > be->x) ? 128 : -128;
            player_take_damage(boss_state.atk, kb, -64);
            /* SFX_PLAYER_HIT + shake handled inside player_take_damage */
        }
    }

    /* Tesla grid toggle: switches every 90 frames with SFX */
    tesla_timer++;
    if (tesla_timer >= 90) {
        tesla_timer = 0;
        collision_tesla_active = !collision_tesla_active;
        if (collision_tesla_active) audio_play_sfx(SFX_TESLA_ZAP);
    }

    /* Corruption drain: 1 HP every 60 frames while on corrupt tile */
    corrupt_timer++;
    if (corrupt_timer >= 60) corrupt_timer = 0;
    if (p && player_is_alive() && corrupt_timer == 0 && player_state.invincible_timer == 0) {
        int ppx = (int)(p->x >> 8) + (int)(p->width >> 1);
        int ppy = (int)(p->y >> 8) + (int)p->height - 2;
        int tile = collision_tile_at(ppx, ppy);
        if (tile == TILE_CORRUPT) {
            player_take_damage(1, 0, 0);
            particle_spawn(p->x + FP8(6), p->y + FP8(10), 0, -32, PART_SPARK, 10);
            hud_floattext_spawn(p->x, p->y - FP8(8), 1, 0);
        }
    }

    /* Stream tile push: nudge player horizontally */
    if (p && player_is_alive()) {
        int ppx = (int)(p->x >> 8) + (int)(p->width >> 1);
        int ppy = (int)(p->y >> 8) + (int)p->height - 2;
        int tile = collision_tile_at(ppx, ppy);
        if (tile == TILE_STREAM) {
            p->x += FP8(1); /* Push right */
        }
    }

    /* Hazard tile damage (spikes, beams, tesla) */
    if (hazard_cd > 0) hazard_cd--;
    if (p && player_is_alive() && hazard_cd == 0 && player_state.invincible_timer == 0) {
        int ppx = (int)(p->x >> 8) + (int)(p->width >> 1);
        int ppy = (int)(p->y >> 8) + (int)p->height - 2;
        if (collision_point_hazard(ppx, ppy)) {
            player_take_damage(3, 0, -128);
            hazard_cd = 30; /* Half second cooldown between hazard ticks */
            /* Hazard damage particles at player feet */
            particle_burst(p->x + FP8(6), p->y + FP8(12), 4, PART_SPARK, 160, 12);
            /* Brief red BG flash for hazard pain + damage number */
            pal_bg_mem[0] = RGB15(8, 0, 0);
            hud_floattext_spawn(p->x, p->y - FP8(8), 3, 0);
            hud_notify("HAZARD!", 20);
        }
    }

    /* Track kill-based contract progress */
    enemies_killed = enemy_get_kills();
    /* Sync kill count to contract for pause menu display */
    {
        Contract* kc = quest_get_active();
        if (kc) kc->kills = (enemies_killed > 255) ? 255 : (u8)enemies_killed;
        /* Kill milestone tracking (no on-screen notification — reduces noise) */
        if (kc && kc->kill_target > 0 &&
            (kc->type == CONTRACT_EXTERMINATE || kc->type == CONTRACT_SURVIVAL)) {
            int target = kc->kill_target;
            if (enemies_killed >= target * 9 / 10) milestone_shown |= 2;
            if (enemies_killed >= target / 2) milestone_shown |= 1;
        }
    }

    /* Combo counter — detect new kills */
    {
        int new_kills = enemies_killed - prev_kills;
        prev_kills = enemies_killed;
        if (new_kills > 0) {
            combo_count += new_kills;
            combo_timer = COMBO_WINDOW;
            if (combo_count >= 3) {
                /* Show combo only for 3+ kills */
                static char combo_buf[8];
                combo_buf[0] = 'x';
                combo_buf[1] = (char)('0' + (combo_count < 10 ? combo_count : 9));
                combo_buf[2] = combo_count >= 10 ? '+' : '!';
                combo_buf[3] = '\0';
                hud_notify(combo_buf, 20);
                if (combo_count >= 3 && p) {
                    particle_burst(p->x, p->y - FP8(8), 2, PART_STAR, 160, 14);
                }
                /* Bonus XP: combo × 5 */
                player_add_xp(combo_count * 5);
                /* Combo heal: 1 HP at 5+, 2 HP at 10+ */
                if (combo_count >= 5 && player_is_alive()) {
                    int heal = (combo_count >= 10) ? 2 : 1;
                    if (player_state.hp < player_state.max_hp) {
                        player_state.hp += (s16)heal;
                        if (player_state.hp > player_state.max_hp)
                            player_state.hp = player_state.max_hp;
                    }
                }
                /* Combo 10+: 20% chance bonus loot drop */
                if (combo_count >= 10 && (int)rand_range(100) < 20 && p) {
                    int t = (int)player_state.level / 3 + 1;
                    itemdrop_roll(p->x, p->y - FP8(16), t, 1, (int)player_state.lck);
                }
                /* Track highest combo */
                if (combo_count > (int)game_stats.highest_combo)
                    game_stats.highest_combo = (u8)(combo_count > 255 ? 255 : combo_count);
            }
        }
        if (combo_timer > 0) {
            combo_timer--;
            if (combo_timer == 0) combo_count = 0;
        }
    }

    /* Section tracking — show name when entering new section */
    if (p) {
        int px_tile = (int)(p->x >> 8) / 8;
        int sec = px_tile / 16;
        if (sec != current_section && sec >= 0 && sec < NUM_SECTIONS) {
            current_section = sec;
        }
    }

    /* Wave respawning for survival/exterminate contracts */
    {
        Contract* ac = quest_get_active();
        if (ac && (ac->type == CONTRACT_SURVIVAL || ac->type == CONTRACT_EXTERMINATE)) {
            /* Only respawn if player hasn't met kill target yet */
            if (enemies_killed < ac->kill_target) {
                wave_timer--;
                if (wave_timer <= 0) {
                    /* Wave escalation: after 90s, spawn faster and more */
                    int escalated = (mission_frames > 5400) ? 1 : 0;
                    wave_timer = escalated ? (WAVE_INTERVAL * 2 / 3) : WAVE_INTERVAL;
                    int alive = enemy_count_alive();
                    if (alive < WAVE_MIN_ALIVE + (escalated ? 2 : 0)) {
                        /* Spawn 2-4 (or 3-5 escalated) enemies near player */
                        int spawn_count = 2 + escalated + (int)rand_range(3);
                        int ptx = p ? (int)(p->x >> 8) / 8 : 64;
                        int tier = ac->tier;
                        int bb_scale = bb_state.active
                                     ? bugbounty_stat_scale(bb_state.tier) : 256;
                        for (int w = 0; w < spawn_count; w++) {
                            /* Spawn offset: 6-12 tiles ahead of player in random direction */
                            int off = 6 + (int)rand_range(7);
                            int stx = ptx + ((rand_range(2) == 0) ? off : -off);
                            /* Clamp to map bounds */
                            if (stx < 2) stx = 2;
                            if (stx > NET_MAP_W - 3) stx = NET_MAP_W - 3;
                            /* Find ground level at spawn X by scanning down */
                            int sty = 27;
                            for (int sy = 10; sy < NET_MAP_H - 1; sy++) {
                                if (level_data.collision[sy * NET_MAP_W + stx] == TILE_SOLID) {
                                    sty = sy - 1;
                                    break;
                                }
                            }
                            /* Pick enemy type — use BB composition if active */
                            int roll = (int)rand_range(100);
                            int subtype;
                            if (bb_state.active) {
                                static const u8 bb_comp[BB_TIER_COUNT][6] = {
                                    { 15, 30, 40, 55, 65, 100 },
                                    { 10, 20, 30, 50, 60, 100 },
                                    {  5, 15, 20, 40, 55, 100 },
                                    {  0,  5, 10, 35, 55, 100 },
                                    {  0,  0,  0,  0,  0, 100 },
                                };
                                int bt = bb_state.tier;
                                if (bt >= BB_TIER_COUNT) bt = BB_TIER_COUNT - 1;
                                if (roll < bb_comp[bt][0]) subtype = ENEMY_SENTRY;
                                else if (roll < bb_comp[bt][1]) subtype = ENEMY_PATROL;
                                else if (roll < bb_comp[bt][2]) subtype = ENEMY_FLYER;
                                else if (roll < bb_comp[bt][3]) subtype = ENEMY_SHIELD;
                                else if (roll < bb_comp[bt][4]) subtype = ENEMY_SPIKE;
                                else subtype = ENEMY_HUNTER;
                            } else if (tier <= 2) {
                                if (roll < 40) subtype = ENEMY_SENTRY;
                                else if (roll < 80) subtype = ENEMY_PATROL;
                                else subtype = ENEMY_FLYER;
                            } else if (tier <= 4) {
                                if (roll < 20) subtype = ENEMY_SENTRY;
                                else if (roll < 40) subtype = ENEMY_PATROL;
                                else if (roll < 55) subtype = ENEMY_FLYER;
                                else if (roll < 75) subtype = ENEMY_SHIELD;
                                else subtype = ENEMY_HUNTER;
                            } else {
                                if (roll < 10) subtype = ENEMY_SENTRY;
                                else if (roll < 25) subtype = ENEMY_PATROL;
                                else if (roll < 35) subtype = ENEMY_FLYER;
                                else if (roll < 50) subtype = ENEMY_SHIELD;
                                else if (roll < 60) subtype = ENEMY_SPIKE;
                                else subtype = ENEMY_HUNTER;
                            }
                            Entity* we = enemy_spawn(subtype, stx, sty, tier);
                            if (!we) break; /* Entity pool exhausted */
                            /* Per-section difficulty scaling for wave spawns */
                            {
                                int section = stx / 16;
                                if (section > 3) {
                                    int sec_scale = 256 + (section / 4) * 26;
                                    we->hp = (s16)((we->hp * sec_scale) >> 8);
                                    enemy_scale_atk(we, sec_scale);
                                }
                            }
                            if (bb_scale != 256) {
                                we->hp = (s16)((we->hp * bb_scale) >> 8);
                                enemy_scale_atk(we, bb_scale);
                            }
                        }
                    }
                }
            }
        }
    }

    /* Update HUD kill objective for exterminate/survival contracts */
    {
        Contract* ac = quest_get_active();
        if (ac && (ac->type == CONTRACT_EXTERMINATE || ac->type == CONTRACT_SURVIVAL)) {
            hud_set_objective(enemies_killed, ac->kill_target);
        } else {
            hud_set_objective(0, 0);
        }
    }

    /* Track boss defeat for contract exit condition */
    if (boss_state.defeated && !contract_boss_defeated) {
        contract_boss_defeated = 1;
        hud_notify("BOSS DEFEATED!", 60);
        audio_play_music(MUS_VICTORY);
        /* Clear stage 3 ambient darkening */
        REG_BLDCNT = 0;
        REG_BLDY = 0;
    }

    /* Update item drops */
    itemdrop_update_all();

    /* Check item pickups */
    if (p) {
        itemdrop_check_pickup(p->x, p->y);
    }

    /* Ability updates handled inside player_update() */

    /* Boss approach warning — show WARNING when nearing arena */
    if (!boss_warning_shown && !boss_intro_done && boss_is_active() && p) {
        int px_tile = (int)(p->x >> 8) / 8;
        int arena_start = (NUM_SECTIONS - 1) * 16;
        if (px_tile >= arena_start - 32) { /* 2 sections before arena */
            boss_warning_shown = 1;
            hud_notify("WARNING", 45);
            audio_play_sfx(SFX_BOSS_PHASE);
        }
    }

    /* Boss intro cinematic — trigger when player enters boss arena section */
    if (!boss_intro_done && boss_is_active() && p) {
        int px_tile = (int)(p->x >> 8) / 8;
        int arena_start = (NUM_SECTIONS - 1) * 16;
        if (px_tile >= arena_start - 2) {
            boss_intro_done = 1;
            sub_state = NETSUB_BOSS_INTRO;
            timer = 80; /* 80-frame cinematic */
            video_shake(4, 2);
            audio_play_sfx(SFX_BOSS_PHASE);
            return; /* Don't process rest of update this frame */
        }
    }

    /* Check exit — contract conditions must be met */
    if (check_exit() && !level_complete) {
        if (quest_check_exit_condition(enemies_killed, contract_boss_defeated)) {
            level_complete = 1;
            sub_state = NETSUB_EXIT_ANIM;
            timer = 90; /* Extended for stats display */
            audio_play_sfx(SFX_LEVEL_DONE);
            hud_notify("LEVEL CLEAR!", 60);

            /* Mission achievement checks */
            /* Speed Run: complete in < 2 minutes (7200 frames) */
            if (mission_frames < 7200) {
                ach_unlock_celebrate(ACH_SPEED_RUN);
            }
            /* Untouchable: no damage taken */
            if (game_stats.damage_taken == mission_start_dmg_taken) {
                ach_unlock_celebrate(ACH_UNTOUCHABLE);
            }
            /* Pacifist: 0 kills on retrieval contract */
            {
                Contract* pac = quest_get_active();
                if (pac && pac->type == CONTRACT_RETRIEVAL && enemies_killed == 0) {
                    ach_unlock_celebrate(ACH_PACIFIST);
                }
            }
            /* True Ghost: complete Act 5 mission without any enemy entering chase */
            if (quest_state.current_act == 5 && enemy_get_chase_count() == 0) {
                ach_unlock_celebrate(ACH_TRUE_GHOST);
            }
            video_flash_start(2, 8);
            video_shake(3, 1);
            /* Celebration burst from player position */
            {
                Entity* cp = player_get();
                if (cp) {
                    s32 cx = cp->x + ((s32)cp->width << 7);
                    s32 cy = cp->y + ((s32)cp->height << 7);
                    particle_burst(cx, cy - FP8(4), 4, PART_STAR, 200, 18);
                }
            }
        } else {
            /* Show reason exit is blocked */
            Contract* ac = quest_get_active();
            if (ac && ac->type == CONTRACT_EXTERMINATE) {
                hud_notify("KILL MORE!", 30);
            } else if (ac && ac->type == CONTRACT_SURVIVAL) {
                hud_notify("SURVIVE!", 30);
            } else {
                hud_notify("DEFEAT BOSS!", 30);
            }
        }
    }

    /* Camera follow player */
    Entity* player = player_get();
    if (player) {
        camera_update(&cam, player->x, player->y);
    }

    /* Clamp camera to level bounds */
    int max_cam_x = (NET_MAP_W * 8 - SCREEN_WIDTH) * 256;
    int max_cam_y = (NET_MAP_H * 8 - SCREEN_HEIGHT) * 256;
    if (cam.x < 0) cam.x = 0;
    if (cam.y < 0) cam.y = 0;
    if (cam.x > max_cam_x) cam.x = max_cam_x;
    if (cam.y > max_cam_y) cam.y = max_cam_y;

    /* Stream columns */
    networld_stream_columns(cam.x >> 8);

    /* Pass camera position to HUD for section minimap + floattext */
    hud_set_camera_x(cam.x);
    hud_set_camera(cam.x, cam.y);

    /* Ambient BG palette cycling per section type */
    ambient_timer++;
    mission_frames++;
    {
        /* Determine which section the camera center is in */
        int center_px = (cam.x >> 8) + SCREEN_WIDTH / 2;
        int sec_idx = center_px / (16 * 8); /* 16 tiles × 8px per section */
        if (sec_idx < 0) sec_idx = 0;
        if (sec_idx >= NUM_SECTIONS) sec_idx = NUM_SECTIONS - 1;
        int sec_type = level_data.sections[sec_idx];

        /* Triangle wave: 0→15→0 over 32 frames */
        int wave = ambient_timer & 31;
        int pulse = (wave < 16) ? wave : (31 - wave); /* 0-15-0 */

        switch (sec_type) {
        case SECT_SECURITY:
            /* Red hazard pulse on palette index 10 (hazard dim) */
            pal_bg_mem[1 * 16 + 10] = (u16)RGB15(24 + pulse / 2, 4, 2);
            pal_bg_mem[1 * 16 + 11] = (u16)RGB15(31, 10 + pulse / 2, 6);
            break;
        case SECT_WATERFALL:
            /* Flowing blue shift on circuit glow entries */
            pal_bg_mem[1 * 16 + 8] = (u16)RGB15(0, 16 + pulse / 2, 24 + pulse / 4);
            pal_bg_mem[1 * 16 + 14] = (u16)RGB15(6, 10 + pulse, 18 + pulse / 2);
            break;
        case SECT_CACHE:
            /* Golden warmth pulse on accent entries */
            pal_bg_mem[1 * 16 + 12] = (u16)RGB15(22 + pulse / 2, 18 + pulse / 3, 4);
            pal_bg_mem[1 * 16 + 13] = (u16)RGB15(31, 28, 10 + pulse / 3);
            break;
        case SECT_BOSS:
            /* Ominous slow pulse on circuit glow */
            pal_bg_mem[1 * 16 + 9] = (u16)RGB15(4 + pulse / 3, 28 - pulse / 2, 31);
            break;
        case SECT_GAUNTLET:
            /* Aggressive orange pulse on hazard entries */
            pal_bg_mem[1 * 16 + 10] = (u16)RGB15(24 + pulse / 3, 8 + pulse / 2, 2);
            pal_bg_mem[1 * 16 + 11] = (u16)RGB15(31, 14 + pulse / 3, 4);
            break;
        default:
            /* Subtle circuit glow breathe on all other sections */
            if ((ambient_timer & 3) == 0) {
                int slow = (ambient_timer >> 2) & 31;
                int sp = (slow < 16) ? slow : (31 - slow);
                pal_bg_mem[1 * 16 + 9] = (u16)RGB15(4, 28 + sp / 5, 31);
            }
            break;
        }

        /* Ambient section particles — spawn at random visible X */
        if ((ambient_timer & 15) == 0) { /* Every 16 frames */
            s32 rx = cam.x + (s32)((int)rand_range(SCREEN_WIDTH) * 256);
            s32 top_y = cam.y;
            s32 bot_y = cam.y + FP8(SCREEN_HEIGHT);
            switch (sec_type) {
            case SECT_WATERFALL:
                /* Falling data droplets from top */
                particle_spawn(rx, top_y, 0, 128, PART_SPARK, 30);
                break;
            case SECT_SECURITY:
                /* Red spark flicker at random position */
                particle_spawn(rx, top_y + (s32)((int)rand_range(SCREEN_HEIGHT) * 256),
                               (s16)((int)rand_range(65) - 32), (s16)((int)rand_range(65) - 32),
                               PART_SPARK, 12);
                break;
            case SECT_CACHE:
                /* Golden dust motes rising from floor */
                particle_spawn(rx, bot_y - FP8(8), (s16)((int)rand_range(33) - 16), -64, PART_STAR, 28);
                break;
            case SECT_BOSS:
                /* Ominous dim sparkles */
                particle_spawn(rx, top_y + (s32)((int)rand_range(SCREEN_HEIGHT) * 256),
                               0, -16, PART_STAR, 24);
                break;
            case SECT_GAUNTLET:
                /* Embers rising */
                particle_spawn(rx, bot_y, (s16)((int)rand_range(33) - 16), -96, PART_BURST, 20);
                break;
            case SECT_NETWORK:
                /* Data streams: fast vertical spark trails */
                particle_spawn(rx, top_y, 0, 200, PART_SPARK, 20);
                break;
            case SECT_TRANSIT:
                /* Floating energy motes drifting horizontally */
                particle_spawn(cam.x - FP8(8),
                               top_y + (s32)((int)rand_range(SCREEN_HEIGHT) * 256),
                               96, (s16)((int)rand_range(33) - 16), PART_STAR, 30);
                break;
            default:
                /* Subtle ambient: very occasional faint sparkle */
                if ((ambient_timer & 31) == 0) {
                    particle_spawn(rx, top_y + (s32)((int)rand_range(SCREEN_HEIGHT) * 256),
                                   0, -8, PART_STAR, 20);
                }
                break;
            }
        }
    }

    /* Boss fight vignette: darken parallax BG when boss is active */
    if (boss_is_active()) {
        /* Pulsing dark purple tint — ominous atmosphere */
        int bwave = ambient_timer & 63;
        int bpulse = (bwave < 32) ? bwave / 4 : (63 - bwave) / 4; /* 0-8-0 */
        pal_bg_mem[6 * 16 + 0] = (u16)RGB15(2 + bpulse / 3, 0, 2 + bpulse / 4);
    }

    /* Screen edge danger indicator — tint parallax BG red when enemies near edge */
    {
        int danger = 0; /* 0=safe, 1-4 = intensity */
        int hw = entity_get_high_water();
        int scr_l = cam.x >> 8;
        int scr_r = scr_l + SCREEN_WIDTH;
        int scr_t = cam.y >> 8;
        int scr_b = scr_t + SCREEN_HEIGHT;
        for (int i = 0; i < hw; i++) {
            Entity* de = entity_get(i);
            if (!de || de->type != ENT_ENEMY || de->hp <= 0) continue;
            int ex = (int)(de->x >> 8);
            int ey = (int)(de->y >> 8);
            /* Check if enemy is just off-screen (within 24px of edge) */
            if ((ex > scr_l - 24 && ex < scr_l) ||
                (ex > scr_r && ex < scr_r + 24) ||
                (ey > scr_t - 24 && ey < scr_t) ||
                (ey > scr_b && ey < scr_b + 24)) {
                danger++;
                if (danger >= 3) break;
            }
        }
        /* Apply subtle red tint to parallax BG palette entry 1 */
        if (danger > 0) {
            int r = 4 + danger * 3; /* 7-13 red component */
            pal_bg_mem[6 * 16 + 1] = (u16)RGB15(r, 2, 4);
        } else {
            /* Restore normal parallax color */
            pal_bg_mem[6 * 16 + 1] = (u16)RGB15(4, 4, 8);
        }
    }

    /* Per-act ambient BG tint + low HP warning */
    {
        /* Subtle per-act base tint for pal_bg_mem[0] (void/background color) */
        static const u16 act_bg_tint[7] = {
            RGB15_C(1, 1, 2),   /* Freelance: faint blue */
            RGB15_C(2, 0, 1),   /* Act 1 Glitch: dark magenta */
            RGB15_C(0, 1, 2),   /* Act 2 Traceback: cool blue */
            RGB15_C(1, 2, 1),   /* Act 3 Deep Packet: organic green */
            RGB15_C(2, 1, 0),   /* Act 4 Zero Day: decay amber */
            RGB15_C(1, 1, 1),   /* Act 5 Ghost Protocol: near-black grey */
            RGB15_C(2, 0, 2),   /* Act 6 Trace Route: chaotic purple */
        };
        int tint_act = (int)quest_state.current_act;
        if (tint_act < 0) tint_act = 0;
        if (tint_act > 6) tint_act = 6;

        if (player_is_alive() && player_state.hp > 0 &&
            player_state.hp <= player_state.max_hp / 4) {
            /* Pulsing red tint on BG0 palette entry 0 (background color) */
            int beat = ambient_timer & 23; /* 24-frame cycle */
            int pulse = (beat < 12) ? beat : (23 - beat); /* 0→12→0 triangle */
            pal_bg_mem[0] = (u16)RGB15(pulse / 2, 0, 0);
        } else {
            pal_bg_mem[0] = act_bg_tint[tint_act];
        }
    }

    /* Update HUD notification */
    hud_notify_update();
    hud_floattext_update();
}

void state_net_update(void) {
    switch (sub_state) {
    case NETSUB_FADEIN:
        timer--;
        REG_BLDY = (u16)timer;
        if (timer <= 0) {
            sub_state = NETSUB_PLAY;
            REG_BLDCNT = 0;
            REG_BLDY = 0;
        }
        break;

    case NETSUB_PLAY:
        update_play();
        break;

    case NETSUB_BOSS_INTRO:
    {
        timer--;
        video_shake_update();
        particle_update();

        Entity* be = boss_get_entity();

        /* Phase 1 (frames 80-50): screen darkens, ominous particles from boss */
        if (timer > 50 && be) {
            if ((timer & 3) == 0) {
                particle_burst(be->x + ((s32)be->width << 7),
                               be->y + ((s32)be->height << 7),
                               2, PART_STAR, 200, 18);
                particle_spawn(be->x + ((s32)be->width << 7),
                               be->y - FP8(4), 0, -80, PART_SPARK, 14);
            }
            /* Darken BG — increasing intensity */
            int dark = (80 - timer) / 6; /* 0→5 over 30 frames */
            if (dark > 6) dark = 6;
            REG_BLDCNT = BLD_BLACK | BLD_BG1 | BLD_BG2;
            REG_BLDY = (u16)(dark);
        }
        /* Phase 2 (frames 50-20): boss name reveal + dramatic flash */
        else if (timer > 20) {
            if (timer == 50) {
                static const char* const boss_names[BOSS_TYPE_COUNT] = {
                    "< FIREWALL >", "< BLACKOUT >", "< WORM >",
                    "< NEXUS >", "< ROOT ACCESS >", "< DAEMON >"
                };
                int bt = (int)boss_state.type;
                if (bt >= 0 && bt < BOSS_TYPE_COUNT) {
                    hud_notify(boss_names[bt], 45);
                }
                audio_play_sfx(SFX_BOSS_EXPLODE);
                video_shake(6, 2);
                video_flash_start(1, 4);
                if (be) {
                    s32 bcx = be->x + ((s32)be->width << 7);
                    s32 bcy = be->y + ((s32)be->height << 7);
                    particle_burst(bcx, bcy, 4, PART_BURST, 250, 16);
                }
            }
            if (be && (timer & 5) == 0) {
                particle_burst(be->x + ((s32)be->width << 7),
                               be->y - FP8(8), 2, PART_SPARK, 160, 12);
            }
            REG_BLDCNT = BLD_BLACK | BLD_BG1 | BLD_BG2;
            int fade = (timer - 20) * 4 / 30; /* 4→0 over 30 frames */
            REG_BLDY = (u16)(fade > 0 ? fade : 0);
        }
        /* Phase 3 (frames 20-0): fade back to normal */
        else {
            REG_BLDCNT = 0;
            REG_BLDY = 0;
        }

        if (timer <= 0) {
            REG_BLDCNT = 0;
            REG_BLDY = 0;
            sub_state = NETSUB_PLAY;
        }
        break;
    }

    case NETSUB_EXIT_ANIM:
        timer--;
        hud_notify_update();
        particle_update();

        /* Celebration particles — burst from player every 8 frames */
        {
            Entity* ep = player_get();
            if (ep && (timer & 7) == 0) {
                particle_burst(ep->x, ep->y - FP8(4), 3, PART_STAR, 200, 20);
                particle_burst(ep->x, ep->y, 2, PART_SPARK, 160, 12);
            }
        }

        /* Mission stats display (frames 60-20) */
        if (timer <= 60 && timer > 20) {
            if (timer == 60) {
                /* Draw stats on BG0 */
                int sec = mission_frames / 60;
                int min = sec / 60;
                sec = sec % 60;
                text_clear_rect(6, 6, 18, 7);
                text_print(7, 6, "-- STATS --");
                {
                    char buf[20];
                    /* Kills */
                    buf[0] = 'K'; buf[1] = 'i'; buf[2] = 'l'; buf[3] = 'l'; buf[4] = 's'; buf[5] = ':'; buf[6] = ' ';
                    text_print_int(14, 7, enemies_killed);
                    text_print(7, 7, "Kills:");
                    /* Time */
                    buf[0] = 'T'; buf[1] = 'i'; buf[2] = 'm'; buf[3] = 'e'; buf[4] = ':'; buf[5] = ' ';
                    buf[6] = (char)('0' + min);
                    buf[7] = ':';
                    buf[8] = (char)('0' + sec / 10);
                    buf[9] = (char)('0' + sec % 10);
                    buf[10] = '\0';
                    text_print(7, 8, buf);
                    /* Damage */
                    text_print(7, 9, "Dmg:");
                    text_print_int(14, 9, (int)game_stats.damage_dealt);
                    /* Combos */
                    text_print(7, 10, "Combo:");
                    text_print_int(14, 10, (int)game_stats.highest_combo);
                    /* Credits earned */
                    {
                        Contract* sc = quest_get_active();
                        if (sc) {
                            text_print(7, 11, "Credits:");
                            text_print_int(16, 11, (int)sc->reward_credits);
                        }
                    }
                }
            }
        }

        /* Fade to white in last 20 frames */
        if (timer < 20 && timer > 0) {
            REG_BLDCNT = BLD_WHITE | BLD_ALL;
            REG_BLDY = (u16)(16 - (timer * 16 / 20));
        }

        if (timer <= 0) {
            /* Complete contract */
            Contract* c = quest_get_active();
            int was_final_act = 0;
            if (c) {
                /* Check if this is act 6 story completion (DAEMON defeated) */
                if (c->type == CONTRACT_STORY && c->story_mission == STORY_MISSIONS) {
                    was_final_act = 1;
                }
                /* Award credits (saturate to u16 max) */
                {
                    u32 total = (u32)player_state.credits + c->reward_credits;
                    player_state.credits = (total > 0xFFFF) ? (u16)0xFFFF : (u16)total;
                }
                /* Award XP — scales with tier and mission for meaningful progression */
                {
                    int xp = 30 + c->tier * 15;
                    if (c->story_mission > 0) xp += c->story_mission * 10;
                    player_add_xp(xp);
                }
                quest_complete_active();
            }
            /* Complete bug bounty run (updates high score) */
            if (bb_state.active) {
                bugbounty_complete();
            }
            /* Clear blend registers before transition */
            REG_BLDCNT = 0;
            REG_BLDY = 0;
            /* Final act → win screen, otherwise back to terminal */
            if (was_final_act) {
                game_request_state = STATE_WIN;
            } else {
                game_request_state = STATE_TERMINAL;
            }
        }
        break;

    case NETSUB_DEATH:
        timer--;
        video_shake_update();
        video_hit_flash_update();
        particle_update();

        /* Mosaic dissolve on player sprite (frames 90-50) */
        if (timer > 50) {
            int moz = (90 - timer) / 3; /* 0→13 over 40 frames */
            if (moz > 15) moz = 15;
            video_mosaic_obj(moz);
        }
        /* Death stats display (frames 45-15) */
        if (timer <= 45 && timer > 15) {
            if (timer == 45) {
                int sec = mission_frames / 60;
                int min = sec / 60;
                sec %= 60;
                text_clear_rect(7, 7, 16, 5);
                text_print(9, 7, "- DEFEATED -");
                text_print(8, 9, "Kills:");
                text_print_int(15, 9, enemies_killed);
                text_print(8, 10, "Time:");
                {
                    char buf[8];
                    buf[0] = (char)('0' + min);
                    buf[1] = ':';
                    buf[2] = (char)('0' + sec / 10);
                    buf[3] = (char)('0' + sec % 10);
                    buf[4] = '\0';
                    text_print(14, 10, buf);
                }
                text_print(8, 11, "Dmg dealt:");
                text_print_int(19, 11, (int)game_stats.damage_dealt);
            }
        }
        /* Fade to dark red tint (frames 50-0) */
        if (timer <= 50 && timer > 0) {
            REG_BLDCNT = BLD_BLACK | BLD_ALL;
            REG_BLDY = (u16)((50 - timer) * 12 / 50); /* 0→12 */
            /* Tint BG palette toward red */
            if ((timer & 7) == 0) {
                int r_boost = (50 - timer) / 6; /* 0→8 */
                pal_bg_mem[0] = (u16)RGB15(r_boost, 0, 0);
            }
        }

        if (timer <= 0) {
            REG_BLDCNT = 0;
            REG_BLDY = 0;
            video_mosaic_obj(0);
            game_request_state = STATE_GAMEOVER;
        }
        break;

    case NETSUB_PAUSE:
        video_shake_update(); /* Decay screen shake even while paused */
        if (input_hit(KEY_DOWN)) {
            pause_cursor++;
            if (pause_cursor > 1) pause_cursor = 0;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_UP)) {
            pause_cursor--;
            if (pause_cursor < 0) pause_cursor = 1;
            audio_play_sfx(SFX_MENU_SELECT);
        }
        if (input_hit(KEY_A) || input_hit(KEY_START)) {
            if (pause_cursor == 0) {
                /* Resume */
                sub_state = NETSUB_PLAY;
                text_clear_rect(1, 6, 28, 12);
            } else {
                /* Disconnect */
                game_request_state = STATE_TERMINAL;
            }
        }
        if (input_hit(KEY_B)) {
            sub_state = NETSUB_PLAY;
            text_clear_rect(1, 6, 28, 12);
        }
        break;
    }
}

void state_net_draw(void) {
    /* Set BG scroll from camera (includes parallax + shake) */
    video_scroll_parallax(cam.x, cam.y);

    /* Draw player */
    player_draw(cam.x, cam.y);

    /* Draw enemies */
    enemy_draw_all(cam.x, cam.y);

    /* Draw boss */
    boss_draw(cam.x, cam.y);

    /* Draw projectiles + particles */
    projectile_draw_all(cam.x, cam.y);
    particle_draw(cam.x, cam.y);

    /* Draw item drops */
    itemdrop_draw_all(cam.x, cam.y);

    /* Boss stage 3 ambient darkening: slight BG dim for atmosphere */
    if (sub_state == NETSUB_PLAY && boss_state.active && boss_state.stage >= 3 &&
        boss_state.phase != BPHASE_DEAD && boss_state.phase != BPHASE_TRANSITION) {
        /* Pulse between dark 2-4 on 16-frame cycle */
        int pulse = (ambient_timer >> 2) & 3;
        int dark = 2 + (pulse < 2 ? pulse : (3 - pulse)); /* 2-3-4-3 wave */
        REG_BLDCNT = BLD_BLACK | BLD_BG1 | BLD_BG2;
        REG_BLDY = (u16)dark;
    }

    /* Update visual transitions */
    video_transition_update();

    /* HUD always on top */
    hud_draw();
    hud_floattext_draw();

    /* Combo multiplier display on row 3 — persistent during active combo */
    if (combo_count >= 2 && combo_timer > 0) {
        /* Blink faster as timer runs low */
        int show = 1;
        if (combo_timer < 60) show = (combo_timer >> 2) & 1; /* Fast blink last second */
        if (show) {
            text_print(0, 3, "COMBO");
            text_put_char(5, 3, ' ');
            text_put_char(6, 3, 'x');
            if (combo_count >= 10) {
                text_print_int(7, 3, combo_count);
            } else {
                text_put_char(7, 3, (char)('0' + combo_count));
            }
            /* XP bonus indicator */
            text_put_char(combo_count >= 10 ? 9 : 8, 3, '+');
        } else {
            text_clear_rect(0, 3, 10, 1);
        }
    } else {
        text_clear_rect(0, 3, 10, 1);
    }

    /* Mission type info removed from HUD — reduces screen noise */

    /* Pause overlay */
    if (sub_state == NETSUB_PAUSE) {
        text_print(9, 7, "=== PAUSED ===");
        text_print(10, 9, (pause_cursor == 0) ? "> Resume" : "  Resume");
        text_print(10, 11, (pause_cursor == 1) ? "> Disconnect" : "  Disconnect");

        /* Player stats on pause */
        text_print(2, 13, "Lv");
        text_print_int(4, 13, player_state.level);
        text_print(10, 13, "CR:");
        text_print_int(13, 13, player_state.credits);

        /* Contract progress */
        Contract* pac = quest_get_active();
        if (pac) {
            text_print(2, 15, quest_get_type_name(pac->type));
            if (pac->type == CONTRACT_EXTERMINATE || pac->type == CONTRACT_SURVIVAL) {
                text_print(2, 16, "Kills:");
                text_print_int(9, 16, pac->kills);
                text_put_char(12, 16, '/');
                text_print_int(13, 16, pac->kill_target);
            }
        }
    }

    /* Low HP danger pulse — blink red warning at screen edges */
    if (sub_state == NETSUB_PLAY && player_is_alive() &&
        player_state.max_hp > 0 &&
        player_state.hp * 4 <= player_state.max_hp) {
        /* Flash "!!" on both sides of row 9 every 8 frames */
        if ((ambient_timer >> 3) & 1) {
            text_put_char(0, 8, '!');
            text_put_char(1, 8, '!');
            text_put_char(28, 8, '!');
            text_put_char(29, 8, '!');
        } else {
            text_put_char(0, 8, ' ');
            text_put_char(1, 8, ' ');
            text_put_char(28, 8, ' ');
            text_put_char(29, 8, ' ');
        }
    }

    /* Offscreen enemy proximity arrows (during gameplay only) */
    if (sub_state == NETSUB_PLAY) {
        int left_threat = 0, right_threat = 0;
        int scr_left = (int)(cam.x >> 8);
        int scr_right = scr_left + SCREEN_WIDTH;
        int hw = entity_get_high_water();
        for (int i = 0; i < hw && (!left_threat || !right_threat); i++) {
            Entity* e = entity_get(i);
            if (!e || e->type != ENT_ENEMY || e->hp <= 0) continue;
            int ex = (int)(e->x >> 8);
            if (ex < scr_left - 48) continue;
            if (ex > scr_right + 48) continue;
            if (ex < scr_left) left_threat = 1;
            if (ex >= scr_right) right_threat = 1;
        }
        /* Draw arrows — row 9 edges (only when not used by damage direction) */
        if (left_threat && (ambient_timer & 4)) {
            text_put_char(0, 9, '<');
        }
        if (right_threat && (ambient_timer & 4)) {
            text_put_char(29, 9, '>');
        }
    }

    /* Death overlay */
    if (sub_state == NETSUB_DEATH) {
        if (timer < 60) {
            text_print(7, 8, "CONNECTION LOST");
            text_print(9, 10, "Lv:");
            text_print_int(12, 10, player_state.level);
            Contract* dac = quest_get_active();
            if (dac) {
                text_print(9, 11, quest_get_type_name(dac->type));
                text_print(9, 12, "Kills:");
                text_print_int(16, 12, dac->kills);
            }
        }
    }
}

void state_net_exit(void) {
    text_clear_all();
    projectile_clear_all();
    particle_clear();
    itemdrop_clear_all();

    /* Hide all OAM sprites so they don't bleed into non-gameplay states */
    for (int i = 0; i < SPRITE_MAX; i++) {
        OBJ_ATTR* spr = sprite_get(i);
        if (spr) obj_hide(spr);
    }

    /* Reset music tempo and blend registers */
    audio_set_intensity(0);
    collision_tesla_active = 0;
    REG_BLDCNT = 0;
    REG_BLDY = 0;

    /* Reset BG scroll */
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    REG_BG2HOFS = 0;
    REG_BG2VOFS = 0;
}
