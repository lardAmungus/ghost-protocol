#ifndef ENGINE_SAVE_H
#define ENGINE_SAVE_H

#include <tonc.h>
#include "game/loot.h"

#define SAVE_MAGIC 0x6750726F  /* "gPro" — Ghost Protocol */
#define SAVE_SLOTS 3
#define SAVE_SLOT_SIZE 512

/*
 * SaveData — 512 bytes per slot, 3 slots in SRAM.
 * Stores full player state, inventory, quest progress, shop state.
 */
typedef struct {
    u32 magic;              /* Validates save exists (4) */
    u16 checksum;           /* CRC-16 of remaining fields (2) */
    /* Player core */
    u8  player_class;       /* CLASS_ASSAULT/INFILTRATOR/TECHNOMANCER (1) */
    u8  player_level;       /* 1-20 (1) */
    s16 player_hp;          /* Current HP (2) */
    s16 player_max_hp;      /* Max HP (2) */
    u16 player_xp;          /* Current XP (2) */
    s16 player_atk;         /* (2) */
    s16 player_def;         /* (2) */
    s16 player_spd;         /* (2) */
    s16 player_lck;         /* (2) */
    u16 credits;            /* Currency (2) */
    u8  ability_unlocks;    /* Bitmask (1) */
    /* Quest progress */
    u8  quest_act;          /* Story act 0-5 (1) */
    u8  story_mission;      /* Completed story missions 0-20 (1) */
    u8  boss_defeated[6];   /* Which bosses beaten (6) */
    u8  contracts_completed;/* Total side contracts done (1) */
    /* Equipment */
    u8  equipped_idx;       /* Which inventory slot is equipped (1) */
    u8  inventory_count;    /* Number of items (1) */
    /* Shop */
    u8  shop_purchases[6];  /* Purchase counts per item (6) */
    /* Padding to align inventory */
    u8  pad[1];             /* (1) — total header: 42 bytes */
    /* Inventory: 20 items x 8 bytes = 160 bytes */
    LootItem inventory[INVENTORY_SIZE]; /* (160) */
    /* Bug bounty persistence */
    u16 bb_high_scores[5];  /* Per-tier high scores (10) */
    u8  bb_highest_unlocked;/* Highest tier unlocked 0-4 (1) */
    u8  bb_total_runs;      /* Total completed runs (1) */
    /* Skill tree & evolution (Phase 1) */
    u8  skill_tree[12];     /* Skill ranks (12 skills x rank 0-3) (12) */
    u8  evolution;          /* EVOLUTION_* (1) */
    u8  skill_points;       /* Unspent skill points (1) */
    /* Crafting (Phase 2) */
    u16 craft_shards;       /* Crafting currency (2) */
    /* Progression (Phase 6) */
    u8  ng_plus;            /* 0=normal, 1+=NG+ cycle (1) */
    u8  choice_flags;       /* Story choice bits (2 bits x 4 choices) (1) */
    u8  codex_unlocks[32];  /* Bitfield for 256 codex entries (32) */
    u8  achievements[3];    /* 24 achievement bits (3) */
    /* Statistics */
    u32 play_time_frames;   /* Total play time in frames (4) */
    u16 total_kills;        /* Enemies killed (2) */
    u16 total_deaths;       /* Player deaths (2) */
    u16 damage_dealt;       /* Total damage dealt (capped at 65535) (2) */
    u16 damage_taken;       /* Total damage taken (capped at 65535) (2) */
    u16 items_found;        /* Loot items found (2) */
    u16 items_crafted;      /* Items crafted (2) */
    u8  highest_combo;      /* Longest kill streak (1) */
    u8  contracts_done;     /* Side contracts completed (1) */
    /* Endgame */
    u16 bb_endgame_contracts; /* Total endless contracts completed (2) */
    u8  bb_boss_contracts;  /* Boss contracts completed (1) */
    u8  bb_threat_level;    /* Permanent difficulty scaling (1) */
    u8  bb_highest_level;   /* Highest player level reached (1) */
    u8  endgame_unlocked;   /* 1=endless mode available (1) */
    /* Reserved for future use */
    u8  reserved[222];      /* Pad to 512 total */
} SaveData;                 /* 512 bytes */

/* Write save data to a slot (0-2). */
void save_write_slot(SaveData* data, int slot);

/* Read save data from a slot (0-2). Returns 1 if valid. */
int save_read_slot(SaveData* data, int slot);

/* Fill with default values. */
void save_defaults(SaveData* data);

/* Check if a slot has valid save data (without full read). */
int save_slot_exists(int slot);

/* Legacy API (slot 0) */
void save_write(SaveData* data);
int save_read(SaveData* data);

#endif /* ENGINE_SAVE_H */
