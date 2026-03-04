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
#include "states/state_ids.h"
#include "states/state_net.h"

/* Net sub-states */
enum {
    NETSUB_FADEIN = 0,
    NETSUB_PLAY,
    NETSUB_EXIT_ANIM,
    NETSUB_DEATH,
    NETSUB_PAUSE,
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
        } else if (tier <= 2) {
            /* Easy: mostly sentries and patrols */
            if (roll < 40) subtype = ENEMY_SENTRY;
            else if (roll < 80) subtype = ENEMY_PATROL;
            else subtype = ENEMY_FLYER;
        } else if (tier <= 4) {
            /* Medium: add shields and hunters */
            if (roll < 20) subtype = ENEMY_SENTRY;
            else if (roll < 40) subtype = ENEMY_PATROL;
            else if (roll < 55) subtype = ENEMY_FLYER;
            else if (roll < 75) subtype = ENEMY_SHIELD;
            else subtype = ENEMY_HUNTER;
        } else {
            /* Hard: all types, weighted toward dangerous ones */
            if (roll < 10) subtype = ENEMY_SENTRY;
            else if (roll < 25) subtype = ENEMY_PATROL;
            else if (roll < 35) subtype = ENEMY_FLYER;
            else if (roll < 50) subtype = ENEMY_SHIELD;
            else if (roll < 60) subtype = ENEMY_SPIKE;
            else subtype = ENEMY_HUNTER;
        }

        Entity* e = enemy_spawn(subtype, tx, ty, tier);

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
    hud_init();
    /* NOTE: Do NOT call inventory_init() here — it wipes persistent inventory */
    itemdrop_init();

    /* Generate level */
    levelgen_generate(seed, tier, is_boss);

    /* Load graphics — act-themed palettes for visual variety */
    int act = (int)quest_state.current_act;
    networld_load_tileset(act);
    networld_load_parallax(seed, act);

    /* Initialize player at level spawn.
     * player_init() memsets player_state to 0, so save/restore persistent fields. */
    {
        u8  saved_class   = player_state.player_class;
        u8  saved_level   = player_state.level;
        u16 saved_xp      = player_state.xp;
        u16 saved_credits = player_state.credits;
        u8  saved_unlocks = player_state.ability_unlocks;
        /* Save combat stats to preserve shop upgrade bonuses across jack-ins */
        s16 saved_max_hp  = player_state.max_hp;
        s16 saved_atk     = player_state.atk;
        s16 saved_def     = player_state.def;
        s16 saved_spd     = player_state.spd;
        s16 saved_lck     = player_state.lck;
        int had_stats     = (saved_atk > 0); /* 0 = first jack-in, uninitialized */

        player_init((int)saved_class);

        /* Restore persistent fields wiped by player_init's memset */
        player_state.level           = saved_level;
        player_state.xp              = saved_xp;
        player_state.credits         = saved_credits;
        player_state.ability_unlocks = saved_unlocks;

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

    /* Start music based on contract */
    if (is_boss) {
        audio_play_music(MUS_BOSS);
    } else if (tier <= 2) {
        audio_play_music(MUS_NET_EASY);
    } else if (tier <= 4) {
        audio_play_music(MUS_NET_HARD);
    } else {
        audio_play_music(MUS_NET_FINAL);
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

    /* Update projectiles */
    projectile_update_all();

    /* Check enemy projectiles vs player */
    check_projectile_vs_player();

    /* Check player projectiles vs enemies/boss */
    enemy_check_player_attack(p);
    boss_check_player_attack(p);

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

    /* Track kill-based contract progress */
    enemies_killed = enemy_get_kills();
    /* Sync kill count to contract for pause menu display */
    {
        Contract* kc = quest_get_active();
        if (kc) kc->kills = (enemies_killed > 255) ? 255 : (u8)enemies_killed;
    }

    /* Wave respawning for survival/exterminate contracts */
    {
        Contract* ac = quest_get_active();
        if (ac && (ac->type == CONTRACT_SURVIVAL || ac->type == CONTRACT_EXTERMINATE)) {
            /* Only respawn if player hasn't met kill target yet */
            if (enemies_killed < ac->kill_target) {
                wave_timer--;
                if (wave_timer <= 0) {
                    wave_timer = WAVE_INTERVAL;
                    int alive = enemy_count_alive();
                    if (alive < WAVE_MIN_ALIVE) {
                        /* Spawn 2-4 new enemies near the player's area */
                        int spawn_count = 2 + (int)rand_range(3);
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
    }

    /* Update item drops */
    itemdrop_update_all();

    /* Check item pickups */
    if (p) {
        itemdrop_check_pickup(p->x, p->y);
    }

    /* Ability updates handled inside player_update() */

    /* Check exit — contract conditions must be met */
    if (check_exit() && !level_complete) {
        if (quest_check_exit_condition(enemies_killed, contract_boss_defeated)) {
            level_complete = 1;
            sub_state = NETSUB_EXIT_ANIM;
            timer = 60;
            audio_play_sfx(SFX_LEVEL_DONE);
            hud_notify("LEVEL CLEAR!", 60);
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

    /* Update HUD notification */
    hud_notify_update();
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

    case NETSUB_EXIT_ANIM:
        timer--;
        hud_notify_update();
        if (timer <= 0) {
            /* Complete contract */
            Contract* c = quest_get_active();
            int was_final_act = 0;
            if (c) {
                /* Check if this is act 5 story completion */
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
        if (timer <= 0) {
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

    /* Draw projectiles */
    projectile_draw_all(cam.x, cam.y);

    /* Draw item drops */
    itemdrop_draw_all(cam.x, cam.y);

    /* HUD always on top */
    hud_draw();

    /* Boss/Story mission tag in top-right */
    if (sub_state == NETSUB_PLAY || sub_state == NETSUB_FADEIN) {
        Contract* tag_c = quest_get_active();
        if (tag_c && tag_c->story_mission > 0) {
            if (tag_c->type == CONTRACT_STORY) {
                text_print(25, 0, "BOSS");
            } else {
                text_print(24, 0, "STORY");
            }
        }
    }

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
    itemdrop_clear_all();

    /* Hide all OAM sprites so they don't bleed into non-gameplay states */
    for (int i = 0; i < SPRITE_MAX; i++) {
        OBJ_ATTR* spr = sprite_get(i);
        if (spr) obj_hide(spr);
    }

    /* Clear blend registers */
    REG_BLDCNT = 0;
    REG_BLDY = 0;

    /* Reset BG scroll */
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;
    REG_BG2HOFS = 0;
    REG_BG2VOFS = 0;
}
